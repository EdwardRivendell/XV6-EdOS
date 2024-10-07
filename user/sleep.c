#include "kernel/types.h"
#include "user.h"

int main(int argc, char* argv[]) {//argc是命令行参数的数量，sleep即函数名是第0个参数，后面多少秒是第1个参数
    if (argc != 2) {
        printf("Sleep needs one argument!\n");  // 检查参数数量是否正确
        exit(-1);
    }

    int ticks = atoi(argv[1]);  // 将字符串参数转为整数
    sleep(ticks);               // 使用系统调用 sleep
    printf("(nothing happens for a little while)\n");
    exit(0);                     // 确保进程退出
}