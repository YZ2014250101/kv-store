#include "kvstore.h"
#include "skiplist.h"
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <functional>
#include <iostream>
#include <memory_resource>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>
static constexpr int max_buffer_len = 1024;
static constexpr int epoll_events_size = 1024;
static constexpr int connblock_size = 1024;
static constexpr int listen_port_count = 1;

// 单个连接
/**
 * @brief Connection类，用于管理网络连接的读写缓冲区和回调函数
 */
class Connection {
public:
    int fd_ { -1 }; // 文件描述符，-1表示无效的文件描述符
    char rbuffer_[max_buffer_len] { 0 }; // 读缓冲区，用于存储接收到的数据
    size_t rcount_ { 0 }; // 读缓冲区中已读取的数据长度

    char wbuffer_[max_buffer_len] { 0 }; // 写缓冲区，用于存储待发送的数据
    size_t wcount_ { 0 }; // 写缓冲区中待发送的数据长度

    size_t next_len_ = max_buffer_len; // 下次期望读取的数据长度

    using Callback = std::function<void(int fd, int events, void* arg)>;
    Callback cb_;
};

// 连接块
struct ConnBlock {
    std::vector<Connection> block;
    ConnBlock* next { nullptr };
    ConnBlock()
        : block(connblock_size) { }
};

// 反应堆
/**
 * @brief Reactor类，实现基于epoll的事件驱动模型
 */
class Reactor {
public:
    /**
     * @brief 构造函数，初始化epoll实例和连接块
     */
    Reactor() {
        // 创建epoll实例，参数1表示初始大小
        epfd_ = epoll_create(1);
        if (epfd_ < 0) {
            std::cerr << "epoll_create err\n";
            return;
        }
        // 创建第一个连接块
        block_header_ = new ConnBlock();
        blk_cnt_ = 1;
    }
    /**
     * @brief 析构函数，关闭epoll实例并释放所有连接块
     */
    ~Reactor() {
        // 关闭epoll实例
        if (epfd_ >= 0)
            close(epfd_);
        // 释放所有连接块
        ConnBlock* cur = block_header_;
        while (cur) {
            ConnBlock* tmp = cur;
            cur = cur->next;
            delete tmp;
        }
    }
    // 禁用拷贝构造和赋值操作
    Reactor(const Reactor&) = delete;
    Reactor& operator=(const Reactor&) = delete;

    /**
     * @brief 获取文件描述符对应的连接对象
     * @param fd 文件描述符
     * @return Connection* 连接对象指针
     */
    Connection* get_conn(int fd) {
        // 计算所在的连接块索引
        int blkidx = fd / connblock_size;
        // 如果需要的块不存在，则创建新块
        while (blkidx >= blk_cnt_) {
            create_connblock();
        }
        // 定位到对应的连接块
        ConnBlock* blk = block_header_;
        for (int i = 1; i < blkidx; ++i) {
            blk = blk->next;
        }
        // 返回连接对象
        return &blk->block[fd % connblock_size];
    }

    /**
     * @brief 创建新的连接块
     * @return int 成功返回0，失败返回错误码
     */
    int create_connblock() {
        // 创建新连接块
        ConnBlock* newblk = new ConnBlock();
        // 找到最后一个连接块
        ConnBlock* end = block_header_;
        while (end->next)
            end = end->next;
        // 将新块添加到链表末尾
        end->next = newblk;
        blk_cnt_++;
        return 0;
    }

    int set_listener(int listenfd, Connection::Callback cb) {
        Connection* conn = get_conn(listenfd);
        conn->fd_ = listenfd;
        conn->cb_ = cb;

        epoll_event ev { };
        ev.data.fd = listenfd;
        ev.events = EPOLLIN;
        epoll_ctl(epfd_, EPOLL_CTL_ADD, listenfd, &ev);
        return 0;
    }

    /**
     * 事件循环函数，使用epoll实现I/O多路复用
     * 持续监听文件描述符上的事件，并根据事件类型调用相应的回调函数
     */
    void loop() {
        // 创建事件数组，用于存储epoll_wait返回的事件
        epoll_event events[epoll_events_size] { };
        // 无限循环，持续监听事件
        while (true) {
            // 调用epoll_wait等待事件发生，-1表示无限等待
            int nready = epoll_wait(epfd_, events, epoll_events_size, -1);
            // 如果没有事件就绪，则继续下一次循环
            if (nready <= 0)
                continue;
            // 遍历所有就绪的事件
            for (int i = 0; i < nready; i++) {
                // 获取当前事件的文件描述符
                int fd = events[i].data.fd;
                // 根据文件描述符获取连接对象
                Connection* conn = get_conn(fd);
                // 如果连接没有设置回调函数，则跳过
                if (!conn->cb_)
                    continue;
                // 检查是否是可读事件
                if (events[i].events & EPOLLIN) {
                    // 调用回调函数处理可读事件
                    conn->cb_(fd, EPOLLIN, this);
                }
                // 检查是否是可写事件
                if (events[i].events & EPOLLOUT) {
                    // 调用回调函数处理可写事件
                    conn->cb_(fd, EPOLLOUT, this);
                }
            }
        }
    }

    int get_epfd_() { return epfd_; }

private:
    int epfd_ = -1;
    ConnBlock* block_header_ = nullptr;
    int blk_cnt_ = 0;
};

// TCP服务端
/**
 * TcpServer类 - 实现一个基于Reactor模式的TCP服务器
 * 支持监听指定端口，处理客户端连接，实现简单的echo功能
 */
class TcpServer {
public:
    /**
     * 构造函数
     * @param port 服务器监听端口
     */
    TcpServer(int port)
        : port_(port) { }
    /**
     * 启动服务器
     * 初始化监听socket，设置回调，进入事件循环
     */
    void start() {
        int listenfd = init_listen(); // 初始化监听socke
        kvstore_.init(16); // 初始化KV存储
        // 设置连接接受回调函数
        reactor_.set_listener(listenfd, [this](int fd, int event, void* arg) { this->accept_cb(fd, event, arg); });
        std::cout << "init finish, listening...\n";
        reactor_.loop(); // 进入事件循环
    }

private:
    int port_; // 服务器监听端口
    Reactor reactor_; // Reactor事件循环对象
    KVStore kvstore_; // KV存储对象
    /**
     * 初始化监听socket
     * @return 返回监听socket的文件描述符，失败返回-1
     */
    int init_listen() {
        int sockfd = socket(AF_INET, SOCK_STREAM, 0);
        sockaddr_in servaddr { }; // 服务器地址结构
        servaddr.sin_family = AF_INET;
        servaddr.sin_addr.s_addr = htonl(INADDR_ANY); // 监听所有网络接口
        servaddr.sin_port = htons(port_); // 设置端口
        int n = bind(sockfd, (sockaddr*)&servaddr, sizeof(servaddr)); // 绑定地址
        if (n < 0)
            return -1;
        listen(sockfd, 10); // 开始监听，最大连接队列10
        std::cout << "listen port:" << port_ << " sockfd:" << sockfd << "\n";
        return sockfd;
    }

    void accept_cb(int fd, int event, void* arg) {
        sockaddr_in cli { };
        socklen_t len = sizeof(cli);
        int clientfd = accept(fd, (sockaddr*)&cli, &len);
        if (clientfd < 0)
            return;
        Reactor* r = (Reactor*)arg;
        Connection* conn = r->get_conn(clientfd);
        conn->fd_ = clientfd;
        conn->rcount_ = 0;
        conn->wcount_ = 0;
        conn->next_len_ = max_buffer_len;
        conn->cb_ = [this](int fd, int event, void* arg) { this->recv_cb(fd, event, arg); };
        epoll_event ev { };
        ev.data.fd = clientfd;
        ev.events = EPOLLIN;
        epoll_ctl(r->get_epfd_(), EPOLL_CTL_ADD, clientfd, &ev);
        std::cout << "client connect:" << clientfd << "\n";
    }

    void recv_cb(int fd, int event, void* arg) {
        Reactor* r = (Reactor*)arg;
        Connection* conn = r->get_conn(fd);
        if (conn == nullptr) {
            return;
        }

        int n = recv(fd, conn->rbuffer_ + conn->rcount_, conn->next_len_, 0);
        if (n <= 0) {
            epoll_ctl(r->get_epfd_(), EPOLL_CTL_DEL, fd, nullptr);
            close(fd);
            return;
        }
        conn->rcount_ += n;

        std::string res = kvstore_.protocal(conn->rbuffer_, conn->rcount_);

        // 防止wbuffer越界！
        if (res.size() >= max_buffer_len) {
            res = "resp too long\r\n";
        }
        const char* p = res.c_str();
        memcpy(conn->wbuffer_, p, res.size());
        conn->wcount_ = res.size();

        conn->rcount_ = 0;
        memset(conn->rbuffer_, 0, max_buffer_len);

        conn->cb_ = [this](int fd, int event, void* arg) { this->send_cb(fd, event, arg); };
        epoll_event ev { };
        ev.data.fd = fd;
        ev.events = EPOLLOUT;
        int ret = epoll_ctl(r->get_epfd_(), EPOLL_CTL_MOD, fd, &ev);
    }

    void send_cb(int fd, int event, void* arg) {
        Reactor* r = (Reactor*)arg;
        Connection* conn = r->get_conn(fd);
        int n = send(fd, conn->wbuffer_, conn->wcount_, 0);
        if (n <= 0) {
            close(fd);
            return;
        }
        conn->wcount_ -= n;
        memset(conn->wbuffer_, 0, max_buffer_len);
        // 切回读
        conn->cb_ = [this](int fd, int event, void* arg) { this->recv_cb(fd, event, arg); };
        epoll_event ev { };
        ev.data.fd = fd;
        ev.events = EPOLLIN;
        epoll_ctl(r->get_epfd_(), EPOLL_CTL_MOD, fd, &ev);
    }
};

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cout << "./server port\n";
        return -1;
    }
    int port = atoi(argv[1]);
    TcpServer server(port);
    server.start();
    return 0;
}
