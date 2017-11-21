import sys
import pytest
import vmprof
from cffi import FFI
from array import array

sample = None
@pytest.mark.skipif("sys.platform == 'win32'")
class TestStack(object):
    def setup_class(cls):
        stack_ffi = FFI()
        stack_ffi.cdef("""
        typedef intptr_t ptr_t;
        int vmp_binary_search_ranges(ptr_t ip, ptr_t * l, int count);
        int vmp_ignore_ip(ptr_t ip);
        int vmp_ignore_symbol_count(void);
        ptr_t * vmp_ignore_symbols(void);
        void vmp_set_ignore_symbols(ptr_t * symbols, int count);
        int vmp_read_vmaps(const char * fname);
        void vmp_native_disable();
        """)
        with open("src/vmp_stack.c", "rb") as fd:
            source = fd.read().decode()
            libs = [] #['unwind', 'unwind-x86_64']
            if sys.platform.startswith('linux'):
                libs = ['unwind', 'unwind-x86_64']
            # trick: compile with _CFFI_USE_EMBEDDING=1 which will not define Py_LIMITED_API
            stack_ffi.set_source("vmprof.test._test_stack", source, include_dirs=['src'],
                                 define_macros=[('_CFFI_USE_EMBEDDING',1), ('PY_TEST',1),
                                                ('VMP_SUPPORTS_NATIVE_PROFILING',1)],
                                 libraries=libs, extra_compile_args=['-Werror', '-g'])

        stack_ffi.compile(verbose=True)
        from vmprof.test import _test_stack as clib
        cls.lib = clib.lib
        cls.ffi = clib.ffi

    def test_binary_search_ranges(self):
        a = array('l', [1,2,10,20,30,40])
        count = len(a)
        abuf = self.ffi.from_buffer(a)
        a_addr = self.ffi.cast("ptr_t*", abuf)
        assert self.lib.vmp_binary_search_ranges(0, a_addr, count) == -1
        assert self.lib.vmp_binary_search_ranges(1, a_addr, count) == 0
        assert self.lib.vmp_binary_search_ranges(2, a_addr, count) == 0
        for i in range(3,10):
            assert self.lib.vmp_binary_search_ranges(i, a_addr, count) == 0
        for i in range(10,21):
            assert self.lib.vmp_binary_search_ranges(i, a_addr, count) == 2
        for i in range(21,30):
            assert self.lib.vmp_binary_search_ranges(i, a_addr, count) == 2 
        for i in range(30,41):
            assert self.lib.vmp_binary_search_ranges(i, a_addr, count) == 4
        assert self.lib.vmp_binary_search_ranges(41, a_addr, count) == -1

    def test_ignore_ip(self):
        a = array('l', [1,2,100,150])
        abuf = self.ffi.from_buffer(a)
        a_addr = self.ffi.cast("ptr_t*", abuf)
        self.lib.vmp_set_ignore_symbols(a_addr, len(a))
        assert self.lib.vmp_ignore_ip(0) == 0
        assert self.lib.vmp_ignore_ip(1) == 1
        assert self.lib.vmp_ignore_ip(2) == 1
        assert self.lib.vmp_ignore_ip(3) == 0
        for i in range(3,100):
            assert self.lib.vmp_ignore_ip(i) == 0
        for i in range(100,151):
            assert self.lib.vmp_ignore_ip(i) == 1
        assert self.lib.vmp_ignore_ip(151) == 0
        self.lib.vmp_set_ignore_symbols(self.ffi.NULL, 0)

    @pytest.mark.skipif("not sys.platform.startswith('linux')")
    def test_read_vmaps(self, tmpdir):
        lib = self.lib
        f1 = tmpdir.join("vmap1")
        f1.write("""\
00000-11111 x y z w python
11111-22222 x y z w python site-packages
""")
        filename = str(f1).encode('utf-8')
        assert lib.vmp_read_vmaps(filename) == 1

        assert lib.vmp_ignore_symbol_count() == 2
        symbols = lib.vmp_ignore_symbols()
        assert symbols[0] == 0 and symbols[1] == 0x22222
        assert lib.vmp_ignore_ip(0x1) == 1
        assert lib.vmp_ignore_ip(0x11111) == 1
        assert lib.vmp_ignore_ip(0x11112) == 1
        assert lib.vmp_ignore_ip(0x22222) == 1
        assert lib.vmp_ignore_ip(0x22223) == 0
        lib.vmp_native_disable()

    @pytest.mark.skipif("not sys.platform.startswith('linux')")
    def test_self_vmaps(self):
        lib = self.lib
        assert lib.vmp_read_vmaps(b"/proc/self/maps") == 1

        count = lib.vmp_ignore_symbol_count()
        assert count >= 2
        symbols = lib.vmp_ignore_symbols()
        min = -1
        for i in range(0, count, 2):
            start, end = symbols[i], symbols[i+1]
            assert min < start
            assert start <= end
            min = end
        lib.vmp_native_disable()


    @pytest.mark.skipif("not sys.platform.startswith('darwin')")
    def test_read_vmaps_darwin(self):
        assert self.lib.vmp_read_vmaps(self.ffi.NULL) == 1

    @pytest.mark.skipif("not sys.platform.startswith('linux')")
    def test_overflow_vmaps(self, tmpdir):
        lib = self.lib
        f1 = tmpdir.join("vmap1")
        lines = []
        for l in range(0,10000, 2):
            # non overlapping ranges that must be considered! 
            lines.append('%x-%x x y z w python' % (l,l+1))
        f1.write('\n'.join(lines))
        filename = str(f1).encode('utf-8')
        assert lib.vmp_read_vmaps(filename) == 1
        for l in range(0, 10000):
            assert self.lib.vmp_ignore_ip(l) == 1
        assert self.lib.vmp_ignore_ip(10001) == 0
