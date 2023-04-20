# HTTP

## HTTP请求报文解析与响应报文生成

### 请求报文

HTTP请求报文的结构如下：

包括请求行、请求头部、空行和请求数据四个部分。

![](https://img-blog.csdnimg.cn/6141ab5159cb4fbaa09d249bdd7201c4.png)



以下是百度的请求包

> GET / HTTP/1.1
> Accept:text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,image/apng,/;q=0.8,application/signed-exchange;v=b3;q=0.9
> Accept-Encoding: gzip, deflate, br
> Accept-Language: zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6
> Connection: keep-alive
> Host: www.baidu.com
> Sec-Fetch-Dest: document
> Sec-Fetch-Mode: navigate
> Sec-Fetch-Site: none
> Sec-Fetch-User: ?1
> Upgrade-Insecure-Requests: 1
> User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/101.0.4951.41 Safari/537.36 Edg/101.0.1210.32
> sec-ch-ua: " Not A;Brand";v=“99”, “Chromium”;v=“101”, “Microsoft Edge”;v=“101”
> sec-ch-ua-mobile: ?0
> sec-ch-ua-platform: “Windows”

上面只包括请求行、请求头和空行，请求数据为空。请求方法是GET，协议版本是HTTP/1.1；请求头是键值对的形式。

![在这里插入图片描述](https://img-blog.csdnimg.cn/da23459cb29243068e2118ae8b79534d.png)

解析过程由`parse()`函数完成；函数根据状态分别调用了

```c++
ParseRequestLine_();//解析请求行
ParseHeader_();//解析请求头
ParseBody_();//解析请求体
```

三个函数对请求行、请求头和数据体进行解析。当然解析请求体的函数还会调用`ParsePost_()`，因为Post请求会携带请求体。

### 响应报文


```HTML
HTTP/1.1 200 OK
Date: Fri, 22 May 2009 06:07:21 GMT
Content-Type: text/html; charset=UTF-8
空行
<html>
      <head></head>
      <body>
            <!--body goes here-->
      </body>
</html>
```
+ 状态行，由HTTP协议版本号， 状态码， 状态消息 三部分组成。
第一行为状态行，（HTTP/1.1）表明HTTP版本为1.1版本，状态码为200，状态消息为OK。

+ 消息报头，用来说明客户端要使用的一些附加信息。
第二行和第三行为消息报头，Date:生成响应的日期和时间；Content-Type:指定了MIME类型的HTML(text/html),编码类型是UTF-8。

+ 空行，消息报头后面的空行是必须的。

+ 响应正文，服务器返回给客户端的文本信息。空行后面的html部分为响应正文。
___

解析请求报文和生成响应报文都是在`HttpConn::process()`函数内完成的。并且是在解析请求报文后随即生成了响应报文。之后这个生成的响应报文便放在缓冲区等待`writev()`函数将其发送给fd。
```c++
//只为了说明逻辑，代码有删减
bool HttpConn::process() {
    request_.Init();//初始化解析类
    if(readBuff_.ReadableBytes() <= 0) {//从缓冲区中读数据
        return false;
    }
    else if(request_.parse(readBuff_)) {//解析数据,根据解析结果进行响应类的初始化
        response_.Init(srcDir, request_.path(), request_.IsKeepAlive(), 200);
    } else {
        response_.Init(srcDir, request_.path(), false, 400);
    }
    response_.MakeResponse(writeBuff_);//生成响应报文放入writeBuff_中
    /* 响应头  iov记录了需要把数据从缓冲区发送出去的相关信息
    iov_base为缓冲区首地址，iov_len为缓冲区长度 */
    iov_[0].iov_base = const_cast<char*>(writeBuff_.Peek());
    iov_[0].iov_len = writeBuff_.ReadableBytes();
    iovCnt_ = 1;

    /* 文件 */
    if(response_.FileLen() > 0  && response_.File()) { //
        iov_[1].iov_base = response_.File();
        iov_[1].iov_len = response_.FileLen();
        iovCnt_ = 2;
    }
    return true;
}
```

