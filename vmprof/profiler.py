import sys
import tempfile

import vmprof
from vmprof.addrspace import AddressSpace
from vmprof.stats import Stats
from vmprof.reader import read_prof, LibraryData, read_jit_symbols_maybe


class VMProfError(Exception):
    pass

class ProfilerContext(object):
    done = False

    def __init__(self):
        self.tmpfile = tempfile.NamedTemporaryFile()

    def __enter__(self):
        vmprof.enable(self.tmpfile.fileno(), 0.001)

    def __exit__(self, type, value, traceback):
        vmprof.disable()
        self.done = True


def read_profile(prof_filename, extra_libs=None, virtual_only=True,
                 include_extra_info=True, lib_cache=None,
                 load_jit_symbols=False):
    if lib_cache is None:
        lib_cache = read_profile.lib_cache
    
    prof = open(str(prof_filename), 'rb')

    period, profiles, virtual_symbols, libs, interp_name = read_prof(prof)

    if not virtual_only or include_extra_info:
        exe_name = libs[0].name
        for lib in libs:
            executable = lib.name == exe_name
            if lib.name in lib_cache:
                lib.get_symbols_from(lib_cache[lib.name], executable)
            else:
                lib.read_object_data(executable)
                lib_cache[lib.name] = lib
    libs.append(
        LibraryData(
            '<virtual>',
            0x7000000000000000,
            0x7fffffffffffffff,
            True,
            symbols=virtual_symbols)
    )
    if extra_libs:
        libs += extra_libs
    addrspace = AddressSpace(libs)
    #
    if load_jit_symbols:
        jitinfo_filename = prof_filename + '.jitinfo'
        JIT_symbols = read_jit_symbols_maybe(jitinfo_filename)
        if interp_name == 'pypy' and JIT_symbols is None:
            print >> sys.stderr, ('WARNING: cannot read JIT symbols from %s' %
                                  jitinfo_filename)
        addrspace.JIT_symbols = JIT_symbols
    #
    return profiles, interp_name, addrspace

read_profile.lib_cache = {}


def read_stats(prof_filename, extra_libs=None,
               virtual_only=True, include_extra_info=True,
               lib_cache=None):

    profiles, interp_name, addrspace = read_profile(prof_filename, extra_libs,
                                                    virtual_only, include_extra_info,
                                                    lib_cache)
    
    filtered_profiles, addr_set, jit_frames = addrspace.filter_addr(profiles,
        virtual_only, include_extra_info, interp_name)
    d = {}
    for addr in addr_set:
        name, _, _, lib = addrspace.lookup(addr)
        if lib is None:
            name = 'jit:' + name
        d[addr] = name
    if include_extra_info:
        d.update(addrspace.meta_data)
    s = Stats(filtered_profiles, d, jit_frames, interp_name)
    s.addrspace = addrspace
    return s


class Profiler(object):
    ctx = None

    def __init__(self):
        self._lib_cache = {}

    def measure(self):
        self.ctx = ProfilerContext()
        return self.ctx

    def get_stats(self):
        if not self.ctx:
            raise VMProfError("no profiling done")
        if not self.ctx.done:
            raise VMProfError("profiling in process")
        res = read_stats(self.ctx.tmpfile.name)
        self.ctx = None
        return res
