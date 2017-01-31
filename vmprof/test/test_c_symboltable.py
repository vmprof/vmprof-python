import os
import sys
import pytest
import vmprof
from vmprof.reader import MARKER_NATIVE_SYMBOLS
from vmshare.binary import read_word, read_string
from cffi import FFI
from array import array

stack_ffi = FFI()
stack_ffi.cdef("""
void dump_all_known_symbols(int fd);
int test_extract(char ** name, int * lineno, char ** src);
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
    return vmp_resolve_addr(&dump_all_known_symbols, gname, 64, lineno, gsrc, 128);
}
"""
    libs = [] #['unwind', 'unwind-x86_64']
    # trick: compile with _CFFI_USE_EMBEDDING=1 which will not define Py_LIMITED_API
    stack_ffi.set_source("vmprof.test._test_symboltable", source, include_dirs=['src'],
                         define_macros=[('_CFFI_USE_EMBEDDING',1),('_PY_TEST',1)], libraries=libs,
                         extra_compile_args=['-g', '-O0'])

sample = None

class TestSymbolTable(object):
    def setup_class(cls):
        stack_ffi.compile(verbose=True)
        from vmprof.test import _test_symboltable as clib
        cls.lib = clib.lib
        cls.ffi = clib.ffi

    def test_dump_all_known_symbols(self, tmpdir):
        lib = self.lib
        f1 = tmpdir.join("symbols")
        with f1.open('wb') as handle:
            lib.dump_all_known_symbols(handle.fileno()) # load ALL symbols!
        addrs = []
        with f1.open('rb') as fd:
            fd.seek(0, os.SEEK_END)
            length = fd.tell()
            fd.seek(0, os.SEEK_SET)
            while True:
                assert fd.read(1) == MARKER_NATIVE_SYMBOLS
                addr = read_word(fd)
                string = read_string(fd).decode('utf-8')
                addrs.append((addr, string))
                if fd.tell() >= length:
                    break
        assert len(addrs) >= 100 # usually we have many many more!!
        symbols_to_be_found = ['dump_all_known_symbols']
        duplicates = []
        names = set()
        for addr, name in addrs:
            assert len(name) > 0
            for i,sym in enumerate(symbols_to_be_found):
                if sym in name:
                    del symbols_to_be_found[i]
                    break
            if (addr,name) in names:
                duplicates.append((addr,name))
            else:
                names.add((addr, name))
        assert len(symbols_to_be_found) == 0
        # property B) see header symboltable.h
        assert len(duplicates) == 0
        addrs = [addr for addr,name in names]
        # property A), see header symboltable.h
        lookup = {}
        for addr,name in names:
            if addr in lookup:
                assert lookup[addr] == name
            lookup[addr] = name

    def test_resolve_addr(self):
        lib = self.lib
        ffi = self.ffi
        name = ffi.new("char**")
        src = ffi.new("char**")
        lineno = ffi.new("int*")
        lib.test_extract(name, lineno, src)

        assert ffi.string(name[0]) == b"dump_all_known_symbols"
        assert ffi.string(src[0]).endswith(b"vmprof/test/_test_symboltable.c")
        # lines are not included in stab
        # assert 20 <= lineno[0] <= 100

