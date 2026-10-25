# KV-Store：基于跳表的轻量KV数据库
> C++ 自研仿Redis KV存储服务，基于Linux epoll + Reactor异步网络，底层使用跳表实现有序KV，简历后端项目

## ✨ 项目简介
本项目实现一个轻量的KV存储服务，参考Redis ZSet底层跳表结构，基于`epoll`实现Reactor反应堆异步网络模型；支持自定义KV命令，key/value使用`std::string`，支持增删查、范围查询，自带测试压测逻辑。
学习方向：Reactor异步IO、epoll、跳表原理、C++面向对象、内存管理、TCP半包处理。
技术栈：C++17 / Linux epoll / CMake / WSL2 / GDB

## 📂 目录结构（和你现在文件完全对齐）
```
kv-store/
├── build/                # cmake构建输出目录（git忽略）
├── test/                 # 压测、测试代码
├── .gitignore
├── CMakeLists.txt        # cmake构建脚本
├── main.cpp              # Reactor服务入口，epoll事件循环、连接回调
├── kvstore.h / kvstore.cpp  # KV协议解析层，命令分发
├── skiplist.h / skiplist.cpp # 跳表存储引擎实现
└── client.cpp            # 客户端（可选，用于测试发送命令）
```

## 📌 核心功能
1. **跳表存储引擎**
    - key、value使用`std::string`，支持任意字符串
    - 随机晋升概率0.25，参考Redis跳表设计
    - 接口：`insert`（插入/覆盖）、`search`（查找）、`remove`（删除）、范围查询
    - 析构自动释放节点内存，防止内存泄漏
2. **Reactor异步网络（main.cpp）**
    - epoll LT水平触发
    - 每个连接独立读写缓冲区，处理TCP字节流半包
    - 回调驱动：accept回调、recv接收回调、send发送回调
3. **KV协议指令**
    - `SET key value`：写入/覆盖键值
    - `GET key`：查询
    - `DELETE key`：删除
    - `ZRANGE start end`：跳表有序范围遍历
4. **压测模块**
    批量压测SET/GET/DELETE，统计耗时、QPS，用于观察跳表性能

## 🚀 编译运行（CMake）
```bash
# 1. 创建构建目录
mkdir build && cd build
# 2. cmake生成makefile
cmake ..
# 3. 编译
make -j4
# 4. 启动服务
./main 8888
```
服务启动后监听端口，可以用nc/telnet连接发送指令：
```bash
nc 127.0.0.1 8888
SET hello world
GET hello
DELETE hello
```

## 🧪 压测输出示例
```
bench start, total rounds: 10000
bench finish
total requests: 30000
time used(ms): 6988
QPS: 4293.07
```

## 💡 核心设计 & 面试问答点
1. **为什么选用跳表？**
    跳表是有序结构，范围查询实现简单；相比红黑树，没有复杂的旋转逻辑，代码更容易实现、可读性好，Redis的zset底层就是跳表；期望O(logn)查找插入删除。
2. **Reactor模型**
    单线程Reactor，epoll监听fd事件；收到可读事件后，从socket内核缓冲区读到用户态rbuffer，交给kvstore协议解析层，再调用跳表接口。
3. **随机层数设计**
    晋升概率0.25；使用线程安全随机函数，避免`rand()`静态变量竞争问题，保证跳表的概率分布稳定。
4. **TCP缓冲区**
    每个连接独立rbuffer/wbuffer，解决TCP流式半包问题，不是按包接收，是字节流。

## 📈 后续优化方向（写简历加分）
- 改造epoll ET边沿触发，减少epoll频繁唤醒
- 增加AOF持久化，落盘恢复数据
- 兼容Redis RESP协议，可以直接用redis-cli连接
- 内存池管理跳表节点，减少频繁malloc、free开销
- 多Reactor（主从Reactor），支持多核利用

## 📜 License
仅供学习交流使用

---
如果你想要更短的 GitHub 首页精简版，我可以再压缩一份。
要不要我顺便帮你写配套的 `.gitignore`？