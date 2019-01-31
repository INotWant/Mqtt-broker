//
// Created by iwant on 2019/1/23.
//

#ifndef MQTT_BROKER_SESSION_H
#define MQTT_BROKER_SESSION_H

#include <stdbool.h>
#include "collection/list.h"
#include "collection/hashTable.h"

/**************************
 *     会话状态 数据结构
 **************************/

typedef struct _SubscribeTreeNode SubscribeTreeNode;

/**
 * 订阅过滤器链表结点（用于维护客户端所拥有的所有订阅过滤器）
 */
typedef struct _SubscribeListNode {
    SubscribeTreeNode *p_sub_tree_node; /* 所处订阅树中的位置 */
    int level;                          /* 订阅过滤器层数 */
} SubscribeListNode;

/**
 * 会话状态 结构体
 */
typedef struct _Session {
    char *p_client_id;          /* 客户端标识符（再申请内存） */
    int client_id_len;          /* 客户端标识符长度 */
    int client_sock;            /* 客户端套接字 */
    bool is_clean_session;      /* 设置清理会话 */
    bool is_online;             /* 是否在线 */
    int ping_time;              /* 心跳时间 */
    long last_req_time;         /* 上次通信的时间戳，可用于判断是否在线 */
    List sub_list;              /* 订阅过滤器链表 */
    List message_id_r_list;     /* 未确认消息的报文标识符，做接收者时用 */
    char *p_match;              /* 记录匹配的消息，只存地址（线程间通信） */
    pthread_mutex_t match_mutex;/* 互斥锁，用于对 p_match 的的互斥 */
    List publish_message_list;  /* 未收到确认的消息，做发送者用 */
    List message_id_s_list;     /* 未收到确认消息的报文标识符，做发送者用 */
    int state;                  /* 状态描述，信号处理函数要处理此。（0一般情况，1表示有订阅信息需要发布到客户端） */
    pthread_mutex_t state_mutex;/* 互斥锁，用于对 state 的的互斥 */
    pthread_t t_id;             /* 对应的线程ID */
} Session;

/**************************
 *     会话状态 相关操作
 **************************/

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
Session *createSession(char *p_client_id, int client_id_len, int client_sock, bool is_clean_session, int ping_time, pthread_t t_id);

/**
 * 判断客户端是否在线
 * @param p_session
 * @return
 */
bool isOnline(Session *p_session);

/**
 * 释放 Session
 * @param p_session
 */
void freeSession(void *p_session);

#endif //MQTT_BROKER_SESSION_H
