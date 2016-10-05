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
    def enable(fileno, period=DEFAULT_PERIOD, memory=False, lines=False):
        if not isinstance(period, float):
            raise ValueError("You need to pass a float as an argument")
        gz_fileno = _gzip_start(fileno)
        _vmprof.enable(gz_fileno, period, memory, lines)
else:
    def enable(fileno, period=DEFAULT_PERIOD, memory=False, lines=False, warn=True):
        if not isinstance(period, float):
            raise ValueError("You need to pass a float as an argument")
        if warn and sys.pypy_version_info[:3] < (4, 1, 0):
            raise Exception("PyPy <4.1 have various kinds of bugs, pass warn=False if you know what you're doing")
        if warn and memory:
            print("Memory profiling is currently unsupported for PyPy. Running without memory statistics.")
        if warn and lines:
            print('Line profiling is currently unsupported for PyPy. Running without lines statistics.\n')
        gz_fileno = _gzip_start(fileno)
        _vmprof.enable(gz_fileno, period)

def disable():
    try:
        _vmprof.disable()
        _gzip_finish()
    except IOError as e:
        raise Exception("Error while writing profile: " + str(e))


_gzip_proc = None

def _gzip_start(fileno):
    """Spawn a gzip subprocess that writes compressed profile data to `fileno`.

    Return the subprocess' input fileno.
    """
    # Prefer system gzip and fall back to Python's gzip module
    if which("gzip"):
        gzip_cmd = ["gzip", "-", "-4"]
    else:
        gzip_cmd = ["python", "-u", "-m", "gzip"]
    global _gzip_proc
    _gzip_proc = subprocess.Popen(gzip_cmd, stdin=subprocess.PIPE,
                                  stdout=fileno, bufsize=-1,
                                  close_fds=(sys.platform != "win32"))
    return _gzip_proc.stdin.fileno()

def _gzip_finish():
    global _gzip_proc
    if _gzip_proc is not None:
        _gzip_proc.stdin.close()
        returncode = _gzip_proc.wait()
        assert returncode == 0, \
               "return code was non zero: %d" % returncode
        _gzip_proc = None
