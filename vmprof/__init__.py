import os
import sys

from . import cli

import _vmprof

from vmprof.reader import read_prof, LibraryData
from vmprof.addrspace import AddressSpace
from vmprof.stats import Stats
from vmprof.profiler import Profiler, read_profile, read_stats


IS_PYPY = hasattr(sys, 'pypy_translation_info')

# it's not a good idea to use a "round" default sampling period, else we risk
# to oversample periodic tasks which happens to run at e.g. 100Hz or 1000Hz:
# http://www.solarisinternals.com/wiki/index.php/DTrace_Topics_Hints_Tips#profile-1001.2C_profile-997.3F
#
# To avoid the problem, we use a period which is "almost" but not exactly
# 1000Hz
DEFAULT_PERIOD = 0.00099

if not IS_PYPY:
    _virtual_ips_so_far = None
    _prof_fileno = -1

    def enable(fileno, period=DEFAULT_PERIOD, filename=None):
        if not isinstance(period, float):
            raise ValueError("You need to pass a float as an argument")
        global _prof_fileno
        global _virtual_ips_so_far

        def pack_virtual_ips(tup):
            import struct

            l = []
            for k, v in tup:
                l.append(b'\x02')
                l.append(struct.pack('QQ', k, len(v)))
                if not isinstance(v, bytes):
                    v = v.encode('utf-8')
                l.append(v)
            return b"".join(l)

        _prof_fileno = fileno
        if _virtual_ips_so_far is not None:
            _vmprof.enable(fileno, period,
                           pack_virtual_ips(_virtual_ips_so_far))
        else:
            _vmprof.enable(fileno, period)

    def disable():
        global _virtual_ips_so_far
        global _prof_fileno

        _vmprof.disable()
        f = os.fdopen(os.dup(_prof_fileno), "rb")
        f.seek(0)
        _virtual_ips_so_far = read_prof(f, virtual_ips_only=True)
        _prof_fileno = -1

else:
    from vmprof.pypyhook import JITInfoWriter
    _jit_info_writer = None
    
    def enable(fileno, period=DEFAULT_PERIOD, warn=True, filename=None):
        global _jit_info_writer
        if not isinstance(period, float):
            raise ValueError("You need to pass a float as an argument")
        if warn and sys.pypy_version_info[:3] <= (2, 6, 0):
            print ("PyPy 2.6.0 and below has a bug in vmprof where "
                   "fork() would disable your profiling. "
                   "Pass warn=False if you know what you're doing")
            raise Exception("PyPy 2.6.0 and below has a bug in vmprof where "
                            "fork() would disable your profiling. "
                            "Pass warn=False if you know what you're doing")
        #
        # ideally, we would like to record JIT info directly inside the vmprof
        # logfile, but to do so we need support from _vmprof which we don't
        # have right now. Even more ideally, the JIT hook to record JIT
        # boundaries should be written in RPython and built-in in _vmprof. In
        # the meantime, we do it at app-level and by writing info to an
        # external file. Ugly but effective.
        jit_info_filename = filename + '.jitinfo'
        _jit_info_writer = JITInfoWriter(jit_info_filename)
        _jit_info_writer.enable()
        _vmprof.enable(fileno, period)

    def disable():
        _vmprof.disable()
        _jit_info_writer.disable()
