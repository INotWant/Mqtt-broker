//
// Created by iwant on 2019/1/22.
//

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "list.h"
#include "../broker.h"

/**
 * 创建链表
 * @param p_list
 * @param alloc_size
 * @param element_size
 * @param delete_hf
 */
void listNew(List *p_list, unsigned int alloc_size, unsigned int element_size, void (*delete_hf)(void *)) {
    p_list->local_size = 0;
    p_list->alloc_size = alloc_size;
    p_list->element_size = element_size;
    p_list->element_head = malloc(alloc_size * element_size);
    if (NULL == p_list->element_head) {
        perror("[error]: malloc fail when create List.");
        exit(ERR_MEMORY_INSUFFICIENT);
    }
    p_list->delete_hf = delete_hf;
}

/**
 * 销毁链表
 * @param p_list
 */
void listFree(List *p_list) {
    if (NULL != p_list->delete_hf) {
        for (int i = 0; i < p_list->local_size; ++i) {
            p_list->delete_hf(p_list->element_head + (p_list->element_size * i));
        }
    }
    free(p_list->element_head);
}

/**
 * 将指定元素插入到链表（尾插）
 * @param p_list
 * @param p_value
 * @return  插入元素在链表中的地址
 */
void *listInsertElement(List *p_list, void *p_value) {
    if (p_list->local_size == p_list->alloc_size) {
        p_list->alloc_size *= 2;
        p_list->element_head = realloc(p_list->element_head, p_list->alloc_size * p_list->element_size);
        if (NULL == p_list->element_head) {
            perror("[error]: malloc fail when create List.");
            exit(ERR_MEMORY_INSUFFICIENT);
        }
    }
    memcpy(p_list->element_head + (p_list->element_size * p_list->local_size), p_value, p_list->element_size);
    ++p_list->local_size;
    return p_list->element_head + (p_list->element_size * (p_list->local_size - 1));
}

/**
 * 将指定元素按序插入到链表
 * @param p_list
 * @param p_value
 * @param compare_hf    比较钩子，第一个参数为链表中的某元素，第二个参数为待插入的元素（即，value）。要求返回值：1，前者大于后者；-1，后者大于前者；0两者相等。
 * @return  插入元素在链表中的地址
 */
void *listInsertElementSort(List *p_list, void *p_value, int (*compare_hf)(void *, void *)) {
    if (p_list->local_size == p_list->alloc_size) {
        p_list->alloc_size *= 2;
        p_list->element_head = realloc(p_list->element_head, p_list->alloc_size * p_list->element_size);
    }
    if (0 == p_list->local_size) {
        memcpy(p_list->element_head, p_value, p_list->element_size);
        ++p_list->local_size;
        return p_list->element_head;
    }
    for (int i = p_list->local_size - 1; i >= 0; --i) {
        void *p_curr = p_list->element_head + (p_list->element_size * i);
        if (compare_hf(p_curr, p_value) >= 1) {
            memcpy(p_curr + p_list->element_size, p_curr, p_list->element_size);
            if (i == 0) {
                memcpy(p_list->element_head, p_value, p_list->element_size);
                ++p_list->local_size;
                return p_list->element_head;
            }
        } else {
            memcpy(p_curr + p_list->element_size, p_value, p_list->element_size);
            ++p_list->local_size;
            return p_curr + p_list->element_size;
        }
    }
    return NULL;
}

/**
 * 返回链表中元素的个数
 * @param p_list
 */
int listElementNum(List *p_list) {
    return p_list->local_size;
}

/**
 * 将指定元素从链表中删除
 * @param p_list
 * @param p_value
 */
void listDeleteElement(List *p_list, void *p_value) {
    if (p_value == NULL) {
        return;
    }
    void *p_curr = p_list->element_head;
    int i = 0;
    while (i < p_list->local_size && p_value != p_curr) {
        p_curr += p_list->element_size;
        ++i;
    }
    if (p_value == p_curr) {
        if (NULL != p_list->delete_hf) {
            p_list->delete_hf(p_value);
        }
        memcpy(p_curr, p_curr + p_list->element_size, (p_list->local_size - (i + 1)) * p_list->element_size);
        --p_list->local_size;
    }
}

/**
 * 根据属性从链表中删除对应元素（只删除与属性值匹配的第一个元素）
 * @param p_list
 * @param p_attr
 * @param attr_hf   此钩子判断元素是否满足删除条件（即，对应的属性相等）。要求，若满足返回 1；不满足返回 0。
 */
void listDeleteElementByAttr(List *p_list, void *p_attr, int (*attr_hf)(void *, void *)) {
    if (NULL == attr_hf) {
        return;
    }
    void *p_curr;
    for (int i = 0; i < p_list->local_size; ++i) {
        p_curr = p_list->element_head + (i * p_list->element_size);
        if (1 == attr_hf(p_curr, p_attr)) {
            if (NULL != p_list->delete_hf) {
                p_list->delete_hf(p_curr);
            }
            memcpy(p_curr, p_curr + p_list->element_size, (p_list->local_size - (i + 1)) * p_list->element_size);
            --p_list->local_size;
            break;
        }
    }
}

/**
 * 查询链表中是否存在指定元素
 * @param p_list
 * @param p_value
 * @param compare_hf
 * @return
 */
bool listExistElement(List *p_list, void *p_value, int (*compare_hf)(void *, void *)) {
    if (NULL == compare_hf) {
        return false;
    }
    void *p_curr;
    for (int i = 0; i < p_list->local_size; ++i) {
        p_curr = p_list->element_head + (i * p_list->element_size);
        if (0 == compare_hf(p_curr, p_value)) {
            return true;
        }
    }
    return false;
}

/**
 * 通过属性在链表中查找指定元素
 * @param p_list
 * @param p_attr
 * @param attr_hf
 * @return
 */
void *listFindElementByAttr(List *p_list, void *p_attr, int (*attr_hf)(void *, void *)) {
    if (NULL == attr_hf) {
        return NULL;
    }
    void *p_curr;
    for (int i = 0; i < p_list->local_size; ++i) {
        p_curr = p_list->element_head + (i * p_list->element_size);
        if (1 == attr_hf(p_curr, p_attr)) {
            return p_curr;
        }
    }
    return NULL;
}

/**
 * 清空链表
 * @param p_list
 */
void listFlush(List *p_list) {
    p_list->local_size = 0;
}
