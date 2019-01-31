//
// Created by iwant on 2019/1/23.
//

#include <stdlib.h>
#include <string.h>
#include "subscribeTree.h"
#include "define.h"

/******************** 一些声明 ********************/

int attrSessionHf(void *p_curr, void *p_session);

/******************** 分割线 ********************/

/**
 * 创建一个新的订阅树节点（动态申请内存）
 * @param p_curr_name
 * @param curr_name_len
 * @return
 */
SubscribeTreeNode *createSubscribeTreeNode(char *p_curr_name, int curr_name_len) {
    SubscribeTreeNode *p_subtree_node = malloc(sizeof(SubscribeTreeNode));
    p_subtree_node->p_curr_name = malloc((size_t) (curr_name_len + 1));
    p_subtree_node->p_curr_name[curr_name_len] = '\0';
    memcpy(p_subtree_node->p_curr_name, p_curr_name, (size_t) curr_name_len);
    listNew(&p_subtree_node->client_list, CLIENT_LIST_SIZE, sizeof(ClientListNode), NULL);
    p_subtree_node->p_parent = NULL;
    p_subtree_node->p_child = NULL;
    p_subtree_node->p_brother = NULL;
    return p_subtree_node;
}

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
                        Session *p_session, SubscribeListNode *p_sub_list_node, char *p_return_code) {
    SubscribeTreeNode *p_level = p_sub_tree_root;
    for (int i = 0; i < topic_filter_level; ++i) {
        char *p_curr_name = p_topic_filter[i];          /* 当前层（level） name */
        SubscribeTreeNode *p_tree_node = p_level;       /* 保证 p_level 一定不为空 */
        SubscribeTreeNode *p_tree_last_node = NULL;     /* 保证其一定也不为空 */
        while (NULL != p_tree_node) {
            if (0 == strcmp(p_curr_name, p_tree_node->p_curr_name)) {
                break;
            }
            p_tree_last_node = p_tree_node;
            p_tree_node = p_tree_node->p_brother;
        }
        if (NULL == p_tree_node) {
            p_tree_node = createSubscribeTreeNode(p_curr_name, (int) strlen(p_curr_name));
            p_tree_node->p_parent = p_tree_last_node;
            p_tree_last_node->p_brother = p_tree_node;
        }
        if (i != topic_filter_level - 1) {
            if (NULL == p_tree_node->p_child) {
                SubscribeTreeNode *p_new_node = createSubscribeTreeNode(p_topic_filter[i + 1],
                                                                        (int) strlen(p_topic_filter[i + 1]));
                p_new_node->p_parent = p_tree_node;
                p_tree_node->p_child = p_new_node;
            }
            p_level = p_tree_node->p_child;
        } else {
            ClientListNode cln;
            cln.p_session = p_session;
            cln.qos = qos;
            cln.p_sub_node = p_sub_list_node;
            listInsertElement(&p_tree_node->client_list, &cln);
            if (0 == qos) {
                *p_return_code = 0x00;
            } else if (1 == qos) {
                *p_return_code = 0x01;
            } else if (2 == qos) {
                *p_return_code = 0x02;
            } else {
                *p_return_code = (char) 0x80;
            }
            return p_tree_node;
        }
    }
    *p_return_code = (char) 0x80;
    return NULL;
}

/**
 *  从订阅树中删除指定主题过滤器（依据主题过滤器）
 * @param p_sub_tree_root
 * @param p_topic_filter
 * @param topic_filter_level
 * @param p_session
 */
void
deleteSubscribeTreeNode(SubscribeTreeNode *p_sub_tree_root, char **p_topic_filter, int topic_filter_level,
                        Session *p_session) {
    // 1）先走到最底层
    SubscribeTreeNode *p_node = p_sub_tree_root;
    for (int i = 0; i < topic_filter_level; ++i) {
        char *p_curr_name = p_topic_filter[i];
        SubscribeTreeNode *p_t_node = p_node;
        while (NULL != p_t_node) {
            if (0 == strcmp(p_curr_name, p_t_node->p_curr_name)) {
                if (i == topic_filter_level - 1) {
                    p_node = p_t_node;               /* p_node 指向最后层对应的节点 */
                } else {
                    p_node = p_t_node->p_child;
                }
                break;
            }
            p_t_node = p_t_node->p_brother;
        }
    }
    // 2）再从下往上删除
    if (NULL != p_node) {
        bool flag = true;                   /* 是否还有节点可以删除 */
        bool is_deleted = false;            /* 标记最后一层是否已经删除 */
        SubscribeTreeNode *p_end_node;
        while (flag && NULL == p_node->p_child && 1 == listElementNum(&p_node->client_list)) {
            SubscribeTreeNode *p_deleted_node = p_node;
            if (p_deleted_node != p_sub_tree_root &&
                p_deleted_node != p_sub_tree_root->p_brother) {     /* 初始化构造的 `+` `#` 结点有免死金牌 */
                is_deleted = true;
                if (p_node == p_node->p_parent->p_child) {           /* 左分支 */
                    p_node->p_parent->p_child = p_node->p_brother;
                    if (NULL != p_node->p_brother) {
                        p_node->p_brother->p_parent = p_node->p_parent;
                    }
                    p_node = p_node->p_parent;
                } else {                                             /* 右分支 */
                    p_node->p_parent->p_brother = p_node->p_brother;
                    if (NULL != p_node->p_brother) {
                        p_node->p_brother->p_parent = p_node->p_parent;
                    }
                    flag = false;
                }
                free(p_deleted_node->p_curr_name);                                      /* 清除申请的空间 */
                ClientListNode *p_client_node = (ClientListNode *) p_deleted_node->client_list.element_head;
                listDeleteElement(&p_session->sub_list, p_client_node->p_sub_node);     /* 清除对应的订阅过滤器链表中的节点 */
                listFree(&p_deleted_node->client_list);                                 /* 清除 client_list 链表 */
            }
        }
        if (!is_deleted) {
            ClientListNode *p_client_node = listFindElementByAttr(&p_end_node->client_list, p_session, attrSessionHf);
            listDeleteElement(&p_session->sub_list, p_client_node->p_sub_node);         /* 清除对应的订阅过滤器链表中的节点 */
            listDeleteElement(&p_end_node->client_list, p_client_node);                 /* 清除对应 client_list 中的节点 */
        }
    }
}

/**
 * 从订阅树中删除指定主题过滤器（依据地址）
 * 注：需要自行删除 session 中所有的订阅链表
 * @param p_sub_tree_root
 * @param p_sub_tree_node
 * @param p_session
 */
void
deleteSubscribeTreeNodeByAddress(SubscribeTreeNode *p_sub_tree_root, SubscribeTreeNode *p_sub_tree_node,
                                 Session *p_session) {
    // 1）直接从下往上删除
    SubscribeTreeNode *p_node = p_sub_tree_node;
    if (NULL != p_node) {
        bool flag = true;                   /* 是否还有节点可以删除 */
        bool is_deleted = false;            /* 标记最后一层是否已经删除 */
        while (flag && NULL == p_node->p_child && 1 == listElementNum(&p_node->client_list)) {
            SubscribeTreeNode *p_deleted_node = p_node;
            if (p_deleted_node != p_sub_tree_root &&
                p_deleted_node != p_sub_tree_root->p_brother) {     /* 初始化构造的 `+` `#` 结点有免死金牌 */
                is_deleted = true;
                if (p_node == p_node->p_parent->p_child) {           /* 左分支 */
                    p_node->p_parent->p_child = p_node->p_brother;
                    if (NULL != p_node->p_brother) {
                        p_node->p_brother->p_parent = p_node->p_parent;
                    }
                    p_node = p_node->p_parent;
                } else {                                             /* 右分支 */
                    p_node->p_parent->p_brother = p_node->p_brother;
                    if (NULL != p_node->p_brother) {
                        p_node->p_brother->p_parent = p_node->p_parent;
                    }
                    flag = false;
                }
                free(p_deleted_node->p_curr_name);                                      /* 清除申请的空间 */
                listFree(&p_deleted_node->client_list);                                 /* 清除 client_list 链表 */
            }
        }
        if (!is_deleted) {
            ClientListNode *p_client_node = listFindElementByAttr(&p_sub_tree_node->client_list, p_session,
                                                                  attrSessionHf);
            listDeleteElement(&p_sub_tree_node->client_list, p_client_node);            /* 清除对应 client_list 中的节点 */
        }
    }
}

/**
 * 删除整个订阅树
 * 注：需要自行删除 session 中所有的订阅链表
 * @param p_sub_tree_root
 */
void deleteSubscribeTree(SubscribeTreeNode *p_sub_tree_root) {
    if (NULL != p_sub_tree_root) {
        if (NULL != p_sub_tree_root->p_child) {
            deleteSubscribeTree(p_sub_tree_root->p_child);
        }
        if (NULL != p_sub_tree_root->p_brother) {
            deleteSubscribeTree(p_sub_tree_root->p_brother);
        }
        // 删除当前节点
        free(p_sub_tree_root->p_curr_name);
        listFree(&p_sub_tree_root->client_list);
        free(p_sub_tree_root);
    }
}

/**
 * 从订阅树中查找出所有满足要求的订阅
 * @param p_sub_tree_root
 * @param p_topic
 * @param topic_level
 * @param p_subscribe_table
 */
void findAllSubscribeInSubscribeTree(SubscribeTreeNode *p_sub_tree_root, char **p_topic, int topic_level,
                                     HashTable *p_subscribe_table) {
    // [说明] p_subscribe_table --> /* key: p_session ,value: qos */
    if (topic_level > 0) {
        SubscribeTreeNode *p_level = p_sub_tree_root;
        char *p_curr_name = p_topic[0];
        SubscribeTreeNode *p_node = p_level;
        char p_session_chars[sizeof(Session *)];
        while (NULL != p_node) {
            if (0 == strcmp("#", p_node->p_curr_name)) {
                char *p_client_char = (char *) p_node->client_list.element_head;
                for (int j = 0; j < p_node->client_list.local_size; ++j) {
                    ClientListNode *p_client = ((ClientListNode *) p_client_char);
                    if (isOnline(p_client->p_session)) {        /* 判断客户端是否存活 */
                        memcpy(p_session_chars, &p_client->p_session, sizeof(Session *));
                        void *value = hashTableGet(p_subscribe_table, p_session_chars, sizeof(Session *));
                        if (NULL == value) {
                            hashTablePut(p_subscribe_table, p_session_chars, sizeof(Session *), (void *) p_client->qos);
                        } else {
                            if (p_client->qos > (int) value) {
                                hashTablePut(p_subscribe_table, p_session_chars, sizeof(Session *),
                                             (void *) p_client->qos);
                            }
                        }
                    }
                    p_client_char += sizeof(ClientListNode);
                }
            } else if (0 == strcmp("+", p_node->p_curr_name) || 0 == strcmp(p_curr_name, p_node->p_curr_name)) {
                if (1 == topic_level) {                 /* 最后一层 */
                    char *p_client_char = (char *) p_node->client_list.element_head;
                    for (int j = 0; j < p_node->client_list.local_size; ++j) {
                        ClientListNode *p_client = ((ClientListNode *) p_client_char);
                        if (isOnline(p_client->p_session)) {    /* 判断客户端是否存活 */
                            memcpy(p_session_chars, &p_client->p_session, sizeof(Session *));
                            void *value = hashTableGet(p_subscribe_table, p_session_chars, sizeof(Session *));
                            if (NULL == value) {
                                hashTablePut(p_subscribe_table, p_session_chars, sizeof(Session *),
                                             (void *) p_client->qos);
                            } else {
                                if (p_client->qos > (int) value) {
                                    hashTablePut(p_subscribe_table, p_session_chars, sizeof(Session *),
                                                 (void *) p_client->qos);
                                }
                            }
                        }
                        p_client_char += sizeof(ClientListNode);
                    }
                } else {                                /* 非最后一层，递归 */
                    findAllSubscribeInSubscribeTree(p_node->p_child, p_topic + 1, topic_level - 1,
                                                    p_subscribe_table);
                }
            }
            p_node = p_node->p_brother;
        }
    }
}

/******************** 内部操作 ********************/

/**
 * 属性比较钩子
 */
int attrSessionHf(void *p_curr, void *p_session) {
    ClientListNode *p_client_node = p_curr;
    if (p_client_node->p_session == p_session) {
        return 1;
    } else {
        return 0;
    }
}
