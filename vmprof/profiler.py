
import _vmprof
import tempfile
from vmprof.addrspace import AddressSpace, Profiles
from vmprof.reader import read_prof, read_ranges, LibraryData, read_sym_file

class VMProfError(Exception):
    pass

class ProfilerContext(object):
    done = False
    
    def __init__(self):
        self.tmpfile = tempfile.NamedTemporaryFile()
        self.symfile = tempfile.NamedTemporaryFile()

    def __enter__(self):
        _vmprof.enable(self.tmpfile.fileno(), self.symfile.fileno())

    def __exit__(self, type, value, traceback):
        _vmprof.disable()
        self.done = True


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
        res = Profiles(*self.read_profile(self.ctx.tmpfile.name,
                                       self.ctx.symfile.name))
        #self.ctx = None
        return res

    def read_profile(self, prof_filename, sym_filename):
        prof = open(prof_filename, 'rb').read()
        prof_sym = open(sym_filename, 'rb').read()

        period, profiles, symmap = read_prof(prof)
        libs = read_ranges(symmap)

        for i, lib in enumerate(libs):
            if lib.name in self._lib_cache:
                libs[i] = self._lib_cache[lib.name]
            else:
                lib.read_object_data()
                self._lib_cache[lib.name] = lib
        libs.append(
            LibraryData(
                '<virtual>',
                0x8000000000000000L,
                0x8fffffffffffffffL,
                True,
                symbols=read_sym_file(prof_sym))
        )
        addrspace = AddressSpace(libs)
        filtered_profiles, addr_set = addrspace.filter_addr(profiles)
        d = {}
        for addr in addr_set:
            name, _ = addrspace.lookup(addr | 0x8000000000000000L)
            d[addr] = name
        return filtered_profiles, d
