cmake_minimum_required(VERSION 3.5.1)
project(MQTT_broker C)

set(CMAKE_C_STANDARD 11)

add_executable(MQTT_broker broker.c define.h collection/list.c collection/list.h broker.h collection/hashTable.c collection/hashTable.h util.c util.h session.c session.h message.c message.h subscribeTree.c subscribeTree.h retainMessage.c retainMessage.h handler.c handler.h)

find_package(Threads REQUIRED)
target_link_libraries(MQTT_broker Threads::Threads)
target_link_libraries(MQTT_broker librt.so.1)
