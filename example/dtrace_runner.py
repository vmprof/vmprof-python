#!/usr/bin/env python
""" Usage:

dtrace_runner.py programe.py [args]
"""

import _vmprof
import subprocess, os, sys

dtrace_consumer = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                               'dtrace-consumer')

p = subprocess.Popen([dtrace_consumer], stdin=subprocess.PIPE)
fileno = p.stdin.fileno()
sys.argv = sys.argv[1:]

_vmprof.enable(fileno, 0.001)
try:
    execfile(sys.argv[0])
finally:
    _vmprof.disable()
    p.stdin.close()
    p.wait()
