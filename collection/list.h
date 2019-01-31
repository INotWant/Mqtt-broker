//
// Created by iwant on 2019/1/22.
// 链表
// 注：链表中存储的是原元素，而不是对应的指针
//

#ifndef MQTT_BROKER_LIST_H
#define MQTT_BROKER_LIST_H


#include <stdbool.h>

typedef struct _List {
    unsigned int local_size;    /* 下一个可用位置 */
    unsigned int alloc_size;    /* 最大可允许存储的元素数 */
    unsigned int element_size;  /* 每个元素的大小 */
    void *element_head;

    void (*delete_hf)(void *);  /* 删除钩子 */
} List;

/**
 * 创建链表
 * @param p_list
 * @param alloc_size
 * @param element_size
 * @param delete_hf
 */
void listNew(List *p_list, unsigned int alloc_size, unsigned int element_size, void (*delete_hf)(void *));

/**
 * 销毁链表
 * @param p_list
 */
void listFree(List *p_list);

/**
 * 将指定元素插入到链表（尾插）
 * @param p_list
 * @param p_value
 * @return 插入元素在链表中的地址
 */
void *listInsertElement(List *p_list, void *p_value);

/**
 * 将指定元素按序插入到链表
 * @param p_list
 * @param p_value
 * @param compare_hf
 * @return 插入元素在链表中的地址
 */
void *listInsertElementSort(List *p_list, void *p_value, int (*compare_hf)(void *, void *));

/**
 * 返回链表中元素的个数
 * @param p_list
 */
int listElementNum(List *p_list);

/**
 * 将指定元素从链表中删除
 * @param p_list
 * @param p_value
 */
void listDeleteElement(List *p_list, void *p_value);

/**
 * 根据属性从链表中删除对应元素（只删除与属性值匹配的第一个元素）
 * @param p_list
 * @param p_attr
 * @param attr_hf 此钩子判断元素是否满足删除条件（即，对应的属性相等）。要求，若满足返回 1；不满足返回 0。
 */
void listDeleteElementByAttr(List *p_list, void *p_attr, int (*attr_hf)(void *, void *));

/**
 * 查询链表中是否存在指定元素
 * @param p_list
 * @param p_value
 * @param compare_hf
 * @return
 */
bool listExistElement(List *p_list, void *p_value, int (*compare_hf)(void *, void *));

/**
 * 通过属性在链表中查找指定元素
 * @param p_list
 * @param p_attr
 * @param attr_hf
 * @return
 */
void *listFindElementByAttr(List *p_list, void *p_attr, int (*attr_hf)(void *, void *));

/**
 * 清空链表
 * @param p_list
 */
void listFlush(List *p_list);

#endif //MQTT_BROKER_LIST_H
