//
// Created by iwant on 2019/1/23.
//

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "message.h"
#include "util.h"

/****************** 一些声明 ******************/

int getRemainLength(const char *p_remain_len);

int getMessageInt(char *p_int);

/******************* 分割线 *******************/

/**
 * 删除报文
 * @param p_message
 */
void deleteMessage(void *p_message) {
    Message *p_m = p_message;
    free(p_m->p_fixed_header);
    free(p_m->p_variable_header);
    free(p_m->p_payload);
}

/**
 * 创建 PublishMessage
 * @param p_pm
 * @param qos
 * @param p_topic
 * @param topic_len
 * @param p_content
 * @param content_len
 * @param message_id
 */
void createPublishMessage(PublishMessage *p_pm, int qos, char *p_topic, int topic_len, char *p_content, int content_len,
                          int message_id) {
    p_pm->qos = qos;
    p_pm->p_topic = malloc((size_t) (topic_len + 1));
    p_pm->p_topic[topic_len] = '\0';
    p_pm->topic_len = topic_len;
    memcpy(p_pm->p_topic, p_topic, (size_t) topic_len);
    p_pm->p_content = malloc((size_t) (content_len + 1));
    p_pm->p_content[content_len] = '\0';
    p_pm->content_len = content_len;
    memcpy(p_pm->p_content, p_content, (size_t) content_len);
    p_pm->message_id = message_id;
}

/**
 * 删除“中间”消息（PublishMessage 结构体）
 * @param p_pm
 */
void deletePublishMessage(void *p_pm) {
    PublishMessage *p_publish_message = p_pm;
    free(p_publish_message->p_topic);
    free(p_publish_message->p_content);
}

/******************** 拆包 ********************/

/**
 * 拆包：固定报头
 * @param p_message
 * @param p_fh
 * @return 固定报头所占的字节数
 */
int parseFixedHeader(char *p_message, FixedHeader *p_fh) {
    p_fh->message_type = ((p_message[0] & 0xF0) >> 4);
    p_fh->flag = (char) (p_message[0] & 0x0F);
    p_fh->remain_len = getRemainLength(p_message + 1);
    if (PUBLISH == p_fh->message_type) {
        p_fh->dup_flag = (bool) getCharNBit(&p_fh->flag, 3);
        p_fh->qos = getCharNBit(&p_fh->flag, 2) * 2;
        p_fh->qos += getCharNBit(&p_fh->flag, 1);
        p_fh->retain_flag = (bool) getCharNBit(&p_fh->flag, 0);
    }
    int byteNum = 1;
    if (p_fh->remain_len <= 127) {
        byteNum += 1;
    } else if (p_fh->remain_len <= 16383) {
        byteNum += 2;
    } else if (p_fh->remain_len <= 2097151) {
        byteNum += 3;
    } else {
        byteNum += 4;
    }
    p_fh->fh_len = byteNum;
    return byteNum;
}

/**
 * 拆包：CONNECT （可变报头 & 有效载荷）
 * @param p_message
 * @param p_fh
 * @param p_conn_vh
 * @param p_conn_payload
 */
void parseConnect(char *p_message, FixedHeader *p_fh, ConnVariableHeader *p_conn_vh, ConnPayload *p_conn_payload) {
    // 0）一些必须的初始化
    p_conn_payload->will_topic_len = 0;
    p_conn_payload->will_message_len = 0;
    p_conn_payload->username_len = 0;
    p_conn_payload->passwd_len = 0;
    // 1）解析可变报头
    p_conn_vh->protocol_name_len = getMessageInt(p_message + p_fh->fh_len);
    p_conn_vh->p_protocol_name = p_message + p_fh->fh_len + 2;
    p_conn_vh->level = char2int(p_message + p_fh->fh_len + 2 + p_conn_vh->protocol_name_len);
    char *p_curr = p_message + p_fh->fh_len + 2 + p_conn_vh->protocol_name_len + 1;
    p_conn_vh->username_flag = (bool) getCharNBit(p_curr, 7);
    p_conn_vh->passwd_flag = (bool) getCharNBit(p_curr, 6);
    p_conn_vh->will_retain = (bool) getCharNBit(p_curr, 5);
    p_conn_vh->will_qos = getCharNBit(p_curr, 4) * 2;
    p_conn_vh->will_qos += getCharNBit(p_curr, 3);
    p_conn_vh->will_flag = (bool) getCharNBit(p_curr, 2);
    p_conn_vh->is_clean_session = (bool) getCharNBit(p_curr, 1);
    p_curr += 1;
    p_conn_vh->ping_time = getMessageInt(p_curr);
    p_curr += 2;
    // 2）解析有效载荷
    p_conn_payload->client_id_len = getMessageInt(p_curr);
    p_curr += 2;
    p_conn_payload->p_client_id = p_curr;
    p_curr += p_conn_payload->client_id_len;
    if (p_conn_vh->will_flag) {
        p_conn_payload->will_topic_len = getMessageInt(p_curr);
        p_curr += 2;
        p_conn_payload->p_will_topic = p_curr;
        p_curr += p_conn_payload->will_topic_len;
        p_conn_payload->will_message_len = getMessageInt(p_curr);
        p_curr += 2;
        p_conn_payload->p_will_message = p_curr;
        p_curr += p_conn_payload->will_message_len;
    }
    if (p_conn_vh->username_flag) {
        p_conn_payload->username_len = getMessageInt(p_curr);
        p_curr += 2;
        p_conn_payload->p_username = p_curr;
        p_curr += p_conn_payload->username_len;
    }
    if (p_conn_vh->passwd_flag) {
        p_conn_payload->passwd_len = getMessageInt(p_curr);
        p_curr += 2;
        p_conn_payload->p_passwd = p_curr;
    }
}

/**
 * 拆包：PUBLISH （可变报头 & 有效载荷）
 * @param p_message
 * @param p_fh
 * @param p_conn_vh
 * @param p_conn_payload
 */
void parsePublish(char *p_message, FixedHeader *p_fh, PublishVariableHeader *p_pub_vh, PublishPayload *p_pub_payload) {
    // 1）解析可变报头
    int vh_len = 0;
    char *p_curr = p_message + p_fh->fh_len;
    p_pub_vh->topic_len = getMessageInt(p_curr);
    p_curr += 2;
    vh_len += 2;
    p_pub_vh->p_topic = p_curr;
    p_curr += p_pub_vh->topic_len;
    vh_len += p_pub_vh->topic_len;
    if (p_fh->qos >= 1) {
        p_pub_vh->message_id = getMessageInt(p_curr);
        p_curr += 2;
        vh_len += 2;
    }
    // 2）解析有效载荷
    p_pub_payload->content_len = p_fh->remain_len - vh_len;
    p_pub_payload->p_content = p_curr;
}

/**
 * 拆包：通用可变报头（PUBACK or PUBREC or PUBREL or PUBCOMP or SUBSCRIBE or UNSUBSCRIBE）
 * @param p_message
 * @param p_common_vh
 */
void parseCommonVariableHeader(char *p_message, CommonVariableHeader *p_common_vh) {
    p_common_vh->message_id = getMessageInt(p_message);
}

/**
 * 拆包：SUBSCRIBE（可变报头 & 有效载荷）
 * @param p_message
 * @param p_fh
 * @param p_vh
 * @param p_sub_payload
 */
void parseSubscribe(char *p_message, FixedHeader *p_fh, CommonVariableHeader *p_vh, SubscribePayload *p_sub_payload) {
    char *p_curr = p_message + p_fh->fh_len;
    int read_len = 0;
    // 1）解析可变报头
    parseCommonVariableHeader(p_curr, p_vh);
    p_curr += 2;
    read_len += 2;
    // 2）解析有效载荷
    int topic_filter_len = 0;
    while (read_len < p_fh->remain_len) {
        int t_len = getMessageInt(p_curr);
        p_curr += 3;
        p_curr += t_len;
        read_len += (3 + t_len);
        ++topic_filter_len;
    }
    p_sub_payload->topic_filter_len = topic_filter_len;
    p_sub_payload->p_topic_filter_qos = malloc(topic_filter_len * sizeof(int));
    p_sub_payload->p_topic_filter = malloc(topic_filter_len * sizeof(char *));
    p_curr = p_message + p_fh->fh_len + 2;
    for (int i = 0; i < topic_filter_len; ++i) {
        int t_len = getMessageInt(p_curr);
        p_curr += 2;
        p_sub_payload->p_topic_filter[i] = malloc((size_t) (t_len + 1));
        p_sub_payload->p_topic_filter[i][t_len] = '\0';
        memcpy(p_sub_payload->p_topic_filter[i], p_curr, (size_t) t_len);
        p_curr += t_len;
        p_sub_payload->p_topic_filter_qos[i] = char2int(p_curr);
        p_curr += 1;
    }
}

/**
 * 拆包：UNSUBSCRIBE（可变报头 & 有效载荷）
 * @param p_message
 * @param p_fh
 * @param p_vh
 * @param p_unsub_payload
 */
void
parseUnsubscribe(char *p_message, FixedHeader *p_fh, CommonVariableHeader *p_vh, UnsubscribePayload *p_unsub_payload) {
    char *p_curr = p_message + p_fh->fh_len;
    int read_len = 0;
    // 1）解析可变报头
    parseCommonVariableHeader(p_curr, p_vh);
    p_curr += 2;
    read_len += 2;
    // 2）解析有效载荷
    int topic_filter_len = 0;
    while (read_len < p_fh->remain_len) {
        int t_len = getMessageInt(p_curr);
        p_curr += 2;
        p_curr += t_len;
        read_len += (2 + t_len);
        ++topic_filter_len;
    }
    p_unsub_payload->topic_filter_len = topic_filter_len;
    p_unsub_payload->p_topic_filter = malloc(topic_filter_len * sizeof(char *));
    p_curr = p_message + p_fh->fh_len + 2;
    for (int i = 0; i < topic_filter_len; ++i) {
        int t_len = getMessageInt(p_curr);
        p_curr += 2;
        p_unsub_payload->p_topic_filter[i] = malloc((size_t) (t_len + 1));
        p_unsub_payload->p_topic_filter[i][t_len] = '\0';
        memcpy(p_unsub_payload->p_topic_filter[i], p_curr, (size_t) t_len);
        p_curr += t_len;
    }
}

/******************** 封包 ********************/

/**
 * 封包：固定报头
 * @param p_message
 * @param p_fh
 * @return
 */
int packageFixedHeader(char *p_message, FixedHeader *p_fh) {
    int byteNum = 1;
    p_message[0] = (char) ((p_fh->message_type << 4) + (p_fh->flag & 0x0F));
    if (p_fh->remain_len <= 127) {
        byteNum += 1;
        p_message[1] = (char) p_fh->remain_len;
        setCharNBit(p_message + 1, 7, 0);
    } else if (p_fh->remain_len <= 16383) {
        byteNum += 2;
        p_message[1] = (char) (p_fh->remain_len % 128);
        setCharNBit(p_message + 1, 7, 1);
        p_message[2] = (char) (p_fh->remain_len / 128);
        setCharNBit(p_message + 2, 7, 0);
    } else if (p_fh->remain_len <= 2097151) {
        byteNum += 3;
        p_message[1] = (char) (p_fh->remain_len % 128);
        setCharNBit(p_message + 1, 7, 1);
        p_message[2] = (char) (p_fh->remain_len / 128 % 128);
        setCharNBit(p_message + 2, 7, 1);
        p_message[3] = (char) (p_fh->remain_len / 128 / 128);
        setCharNBit(p_message + 3, 7, 0);
    } else {
        byteNum += 4;
        p_message[1] = (char) (p_fh->remain_len % 128);
        setCharNBit(p_message + 1, 7, 1);
        p_message[2] = (char) (p_fh->remain_len / 128 % 128);
        setCharNBit(p_message + 2, 7, 1);
        p_message[3] = (char) (p_fh->remain_len / 16384 % 128);
        setCharNBit(p_message + 3, 7, 1);
        p_message[4] = (char) (p_fh->remain_len / 2097152);
        setCharNBit(p_message + 4, 7, 0);
    }
    return byteNum;
}

/**
 * 封包：CONNACK
 * @param p_message
 * @param is_clean_session
 * @param return_code
 */
void packageConnack(char *p_message, bool is_clean_session, char return_code) {
    FixedHeader fh;
    fh.message_type = CONNACK;
    fh.flag = 0x00;
    fh.remain_len = 2;
    p_message += packageFixedHeader(p_message, &fh);
    p_message[0] = is_clean_session;
    p_message[1] = return_code;
}

/**
 * 封包：PUBACK
 * @param p_message
 * @param p_fh
 * @param message_id
 */
void packageAck(char *p_message, FixedHeader *p_fh, int message_id) {
    packageFixedHeader(p_message, p_fh);
    int2TwoChar(message_id, p_message + 2);
}

/**
 * 封包：PUBLISH
 * @param p_message
 * @param p_fh
 * @param p_topic
 * @param topic_len
 * @param message_id
 * @param p_content
 * @param content_len
 */
void packagePublish(char *p_message, FixedHeader *p_fh, char *p_topic, int topic_len, int message_id, char *p_content,
                    int content_len) {
    int byte_num = packageFixedHeader(p_message, p_fh);
    int2TwoChar(topic_len, p_message + byte_num);
    byte_num += 2;
    memcpy(p_message + byte_num, p_topic, (size_t) topic_len);
    byte_num += topic_len;
    if (p_fh->qos >= 1) {
        int2TwoChar(message_id, p_message + byte_num);
        byte_num += 2;
    }
    memcpy(p_message + byte_num, p_content, (size_t) content_len);
}

/**
 * 封包：SUBACK
 * @param p_message
 * @param p_fh
 * @param message_id
 * @param return_code_len
 * @param p_return_code
 */
void packageSuback(char *p_message, FixedHeader *p_fh, int message_id, int return_code_len, char *p_return_code) {
    int byte_num = packageFixedHeader(p_message, p_fh);
    int2TwoChar(message_id, p_message + byte_num);
    byte_num += 2;
    memcpy(p_message + byte_num, p_return_code, (size_t) return_code_len);
}

/****************** 内部操作 ******************/

/**
 * 获取剩余长度
 */
int getRemainLength(const char *p_remain_len) {
    int count = 0;
    int base = 128;
    int remainLength = (p_remain_len[count] & 0x7F);
    while ((p_remain_len[count] >> 7) & 0x01 != 0) {
        count++;
        remainLength += (p_remain_len[count] & 0x7F) * base;
        base *= 128;
    }
    return remainLength;
}

/**
 * 用于读取报文中的所有整型
 */
int getMessageInt(char *p_int) {
    int num = 0;
    num += (char2int(p_int) * 256);
    num += (char2int(p_int + 1));
    return num;
}
