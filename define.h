//
// Created by iwant on 2019/1/22.
//

#ifndef MQTT_BROKER_DEFINE_H
#define MQTT_BROKER_DEFINE_H

#define IP "127.0.0.1"
#define PORT 1883

#define THREAD_STACK_SIZE 8 * 1024 * 1024

#define SESSION_TABLE_SIZE 1024 * 1024
#define SHM_NUM_TABLE_SIZE 1024 * 1024
#define SUBSCRIBE_TABLE_SIZE 1024 * 1024

#define RETAIN_MESSAGE_LIST_SIZE 100
#define SUBSCRIBE_LIST_SIZE 100
#define MESSAGE_ID_LIST_SIZE 8
#define PUBLISH_MESSAGE_LIST_SIZE 10
#define CLIENT_LIST_SIZE 4

#define BACKLOG SOMAXCONN

#define SESSION_SHM_NUM 10
#define MATCH_SIZE (1 + 5*SESSION_SHM_NUM)

#define TIME_INTERVAL 20

#endif //MQTT_BROKER_DEFINE_H
