//
// Created by iwant on 2019/1/22.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hashTable.h"
#include "../broker.h"

/**
 * 创建新的 kv
 */
kv *kvNew(char *key, int len, void *value) {
    kv *p_kv = malloc(sizeof(kv));
    if (NULL == p_kv) {
        perror("malloc fail when create kv.");
        exit(ERR_MEMORY_INSUFFICIENT);
    }
    char *new_key = malloc((size_t) len);
    if (NULL == new_key) {
        perror("malloc fail when create kv.");
        exit(ERR_MEMORY_INSUFFICIENT);
    }
    memcpy(new_key, key, (size_t) len);
    p_kv->key = new_key;
    p_kv->key_len = len;
    p_kv->value = value;
    p_kv->p_next = NULL;
    return p_kv;
}

/**
 * 销毁 kv
 */
void kvFree(kv *p_kv, void (*free_value_hf)(void *)) {
    free(p_kv->key);
    if (NULL != free_value_hf) {
        free_value_hf(p_kv->value);
    }
    free(p_kv);
}

/**
 * 哈希函数
 */
unsigned int hashFunction(char *key, int len) {
    unsigned int hash = 0;
    for (int i = 0; i < len; ++i) {
        hash = (hash << 5) + hash + *key++;
    }
    return hash;
}

/**
 * 创建哈希表（初始化）
 * @param hash_table
 * @param table_size
 * @param free_value_hf
 */
void hashTableNew(HashTable *hash_table, unsigned int table_size, void (*free_value_hf)(void *)) {
    hash_table->table_size = table_size;
    hash_table->element_num = 0;
    hash_table->table = malloc(sizeof(kv *) * table_size);
    if (NULL == hash_table->table) {
        perror("malloc fail when create hash table.");
        exit(ERR_MEMORY_INSUFFICIENT);
    }
    memset(hash_table->table, 0, sizeof(kv *) * table_size);
    hash_table->free_value_hf = free_value_hf;
}

/**
 * 销毁哈希表
 * @param hash_table
 */
void hashTableFree(HashTable *hash_table) {
    if (NULL != hash_table) {
        for (int i = 0; i < hash_table->table_size; ++i) {
            if (NULL != (hash_table->table[i])) {
                kv *p_curr = hash_table->table[i];
                kv *p_next;
                while (NULL != p_curr) {
                    p_next = p_curr->p_next;
                    kvFree(p_curr, hash_table->free_value_hf);
                    p_curr = p_next;
                }
            }
        }
        free(hash_table->table);
    }
}

/**
 * Put 新的键值对
 * @param hash_table
 * @param key
 * @param len
 * @param value
 */
void hashTablePut(HashTable *hash_table, char *key, int len, void *value) {
    int hash_value = hashFunction(key, len);
    int table_index = hash_value % hash_table->table_size;
    if (NULL == hash_table->table[table_index]) {
        hash_table->table[table_index] = kvNew(key, len, value);
    } else {
        kv *p_curr = hash_table->table[table_index];
        kv *p_pre;
        while (NULL != p_curr) {
            if (len == p_curr->key_len && memcmp(p_curr->key, key, (size_t) len) == 0) {
                if (NULL != hash_table->free_value_hf) {
                    hash_table->free_value_hf(p_curr->value);
                }
                p_curr->value = value;
                return;
            }
            p_pre = p_curr;
            p_curr = p_curr->p_next;
        }
        p_pre->p_next = kvNew(key, len, value);
    }
    ++hash_table->element_num;
}

/**
 * 根据 key 查找 value
 * @param hash_table
 * @param key
 * @param len
 */
void *hashTableGet(HashTable *hash_table, char *key, int len) {
    int hash_value = hashFunction(key, len);
    int table_index = hash_value % hash_table->table_size;
    kv *p_curr = hash_table->table[table_index];
    while (NULL != p_curr) {
        if (len == p_curr->key_len && memcmp(p_curr->key, key, (size_t) len) == 0) {
            return p_curr->value;
        }
        p_curr = p_curr->p_next;
    }
    return NULL;
}

/**
 * 依据 key 删除对应键值对
 * @param hash_table
 * @param key
 * @param len
 */
void hashTableRemove(HashTable *hash_table, char *key, int len) {
    int hash_value = hashFunction(key, len);
    int table_index = hash_value % hash_table->table_size;
    if (NULL != hash_table->table[table_index]) {
        kv *p_curr = hash_table->table[table_index];
        kv *p_pre = NULL;
        while (NULL != p_curr) {
            if (len == p_curr->key_len && memcmp(p_curr->key, key, (size_t) len) == 0) {
                break;
            }
            p_pre = p_curr;
            p_curr = p_curr->p_next;
        }
        if (NULL != p_curr) {
            if (NULL == p_pre) {
                hash_table->table[table_index] = p_curr->p_next;
            } else {
                p_pre->p_next = p_curr->p_next;
            }
            kvFree(p_curr, hash_table->free_value_hf);
            --hash_table->element_num;
        }
    }
}
