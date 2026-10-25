#include "../kvstore.h"
#include <iostream>

int main() {
    KVStore kv;
    kv.init(10); // max_level=10

    auto run_test = [&](const char* buf, size_t n) {
        std::string resp = kv.protocal((char*)buf, n);
        std::cout << "req: [" << buf << "]  resp: [" << resp << "]" << std::endl;
    };

    std::cout << "==== test SET ====\n";
    run_test("set name yjc", 12);

    std::cout << "==== test GET ====\n";
    run_test("get name", 8);

    std::cout << "==== test GET not exist ====\n";
    run_test("get age", 7);

    std::cout << "==== test DEL ====\n";
    run_test("del name", 8);

    std::cout << "==== test DEL again ====\n";
    run_test("del name", 8);

    std::cout << "==== test wrong cmd ====\n";
    run_test("abc 123", 7);

    std::cout << "==== test param error ====\n";
    run_test("set onlyone", 10);

    return 0;
}
