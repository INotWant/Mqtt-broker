//
// Created by iwant on 2019/1/23.
//

#ifndef MQTT_BROKER_SUBSCRIBETREE_H
#define MQTT_BROKER_SUBSCRIBETREE_H

#include "collection/list.h"
#include "session.h"

/**************************
 *     订阅树 数据结构
 **************************/

/**
 * 描述一个订阅过滤器的属性（拥有此主题过滤器的客户端属性等）
 */
typedef struct _ClientListNode {
    Session *p_session;             /* 对应的会话状态 */
    int qos;                        /* 服务质量 */
    SubscribeListNode *p_sub_node;  /* 所在订阅过滤器链表中的位置（订阅过滤器链表详见 session.h ） */
} ClientListNode;

/**
 * 订阅树结点
 */
typedef struct _SubscribeTreeNode {
    char *p_curr_name;                      /* 当前层的命名 */
    List client_list;                       /* 主题过滤器链表 */
    struct _SubscribeTreeNode *p_parent;    /* 父节点 */
    struct _SubscribeTreeNode *p_child;     /* 左孩子 */
    struct _SubscribeTreeNode *p_brother;   /* 右兄弟 */
} SubscribeTreeNode;

/**************************
 *     订阅树 相关操作
 **************************/

/**
 * 创建一个新的订阅树节点（动态申请内存）
 * @param p_curr_name
 * @param curr_name_len
 * @return
 */
SubscribeTreeNode *createSubscribeTreeNode(char *p_curr_name, int curr_name_len);

/**
 * 向订阅树中插入一个主题过滤器
 * @param p_sub_tree_root
 * @param p_topic_filter
 * @param topic_filter_level
 * @param qos
 * @param p_session
 * @param p_sub_list_node
 * @param p_return_code
 * @return 主题过滤器最后层对应的节点
 */
SubscribeTreeNode *
insertSubscribeTreeNode(SubscribeTreeNode *p_sub_tree_root, char **p_topic_filter, int topic_filter_level, int qos,
                        Session *p_session, SubscribeListNode *p_sub_list_node, char *p_return_code);

/**
 *  从订阅树中删除指定主题过滤器
 * @param p_sub_tree_root
 * @param p_topic_filter
 * @param topic_filter_level
 * @param p_session
 */
void
deleteSubscribeTreeNode(SubscribeTreeNode *p_sub_tree_root, char **p_topic_filter, int topic_filter_level,
                        Session *p_session);

/**
 * 从订阅树中删除指定节点
 * @param p_sub_tree_root
 * @param p_sub_tree_node
 * @param p_session
 */
void
deleteSubscribeTreeNodeByAddress(SubscribeTreeNode *p_sub_tree_root, SubscribeTreeNode *p_sub_tree_node,
                                 Session *p_session);

/**
 * 删除整个订阅树
 * @param p_sub_tree_root
 */
void deleteSubscribeTree(SubscribeTreeNode *p_sub_tree_root);

/**
 * 从订阅树中查找出所有满足要求的订阅
 * @param p_sub_tree_root
 * @param p_topic
 * @param topic_level
 * @param p_subscribe_table
 */
void findAllSubscribeInSubscribeTree(SubscribeTreeNode *p_sub_tree_root, char **p_topic, int topic_level,
                                     HashTable *p_subscribe_table);

#endif //MQTT_BROKER_SUBSCRIBETREE_H
