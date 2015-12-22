
// Extension module with slow code
// Test profiling of native code
//
// Compile with:
// g++ -fPIC -g -O0 --shared -o slow_ext.so slow_ext.cpp

#include <stdio.h>

extern "C" {
int slow1(int n);
int slow2(int n);
}

int slow1(int n)
{
    int sum = 0;
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < 100000; j++) {
            sum += i*j;
        }
    }

    return sum;
}

int slow2(int n)
{
    return slow1(n);
}



