import vmprof
import tempfile

from vmprof.stats import Stats
from vmprof.reader import read_prof


class VMProfError(Exception):
    pass

class ProfilerContext(object):
    done = False

    def __init__(self, name, memory, use_wall_time):
        if name is None:
            self.tmpfile = tempfile.NamedTemporaryFile(delete=False)
        else:
            self.tmpfile = open(name, "wb")
        self.memory = memory
        self.use_wall_time = use_wall_time

    def __enter__(self):
        vmprof.enable(self.tmpfile.fileno(), 0.001, self.memory, self.use_wall_time)

    def __exit__(self, type, value, traceback):
        vmprof.disable()
        self.tmpfile.close()
        self.done = True


def read_profile(prof_filename):
    prof = open(str(prof_filename), 'rb')

    period, profiles, virtual_symbols, interp_name = read_prof(prof)

    jit_frames = {}
    d = dict(virtual_symbols)
    s = Stats(profiles, d, jit_frames, interp_name)
    return s


class Profiler(object):
    ctx = None

    def __init__(self):
        self._lib_cache = {}

    def measure(self, name=None, memory=False, use_wall_time=False):
        self.ctx = ProfilerContext(name, memory, use_wall_time)
        return self.ctx

    def get_stats(self):
        if not self.ctx:
            raise VMProfError("no profiling done")
        if not self.ctx.done:
            raise VMProfError("profiling in process")
        res = read_profile(self.ctx.tmpfile.name)
        self.ctx = None
        return res

