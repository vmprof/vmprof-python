import os
import sys

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
        _vmprof.enable(fileno, period, memory, lines)

    def disable():
        _vmprof.disable()

    def enable_jitlog(fileno):
        raise ValueError("Jitlog cannot be enabled on CPython")

    def disable_jitlog():
        raise ValueError("Jitlog cannot be disabled on CPython")

else:
    def enable(fileno, period=DEFAULT_PERIOD, memory=False, lines=False, warn=True):
        if not isinstance(period, float):
            raise ValueError("You need to pass a float as an argument")
        if warn and sys.pypy_version_info[:3] < (4, 1, 0):
            print("PyPy <4.1 have various kinds of bugs, pass warn=False if you know what you're doing\n")
            raise Exception("PyPy <4.1 have various kinds of bugs, pass warn=False if you know what you're doing")
        if warn and lines:
            print('Line profiling is currently unsupported for PyPy. Running without lines statistics.\n')
        _vmprof.enable(fileno, period)

    def disable():
        _vmprof.disable()

    def enable_jitlog(fileno):
        """ Should be a different file than the one provided
            to vmprof.enable(...). Otherwise the profiling data might
            be broken.
        """
        _vmprof.enable_jitlog(fileno)

    def disable_jitlog():
        _vmprof.disable_jitlog()
