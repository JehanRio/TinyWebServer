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

            1. 连接事件－－＞handleListen()

            2. 写事件－－＞handleWrite()

            3. 读事件－－＞handleRead()

        3. 事件处理完毕，修改反应堆，再跳到２处循环执行

    4. handleListen: 新初始化一个ＨttpConnection对象

    5. handleWrite：　对应连接对象进行处理－－＞若处理成功，则监听事件转换成　读　事件

    6. handleRead：　 对应连接对象进行处理－－＞若处理成功，则监听事件转换成　写　事件
