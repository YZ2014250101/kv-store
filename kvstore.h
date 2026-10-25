#ifndef KVSTORE_H
#define KVSTORE_H

#include "skiplist.h"
#include <string.h>
#include <string>
#include <vector>
#define max_tokens 32
enum CmdType { SET, GET, DEL, UNKNOWN };

class KVStore {
public:
    KVStore();
    ~KVStore();

    // 初始化，传入跳表最大层数（比如16）
    int init(int max_level);

    // SET key value
    int set(const std::string& key, const std::string& value);

    // GET key，找到返回true，val带出结果；没找到返回false
    bool get(const std::string& key, std::string& out_val);

    // DEL key
    int del(const std::string& key);

    // EXISTS key
    bool exists(const std::string& key);

    // 返回总kv数量
    int size() const;

    // 打印全部（调试用）
    void print() const;

    // 解析命令
    int parseCmd(const std::vector<std::string> token, int len);

    int splitTokens(std::vector<std::string>& tokens, const std::string& msg);

    std::string protocal(char* buffer, size_t max_buffer_len);

private:
    Skiplist _sl;
};

#endif
