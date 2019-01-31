//
// Created by iwant on 2019/1/25.
//

#ifndef MQTT_BROKER_HANDLER_H
#define MQTT_BROKER_HANDLER_H

#include "message.h"
#include "broker.h"

/**
 * 响应 CONNECT
 * @param client_sock
 * @param pp_session
 * @param p_broker
 * @param p_vh
 * @param p_payload
 */
void handlerConnect(int client_sock, Session **pp_session, Broker *p_broker, ConnVariableHeader *p_vh,
                    ConnPayload *p_payload);

/**
 * 响应 PUBLISH
 * @param p_session
 * @param p_broker
 * @param p_fh
 * @param p_vh
 * @param p_payload
 * @param p_shm_table
 * @param p_shm_table_mutex
 */
void
handlerPublish(Session *p_session, Broker *p_broker, FixedHeader *p_fh, PublishVariableHeader *p_vh,
               PublishPayload *p_payload, HashTable *p_shm_table, pthread_mutex_t *p_shm_table_mutex);

/**
 * 响应 PUBACK
 * @param p_session
 * @param message_id
 */
void handlerPuback(Session *p_session, int message_id);

/**
 * 响应 PUBREC
 * @param p_session
 * @param message_id
 */
void handlerPubrec(Session *p_session, int message_id);

/**
 * 响应 pubrel
 * @param p_session
 * @param message_id
 */
void handlerPubrel(Session *p_session, int message_id);

/**
 * 响应 pubcomp
 * @param p_session
 * @param message_id
 */
void handlerPubcomp(Session *p_session, int message_id);

/**
 * 响应 SUBSCRIBE
 * @param p_session
 * @param p_broker
 * @param p_fh
 * @param p_vh
 * @param p_sub_payload
 */
void handlerSubscribe(Session *p_session, Broker *p_broker, FixedHeader *p_fh, CommonVariableHeader *p_vh,
                      SubscribePayload *p_sub_payload);

/**
 * 响应 UNSUBSCRIBE
 * @param p_session
 * @param p_broker
 * @param p_fh
 * @param p_vh
 * @param p_unsub_payload
 */
void handlerUnsubscribe(Session *p_session, Broker *p_broker, FixedHeader *p_fh, CommonVariableHeader *p_vh,
                        UnsubscribePayload *p_unsub_payload);

/**
 * 响应 PINGREQ
 * @param p_session
 */
void handlerPingreq(Session *p_session);

/**
 * 响应 DISCONNECT
 * @param p_session
 * @param p_broker
 */
void handlerDisconnect(Session *p_session, Broker *p_broker);

/**
 * 发布（推送）消息总逻辑
 * @param p_session
 * @param p_shm_table
 * @param p_shm_table_mutex
 */
void handlerSend(Session *p_session, HashTable *p_shm_table, pthread_mutex_t *p_shm_table_mutex);

/**
 * 发布 qos0 消息
 * @param client_sock
 * @param p_pm
 */
void handlerSendQos0(Session *p_session, PublishMessage *p_pm);

/**
 * 发布 qos1 消息
 * @param p_session
 * @param p_pm
 */
void handlerSendQos1(Session *p_session, PublishMessage *p_pm);

/**
 * 发布 qos2 消息
 * @param p_session
 * @param p_pm
 */
void handlerSendQos2(Session *p_session, PublishMessage *p_pm);

#endif //MQTT_BROKER_HANDLER_H
