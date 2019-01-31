//
// Created by iwant on 2019/1/22.
// 哈希表
// 注：value 这里存的是“指针”
//

#ifndef MQTT_BROKER_HASHTABLE_H
#define MQTT_BROKER_HASHTABLE_H

/**
 * 哈希表中的键值对
 */
typedef struct _kv {
    struct _kv *p_next;
    char *key;
    int key_len;
    void *value;
} kv;

/**
 * 哈希表
 */
typedef struct _HashTable {
    kv **table;
    unsigned int table_size;
    int element_num;

    void (*free_value_hf)(void *);
} HashTable;

/**
 * 创建哈希表（初始化）
 * @param hash_table
 * @param table_size
 * @param free_value_hf
 */
void hashTableNew(HashTable *hash_table, unsigned int table_size, void (*free_value_hf)(void *));

/**
 * 销毁哈希表
 * @param hash_table
 */
void hashTableFree(HashTable *hash_table);

/**
 * Put 新的键值对
 * @param hash_table
 * @param key
 * @param len
 * @param value
 */
void hashTablePut(HashTable *hash_table, char *key, int len, void *value);

/**
 * 根据 key 查找 value
 * @param hash_table
 * @param key
 * @param len
 */
void *hashTableGet(HashTable *hash_table, char *key, int len);

/**
 * 依据 key 删除对应键值对
 * @param hash_table
 * @param key
 * @param len
 */
void hashTableRemove(HashTable *hash_table, char *key, int len);

#endif //MQTT_BROKER_HASHTABLE_H
