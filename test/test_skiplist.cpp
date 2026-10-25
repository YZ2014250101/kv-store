#include "../skiplist.h"
#include <iostream>
int main() {
    Skiplist sl;
    sl.listInit(16);

    sl.insert("z1", "val1");
    sl.insert("z2", "val2");
    sl.insert("z3", "val3");
    sl.insert("z4", "val4");
    sl.print();

    std::cout << "--- erase z2 ---\n";
    sl.erase("z2");
    sl.print();

    auto p = sl.find("z3");
    if (p) {
        std::cout << "find z3 = " << p->value << "\n";
        p->value = "new_val";
        std::cout << "after modify z3 = " << p->value << "\n";
    }
    return 0;
}
