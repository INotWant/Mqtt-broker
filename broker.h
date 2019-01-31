//
// Created by iwant on 2019/1/22.
//

#ifndef MQTT_BROKER_BROKER_H
#define MQTT_BROKER_BROKER_H

/**************************
 *     Broker 数据结构
 **************************/

#include "collection/list.h"
#include "subscribeTree.h"

/**
 * Broker 结构体
 */
typedef struct _Broker {
    List retain_message_list;           /* 保留信息链表 */
    SubscribeTreeNode *p_sub_tree_root; /* 订阅数根节点 */
    HashTable session_table;            /* 会话状态表 */     // TODO 修改 key 为 client_id
    int server_sock;                    /* 服务器 socket */
} Broker;

/**
 * 错误码
 */
enum ErrorCode {
    ERR_SUCCESS = 0x0000,
    ERR_MEMORY_INSUFFICIENT = 0x0001,
    ERR_SOCKET = 0x0002,
};

/**************************
 *     Broker 相关操作
 **************************/

/**
 * Broker 初始化
 */
void initBroker();

/**
 * 启动 Broker
 */
void startBroker();

#endif //MQTT_BROKER_BROKER_H
