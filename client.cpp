// bench_client.cpp
#include <arpa/inet.h>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 9998
#define BUF_SIZE 1024

// 发送一条命令，读取响应
bool send_cmd(int sockfd, const char* cmd, char* resp, size_t resp_len) {
    char send_buf[BUF_SIZE];
    snprintf(send_buf, BUF_SIZE, "%s\n", cmd);
    ssize_t n = send(sockfd, send_buf, strlen(send_buf), 0);
    if (n <= 0) {
        perror("send");
        return false;
    }

    memset(resp, 0, resp_len);
    n = recv(sockfd, resp, resp_len - 1, 0);
    if (n <= 0) {
        perror("recv");
        return false;
    }
    return true;
}

int main(int argc, char** argv) {
    // 默认压测10000次，可以传参修改：./bench_client 50000
    int total = 10000;
    if (argc >= 2) {
        total = atoi(argv[1]);
    }
    std::cout << "bench start, total rounds: " << total << "\n";

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return -1;
    }

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(SERVER_PORT);
    if (inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr) <= 0) {
        perror("inet_pton");
        close(sockfd);
        return -1;
    }

    if (connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("connect, please start server ./main 9998 first");
        close(sockfd);
        return -1;
    }

    auto start = std::chrono::steady_clock::now();
    char resp[BUF_SIZE];

    for (int i = 0; i < total; i++) {
        char set_cmd[BUF_SIZE];
        char get_cmd[BUF_SIZE];
        char del_cmd[BUF_SIZE];
        // key带上序号，避免key冲突
        snprintf(set_cmd, BUF_SIZE, "set testkey_%d value_%d", i, i);
        snprintf(get_cmd, BUF_SIZE, "get testkey_%d", i);
        snprintf(del_cmd, BUF_SIZE, "del testkey_%d", i);

        if (!send_cmd(sockfd, set_cmd, resp, BUF_SIZE)) {
            std::cout << "set failed i=" << i << "\n";
            break;
        }
        if (!send_cmd(sockfd, get_cmd, resp, BUF_SIZE)) {
            std::cout << "get failed i=" << i << "\n";
            break;
        }
        if (!send_cmd(sockfd, del_cmd, resp, BUF_SIZE)) {
            std::cout << "del failed i=" << i << "\n";
            break;
        }
    }

    auto end = std::chrono::steady_clock::now();
    auto cost_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    // 一轮 = set + get + del 3次请求
    long long req_cnt = total * 3;
    double qps = req_cnt * 1000.0 / cost_ms;

    std::cout << "bench finish\n";
    std::cout << "total requests: " << req_cnt << "\n";
    std::cout << "time used(ms): " << cost_ms << "\n";
    std::cout << "QPS: " << qps << "\n";

    close(sockfd);
    return 0;
}
