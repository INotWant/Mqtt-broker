#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <errno.h>

#include "broker.h"
#include "define.h"
#include "message.h"
#include "util.h"
#include "handler.h"

Broker broker;
HashTable shm_num_table;            /* key:clientID+messageId，value:number。用于记录共享内存的使用，为 0 时清除对应共享内存 */
pthread_mutex_t shm_table_mutex;    /* 互斥锁，对 shm_num_table 的互斥操作 */

/**
 * Broker 初始化
 */
void initBroker() {
    //0）初始化随机数种子
    time_t t;
    srand((unsigned) time(&t));

    // 1）初始化 broker
    // 1.1）为订阅树创建 # 与 / 结点
    char curr_name = '#';
    broker.p_sub_tree_root = createSubscribeTreeNode(&curr_name, 1);
    curr_name = '+';
    broker.p_sub_tree_root->p_brother = createSubscribeTreeNode(&curr_name, 1);
    // 1.2）初始化 session_table
    hashTableNew(&broker.session_table, SESSION_TABLE_SIZE, freeSession);
    // 1.3）初始化 retain_message_list
    listNew(&broker.retain_message_list, RETAIN_MESSAGE_LIST_SIZE, sizeof(Message), deleteMessage);
    // 1.4）初始化 server_socket
    broker.server_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (-1 == broker.server_sock) {
        perror("[Error] 1 ");
        exit(ERR_SOCKET);
    }
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(struct sockaddr_in));
    server_addr.sin_family = AF_INET;                       /* IPv4 */
    server_addr.sin_addr.s_addr = inet_addr(IP);            /* IP地址 */
    server_addr.sin_port = htons(PORT);                     /* 端口 */
    if (-1 == bind(broker.server_sock, (struct sockaddr *) &server_addr, sizeof(server_addr))) {
        perror("[Error] 2 ");
        exit(ERR_SOCKET);
    }
    // 2）初始化 shm_num_table
    hashTableNew(&shm_num_table, SHM_NUM_TABLE_SIZE, NULL);
    // 3）初始化 shm_table_mutex
    pthread_mutex_init(&shm_table_mutex, NULL);
    printf("[Info]: initialize broker successfully.\n");
}

/**
 * 超时信号处理
 */
void sigTimeoutHandler(union sigval v) {
    fprintf(stderr, "[Error]: time out.(enter)\n");
    Session *p_session = v.sival_ptr;
    if (NULL != p_session) {
        close(p_session->client_sock);
    }
    fprintf(stderr, "[Error]: time out.(exit)\n");
}

/**
 * 读取一个完整的报文（注：释放 *pp_message 所指向的内存）
 * @param client_sock
 * @param p_session
 * @param pp_message
 * @param p_fh
 * @return 此报文所占字节数（返回，0 表示什么都没有读到；-1 表示异常，连接需断开；-2 表有消息待发布）
 */
int readAMessage(int client_sock, Session *p_session, char **pp_message, FixedHeader *p_fh) {
    // 1）预：设置定时器
    int time_interval = 0;      /* 时间间隔 */
    if (NULL != p_session) {
        time_interval = 15 * p_session->ping_time / 10;
    } else {
        time_interval = TIME_INTERVAL;
    }
    struct sigevent evp;
    memset(&evp, 0, sizeof(evp));
    evp.sigev_value.sival_ptr = p_session;      /* 携带参数 */
    evp.sigev_notify = SIGEV_THREAD;            /* 定时器到期后，发送信号 */
    evp.sigev_notify_function = sigTimeoutHandler;

    timer_t timer_id;
    if (-1 == timer_create(CLOCK_REALTIME, &evp, &timer_id)) {  /* 构建定时器 */
        perror("[Error] 3 ");
        return -1;
    }
    struct itimerspec ts;
    ts.it_value.tv_sec = time_interval;
    ts.it_value.tv_nsec = 0;
    ts.it_interval = ts.it_value;
    timer_settime(timer_id, CLOCK_REALTIME, &ts, NULL);          /* 开始定时器 */

    // 2）读取逻辑
    char buf_array[5];
    int read_num = 0;
    bool is_continue;
    ssize_t read_once_num;
    read_once_num = read(client_sock, buf_array + read_num, 1);      /* 注意粘包问题，故每次只读 1 字节 */
    if (-1 == read_once_num) {
        if (EINTR == errno) {           /* 若成立，表明是“信号”造成的中断 */
            if (NULL != p_session) {
                pthread_mutex_lock(&p_session->state_mutex);
                if (1 == p_session->state) {      /* 有消息发布 */
                    p_session->state = 0;
                    pthread_mutex_unlock(&p_session->state_mutex);
                    timer_delete(timer_id);
                    return -2;
                }
                pthread_mutex_unlock(&p_session->state_mutex);
            }
        } else {
            perror("[Error] 3 ");
            return -1;
        }
    }
    read_num += read_once_num;
    do {
        read_once_num = read(client_sock, buf_array + read_num, 1);
        if (-1 == read_once_num) {
            if (EINTR == errno) {        /* 若成立，表明是“信号”造成的中断 */
                if (NULL != p_session) {
                    pthread_mutex_lock(&p_session->state_mutex);
                    if (1 == p_session->state) {
                        p_session->state = 0;
                        pthread_mutex_unlock(&p_session->state_mutex);
                        continue;
                    }
                    pthread_mutex_unlock(&p_session->state_mutex);
                }
            } else {
                perror("[Error] 5 ");
                timer_delete(timer_id);
                return -1;
            }
        }
        read_num += read_once_num;
        is_continue = (bool) getCharNBit(&buf_array[read_num - 1], 7);
    } while (is_continue && read_num < 5);
    int message_len = 0;
    parseFixedHeader(buf_array, p_fh);
    message_len = p_fh->fh_len + p_fh->remain_len;
    *pp_message = malloc((size_t) message_len);
    memcpy(*pp_message, buf_array, (size_t) p_fh->fh_len);      /* 读取固定报头 */
    while (read_num < message_len) {                            /* 读取其他 */
        read_once_num = read(client_sock, *pp_message + read_num, (size_t) (message_len - read_num));
        if (-1 == read_once_num) {
            if (EINTR == errno) {        /* 若成立，表明是“信号”造成的中断 */
                if (NULL != p_session) {
                    pthread_mutex_lock(&p_session->state_mutex);
                    if (1 == p_session->state) {
                        p_session->state = 0;
                        pthread_mutex_unlock(&p_session->state_mutex);
                        continue;
                    }
                    pthread_mutex_unlock(&p_session->state_mutex);
                }
            } else {
                perror("[Error] 6 ");
                timer_delete(timer_id);
                return -1;
            }
        }
        read_num += read_once_num;
    }
    // 3）善后：删除定时器
    timer_delete(timer_id);
    return message_len;
}

/**
 * 订阅消息待发布信号处理
 */
void sigPublishHandler(int sig, siginfo_t *info, void *act) {
    printf("[Info]: waiting for publishing.(enter)\n");
    Session *p_session = info->si_value.sival_ptr;
    pthread_mutex_lock(&p_session->state_mutex);
    p_session->state = 1;
    pthread_mutex_unlock(&p_session->state_mutex);
    printf("[Info]: waiting for publishing.(exit)\n");
}

/**
 * 为每一个“连接”，创建一个新的线程。此函数为线程执行逻辑
 * @param args
 * @return
 */
void *runForAccept(void *args) {
    int client_sock = (int) args;
    Session *p_session = NULL;
    // 预：信号相关（处理发布的相关信号）
    struct sigaction act;
    sigemptyset(&act.sa_mask);
    act.sa_sigaction = sigPublishHandler;
    act.sa_flags = SA_SIGINFO;
    sigaction(SIGUSR1, &act, NULL);                /* 设置对 SIGALRM 信号的处理 */

    while (true) {
        // 0）读取共享内存，查看是否有需要发布的消息
        if (NULL != p_session) {
            handlerSend(p_session, &shm_num_table, &shm_table_mutex);
        }
        // 1）读取一个完整报文，并存入 p_message 中
        char *p_message;
        FixedHeader fh;
        int ret = readAMessage(client_sock, p_session, &p_message, &fh);
        if (-1 == ret) {      /* 断开连接 */
            if (NULL != p_session) {
                if (p_session->is_clean_session) {
                    printf("[Info]: clean client(%s).\n", p_session->p_client_id);
                    char sock_chars[4];
                    int2FourChar(client_sock, sock_chars);
                    hashTableRemove(&broker.session_table, sock_chars, 4);
                }
            }
            printf("[Info]: thread exit.\n");
            return 0;
        } else if (ret <= 0) {
            continue;
        }
        if (NULL != p_session) {
            p_session->last_req_time = getCurrentTime();    /* 更新上一次通信时间 */
        }
        // 2）根据报文控制类型做相应的响应
        switch (fh.message_type) {
            case CONNECT: {
                ConnVariableHeader conn_vh;
                ConnPayload conn_payload;
                parseConnect(p_message, &fh, &conn_vh, &conn_payload);

                printf("[Info]: Get a connect, clientId is ");
                printNchar(conn_payload.p_client_id, conn_payload.client_id_len);
                printf(".\n");

                handlerConnect(client_sock, &p_session, &broker, &conn_vh, &conn_payload);

                free(p_message);
                if (NULL == p_session) {                /* 由于版本问题未创建连接，关闭连接，退出线程 */
                    fprintf(stderr, "[Error]: unacceptable protocol version, close sock(%d).\n", client_sock);
                    printf("[Info]: close socket(%d).\n", client_sock);
                    close(client_sock);
                    return 0;
                }
                break;
            }
            case PUBLISH: {
                PublishVariableHeader pub_vh;
                PublishPayload pub_payload;
                parsePublish(p_message, &fh, &pub_vh, &pub_payload);

                printf("[Info]: Get a publish(%d), topic is ", pub_vh.message_id);
                printNchar(pub_vh.p_topic, pub_vh.topic_len);
                printf(", qos is %d.\n", fh.qos);

                handlerPublish(p_session, &broker, &fh, &pub_vh, &pub_payload, &shm_num_table, &shm_table_mutex);

                free(p_message);
                break;
            }
            case PUBACK: {
                CommonVariableHeader vh;
                parseCommonVariableHeader(p_message + 2, &vh);

                printf("[Info]: Get a puback about client(%s).\n", p_session->p_client_id);
                handlerPuback(p_session, vh.message_id);

                free(p_message);
                break;
            }
            case PUBREC: {
                CommonVariableHeader vh;
                parseCommonVariableHeader(p_message + 2, &vh);

                printf("[Info]: Get a pubrec about client(%s).\n", p_session->p_client_id);
                handlerPubrec(p_session, vh.message_id);

                free(p_message);
                break;
            }
            case PUBREL: {
                CommonVariableHeader vh;
                parseCommonVariableHeader(p_message + 2, &vh);

                printf("[Info]: Get a pubrel about client(%s).\n", p_session->p_client_id);
                handlerPubrel(p_session, vh.message_id);

                free(p_message);
                break;
            }
            case PUBCOMP: {
                CommonVariableHeader vh;
                parseCommonVariableHeader(p_message + 2, &vh);

                printf("[Info]: Get a pubcomp about client(%s).\n", p_session->p_client_id);
                handlerPubcomp(p_session, vh.message_id);

                free(p_message);
                break;
            }
            case SUBSCRIBE: {
                CommonVariableHeader vh;
                SubscribePayload sub_payload;
                parseSubscribe(p_message, &fh, &vh, &sub_payload);

                printf("[Info]: get a subscribe of client(");
                printNchar(p_session->p_client_id, p_session->client_id_len);
                printf("). Topic filter has:\n\t");
                for (int i = 0; i < sub_payload.topic_filter_len; ++i) {
                    if (i != sub_payload.topic_filter_len - 1) {
                        printf("%s ", sub_payload.p_topic_filter[i]);
                    } else {
                        printf("%s.\n", sub_payload.p_topic_filter[i]);
                    }
                }

                handlerSubscribe(p_session, &broker, &fh, &vh, &sub_payload);

                free(p_message);
                break;
            }
            case UNSUBSCRIBE: {
                CommonVariableHeader vh;
                UnsubscribePayload unsub_payload;
                parseUnsubscribe(p_message, &fh, &vh, &unsub_payload);

                printf("[Info]: get a unsubscribe of client(");
                printNchar(p_session->p_client_id, p_session->client_id_len);
                printf("). Topic filter has:\n\t");
                for (int i = 0; i < unsub_payload.topic_filter_len; ++i) {
                    if (i != unsub_payload.topic_filter_len - 1) {
                        printf("%s ", unsub_payload.p_topic_filter[i]);
                    } else {
                        printf("%s.\n", unsub_payload.p_topic_filter[i]);
                    }
                }

                handlerUnsubscribe(p_session, &broker, &fh, &vh, &unsub_payload);

                free(p_message);
                break;
            }
            case PINGREQ: {
                printf("[Info]: get a pingreq of client(");
                printNchar(p_session->p_client_id, p_session->client_id_len);
                printf(").\n");

                handlerPingreq(p_session);

                free(p_message);
                break;
            }
            case DISCONNECT: {
                printf("[Info]: get a disconnect of client(");
                printNchar(p_session->p_client_id, p_session->client_id_len);
                printf(").\n");

                handlerDisconnect(p_session, &broker);

                free(p_message);
                return 0;         /* 退出线程 */
            }
            default: {
                fprintf(stderr, "[Error]: error message type.");

                free(p_message);
                break;
            }
        }
    }
}

/**
 * 启动 Broker
 */
void startBroker() {
    printf("[Info]: start broker.\n");

    listen(broker.server_sock, BACKLOG);    /* 开始监听 */
    struct sockaddr_in client_addr;
    socklen_t client_addr_size = sizeof(client_addr);

    int ret;
    pthread_t p_id;                                                 /* 存放线程 ID */
    pthread_attr_t attr;                                            /* 创建线程属性 */
    pthread_attr_init(&attr);                                       /* 初始化 */
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);    /* 设置为分离 */
    pthread_attr_setstacksize(&attr, THREAD_STACK_SIZE);

    while (true) {
        int client_sock = accept(broker.server_sock, (struct sockaddr *) &client_addr, &client_addr_size);  /* 接受请求 */
        if (-1 == client_sock) {
            perror("[Error] 7 ");
            continue;
        }
        printf("[Info]: accept a new client.\n");
        ret = pthread_create(&p_id, &attr, runForAccept, (void *) client_sock);
        if (ret) {
            fprintf(stderr, "[Error]: create a new thread failed for client_sock(%d). this socket is closed.\n",
                    client_sock);
            close(client_sock);
            continue;
        } else {
            printf("[Info]: create a new thread(%u) successfully for client_sock(%d).\n", (unsigned int) p_id,
                   client_sock);
        }
    }
}

int main() {
    initBroker();
    startBroker();
    return 0;
}