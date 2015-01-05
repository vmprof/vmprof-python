
""" Test the actual run
"""

import tempfile
import _vmprof
import vmprof

def function_foo():
    for i in range(10000000):
        pass


def test_basic():
    
    tmpfile = tempfile.NamedTemporaryFile()
    symfile = tempfile.NamedTemporaryFile()
    _vmprof.enable(tmpfile.fileno(), symfile.fileno())
    function_foo()
    _vmprof.disable()
    assert "function_foo" in  open(symfile.name).read()

def test_enable_disable():
    prof = vmprof.Profiler()
    with prof.measure():
        function_foo()
    stats = prof.get_stats()
    xxx
