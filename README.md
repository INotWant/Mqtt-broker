# MQTT broker
============
This toy implements MQTT's broker in Linux.

## 概述

目标：在 `Linux` 平台上实现 `MQTT` 协议。有以下几点说明：

1. 有关 MQTT 协议的介绍，请见 [官网](http://mqtt.org/ "MQTT") 或者阅读 docs 文件夹下的相关文件
2. 针对 MQTT 协议的版本号：`v3.1.1`
3. 目前为止，协议的完成度（只列出未实现的内容）
    - [ ] 遗言机制
    - [ ] 保留消息
    - [ ] 密码登录
    - [ ] 数据持久化
4. 可优化之处
    - [ ] 会话状态哈希表的 key 可以修改为会话对应的 client_id ，现在其为会话对应的 client_sock
    - [ ] 使用多路复用
5. 【注】：此处只实现 MQTT 的 **broker** 端

## 环境

开发 & 运行环境

1. IDE：`CLion 2018.3.3`
2. 构建工具：`CMake`
3. Toolchains：连接远程主机 `ubuntu 16.04`

## 实现

以下两图描述了此处实现 MQTT 的思路：

![data_structure](https://github.com/INotWant/Mqtt-broker/blob/master/docs/images/data_structure.png "重要数据结构")
![run_model](https://github.com/INotWant/Mqtt-broker/blob/master/docs/images/mqtt_run_model.png "运行模型")

## 测试

由于这里只实现了 MQTT 的 broker 端，所以测试中 MQTT 的客户端使用：`mosquitto-clients`。

附：在 ubuntu 上安装 mosquitto-clients 以及对它的使用
- 只需要一条命令：`sudo apt-get install mosquitto-clients`
- [mosquitto_pub和mosquitto_sub 命令参数](https://blog.csdn.net/yangbingzhou/article/details/51324471)

测试步骤及结果

1. 首先运行 `broker.c` 以启动 broker 端，得到如下提示：

![start_broker](https://github.com/INotWant/Mqtt-broker/blob/master/docs/images/start_broker.png "start_broker")

2. 测试客户端订阅
	- 客户端订阅
  
	![sub_1](https://github.com/INotWant/Mqtt-broker/blob/master/docs/images/sub_1.png "sub_1")
  
	- broker 响应
  
	![broker_1](https://github.com/INotWant/Mqtt-broker/blob/master/docs/images/broker_1.png "broker_1")
  
3. 测试客户端发布（qos 0 1 2）
	- 客户端发布
  
	![pub_2](https://github.com/INotWant/Mqtt-broker/blob/master/docs/images/pub_2.png "pub_2")
  
	- broker 响应
  
	![broker_2](https://github.com/INotWant/Mqtt-broker/blob/master/docs/images/broker_2.png "broker_2")
  
	- 客户端接收
  
	![sub_2](https://github.com/INotWant/Mqtt-broker/blob/master/docs/images/sub_1.png "sub_2")
  
## 关于

写在最后：

1. 时间有限，Bug 无限。请多包涵~~
2. 对 c 不熟，可能写的不漂亮。请多包涵~~
3. :star: :star: :star: :two_hearts:
