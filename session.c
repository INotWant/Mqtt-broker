//
// Created by iwant on 2019/1/23.
//

#include <stdlib.h>
#include <string.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <stdio.h>
#include <pthread.h>
#include "session.h"
#include "util.h"
#include "define.h"
#include "message.h"
#include "subscribeTree.h"
#include "broker.h"

extern Broker broker;

/**
 * 创建 Session（动态申请内存）
 * @param p_client_id
 * @param client_id_len
 * @param client_sock
 * @param is_clean_session
 * @param ping_time
 * @param t_id
 * @return
 */
Session *createSession(char *p_client_id, int client_id_len, int client_sock, bool is_clean_session, int ping_time,
                       pthread_t t_id) {
    Session *p_session = malloc(sizeof(Session));
    p_session->p_client_id = malloc((size_t) (client_id_len + 1));
    p_session->p_client_id[client_id_len] = '\0';
    memcpy(p_session->p_client_id, p_client_id, (size_t) client_id_len);
    p_session->client_id_len = client_id_len;
    p_session->client_sock = client_sock;
    p_session->is_clean_session = is_clean_session;
    p_session->is_online = true;
    p_session->ping_time = ping_time;
    p_session->last_req_time = getCurrentTime();
    listNew(&p_session->sub_list, SUBSCRIBE_LIST_SIZE, sizeof(SubscribeListNode), NULL);
    listNew(&p_session->message_id_r_list, MESSAGE_ID_LIST_SIZE, sizeof(int), NULL);
    p_session->p_match = malloc(MATCH_SIZE);
    memset(p_session->p_match, 0, MATCH_SIZE);
    pthread_mutex_init(&p_session->match_mutex, NULL);
    listNew(&p_session->publish_message_list, PUBLISH_MESSAGE_LIST_SIZE, sizeof(PublishMessage), deletePublishMessage);
    listNew(&p_session->message_id_s_list, MESSAGE_ID_LIST_SIZE, sizeof(int), NULL);
    p_session->state = 0;
    pthread_mutex_init(&p_session->state_mutex, NULL);
    p_session->t_id = t_id;
    return p_session;
}

/**
 * 判断客户端是否在线
 * @param p_session
 * @return
 */
bool isOnline(Session *p_session) {
    if ((getCurrentTime() - p_session->last_req_time) > 15 * p_session->ping_time / 10) {
        p_session->is_online = false;
    } else {
        p_session->is_online = true;
    }
    return p_session->is_online;
}

/**
 * 释放 Session
 * @param p_session
 */
void freeSession(void *p_session) {
    Session *p_s = p_session;
    free(p_s->p_client_id);
    for (int i = 0; i < p_s->sub_list.local_size; ++i) {
        deleteSubscribeTreeNodeByAddress(broker.p_sub_tree_root,
                                         ((SubscribeListNode *) (p_s->sub_list.element_head +
                                                                 i * p_s->sub_list.element_size))->p_sub_tree_node,
                                         p_session);
    }
    listFree(&p_s->sub_list);
    listFree(&p_s->message_id_r_list);
    free(p_s->p_match);
    pthread_mutex_destroy(&p_s->match_mutex);
    listFree(&p_s->publish_message_list);
    listFree(&p_s->message_id_s_list);
    pthread_mutex_destroy(&p_s->state_mutex);
}
