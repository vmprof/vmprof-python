
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
        """, extra_compile_args=['-g', '-O3'])
        ffi.compile()
        from vmprof.test import _test_native

        cls.lib = _test_native.lib
        cls.ffi = _test_native.ffi

    def test_simple_sample(self):
        global sample
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

        names = [name for (addr, name) in sample]
        assert re.match(r'.*sample_stack_now .*g .*f', ' '.join(names))
        assert re.match(r'.*sample_stack_now .*g .*native_callback_g .*f', ' '.join(names))

        stats = p.get_stats()
        not_found = []
        for addr, name in sample:
            if (addr & ~1) not in stats.adr_dict:
                not_found.append((addr, name))

        assert len(not_found) == 0, "there are some symbols that cannot be connected"
