# WebServer
这是整个WebServer最顶层的接口，每连接进来一个用户，就有一个新的套接字创建并加入epoller中，并且给该用户创建一个时间结点，加入时间堆中，同时将该用户加入users哈希表中（key:fd,value:HttpConn)

## 软件层次的设计
            WebServer                     :服务器逻辑框架：　epoller监听＋线程池读写
                |
                |
        Epoller    Timer                  :epoll操作封装，　定时器给连接计时
            |        |
            ----------
                |
          HttpConnection                  ：把监听连接返回的文件描述符封装成一个连接实例, 对readv, write网络数据传输进行封装，　管理连接
          |            |
    HttpRequest  HttpResponse             ：请求操作封装，响应操作封装，业务逻辑
          |            |
          --------------
                |
              Buffer                      ：读写缓冲区
  
  ThreadPool                              : 线程池，负责读写操作（上图上两层属于主线程，下三层属于线程池）
  Log                                     : 日志类

## 设计
按照软件分层设计的草图，WebServer设计目标为：

+ 监听IO事件
+ 处理超时连接
数据：
int port_; 　　　　//端口

int timeoutMS_; 　　　　//毫秒MS,定时器的默认过期时间

bool isClose_; 　　　//服务启动标志

int listenFd_; 　　//监听文件描述符

bool openLinger_;　　//优雅关闭选项

char* srcDir_;　　　//需要获取的路径

uint32_t listenEvent_;　//初始监听描述符监听设置

uint32_t connectionEvent_;//初始连接描述符监听设置

std::unique_ptrtimer_;　 //定时器

std::unique_ptr threadpool_; //线程池

std::unique_ptr epoller_; //反应堆

std::unordered_map<int, HTTPconnection> users_;//连接队列

+ 函数：

    1. 构造函数: 设置服务器参数　＋　初始化定时器／线程池／反应堆／连接队列

    2. 析构函数: 关闭listenFd_，　销毁　连接队列/定时器／线程池／反应堆

    3. 主函数start()

        1. 创建端口，绑定端口，监听端口，　创建epoll反应堆，　将监听描述符加入反应堆

        2. 等待事件就绪

            1. 连接事件－－＞DealListen()

            2. 写事件－－＞DealWrite()

            3. 读事件－－＞DealRead()

        3. 事件处理完毕，修改反应堆，再跳到２处循环执行

    4. DealListen:  新初始化一个ＨttpConnection对象

    5. DealWrite：　对应连接对象进行处理－－＞若处理成功，则监听事件转换成　读　事件

    6. DealRead：　 对应连接对象进行处理－－＞若处理成功，则监听事件转换成　写　事件
## Epoller
对增删查改的简单封装。
## WebServer 类详解
### 1. 初始化
```c++
threadpool_(new ThreadPool(threadNum))
InitSocket_();//初始化Socket连接
InitEventMode_(trigMode);//初始化事件模式
SqlConnPool::Instance()->Init();//初始化数据库连接池
Log::Instance()->init(logLevel, "./log", ".log", logQueSize);   
```
创建线程池：线程池的构造函数中会创建线程并且detach()

初始化Socket的函数`InitSocket_();` C/S中，服务器套接字的初始化无非就是socket - bind - listen - accept - 发送接收数据这几个过程；函数执行到listen后，把前面得到的listenfd添加到epoller模型中，即把accept()和接收数据的操作交给epoller处理了。并且把该监听描述符设置为非阻塞。

初始化事件模式函数`InitEventMode_(trigMode);`，将`listenEvent_` 和 `connEvent_`都设置为EPOLLET模式。

初始化数据库连接池`SqlConnPool::Instance()->Init();`创造单例连接池，执行初始化函数。

初始化日志系统：在初始化函数中，创建阻塞队列和写线程，并创建日志。

### 2. 启动WebServer
接下来启动WebServer，首先需要设定`epoll_wait()`等待的时间，这里我们选择调用定时器的`GetNextTick()`函数，这个函数的作用是返回最小堆堆顶的连接设定的过期时间与现在时间的差值。这个时间的选择可以保证服务器等待事件的时间不至于太短也不至于太长。接着调用`epoll_wait()`函数，返回需要已经就绪事件的数目。这里的就绪事件分为两类：收到新的http请求和其他的读写事件。
这里设置两个变量fd和events分别用来存储就绪事件的文件描述符和事件类型。

1.收到新的HTTP请求的情况

在fd==listenFd_的时候，也就是收到新的HTTP请求的时候，调用函数DealListen_();处理监听，接受客户端连接；

2.已经建立连接的HTTP发来IO请求的情况

在events& EPOLLIN 或events & EPOLLOUT为真时，需要进行读写的处理。分别调用 DealRead_(&users_[fd])和DealWrite_(&users_[fd]) 函数。这里需要说明：DealListen_()函数并没有调用线程池中的线程，而DealRead_(&users_[fd])和DealWrite_(&users_[fd]) 则都交由线程池中的线程进行处理了。

### 3. I/O处理的具体流程
`DealRead_(&users_[fd])`和`DealWrite_(&users_[fd])` 通过调用
```c++
threadpool_->AddTask(std::bind(&WebServer::OnRead_, this, client));     //读
threadpool_->AddTask(std::bind(&WebServer::OnWrite_, this, client));    //写
```
函数来取出线程池中的线程继续进行读写，而主进程这时可以继续监听新来的就绪事件了。

`OnRead_()`和`OnWrite_()`函数分别进行读写的处理。

`OnRead_()`函数首先把数据从缓冲区中读出来(调用HttpConn的read,read调用ReadFd读取到读缓冲区BUFFER)，然后交由逻辑函数`OnProcess()`处理。这里多说一句，`process()`函数在解析请求报文后随即就生成了响应报文等待OnWrite_()函数发送。

这里必须说清楚OnRead_()和OnWrite_()函数进行读写的方法，那就是：**分散读和集中写**
> 分散读（scatter read）和集中写（gatherwrite）具体来说是来自读操作的输入数据被分散到多个应用缓冲区中，而来自应用缓冲区的输出数据则被集中提供给单个写操作。
这样做的好处是：它们只需一次系统调用就可以实现在文件和进程的多个缓冲区之间传送数据，免除了多次系统调用或复制数据的开销。

`OnWrite_()`函数首先把之前根据请求报文生成的响应报文从缓冲区交给fd，传输完成后修改该fd的events.

``OnProcess()``就是进行业务逻辑处理（解析请求报文、生成响应报文）的函数了。具体可看http中的readme.md

参考博客：https://blog.csdn.net/ccw_922/article/details/124530436