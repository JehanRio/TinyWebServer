#include "buffer.h"

Buffer::Buffer(int initBuffSize) : buffer_(initBuffSize), readPos_(0), writePos_(0) {}

// 可读的数量：写下标 - 读下标
size_t Buffer::ReadableBytes() const {
    return writePos_ - readPos_;
}

// 可写的数量：buffer大小 - 写下标
size_t Buffer::WritableBytes() const {
    return buffer_.size() - writePos_;
}

// 可预留空间：已经读过的就没用了，等于读下标
size_t Buffer::PrependableBytes() const {
    return readPos_;
}

// 当前读下标的指针
const char* Buffer::Peek() const {
    return BeginPtr_() + readPos_;
}

// 读取len长度，移动读下标
void Buffer::Retrieve(size_t len) {
    assert(len <= ReadableBytes());
    readPos_ += len;
}

// 读取到end位置
void Buffer::RetrieveUntil(const char* end) {
    assert(Peek() <= end );
    Retrieve(end - Peek());
}

// 取初所有数据，buffer归零，读写下标归零
void Buffer::RetrieveAll() {
    bzero(&buffer_[0], buffer_.size()); // 用bzero操作vector
    readPos_ = 0;
    writePos_ = 0;
}

// 取出剩余可读的str
std::string Buffer::RetrieveAllToStr() {
    std::string str(Peek(), ReadableBytes());
    RetrieveAll();
    return str;
}

// 写位置的指针，同Peek()
const char* Buffer::BeginWriteConst() const {
    return BeginPtr_() + writePos_;
}

// 写位置的指针
char* Buffer::BeginWrite() {
    return BeginPtr_() + writePos_;
}

// 移动写下标，在Append中使用
void Buffer::HasWritten(size_t len) {
    writePos_ += len;
} 

// 添加str到缓冲区
void Buffer::Append(const std::string& str) {
    Append(str.data(), str.length());
}

void Buffer::Append(const void* data, size_t len) {
    assert(data);
    Append(static_cast<const char*>(data), len);
}

void Buffer::Append(const char* str, size_t len) {
    assert(str);
    EnsureWriteable(len);   // 确保可写的长度
    std::copy(str, str + len, BeginWrite());    // 将str放到写下标开始的地方
    HasWritten(len);    // 移动写下标
}

void Buffer::Append(const Buffer& buff) {   // 将buffer中的读下标的地方放到该buffer中的写下标位置
    Append(buff.Peek(), buff.ReadableBytes());
}

// 确保可写的长度
void Buffer::EnsureWriteable(size_t len) {
    if(WritableBytes() < len) { // 如果长度不够了，要扩展
        MakeSpace_(len);
    }
    assert(WritableBytes() >= len);
}

// 将fd的内容读到缓冲区，即writable的位置
ssize_t Buffer::ReadFd(int fd, int* saveErrno) {
    char buff[65535];   // 栈区
    struct iovec iov[2];
    const size_t writable = WritableBytes();
    /* 分散读， 保证数据全部读完 */
    iov[0].iov_base = BeginPtr_() + writePos_;
    iov[0].iov_len = writable;
    iov[1].iov_base = buff;
    iov[1].iov_len = sizeof(buff);

    const ssize_t len = readv(fd, iov, 2);
    if(len < 0) {
        *saveErrno = errno;
    }
    else if(static_cast<size_t>(len) <= writable) { // 若len小于writable，说明写区可以容纳len
        writePos_ += len;   // 直接移动写下标
    }
    else {
        writePos_ = buffer_.size(); // 写区写满了,下标移到最后
        Append(buff, len - writable);   // 剩余的长度
    }
    return len;
}

// 将buffer中可读的区域写入fd中
ssize_t Buffer::WriteFd(int fd, int* saveErrno) {
    size_t readSize = ReadableBytes();
    ssize_t len = write(fd, Peek(), readSize);
    if(len < 0) {
        *saveErrno = errno;
        return len;
    } 
    readPos_ += len;
    return len;
}

// buffer开头
char* Buffer::BeginPtr_() {
    return &*buffer_.begin();
}

const char* Buffer::BeginPtr_() const {
    return &*buffer_.begin();
}

// 扩展空间
void Buffer::MakeSpace_(size_t len) {
    if(WritableBytes() + PrependableBytes() < len) {    // 若prependable空间也不够，只能扩展buffer
        buffer_.resize(writePos_ + len + 1);
    } 
    else {
        size_t readable = ReadableBytes();
        std::copy(BeginPtr_() + readPos_, BeginPtr_() + writePos_, BeginPtr_());    // 将readable搬到最前面
        readPos_ = 0;
        writePos_ = readPos_ + readable;
        assert(readable == ReadableBytes());
    }
}