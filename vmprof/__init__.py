import os
import sys

from . import cli

import _vmprof

from vmprof.reader import read_prof, LibraryData
from vmprof.addrspace import AddressSpace
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
    def enable(fileno, period=DEFAULT_PERIOD):
        if not isinstance(period, float):
            raise ValueError("You need to pass a float as an argument")
        _vmprof.enable(fileno, period)

    def disable():
        _vmprof.disable()

else:
    def enable(fileno, period=DEFAULT_PERIOD, warn=True):
        if not isinstance(period, float):
            raise ValueError("You need to pass a float as an argument")
        if warn and sys.pypy_version_info[:3] <= (2, 6, 0):
            print ("PyPy 2.6.0 and below has a bug in vmprof where "
                   "fork() would disable your profiling. "
                   "Pass warn=False if you know what you're doing")
            raise Exception("PyPy 2.6.0 and below has a bug in vmprof where "
                            "fork() would disable your profiling. "
                            "Pass warn=False if you know what you're doing")
        _vmprof.enable(fileno, period)

    def disable():
        _vmprof.disable()
