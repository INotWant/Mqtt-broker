//
// Created by iwant on 2019/1/22.
//

#include <stdio.h>
#include <sys/sem.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include "util.h"

/**
 * 获取指定字节的第n位。（最右端为0位）
 * @param p_char
 * @param n
 * @return
 */
int getCharNBit(const char *p_char, int n) {
    return *p_char >> n & 0x01;
}

/**
 * 设置字节的指定位为指定值
 */
void setCharNBit(char *pChar, int n, int value) {
    if (value == 1) {
        *pChar |= (1 << n);
    } else {
        *pChar &= ~(1 << n);
    }
}

/**
 * 连续打印 n 个字符
 * @param p_char
 * @param n
 */
void printNchar(char *p_char, int n) {
    for (int i = 0; i < n; ++i) {
        printf("%c", p_char[i]);
    }
}

/**
 * 将整型转为 char[4]
 */
void int2FourChar(int num, char *p_char_head) {
    memcpy(p_char_head, &num, 4);
}

/**
 * 用于从无符号整数（messageId）转为（2字节）char
 */
void int2TwoChar(int num, char *p_char) {
    *p_char = (char) ((num >> 8) & 0xFF);
    *(p_char + 1) = (char) (num & 0xFF);
}

/**
 * 从 char[4] 转为 int
 */
void charArray2Int(char *p_char_head, int *pNum) {
    memcpy(pNum, p_char_head, 4);
}

/**
* 将指定的字节转换为整数
*/
int char2int(const char *p_char) {
    int base = 1;
    int num = 0;
    for (int i = 0; i <= 7; i++) {
        if ((*p_char >> i) & 0x01 != 0) {
            num += base;
        }
        base *= 2;
    }
    return num;
}

/**
 * 获取一个时间戳
 */
long getCurrentTime() {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return ts.tv_sec;
}

/**
 * 获取一个随机数
 * 返回：0~32767
 */
int getRandomNum() {
    return rand();
}

/**
 * 切分主题名
 */
void splitTopic(char *p_topic, int topic_len, char ***pp_topic, int *p_topic_level) {
    // [说明]
    // 1）对以 `/` 开头的主题名，在其最左边添加 `+`；
    // 2）若出现连续的 `/` ，如 `///`，则以一个为准；
    // 3）为了方便操作，在每一个主题名的最右面添加 `/`;
    char *p_temp_topic;
    int topic_array_len;
    if ('/' == p_topic[0]) {
        topic_array_len = topic_len + 2;
        p_temp_topic = malloc((size_t) topic_array_len);
        p_temp_topic[0] = '+';
        memcpy(p_temp_topic + 1, p_topic, (size_t) topic_len);
        p_temp_topic[topic_len + 1] = '/';
    } else {
        topic_array_len = topic_len + 1;
        p_temp_topic = malloc((size_t) topic_array_len);
        memcpy(p_temp_topic, p_topic, (size_t) topic_len);
        p_temp_topic[topic_len] = '/';
    }
    *p_topic_level = 0;             /* 清空 */
    // 第一遍：得到主题名的层数
    int pre_index = -1;             /* 记录上一个 / 出现的位置 */
    for (int i = 0; i < topic_array_len; ++i) {
        if ('/' == p_temp_topic[i]) {
            if (i - pre_index > 1) {
                ++(*p_topic_level);
            }
            pre_index = i;
        }
    }
    *pp_topic = malloc(((size_t) *p_topic_level) * sizeof(char *));
    // 第二遍：正式切分
    pre_index = -1;
    int n = 0;
    for (int i = 0; i < topic_array_len; ++i) {
        if ('/' == p_temp_topic[i]) {
            if (i - pre_index > 1) {
                (*pp_topic)[n] = malloc((size_t) (i - pre_index));
                (*pp_topic)[n][i - pre_index - 1] = '\0';
                memcpy((*pp_topic)[n], p_temp_topic + pre_index + 1, (size_t) (i - pre_index - 1));
                ++n;
            }
            pre_index = i;
        }
    }
    free(p_temp_topic);
}
