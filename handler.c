//
// Created by iwant on 2019/1/25.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/shm.h>
#include <signal.h>
#include <pthread.h>
#include "handler.h"
#include "util.h"
#include "define.h"


/******************** 一些声明 ********************/

int writeHandler(int client_sock, char *p_message, int message_len);

int compareMessageIdHf(void *p_curr, void *p_value);

int compareLevelHf(void *p_curr, void *p_value);

int attrMessageIdHf(void *p_curr, void *p_attr);

/**
 * 用于 runForPublish 传值
 */
typedef struct _ArgData {
    Broker *p_broker;
    int qos;
    PublishVariableHeader vh;
    PublishPayload payload;
    HashTable *p_shm_num_table;
    pthread_mutex_t *p_shm_table_mutex;
} ArgData;

void *runForPublish(void *args);

/******************** 分割线 ********************/

/**
 * 响应 CONNECT
 * @param client_sock
 * @param pp_session
 * @param p_broker
 * @param p_vh
 * @param p_payload
 */
void handlerConnect(int client_sock, Session **pp_session, Broker *p_broker, ConnVariableHeader *p_vh,
                    ConnPayload *p_payload) {
    // 0）判断连接是否合法
    // TODO for test, uncheck
    /*
    char return_code;
    if (4 != p_vh->protocol_name_len || 4 != p_vh->level) {
        return_code = 0x01;
    } else {
        char protocol_name[5];
        protocol_name[4] = '\0';
        memcpy(protocol_name, p_vh->p_protocol_name, (size_t) p_vh->protocol_name_len);
        if (0 != strcpy("MQTT", protocol_name)) {
            return_code = 0x01;
        } else {
            return_code = 0x00;
        }
    }
    */

    char return_code = 0x00;

    Session *p_session = NULL;
    if (return_code == 0x00) {
        // 1）根据 client_id 查找“会话”
        char client_id[p_payload->client_id_len + 1];
        client_id[p_payload->client_id_len] = '\0';
        memcpy(client_id, p_payload->p_client_id, (size_t) p_payload->client_id_len);
        kv *p_select_kv = NULL;
        for (int i = 0; i < p_broker->session_table.table_size && NULL == p_select_kv; ++i) {
            kv *p_curr_kv = p_broker->session_table.table[i];
            while (NULL != p_curr_kv) {
                if (((Session *) p_curr_kv->value)->client_id_len == p_payload->client_id_len) {
                    if (0 == memcmp(((Session *) p_curr_kv->value)->p_client_id, client_id,
                                    (size_t) p_payload->client_id_len)) {
                        p_select_kv = p_curr_kv;
                        break;
                    };
                }
                p_curr_kv = p_curr_kv->p_next;
            }
        }
        // 2）构建或者恢复“会话”
        if (NULL != p_select_kv) {   /* 存在之前的会话 */
            p_session = p_select_kv->value;
            if (p_vh->is_clean_session) {
                hashTableRemove(&p_broker->session_table, p_select_kv->key, 4);
                p_session = createSession(p_payload->p_client_id, p_payload->client_id_len, client_sock,
                                          p_vh->is_clean_session, p_vh->ping_time, pthread_self());

                printf("[Info]: create a session for client(%s).\n", p_session->p_client_id);

                char sock_chars[4];
                int2FourChar(client_sock, sock_chars);
                hashTablePut(&p_broker->session_table, sock_chars, 4, p_session);
            } else {
                p_session->client_sock = client_sock;
                p_session->t_id = pthread_self();
                p_session->last_req_time = getCurrentTime();
                p_session->state = 0;
                isOnline(p_session);

                printf("[Info]: recover a session for client(%s).\n", p_session->p_client_id);
            }
        } else {                     /* 不存在之前的会话 */
            p_session = createSession(p_payload->p_client_id, p_payload->client_id_len, client_sock,
                                      p_vh->is_clean_session,
                                      p_vh->ping_time, pthread_self());

            printf("[Info]: create a session for client(%s).\n", p_session->p_client_id);

            char sock_chars[5];
            sock_chars[4] = '\0';
            int2FourChar(client_sock, sock_chars);
            hashTablePut(&p_broker->session_table, sock_chars, 4, p_session);
        }
    }
    *pp_session = p_session;
    // 3）发送 CONNACK
    // 3.1）封包
    int message_len = 4;
    char p_message[message_len];
    packageConnack(p_message, p_vh->is_clean_session, return_code);
    // 3.2）发送
    if (-1 == writeHandler(client_sock, p_message, message_len)) {
        fprintf(stderr, "[Error]: send connack error about sock(%d).\n", client_sock);
    } else {
        printf("[Info]: send connack success about sock(%d).\n", client_sock);
    }
    if (NULL != p_session) {
        // 4）查看是否有之前未确认的消息，若有则发送。先发 PUBLISH 再发 PUBREL
        for (int j = 0; j < listElementNum(&p_session->publish_message_list); ++j) {        /* 发送 PUBLISH */
            PublishMessage *p_pm = (PublishMessage *) (p_session->publish_message_list.element_head +
                                                       j * p_session->publish_message_list.element_size);
            int remain_len = 2 + p_pm->topic_len + 2 + p_pm->content_len;
            FixedHeader fh;
            fh.message_type = PUBLISH;
            fh.flag = (char) (8 + p_pm->qos * 2);
            fh.remain_len = remain_len;
            fh.fh_len = 1;
            if (remain_len <= 127) {
                fh.fh_len += 1;
            } else if (remain_len <= 16383) {
                fh.fh_len += 2;
            } else if (remain_len <= 2097151) {
                fh.fh_len += 3;
            } else {
                fh.fh_len += 4;
            }
            char p_publish_message[fh.fh_len + remain_len];
            packagePublish(p_publish_message, &fh, p_pm->p_topic, p_pm->topic_len, p_pm->message_id, p_pm->p_content,
                           p_pm->content_len);
            if (-1 == writeHandler(p_session->client_sock, p_publish_message, fh.fh_len + remain_len)) {
                fprintf(stderr, "[Error]: resend publish(%d) to client(%s) failed.\n", p_pm->message_id,
                        p_session->p_client_id);
            } else {
                printf("[Info]: resend publish(%d) to client(%s) successfully.\n", p_pm->message_id,
                       p_session->p_client_id);
            }
        }
        for (int k = 0; k < listElementNum(&p_session->message_id_s_list); ++k) {           /* 发送 PUBREL */
            int *p_message_id = (int *) (p_session->publish_message_list.element_head +
                                         k * p_session->publish_message_list.element_size);
            char p_pubrel_message[4];
            FixedHeader fh;
            fh.message_type = PUBREL;
            fh.flag = 0x00;
            fh.remain_len = 2;
            fh.fh_len = 2;
            packageAck(p_pubrel_message, &fh, *p_message_id);
            if (-1 == writeHandler(p_session->client_sock, p_pubrel_message, 4)) {
                fprintf(stderr, "[Error]: resend pubrel(%d) to client(%s) failed.\n", *p_message_id,
                        p_session->p_client_id);
            } else {
                printf("[Info]: resend pubrel(%d) to client(%s) successfully.\n", *p_message_id,
                       p_session->p_client_id);
            }
        }
        listFlush(&p_session->publish_message_list);
        listFlush(&p_session->message_id_s_list);
    }
}

/**
 * 响应 PUBLISH
 * @param p_session
 * @param p_broker
 * @param p_fh
 * @param p_vh
 * @param p_payload
 * @param p_shm_table
 * @param p_shm_table_mutex
 */
void
handlerPublish(Session *p_session, Broker *p_broker, FixedHeader *p_fh, PublishVariableHeader *p_vh,
               PublishPayload *p_payload, HashTable *p_shm_table, pthread_mutex_t *p_shm_table_mutex) {
    // 0）通过 message_id（报文标识符） 判断是否是新的发布
    if (listExistElement(&p_session->message_id_r_list, &p_vh->message_id, compareMessageIdHf)) {
        printf("[Info]: get a old publish(%d) about client(%s).\n", p_vh->message_id, p_session->p_client_id);
        return;
    }

    // 总：响应逻辑使用 主线程，分发逻辑使用 新线程
    // 【新线程】：分发逻辑
    ArgData *p_arg_data = malloc(sizeof(ArgData));      /* 给新的线程传参 */
    p_arg_data->p_broker = p_broker;
    p_arg_data->qos = p_fh->qos;
    p_arg_data->vh.message_id = p_vh->message_id;
    p_arg_data->vh.topic_len = p_vh->topic_len;
    p_arg_data->vh.p_topic = malloc((size_t) p_vh->topic_len + 1);
    p_arg_data->vh.p_topic[p_vh->topic_len] = '\0';
    memcpy(p_arg_data->vh.p_topic, p_vh->p_topic, (size_t) p_vh->topic_len);
    p_arg_data->payload.content_len = p_payload->content_len;
    p_arg_data->payload.p_content = malloc((size_t) p_payload->content_len);
    memcpy(p_arg_data->payload.p_content, p_payload->p_content, (size_t) p_payload->content_len);
    p_arg_data->p_shm_num_table = p_shm_table;
    p_arg_data->p_shm_table_mutex = p_shm_table_mutex;

    int ret;
    pthread_t p_id;                                                 /* 存放线程 ID */
    pthread_attr_t attr;                                            /* 创建线程属性 */
    pthread_attr_init(&attr);                                       /* 初始化 */
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);    /* 设置为分离 */
    pthread_attr_setstacksize(&attr, THREAD_STACK_SIZE);
    ret = pthread_create(&p_id, &attr, runForPublish, p_arg_data);
    if (ret) {
        fprintf(stderr, "[Error]: create a new thread failed for publishing about client_sock(%d).\n",
                p_session->client_sock);
    } else {
        printf("[Info]: create a new thread successfully for publishing about client_sock(%d).\n",
               p_session->client_sock);
    }
    // 【主线程】：响应逻辑（继续）
    char p_message[4];
    if (1 == p_fh->qos) {           /* qos 1 */
        // 1）封包
        FixedHeader fh;
        fh.message_type = PUBACK;
        fh.flag = 0x00;
        fh.remain_len = 2;
        packageAck(p_message, &fh, p_vh->message_id);
        // 2）发送
        if (-1 == writeHandler(p_session->client_sock, p_message, 4)) {
            // 即使发送失败，broker 也不做处理，交给 client 去做（客户端重连后再次发送此 PUBLISH）。
            fprintf(stderr, "[Error]: send puback(%d) to client(%s) failed.\n", p_vh->message_id,
                    p_session->p_client_id);
        } else {
            printf("[Info]: send puback(%d) to client(%s) successfully.\n", p_vh->message_id,
                   p_session->p_client_id);
        }
    } else if (2 == p_fh->qos) {    /* qos 2 */
        // 1）保存未确认的 message_id （作为接收者）
        listInsertElement(&p_session->message_id_r_list, &p_vh->message_id);
        // 2）封包: PUBREC
        FixedHeader fh;
        fh.message_type = PUBREC;
        fh.flag = 0x00;
        fh.remain_len = 2;
        packageAck(p_message, &fh, p_vh->message_id);
        // 3）发送
        if (-1 == writeHandler(p_session->client_sock, p_message, 4)) {
            // 即使发送失败，broker 也不做处理，交给 client 去做（客户端重连后再次发送此 PUBLISH）。
            fprintf(stderr, "[Error]: send pubrec(%d) to client(%s) failed.\n", p_vh->message_id,
                    p_session->p_client_id);
        } else {
            printf("[Info]: send pubrec(%d) to client(%s) successfully.\n", p_vh->message_id,
                   p_session->p_client_id);
        }
    }
}

/**
 * 响应 PUBACK
 * @param p_session
 * @param message_id
 */
void handlerPuback(Session *p_session, int message_id) {
    // 唯一动作：丢弃确认的消息
    listDeleteElementByAttr(&p_session->publish_message_list, &message_id, attrMessageIdHf);
}

/**
 * 响应 PUBREC
 * @param p_session
 * @param message_id
 */
void handlerPubrec(Session *p_session, int message_id) {
    // 1）丢弃已确认的消息
    listDeleteElementByAttr(&p_session->publish_message_list, &message_id, attrMessageIdHf);
    // 2）存储报文标识符
    listInsertElement(&p_session->message_id_s_list, &message_id);
    // 3）封包 发送
    char p_message[4];
    FixedHeader fh;
    fh.message_type = PUBREL;
    fh.flag = 0x00;
    fh.remain_len = 2;
    packageAck(p_message, &fh, message_id);
    if (-1 == writeHandler(p_session->client_sock, p_message, 4)) {
        fprintf(stderr, "[Error]: send pubrel(%d) to client(%s) failed.\n", message_id, p_session->p_client_id);
    } else {
        printf("[Info]: send pubrel(%d) to client(%s) successfully.\n", message_id, p_session->p_client_id);
    }
}

/**
 * 响应 pubrel
 * @param p_session
 * @param message_id
 */
void handlerPubrel(Session *p_session, int message_id) {
    // 1）丢弃未确认的 message_id （作为接收者）
    listDeleteElement(&p_session->message_id_r_list, &message_id);
    // 2）发送 PUBCOMP
    char p_message[4];
    FixedHeader fh;
    fh.message_type = PUBCOMP;
    fh.flag = 0x00;
    fh.remain_len = 2;
    packageAck(p_message, &fh, message_id);
    // 3）发送
    if (-1 == writeHandler(p_session->client_sock, p_message, 4)) {
        // 即使发送失败，broker 也不做处理，交给 client 去做（客户端重发 PUBREL 报文）
        fprintf(stderr, "[Error]: send pubcomp(%d) to client(%s) failed.\n", message_id, p_session->p_client_id);
    } else {
        printf("[Info]: send pubcomp(%d) to client(%s) successfully.\n", message_id, p_session->p_client_id);
    }
}

/**
 * 响应 pubcomp
 * @param p_session
 * @param message_id
 */
void handlerPubcomp(Session *p_session, int message_id) {
    // 唯一动作：丢弃相应的报文标识符
    listDeleteElement(&p_session->message_id_s_list, &message_id);
}

/**
 * 响应 SUBSCRIBE
 * @param p_session
 * @param p_broker
 * @param p_fh
 * @param p_vh
 * @param p_sub_payload
 */
void handlerSubscribe(Session *p_session, Broker *p_broker, FixedHeader *p_fh, CommonVariableHeader *p_vh,
                      SubscribePayload *p_sub_payload) {
    // 1）处理订阅，并构建“返回码”
    char return_code_array[p_sub_payload->topic_filter_len];
    if (0x02 != p_fh->flag) {
        fprintf(stderr, "[Error]: Illegal flag in process of processing subscribe.\n");
        for (int i = 0; i < p_sub_payload->topic_filter_len; ++i) {
            return_code_array[i] = (char) 0x80;
        }
    } else {
        int topic_filter_level;
        char **p_topic_filter;
        for (int i = 0; i < p_sub_payload->topic_filter_len; ++i) {
            splitTopic(p_sub_payload->p_topic_filter[i], (int) strlen(p_sub_payload->p_topic_filter[i]),
                       &p_topic_filter,
                       &topic_filter_level);
            SubscribeListNode sub_list_node;
            sub_list_node.level = topic_filter_level;
            SubscribeListNode *p_sub_list_node = listInsertElementSort(&p_session->sub_list, &sub_list_node,
                                                                       compareLevelHf);
            p_sub_list_node->p_sub_tree_node = insertSubscribeTreeNode(p_broker->p_sub_tree_root, p_topic_filter,
                                                                       topic_filter_level,
                                                                       p_sub_payload->p_topic_filter_qos[i], p_session,
                                                                       p_sub_list_node, return_code_array + i);
            // 善后
            for (int j = 0; j < topic_filter_level; ++j) {
                free(p_topic_filter[j]);
            }
            free(p_topic_filter);
        }
    }
    // 2）封包 发送
    int remain_len = 2 + p_sub_payload->topic_filter_len;
    FixedHeader fh;
    fh.message_type = SUBACK;
    fh.flag = 0x00;
    fh.remain_len = remain_len;
    fh.fh_len = 1;
    if (remain_len <= 127) {
        fh.fh_len += 1;
    } else if (remain_len <= 16383) {
        fh.fh_len += 2;
    } else if (remain_len <= 2097151) {
        fh.fh_len += 3;
    } else {
        fh.fh_len += 4;
    }
    char p_message[fh.fh_len + remain_len];
    packageSuback(p_message, &fh, p_vh->message_id, p_sub_payload->topic_filter_len, return_code_array);
    if (-1 == writeHandler(p_session->client_sock, p_message, fh.fh_len + remain_len)) {
        fprintf(stderr, "[Error]: send suback(%d) to client(%s) failed.\n", p_vh->message_id, p_session->p_client_id);
    } else {
        printf("[Info]: send suback(%d) to client(%s) successfully.\n", p_vh->message_id, p_session->p_client_id);
    }
}

/**
 * 响应 UNSUBSCRIBE
 * @param client_sock
 * @param p_broker
 * @param p_fh
 * @param p_vh
 * @param p_unsub_payload
 */
void handlerUnsubscribe(Session *p_session, Broker *p_broker, FixedHeader *p_fh, CommonVariableHeader *p_vh,
                        UnsubscribePayload *p_unsub_payload) {
    // 1）取消订阅，构建返回码
    char return_code_array[p_unsub_payload->topic_filter_len];
    if (0x02 != p_fh->flag) {
        fprintf(stderr, "[Error]: Illegal flag in process of processing subscribe.\n");
        for (int i = 0; i < p_unsub_payload->topic_filter_len; ++i) {
            return_code_array[i] = (char) 0x80;
        }
    } else {
        int topic_filter_level;
        char **p_topic_filter;
        for (int i = 0; i < p_unsub_payload->topic_filter_len; ++i) {
            splitTopic(p_unsub_payload->p_topic_filter[i], p_unsub_payload->topic_filter_len, &p_topic_filter,
                       &topic_filter_level);
            deleteSubscribeTreeNode(p_broker->p_sub_tree_root, p_topic_filter, topic_filter_level, p_session);
            // 善后
            for (int j = 0; j < topic_filter_level; ++j) {
                free(p_topic_filter[i]);
            }
            free(p_topic_filter);
        }
    }
    // 2）封包 返回
    int remain_len = 2;
    FixedHeader fh;
    fh.message_type = UNSUBACK;
    fh.flag = 0x00;
    fh.remain_len = remain_len;
    fh.fh_len = 2;
    char p_message[fh.fh_len + remain_len];
    packageAck(p_message, &fh, p_vh->message_id);
    if (-1 == writeHandler(p_session->client_sock, p_message, fh.fh_len + remain_len)) {
        fprintf(stderr, "[Error]: send unsuback(%d) to client(%s) failed.\n", p_vh->message_id, p_session->p_client_id);
    } else {
        printf("[Info]: send unsuback(%d) to client(%s) successfully.\n", p_vh->message_id, p_session->p_client_id);
    }
}

/**
 * 响应 PINGREQ
 * @param p_session
 * @param p_broker
 * @param p_fh
 */
void handlerPingreq(Session *p_session) {
    // 2）封包：PINGRESP
    char p_message[2];
    FixedHeader fh;
    fh.message_type = PINGRESP;
    fh.flag = 0x00;
    fh.remain_len = 0;
    packageFixedHeader(p_message, &fh);
    // 3）发送
    if (-1 == writeHandler(p_session->client_sock, p_message, 2)) {
        fprintf(stderr, "[Error]: send pingresp to client(%s) failed.\n", p_session->p_client_id);
    } else {
        printf("[Info]: send pingresp to client(%s) successfully.\n", p_session->p_client_id);
    }
}

/**
 * 响应 DISCONNECT
 * @param client_sock
 * @param p_broker
 */
void handlerDisconnect(Session *p_session, Broker *p_broker) {
    // 1）处理会话
    if (p_session->is_clean_session) {
        printf("[Info]: delete a session about client(%s).\n", p_session->p_client_id);
        char sock_chars[4];
        int2FourChar(p_session->client_sock, sock_chars);
        hashTableRemove(&p_broker->session_table, sock_chars, 4);
    } else {
        p_session->is_online = false;
        printf("[Info]: keep a session, but client(%s) is offline.\n", p_session->p_client_id);
    }
    // 2）关闭连接
    printf("[Info]: close socket(%d).\n", p_session->client_sock);
    close(p_session->client_sock);
}

/**
 * 发布（推送）消息总逻辑
 * @param p_session
 * @param p_shm_table
 * @param p_shm_table_mutex
 */
void handlerSend(Session *p_session, HashTable *p_shm_table, pthread_mutex_t *p_shm_table_mutex) {
    // 1] 获取所有要发布的内容
    pthread_mutex_lock(&p_session->match_mutex);          /* 加锁 */
    char *p_curr = p_session->p_match;
    int publish_num = char2int(p_curr);
    if (publish_num > 0) {
        p_curr += 1;
        int shm_id_array[publish_num];
        char *p_shm_array[publish_num];
        int qos_array[publish_num];
        for (int i = 0; i < publish_num; ++i) {
            charArray2Int(p_curr, &shm_id_array[i]);
            p_shm_array[i] = shmat(shm_id_array[i], 0, 0);;                         /* 映射共享内存 */
            p_curr += 4;
            qos_array[i] = char2int(p_curr);
            p_curr += 1;
        }
        *(p_session->p_match) = 0;
        pthread_mutex_unlock(&p_session->match_mutex);    /* 释放 */
        // 2] 逐一发布
        for (int j = 0; j < publish_num; ++j) {
            PublishMessage *p_pm = (PublishMessage *) p_shm_array[j];
            p_pm->qos = qos_array[j];
            if (0 == p_pm->qos) {
                handlerSendQos0(p_session, p_pm);
            } else if (1 == p_pm->qos) {
                p_pm->message_id = getRandomNum();  /* 尽管新的报文标识符会与未确认的报文标识符重复，但这是小概率事件，此处不考虑 */
                handlerSendQos1(p_session, p_pm);
            } else if (2 == p_pm->qos) {
                p_pm->message_id = getRandomNum();  /* 尽管新的报文标识符会与未确认的报文标识符重复，但这是小概率事件，此处不考虑 */
                handlerSendQos2(p_session, p_pm);
            }
            // 另：判断共享内存是否需要释放（可能考虑把具体的删除放入优先级更低的进程去处理）
            bool is_need = false;
            pthread_mutex_lock(p_shm_table_mutex);         /* 加锁 */
            char p_char_shm_id[4];
            int2FourChar(shm_id_array[j], p_char_shm_id);
            int remain_len = (int) hashTableGet(p_shm_table, p_char_shm_id, 4);
            if (remain_len == 1) {
                is_need = true;
                hashTableRemove(p_shm_table, p_char_shm_id, 4);
            } else {
                hashTablePut(p_shm_table, p_char_shm_id, 4, (void *) (remain_len - 1));
            }
            pthread_mutex_unlock(p_shm_table_mutex);       /* 释放 */
            if (is_need) {
                deletePublishMessage(p_pm);
                shmctl(shm_id_array[j], IPC_RMID, 0);   /* 删除共享内存 */
            }
        }
    } else {
        pthread_mutex_unlock(&p_session->match_mutex);    /* 释放 */
    }
}

/**
 * 发布 qos0 消息
 * @param client_sock
 * @param p_pm
 */
void handlerSendQos0(Session *p_session, PublishMessage *p_pm) {
    int remain_len = 2 + p_pm->topic_len + p_pm->content_len;
    FixedHeader fh;
    fh.message_type = PUBLISH;
    fh.flag = 0x00;
    fh.remain_len = remain_len;
    fh.qos = 0;
    fh.fh_len = 1;
    if (remain_len <= 127) {
        fh.fh_len += 1;
    } else if (remain_len <= 16383) {
        fh.fh_len += 2;
    } else if (remain_len <= 2097151) {
        fh.fh_len += 3;
    } else {
        fh.fh_len += 4;
    }
    char p_message[fh.fh_len + remain_len];
    packagePublish(p_message, &fh, p_pm->p_topic, p_pm->topic_len, p_pm->message_id, p_pm->p_content,
                   p_pm->content_len);
    if (-1 == writeHandler(p_session->client_sock, p_message, fh.fh_len + remain_len)) {
        fprintf(stderr, "[Error]: send publish qos0 to client(%s) failed, topic is %s.\n", p_session->p_client_id,
                p_pm->p_topic);
    } else {
        printf("[Info]: send publish qos0 to client(%s) successfully, topic is %s.\n", p_session->p_client_id,
               p_pm->p_topic);
    }
}

/**
 * 发布 qos1 消息
 * @param p_session
 * @param p_pm
 */

void handlerSendQos1(Session *p_session, PublishMessage *p_pm) {
    // 1）存储消息
    PublishMessage pub_message;
    createPublishMessage(&pub_message, p_pm->qos, p_pm->p_topic, p_pm->topic_len, p_pm->p_content,
                         p_pm->content_len, p_pm->message_id);
    listInsertElement(&p_session->publish_message_list, &pub_message);
    // 2）封包 发送
    int remain_len = 2 + p_pm->topic_len + 2 + p_pm->content_len;
    FixedHeader fh;
    fh.message_type = PUBLISH;
    fh.flag = 0x02;
    fh.remain_len = remain_len;
    fh.qos = 1;
    fh.fh_len = 1;
    if (remain_len <= 127) {
        fh.fh_len += 1;
    } else if (remain_len <= 16383) {
        fh.fh_len += 2;
    } else if (remain_len <= 2097151) {
        fh.fh_len += 3;
    } else {
        fh.fh_len += 4;
    }
    char p_message[fh.fh_len + remain_len];
    packagePublish(p_message, &fh, pub_message.p_topic, pub_message.topic_len, pub_message.message_id,
                   pub_message.p_content, pub_message.content_len);
    if (-1 == writeHandler(p_session->client_sock, p_message, fh.fh_len + remain_len)) {
        fprintf(stderr, "[Error]: send publish qos1 to client(%s) failed, topic is %s.\n", p_session->p_client_id,
                p_pm->p_topic);
    } else {
        printf("[Info]: send publish qos1 to client(%s) successfully, topic is %s.\n", p_session->p_client_id,
               p_pm->p_topic);
    }
}

/**
 * 发布 qos2 消息
 * @param p_session
 * @param p_pm
 */
void handlerSendQos2(Session *p_session, PublishMessage *p_pm) {
    // 1）存储消息
    PublishMessage pub_message;
    createPublishMessage(&pub_message, p_pm->qos, p_pm->p_topic, p_pm->topic_len, p_pm->p_content,
                         p_pm->content_len, p_pm->message_id);
    listInsertElement(&p_session->publish_message_list, &pub_message);
    // 2）封包发送
    int remain_len = 2 + p_pm->topic_len + 2 + p_pm->content_len;
    FixedHeader fh;
    fh.message_type = PUBLISH;
    fh.flag = 0x04;
    fh.remain_len = remain_len;
    fh.qos = 2;
    fh.fh_len = 1;
    if (remain_len <= 127) {
        fh.fh_len += 1;
    } else if (remain_len <= 16383) {
        fh.fh_len += 2;
    } else if (remain_len <= 2097151) {
        fh.fh_len += 3;
    } else {
        fh.fh_len += 4;
    }
    char p_message[fh.fh_len + remain_len];
    packagePublish(p_message, &fh, pub_message.p_topic, pub_message.topic_len, pub_message.message_id,
                   pub_message.p_content, pub_message.content_len);
    if (-1 == writeHandler(p_session->client_sock, p_message, fh.fh_len + remain_len)) {
        fprintf(stderr, "[Error]: send publish qos2 to client(%s) failed, topic is %s.\n", p_session->p_client_id,
                p_pm->p_topic);
    } else {
        printf("[Info]: send publish qos2 to client(%s) successfully, topic is %s.\n", p_session->p_client_id,
               p_pm->p_topic);
    }
}

/******************** 内部操作 ********************/

/**
 * socket 写封装
 */
int writeHandler(int client_sock, char *p_message, int message_len) {
    int write_num = 0;
    while (write_num < message_len) {
        int t_len = (int) write(client_sock, p_message + write_num, (size_t) (message_len - write_num));
        if (-1 == t_len) {
            return -1;
        }
        write_num += t_len;
    }
    return 0;
}

/**
 * 比较钩子
 */
int compareMessageIdHf(void *p_curr, void *p_value) {
    int curr = *(int *) p_curr;
    int value = *(int *) p_value;
    if (curr > value) {
        return 1;
    } else if (curr == value) {
        return 0;
    }
    return -1;
}

/**
 * 比较层数
 */
int compareLevelHf(void *p_curr, void *p_value) {
    SubscribeListNode *p_node_curr = p_curr;
    SubscribeListNode *p_node_value = p_value;
    if (p_node_curr->level > p_node_value->level) {
        return 1;
    } else if (p_node_curr->level == p_node_value->level) {
        return 0;
    } else {
        return -1;
    }
}

/**
 * 属性比较钩子
 */
int attrMessageIdHf(void *p_curr, void *p_attr) {
    PublishMessage *p_pm = (PublishMessage *) p_curr;
    int attr = *(int *) p_attr;
    if (attr == p_pm->message_id) {
        return 1;
    } else {
        return 0;
    }
}


/**
 * 新线程执行分发逻辑的函数
 * @param args
 * @return
 */
void *runForPublish(void *args) {
    // 0）准备
    ArgData *p_arg_data = args;
    Broker *p_broker = p_arg_data->p_broker;
    PublishVariableHeader *p_vh = &p_arg_data->vh;
    PublishPayload *p_payload = &p_arg_data->payload;
    HashTable *p_shm_num_table = p_arg_data->p_shm_num_table;
    pthread_mutex_t *p_shm_table_mutex = p_arg_data->p_shm_table_mutex;

    // 1）获取订阅该主题的所有会话（客户端）
    // [说明]：此处调用了 findAllSubscribeInSubscribeTree 两次，为了做到 “sport/#” 也匹配单独的 “sport”
    HashTable subscribe_table;              /* key: p_session ,value: qos */
    hashTableNew(&subscribe_table, SUBSCRIBE_TABLE_SIZE, NULL);
    char **p_topic_1;
    int topic_level;
    splitTopic(p_vh->p_topic, p_vh->topic_len, &p_topic_1, &topic_level);   /* （1）匹配，例：/sport */
    findAllSubscribeInSubscribeTree(p_broker->p_sub_tree_root, p_topic_1, topic_level, &subscribe_table);

    char *p_topic_2[topic_level + 1];
    for (int k = 0; k < topic_level; ++k) {
        p_topic_2[k] = p_topic_1[k];
    }
    p_topic_2[topic_level] = "#";                                           /* （2）匹配（添加 # ），例：/sport/# */
    findAllSubscribeInSubscribeTree(p_broker->p_sub_tree_root, p_topic_2, topic_level + 1, &subscribe_table);
    // 善后
    for (int j = 0; j < topic_level; ++j) {
        free(p_topic_1[j]);
    }
    free(p_topic_1);
    if (subscribe_table.element_num > 0) {
        // 2）若有（客户端）订阅，创建共享内存
        int key = p_vh->message_id + getRandomNum();
        int shm_id = shmget(key, sizeof(PublishMessage), 0666 | IPC_CREAT);     /* 创建共享内存 */
        char *p_shm = shmat(shm_id, 0, 0);                                      /* 将共享内存连接到当前进程的地址空间 */
        PublishMessage pm;
        pm.topic_len = p_vh->topic_len;
        pm.p_topic = p_arg_data->vh.p_topic;
        pm.content_len = p_payload->content_len;
        pm.p_content = p_arg_data->payload.p_content;
        memcpy(p_shm, &pm, sizeof(PublishMessage));
        shmdt(p_shm);                                                           /* 把共享内存从当前进程中分离 */
        // 2.1）记录该共享内存要使用的次数
        pthread_mutex_lock(p_shm_table_mutex);        /* 加锁 */
        char p_char_shm_id[4];
        int2FourChar(shm_id, p_char_shm_id);
        hashTablePut(p_shm_num_table, p_char_shm_id, 4, (void *) subscribe_table.element_num);
        pthread_mutex_unlock(p_shm_table_mutex);      /* 释放 */

        // 3）通知各客户端（订阅的），【通过修改它们的共享内存】
        for (int i = 0; i < subscribe_table.table_size; ++i) {
            kv *p = subscribe_table.table[i];
            while (NULL != p) {
                Session *p_s;
                memcpy(&p_s, p->key, sizeof(Session *));
                char qos;
                if (((int) p->value) > p_arg_data->qos) {
                    qos = (char) p_arg_data->qos;
                } else {
                    qos = (char) p->value;
                }
                char *p_match = p_s->p_match;
                pthread_mutex_lock(&p_s->match_mutex);      /* 加锁 */
                int n = char2int(p_match);
                if (n == SESSION_SHM_NUM) {
                    fprintf(stderr, "[Error]: SHM of session is already fulled.\n");
                } else {
                    *p_match = (char) (n + 1);
                    char t_chars[4];
                    int2FourChar(shm_id, t_chars);
                    memcpy(p_match + (5 * n) + 1, t_chars, 4);
                    memcpy(p_match + (5 * n) + 5, &qos, 1);
                }
                pthread_mutex_unlock(&p_s->match_mutex);    /* 释放 */

                union sigval val;
                val.sival_ptr = p_s;
                pthread_sigqueue(p_s->t_id, SIGUSR1, val);

                p = p->p_next;
            }
        }
    }
    // 4）善后
    hashTableFree(&subscribe_table);
    free(p_arg_data);
    return 0;
}
