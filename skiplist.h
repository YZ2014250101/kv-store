#pragma once
#include <string>
using namespace std;
typedef struct Node {
    string key;
    string value;
    Node** next;
} Node;
class Skiplist {
public:
    Skiplist();
    ~Skiplist();

    // 跳表初始化
    // 输入参数：
    //    list：kv_h要传地址
    //    max_level：跳表的最大层级
    // 返回值：0成功，-1失败
    int listInit(int max_level);
    // 销毁跳表：
    // 返回值：0成功，-1失败
    int listDesy();
    // 插入元素：有冲突则是头插法
    // 返回值：0成功，-1失败，-2已经有相应的key
    int insert(string key, string value);
    // 查找元素
    // 返回值：非空成功返回节点，NULL没找到
    Node* find(string key);
    // 删除元素
    // 返回值：0成功，-1失败，-2没有
    int erase(string key);
    // 打印整个跳表
    // 返回值：0成功，-1失败
    int print();

private:
    // 创建节点并初始化
    // 返回值：非空成功，NULL失败
    Node* nodeCreate(string key, string value, int level);
    // 销毁节点
    // 返回值：0成功，-1失败
    int nodeDesy(Node* node);
    Node* head;
    int curLevel;
    int maxLevel;
    int nodeCnt;
};