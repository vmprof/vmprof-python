import vmprof
import tempfile

from vmprof.addrspace import AddressSpace, Stats
from vmprof.reader import read_prof, LibraryData


class VMProfError(Exception):
    pass


class ProfilerContext(object):
    done = False

    def __init__(self):
        self.tmpfile = tempfile.NamedTemporaryFile()

    def __enter__(self):
        vmprof.enable(self.tmpfile.fileno(), 1000)

    def __exit__(self, type, value, traceback):
        vmprof.disable()
        self.done = True


# lib_cache is global on purpose
def read_profile(prof_filename, lib_cache={}, extra_libs=None,
                 virtual_only=True):
    prof = open(prof_filename, 'rb')

    period, profiles, virtual_symbols, libs = read_prof(prof)

    if not virtual_only:
        for i, lib in enumerate(libs):
            if lib.name in lib_cache:
                libs[i] = lib_cache[lib.name]
            else:
                lib.read_object_data(lib.start)
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
    filtered_profiles, addr_set = addrspace.filter_addr(profiles, virtual_only)
    d = {}
    for addr in addr_set:
        name, _, _ = addrspace.lookup(addr)
        d[addr] = name
    return Stats(filtered_profiles, d)


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
        res = read_profile(self.ctx.tmpfile.name)
        self.ctx = None
        return res

