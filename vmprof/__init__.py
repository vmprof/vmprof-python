import os
import sys
import subprocess
try:
    from shutil import which
except ImportError:
    from backports.shutil_which import which

from . import cli

import _vmprof

from vmprof.reader import read_prof
from vmprof.stats import Stats
from vmprof.profiler import Profiler, read_profile


IS_PYPY = '__pypy__' in sys.builtin_module_names

# it's not a good idea to use a "round" default sampling period, else we risk
# to oversample periodic tasks which happens to run at e.g. 100Hz or 1000Hz:
# http://www.solarisinternals.com/wiki/index.php/DTrace_Topics_Hints_Tips#profile-1001.2C_profile-997.3F
#
# To avoid the problem, we use a period which is "almost" but not exactly
# 1000Hz
DEFAULT_PERIOD = 0.00099

if not IS_PYPY:
    def enable(fileno, period=DEFAULT_PERIOD, memory=False):
        if not isinstance(period, float):
            raise ValueError("You need to pass a float as an argument")
        gz_fileno = _gzip(fileno)
        _vmprof.enable(gz_fileno, period, memory)

    def disable():
        _vmprof.disable()
        _gzip_finish()

else:
    def enable(fileno, period=DEFAULT_PERIOD, memory=False, warn=True):
        if not isinstance(period, float):
            raise ValueError("You need to pass a float as an argument")
        if warn and sys.pypy_version_info[:3] < (4, 1, 0):
            print ("PyPy <4.1 have various kinds of bugs, pass warn=False if you know what you're doing")
            raise Exception("PyPy <4.1 have various kinds of bugs, pass warn=False if you know what you're doing")
        gz_fileno = _gzip(fileno)
        _vmprof.enable(gz_fileno, period)

    def disable():
        _vmprof.disable()
        _gzip_finish()

    def enable_jitlog(fileno):
        """ Should be a different file than the one provided
            to vmprof.enable(...). Otherwise the profiling data might
            be broken.
        """
        gz_fileno = _gzip(fileno)
        _vmprof.enable_jitlog(gz_fileno)


_gzip_procs = []

def _gzip(fileno):
    """Spawn a gzip subprocess that writes compressed profile data to `fileno`.

    Return the subprocess' input fileno.
    """
    # Prefer system gzip and fall back to Python's gzip module
    if which("gzip"):
        gzip_cmd = ["gzip", "-", "-4"]
    else:
        gzip_cmd = ["python", "-m", "gzip"]
    proc = subprocess.Popen(gzip_cmd, stdin=subprocess.PIPE,
                            stdout=fileno, bufsize=-1, close_fds=True)
    _gzip_procs.append(proc)
    return proc.stdin.fileno()

def _gzip_finish():
    for proc in _gzip_procs:
        proc.stdin.close()
        proc.wait()
    _gzip_procs[:] = []
