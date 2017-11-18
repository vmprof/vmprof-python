import py
import os
import sys
import pytest
from cffi import FFI
from array import array

if sys.platform != 'win32':

    ffi = FFI()
    ffi.cdef("""
    //void dump_all_known_symbols(int fd);
    int test_extract(char ** name, int * lineno, char ** src);
    int test_extract_sofile(char ** name, int * lineno, char ** src);
    """)
    with open("src/symboltable.c", "rb") as fd:
        source = fd.read().decode()
    source += """

    static char gname[64];
    static char gsrc[128];
    int test_extract(char ** name, int * lineno, char ** src)
    {
        *name = gname;
        *src = gsrc;
        gname[0] = 0;
        gsrc[0] = 0;
        return vmp_resolve_addr(&vmp_resolve_addr, gname, 64,
                                lineno, gsrc, 128);
    }
    int test_extract_sofile(char ** name, int * lineno, char ** src)
    {
        *name = gname;
        *src = gsrc;
        gname[0] = 0;
        gsrc[0] = 0;
        return vmp_resolve_addr(&abs, gname, 64,
                                lineno, gsrc, 128);
    }
    """
    libs = [] #['unwind', 'unwind-x86_64']
    includes = ['src']
    if sys.platform.startswith('linux'):
        for src in ["src/libbacktrace/state.c",
                    "src/libbacktrace/backtrace.c",
                    "src/libbacktrace/fileline.c",
                    "src/libbacktrace/posix.c",
                    "src/libbacktrace/mmap.c",
                    "src/libbacktrace/mmapio.c",
                    "src/libbacktrace/elf.c",
                    "src/libbacktrace/dwarf.c",
                    "src/libbacktrace/sort.c",
                   ]:
            with open(src, "rb") as fd:
                source += fd.read().decode()
        includes.append('src/libbacktrace')

    extra_compile = []
    if sys.platform.startswith("linux"):
        extra_compile = ['-DVMPROF_LINUX', '-DVMPROF_UNIX', '-Werror', '-g']

    # trick: compile with _CFFI_USE_EMBEDDING=1 which will not define Py_LIMITED_API
    ffi.set_source("vmprof.test._test_symboltable", source, include_dirs=includes,
                         define_macros=[('_CFFI_USE_EMBEDDING',1),('_PY_TEST',1)], libraries=libs,
                         extra_compile_args=extra_compile)

    ffi.compile(verbose=True)

@py.test.mark.skipif("sys.platform == 'win32'")
class TestSymbolTable(object):
    def setup_class(cls):
        from vmprof.test import _test_symboltable as clib
        cls.lib = clib.lib
        cls.ffi = clib.ffi

    def test_resolve_addr(self):
        lib = self.lib
        ffi = self.ffi
        name = ffi.new("char**")
        src = ffi.new("char**")
        _lineno = ffi.new("int*")
        lib.test_extract(name, _lineno, src)

        assert ffi.string(name[0]) == b"vmp_resolve_addr"
        assert ffi.string(src[0]).endswith(b"vmprof/test/_test_symboltable.c")
        # lines are not included in stab
        if sys.platform.startswith('linux'):
            with open("vmprof/test/_test_symboltable.c", "rb") as fd:
                lineno = 1
                for line in fd.readlines():
                    if "int vmp_resolve_addr(void * addr," in line.decode():
                        if _lineno[0] == lineno:
                            break
                    lineno += 1
                else:
                    assert False, "could not match line number"

    def test_sofile_in_srcfile(self):
        lib = self.lib
        ffi = self.ffi
        name = ffi.new("char**")
        src = ffi.new("char**")
        _lineno = ffi.new("int*")
        # the idea of this test is to extract some details out of e.g. libc
        # usually linux distros do not ship dwarf information unless you install
        # them.
        lib.test_extract_sofile(name, _lineno, src)

        assert ffi.string(name[0]) == b"abs"
        # should be something like /lib64/libc.so.6 (e.g. on Fedora 25)
        if sys.platform.startswith("linux"):
            assert b"libc" in ffi.string(src[0])
            assert b".so" in ffi.string(src[0])
        elif sys.platform == "darwin":
            # osx
            assert b"libsystem_c.dylib" in ffi.string(src[0])
