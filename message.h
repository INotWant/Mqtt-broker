//
// Created by iwant on 2019/1/23.
//

#ifndef MQTT_BROKER_MESSAGE_H
#define MQTT_BROKER_MESSAGE_H

/**************************
 *     消息（报文） 数据结构
 **************************/

#include <stdbool.h>

/**
 * 所有控制报文（消息）类型
 */
enum MessageType {
    RESERVED_ = 0,
    CONNECT = 1,
    CONNACK = 2,
    PUBLISH = 3,
    PUBACK = 4,
    PUBREC = 5,
    PUBREL = 6,
    PUBCOMP = 7,
    SUBSCRIBE = 8,
    SUBACK = 9,
    UNSUBSCRIBE = 10,
    UNSUBACK = 11,
    PINGREQ = 12,
    PINGRESP = 13,
    DISCONNECT = 14,
    RESERVED__ = 15,
};

/**
 * 固定报头
 */
typedef struct _FixedHeader {
    int message_type;           /* 控制报文类型 */
    int remain_len;             /* 剩余长度 */
    char flag;                  /* 控制报文标志 */
    char reserved;              /* 对齐 */
    // 以下 PUBLISH 报文的固定头部才有（flag 的细化）
    bool dup_flag;              /* 重发标志 */
    int qos;                    /* 服务质量 */
    bool retain_flag;           /* 保留标志 */
    // 固定报头长度（非 MQTT 报文内容）
    int fh_len;                 /* 固定报头长度（非 MQTT 报文内容） */
} FixedHeader;

/******************** CONNECT ********************/

/**
 * CONNECT 可变报头
 */
typedef struct _ConnVariableHeader {
    char *p_protocol_name;      /* 恒指向为 ‘MQTT’ */
    int protocol_name_len;      /* 恒为 4 */
    int level;                  /* 恒为 4 */
    bool username_flag;
    bool passwd_flag;
    bool will_retain;           /* 遗言相关 */
    bool will_flag;
    int will_qos;
    bool is_clean_session;      /* 是否清理会话 */
    char reserved_[3];          /* 对齐 */
    int ping_time;              /* 即 KeepAlive */
} ConnVariableHeader;

/**
 * CONNECT 有效载荷
 */
typedef struct _ConnPayload {
    char *p_client_id;      /* 客户端标识符 */
    int client_id_len;      /* 客户端标识符长度 */
    char *p_will_topic;     /* 遗言主题 */
    int will_topic_len;     /* 遗言主题长度 */
    char *p_will_message;   /* 遗言消息 */
    int will_message_len;   /* 遗言消息长度 */
    char *p_username;       /* 用户名 */
    int username_len;       /* 用户名长度 */
    char *p_passwd;         /* 密码 */
    int passwd_len;         /* 密码长度 */
} ConnPayload;

/******************** PUBLISH ********************/

/**
 * PUBLISH 可变报头
 */
typedef struct _PublishVariableHeader {
    char *p_topic;      /* 发布的主题 */
    int topic_len;
    int message_id;     /* 报文标识符，qos >= 1 时才有 */
} PublishVariableHeader;

/**
 * PUBLISH 有效载荷
 */
typedef struct _PublishPayload {
    char *p_content;    /* 消息内容 */
    int content_len;    /* 内容长度 */
} PublishPayload;

/** PUBACK or PUBREC or PUBREL or PUBCOMP or SUBSCRIBE or UNSUBSCRIBE **/

/**
 * PUBACK or PUBREC or PUBREL or PUBCOMP or SUBSCRIBE or UNSUBSCRIBE 的公共可变报头
 */
typedef struct _CommonVariableHeader {
    int message_id;     /* 报文标识符 */
} CommonVariableHeader;

/******************** SUBSCRIBE ********************/

/**
 * 该结构体中所有指针指向的为重新申请的内存，所以注意释放
 */
typedef struct _SubscribePayload {
    int topic_filter_len;           /* 所含主题过滤器个数 */
    int *p_topic_filter_qos;        /* 订阅质量 */
    char **p_topic_filter;          /* 主题过滤器 */
} SubscribePayload;

/******************** UNSUBSCRIBE ********************/

/**
 * 该结构体中所有指针指向的为重新申请的内存，所以注意释放
 */
typedef struct _UnsubscribePayload {
    int topic_filter_len;           /* 所含主题过滤器个数 */
    char **p_topic_filter;          /* 主题过滤器 */
} UnsubscribePayload;

/******************** MESSAGE（综） ********************/

/**
 * 三个字段都要各自重新申请内存
 */
typedef struct _Message {
    FixedHeader *p_fixed_header;    /* 固定报头 */
    void *p_variable_header;        /* 可变报头 */
    int vh_len;                     /* 可变报头的长度 */
    void *p_payload;                /* 有效载荷 */
    int payload_len;                /* 有效载荷的长度 */
} Message;

/**
 * 用于进程间传递发布内容，或用于发送者未确认的消息存储
 */
typedef struct _PublishMessage {
    int qos;                /* 发布质量 */
    char *p_topic;          /* 发布主题 */
    int topic_len;          /* 主题长度 */
    char *p_content;        /* 发布内容 */
    int content_len;        /* 发布内容长度 */
    int message_id;         /* 发布的报文标识符 */
} PublishMessage;

/**************************
 *     消息（报文） 相关操作
 **************************/

/**
 * 删除报文
 * @param p_message
 */
void deleteMessage(void *p_message);

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
void createPublishMessage(PublishMessage *p_pm, int qos, char *p_topic, int topic_len, char *p_content, int content_len, int message_id);

/**
 * 删除“中间”消息（PublishMessage 结构体）
 * @param p_pm
 */
void deletePublishMessage(void *p_pm);

/******************** 拆包 ********************/

/**
 * 拆包：固定报头
 * @param p_message
 * @param p_fh
 * @return 固定报头所占的字节数
 */
int parseFixedHeader(char *p_message, FixedHeader *p_fh);

/**
 * 拆包：CONNECT （可变报头 & 有效载荷）
 * @param p_message
 * @param p_fh
 * @param p_conn_vh
 * @param p_conn_payload
 */
void parseConnect(char *p_message, FixedHeader *p_fh, ConnVariableHeader *p_conn_vh, ConnPayload *p_conn_payload);

/**
 * 拆包：PUBLISH （可变报头 & 有效载荷）
 * @param p_message
 * @param p_fh
 * @param p_conn_vh
 * @param p_conn_payload
 */
void parsePublish(char *p_message, FixedHeader *p_fh, PublishVariableHeader *p_pub_vh, PublishPayload *p_pub_payload);

/**
 * 拆包：通用可变报头（PUBACK or PUBREC or PUBREL or PUBCOMP or SUBSCRIBE or UNSUBSCRIBE）
 * @param p_message
 * @param p_common_vh
 */
void parseCommonVariableHeader(char *p_message, CommonVariableHeader *p_common_vh);

/**
 * 拆包：SUBSCRIBE（可变报头 & 有效载荷）
 * @param p_message
 * @param p_fh
 * @param p_vh
 * @param p_sub_payload
 */
void parseSubscribe(char *p_message, FixedHeader *p_fh, CommonVariableHeader *p_vh, SubscribePayload *p_sub_payload);

/**
 * 拆包：UNSUBSCRIBE（可变报头 & 有效载荷）
 * @param p_message
 * @param p_fh
 * @param p_vh
 * @param p_unsub_payload
 */
void
parseUnsubscribe(char *p_message, FixedHeader *p_fh, CommonVariableHeader *p_vh, UnsubscribePayload *p_unsub_payload);

/******************** 封包 ********************/

/**
 * 封包：固定报头
 * @param p_message
 * @param p_fh
 * @return
 */
int packageFixedHeader(char *p_message, FixedHeader *p_fh);

/**
 * 封包：CONNACK
 * @param p_message
 * @param is_clean_session
 * @param return_code
 */
void packageConnack(char *p_message, bool is_clean_session, char return_code);

/**
 * 封包：PUBACK PUBREC PUBREL PUBCOMP
 * @param p_message
 * @param p_fh
 * @param message_id
 */
void packageAck(char *p_message, FixedHeader *p_fh, int message_id);

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
                    int content_len);

/**
 * 封包：SUBACK
 * @param p_message
 * @param p_fh
 * @param message_id
 * @param return_code_len
 * @param p_return_code
 */
void packageSuback(char *p_message, FixedHeader *p_fh, int message_id, int return_code_len, char *p_return_code);

#endif //MQTT_BROKER_MESSAGE_H
