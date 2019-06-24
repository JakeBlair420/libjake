#include <stdio.h> // stderr
#define LIBJAKE_KERNEL_BASE 0xfffffff007004000
#define LOG(fmt, args ...) fprintf(stderr, "[libjake] " fmt "\n", ##args)
