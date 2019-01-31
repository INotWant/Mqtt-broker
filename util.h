//
// Created by iwant on 2019/1/22.
//

#ifndef MQTT_BROKER_UTIL_H
#define MQTT_BROKER_UTIL_H

#include <signal.h>

/**
 * 获取指定字节的第 n 位。（最右端为0位）
 */
int getCharNBit(const char *p_char, int n);

/**
 * 设置字节的指定位为指定值
 */
void setCharNBit(char *pChar, int n, int value);

/**
 * 连续打印 n 个字符
 */
void printNchar(char *p_char, int n);

/**
 * 将整型转为 char[4]
 */
void int2FourChar(int num, char *p_char_head);

/**
 * 用于从无符号整数（messageId）转为（2字节）char
 */
void int2TwoChar(int num, char *p_char);

/**
 * 从 char[4] 转为 int
 */
void charArray2Int(char *p_char_head, int *pNum);

/**
 * 将指定的字节转换为整数
 */
int char2int(const char *p_char);

/**
 * 获取一个时间戳
 */
long getCurrentTime();

/**
 * 获取一个随机数
 * 返回：0~32767
 */
int getRandomNum();

/**
 * 切分主题名
 */
void splitTopic(char *p_topic, int topic_len, char ***pp_topic, int *p_topic_level);

#endif //MQTT_BROKER_UTIL_H
