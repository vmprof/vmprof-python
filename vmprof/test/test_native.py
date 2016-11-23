
import re
import vmprof
from cffi import FFI

ffi = FFI()
ffi.cdef("""
extern "Python" static void g(void);
void native_callback_g(void);
""")
ffi.set_source("vmprof.test._test_native", """
static void g(void);
void native_callback_g(void) { g(); }
""")

sample = None

class TestNative(object):
    def setup_class(cls):
        ffi.compile()
        from vmprof.test import _test_native

        cls.lib = _test_native.lib
        cls.ffi = _test_native.ffi

    def barrier_test_call(self, func):
        # indirection to have the barrier_test_call in the sample
        func()

    def test_simple_python_sample(self):
        native_call = self.lib.native_callback_g
        # the order is barrier -> f -> native_call -> g
        @self.ffi.def_extern()
        def g():
            global sample
            sample = vmprof.sample_stack_now()
        def f():
            native_call()
        self.barrier_test_call(f)
        global sample
        assert re.match(r'.*sample_stack_now .*g .*f', ' '.join(sample))

    def test_simple_C_sample(self):
        native_call = self.lib.native_callback_g
        # the order is barrier -> f -> native_call -> g
        @self.ffi.def_extern()
        def g():
            global sample
            sample = vmprof.sample_stack_now()
        def f():
            native_call()
        self.barrier_test_call(f)
        global sample
        # must also record the native frames
        assert re.match(r'.*sample_stack_now .*g .*native_callback_g .*f', ' '.join(sample))
