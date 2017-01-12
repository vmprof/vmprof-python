
import re
import vmprof
from vmprof.profiler import Profiler
import _vmprof
from cffi import FFI

sample = None

class TestNative(object):
    def setup_class(cls):
        ffi = FFI()
        ffi.cdef("""
        extern "Python" static void g(void);
        void native_callback_g(void);
        """)
        ffi.set_source("vmprof.test._test_native", """
        static void g(void);
        void native_callback_g(void) { g(); }
        """)
        ffi.compile(debug=True)
        from vmprof.test import _test_native

        cls.lib = _test_native.lib
        cls.ffi = _test_native.ffi

    def test_simple_sample(self):
        native_call = self.lib.native_callback_g
        # the order is f -> ... -> native_call_g -> ... -> g
        @self.ffi.def_extern()
        def g():
            global sample
            sample = vmprof.sample_stack_now()
        def f():
            native_call()
        p = Profiler()
        with p.measure():
            f()

        global sample
        assert re.match(r'.*sample_stack_now .*g .*f', ' '.join(sample))
        assert re.match(r'.*sample_stack_now .*g .*native_callback_g .*f', ' '.join(sample))
