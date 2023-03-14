# A C++轻量级、高性能、高并发的Web服务器

## Introduction

该原项目是牛客网的WebServer，实现了一个在Linux环境下C++开发的轻量级、高性能、高并发的web服务器。

目前仅是将项目复现了一遍，该项目用到的技术框架有：

+ 线程池+非阻塞socket+epoll+事件处理的并发模型
+ 状态机解析HTTP请求

> 后续会逐渐加入同步/异步日志系统和数据库等功能。

## Environment

+ OS:Ubuntu 18
+ VScode
+ g++ 7.5

## Build

`g++ *.cpp -pthread`

## Usage

`./a.out`

## Model

该项目采用**模拟Proactor模型**。

- 使用同步I/O方式模拟出Proactor模式。
- 原理是: `主线程执行读写操作`，读写完成之后，主线程向工作线程通知这一"完成事件"。从`工作线程`的角度来看，它们就`直接获取了数据读写的结果`，接下来要做的只是对读写的结果进行逻辑处理。

+ 使用同步I/O模型(仍以epoll_wait为例)模拟出的Proactor模式的工作流程如下:(其中socket为连接socket)

  1. `主线程`往epoll内核事件表中`注册socket上的读就绪事件`。

  2. `主线程`调用epoll_wait`等待socket上有数据可读`。

  3. 当socket上有数据可读时，epoll_wait通知主线程。`主线程`从socket上循环`读取数据`，将读取到的数据封装成一个请求对象并`插入到请求队列`中。

  4. 睡眠在请求队列上的某个`工作线程被唤醒`，它获得请求对象并`处理客户请求`，然后往epoll内核表中`注册socket上的写就绪事件`。

  5. `主线程`调用epoll_wait等待socket可读。

  6. 当socket可写时，epoll_wait`通知主线程`。`主线程往socket上写入服务器处理客户请求的结果`。
+ 总结: 主线程负责I/O操作，工作线程仅负责数据的处理(业务逻辑)。

![image-20221020180055400](https://img-blog.csdnimg.cn/img_convert/d3dcc8214466753341324efac3db15a3.png)

## 线程池

线程池是由服务器预先创建的一组子线程，线程池中的线程数量应该和CPU数量差不多。线程池中的所有子线程都运行这相同的代码。当有新的任务来时，主线程将通过某种方式选择线程池中的某一个子线程进行服务。相比于动态的创建子线程，选择一个已经存在的子线程的代价显然要小得多。至于主线程选择哪个子线程来为新任务服务，则有多种方式：

+ 主线程使用某种算法来主动选择子线程。最简单、最常用的算法是随机算法和Round Robin（轮流选取）算法，但更优秀、更智能的算法将使任务在各个工作线程中更均匀地分配，从而减轻服务器的整体压力。
+ 主线程和所有子线程通过一个共享的工作队列来同步，子线程都睡眠在该工作队列上。当有新的任务到来时，主线程将任务添加到工作队列中。这将唤醒正在等待任务的子线程，不过只有一个子线程将获得新任务的“接管权”，它可以从工作队列中取出任务并执行，而其他子线程将继续睡眠在工作队列上。

该项目的线程池创建时创建了8个脱离子线程，子线程执行线程池中的任务（`pool->run()`）。

`run()` 函数中则将请求队列即`std::list<T*> m_workqueue`中的任务取出执行。其中，模板任务T中必须含有process()函数，这样才能将线程池中的任务和业务逻辑任务相连接。

> ps. 需要在threadpool.cpp中声明模板类型，否则通过不了编译，我也不知道为什么，网上查阅资料说模板定义和实现一般都放在一个文件即.h文件中，放在两个文件就会出现无定义这样的情况。

## HTTP请求报文

### 有限状态机

逻辑单元内部的一种高效编程方法：有效状态机。

有的应用层协议头部包含数据包类型字段，每种类型可以映射为逻辑单元的一种执行状态卖服务器可以根据它来编写相应的处理逻辑。

+ 状态机有 3 个组成部分：状态、事件、动作。
  + 状态：所有可能存在的状态。包括当前状态和条件满足后要迁移的状态。
  + 事件：也称为转移条件，当一个条件被满足，将会触发一个动作，或者执行一次状态的迁移。
  + 动作：条件满足后执行的动作。动作执行完毕后，可以迁移到新的状态，也可以仍旧保持原状态。动作不是* 必需的，当条件满足后，也可以不执行任何动作，直接迁移到新状态。

### 逻辑代码的实现

+ `run()`中执行任务类的接口函数，`process()`

+ `process()`先解析HTTP请求`process_read()`，再生成相应`process_write()`，这样主函数中的epollfd就能检测到写事件，即进行`write()`

  + `process_read`：这里就用到了上面提到的有限状态机。首先每次获取一行数据，然后在switch中执行`parse_request_line(char* text)`

    + `parse_request_line(char* text)`：解析请求首行，获取请求方法、目标url、HTTP版本，成功则返回一个新的状态，即解析请求头`parse_headers(char* text)`

    > 若读取不完整，返回了`NO_REQUEST`，需要再次将文件描述符置为EPOLLIN，因为我们采取了ET边沿触发。

  + `process_write()`：根据返回的状态，进行相应的状态信息和状态内容进行返回。

    + 写的时候采取分散写`writev`的方式。其中一个为写缓冲区的buf，另一个为通过客户请求的目标文件（即HTML文件）被mmap到内存中的地址。

# Others

## 定时检测非活跃连接

+ 创建用户数据结构类和定时器类，再创建一个链表，连接各个定时器。
+ 还是需要创建一个epoll，每连接一个用户，就将监听到一个新用户，然后将该用户放入用户数组中，并把他的定时器放入链表。
+ 创建一个时钟，每过一定时间进行依次定时器回调函数即`timer_lst.tick()`，处理掉过期的任务

+ `cb_func(client_data*)`：为定时回调函数，回调函数处理的客户数。

采用telnet进行通信登录，具体命令为：`telnet IP 端口号`

## 服务器压力测试

Webbench是Linux上一款知名的、优秀的web性能压力测试工具。

+ 测试处在相同硬件上，不同服务的性能以及不同硬件上同一个服务的运行状况。
+ 展示服务器的两项内容：每秒钟响应请求数和每秒钟传输数据量。

基本原理：webbench首先fork出多个子进程，每个子进程都循环做web访问测试。子进程把访问的结果通过pipe告诉父进程，父进程做最终的捅机结果。

```shell
webbench -c 1000 -t 30 http://192.168.126.130/index.html

参数：
	-c 表示客户端数
	-t 表示时间
```

