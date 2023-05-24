# A C++轻量级、高性能、高并发的Web服务器
## Introduction
该项目仅用于学习，感谢@markparticle学长的项目，看了他的源码后自己手敲一遍加深印象的。他的Readme写的很清晰，下次仓库Readme也要用这样的方式写。

## Function
* 利用IO复用技术Epoll与线程池实现多线程的Reactor高并发模型；
* 利用正则与状态机解析HTTP请求报文，实现处理静态资源的请求；
* 利用标准库容器封装char，实现自动增长的缓冲区；
* 基于小根堆实现的定时器，关闭超时的非活动连接；
* 利用单例模式与阻塞队列实现异步的日志系统，记录服务器运行状态；
* 利用RAII机制实现了数据库连接池，减少数据库连接建立与关闭的开销，同时实现了用户注册登录功能。

* 增加logsys,threadpool测试单元(todo: timer, sqlconnpool, httprequest, httpresponse) 

## Environment
* Ubuntu 18
* Modern C++
* MySql
* Vscode
* git

## 目录树
```
.
├── code           源代码
│   ├── buffer
│   ├── config
│   ├── http
│   ├── log
│   ├── timer
│   ├── pool
│   ├── server
│   └── main.cpp
├── test           单元测试
│   ├── Makefile
│   └── test.cpp
├── resources      静态资源
│   ├── index.html
│   ├── image
│   ├── video
│   ├── js
│   └── css
├── bin            可执行文件
│   └── server
├── log            日志文件
├── webbench-1.5   压力测试
├── build          
│   └── Makefile
├── Makefile
├── LICENSE
└── readme.md
```
## Build & Usage
```
make
./bin/server

需要先配置好对应的数据库
bash
// 建立yourdb库
create database yourdb;

// 创建user表
USE yourdb;
CREATE TABLE user(
    username char(50) NULL,
    password char(50) NULL
)ENGINE=InnoDB;

// 添加数据
INSERT INTO user(username, password) VALUES('name', 'password');
```

## Test
```bash
日志、线程池测试：
cd test
make
./test


服务器压力测试：
cd webbench-1.5
make
webbench -c 1000 -t 30 http://ip:port/

参数：
	-c 表示客户端数
	-t 表示时间
```

![](./imgs/pressure.png)


## Thanks
Linux高性能服务器编程，游双著. 

[@qinguoyi](https://github.com/qinguoyi/TinyWebServer)

[@markparticle](https://github.com/markparticle/WebServer)
