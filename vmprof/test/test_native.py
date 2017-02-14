import py
import time
import re
import sys
from cffi import FFI


sample = None

if sys.platform != 'win32':
    ffi = FFI()
    ffi.cdef("""
    extern "Python" static void g(void);
    void native_callback_g(void);
    """)
    ffi.set_source("vmprof.test._test_native", """
    static void g(void);
    __attribute__((noinline)) // clang, do not inline
    void native_callback_g(void) { g(); }
    """, extra_compile_args=['-g'])
    ffi.compile()

@py.test.mark.skipif("sys.platform == 'win32'")
class TestNative(object):
    def setup_class(cls):
        from vmprof.test import _test_native
        cls.lib = _test_native.lib
        cls.ffi = _test_native.ffi

    def test_simple_sample(self):
        import vmprof
        global sample
        native_call = self.lib.native_callback_g

        # the order is f -> ... -> native_call_g -> ... -> g
        @self.ffi.def_extern()
        def g():
            global sample
            sample = vmprof.sample_stack_now()
            x = []
            for i in range(100000):
                x.append('abc')
                if i % 1000 == 0:
                    x = []
            del x

        def f():
            native_call()
        p = vmprof.profiler.Profiler()
        with p.measure(native=True):
            f()

        stats = p.get_stats()
        names = []
        for addr in sample:
            if addr in stats.adr_dict:
                name = stats.get_name(addr)
            else:
                name, lineno, srcfile = vmprof.resolve_addr(addr)
            names.append(name)

        found = list(stats.find_addrs_containing_name('native_callback_g'))
        assert len(found) >= 1
        addr = found[0]

        lang, sym, line, file = stats.get_addr_info(addr)
        assert lang == 'n' and 'native_callback_g' in sym

        assert re.match(r'.*sample_stack_now .*g .*f', ' '.join(names))
        assert re.match(r'.*sample_stack_now .*g .*native_callback_g .*f', ' '.join(names))
        print(names)
