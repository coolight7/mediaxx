#include "mediaxx.h"
#include <iostream>

using namespace std;

void test() {
    cout << sum(1, 2) << endl;
    get_libav_version();
}

int main(int argn, char** argv) {
    std::cout << "======= Test Start =======" << std::endl;
    test();
    std::cout << "======= Test Done =======" << std::endl;
    std::cout << ">>>";
    int num = 0;
    cin >> num;
    return 0;
}