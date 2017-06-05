import vmprof
import tempfile

from vmprof.stats import Stats
from vmprof.reader import _read_prof


class VMProfError(Exception):
    pass

class ProfilerContext(object):
    done = False

    def __init__(self, name, period, memory, native, real_time):
        if name is None:
            self.tmpfile = tempfile.NamedTemporaryFile("w+b", delete=False)
        else:
            self.tmpfile = open(name, "w+b")
        self.filename = self.tmpfile.name
        self.period = period
        self.memory = memory
        self.native = native
        self.real_time = real_time

    def __enter__(self):
        vmprof.enable(self.tmpfile.fileno(), self.period, self.memory,
                      native=self.native, real_time=self.real_time)

    def __exit__(self, type, value, traceback):
        vmprof.disable()
        self.tmpfile.close() # flushes the stream
        self.done = True


def read_profile(prof_file):
    file_to_close = None
    if not hasattr(prof_file, 'read'):
        prof_file = file_to_close = open(str(prof_file), 'rb')

    state = _read_prof(prof_file)

    if file_to_close:
        file_to_close.close()

    jit_frames = {}
    d = dict(state.virtual_ips)
    s = Stats(state.profiles, d, jit_frames, interp=state.interp_name,
              start_time=state.start_time, end_time=state.end_time,
              meta=state.meta, state=state)
    return s


class Profiler(object):
    ctx = None

    def __init__(self):
        self._lib_cache = {}

    def measure(self, name=None, period=0.001, memory=False, native=False, real_time=False):
        self.ctx = ProfilerContext(name, period, memory, native, real_time)
        return self.ctx

    def get_stats(self):
        if not self.ctx:
            raise VMProfError("no profiling done")
        if not self.ctx.done:
            raise VMProfError("profiling in process")
        res = read_profile(self.ctx.tmpfile.name)
        self.ctx = None
        return res

