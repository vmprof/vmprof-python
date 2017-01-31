import vmprof
import tempfile

from vmprof.stats import Stats
from vmprof.reader import read_prof


class VMProfError(Exception):
    pass

class ProfilerContext(object):
    done = False

    def __init__(self, name, memory, native):
        if name is None:
            self.tmpfile = tempfile.NamedTemporaryFile(delete=False)
        else:
            self.tmpfile = open(name, "wb")
        self.filename = self.tmpfile.name
        self.memory = memory
        self.native = native

    def __enter__(self):
        vmprof.enable(self.tmpfile.fileno(), 0.001, self.memory, native=self.native)

    def __exit__(self, type, value, traceback):
        vmprof.disable()
        self.tmpfile.close() # flushes the stream
        self.done = True


def read_profile(prof_file):
    file_to_close = None
    if not hasattr(prof_file, 'read'):
        prof_file = file_to_close = open(str(prof_file), 'rb')

    period, profiles, virtual_symbols, interp_name, meta, start_time, end_time = read_prof(prof_file)

    if file_to_close:
        file_to_close.close()

    jit_frames = {}
    d = dict(virtual_symbols)
    s = Stats(profiles, d, jit_frames, interp=interp_name,
              start_time=start_time, end_time=end_time, meta=meta)
    return s


class Profiler(object):
    ctx = None

    def __init__(self):
        self._lib_cache = {}

    def measure(self, name=None, memory=False, native=False):
        self.ctx = ProfilerContext(name, memory, native)
        return self.ctx

    def get_stats(self):
        if not self.ctx:
            raise VMProfError("no profiling done")
        if not self.ctx.done:
            raise VMProfError("profiling in process")
        res = read_profile(self.ctx.tmpfile.name)
        self.ctx = None
        return res

