import os
import subprocess
import sys
import contextlib

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
        _vmprof.enable(fileno, period, memory)

    def disable():
        _vmprof.disable()

else:
    def enable(fileno, period=DEFAULT_PERIOD, memory=False, warn=True):
        if not isinstance(period, float):
            raise ValueError("You need to pass a float as an argument")
        if warn and sys.pypy_version_info[:3] < (4, 1, 0):
            print ("PyPy <4.1 have various kinds of bugs, pass warn=False if you know what you're doing")
            raise Exception("PyPy <4.1 have various kinds of bugs, pass warn=False if you know what you're doing")
        _vmprof.enable(fileno, period)

    def disable():
        _vmprof.disable()

    def enable_jitlog(fileno):
        """ Should be a different file than the one provided
            to vmprof.enable(...). Otherwise the profiling data might
            be broken.
        """
        _vmprof.enable_jitlog(fileno)


@contextlib.contextmanager
def profile(outfile, pipecmd=None, period=DEFAULT_PERIOD, memory=False):
    """Utility context manager which calls vmprof.enable() and vmprof.disable().

    outfile is path to output file.

    This function support compression via pipe command::

        with vmprof.profile("out.prof.gz", pipecmd=["/usr/bin/gzip", "-4"]):
            main()
    """
    with open(outfile, 'wb') as of:
        proc = None
        fileno = of.fileno()
        if pipecmd is not None:
            proc = subprocess.Popen(pipecmd, bufsize=-1, stdin=subprocess.PIPE, stdout=of.fileno())
            fileno = proc.stdin.fileno()
        enable(fileno, period, memory)
        try:
            yield
        finally:
            disable()
            if proc:
                proc.stdin.close()
                proc.wait()
