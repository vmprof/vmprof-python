
# Test profiling of mixed Python / native code stacks

from ctypes import *

h = cdll.LoadLibrary("./slow_ext.so")

def more_slow():
    h.slow2(10000)

def slow_wrap():
    h.slow2(20000)
    more_slow()

if __name__ == '__main__':
    slow_wrap()

