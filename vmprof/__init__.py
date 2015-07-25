import os
import sys

from . import com
from . import cli

import _vmprof

from vmprof.reader import read_prof, LibraryData
from vmprof.addrspace import AddressSpace
from vmprof.stats import Stats
from vmprof.profiler import Profiler, read_profile


IS_PYPY = hasattr(sys, 'pypy_translation_info')


if not IS_PYPY:
    _virtual_ips_so_far = None
    _prof_fileno = -1

    def enable(fileno, period=0.001):
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
    def enable(fileno, period=0.001, warn=True):
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
