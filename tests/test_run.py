
""" Test the actual run
"""

import tempfile
import _vmprof
import vmprof
from vmprof.profiler import symfile

def function_foo():
    for i in range(10000000):
        pass

def function_bar():
    function_foo()

foo_full_name = "py:function_foo:%s:%d" % (function_foo.__code__.co_filename,
                                           function_foo.__code__.co_firstlineno)
bar_full_name = "py:function_foo:%s:%d" % (function_bar.__code__.co_filename,
                                           function_bar.__code__.co_firstlineno)


def test_basic():
    
    tmpfile = tempfile.NamedTemporaryFile()
    _vmprof.enable(tmpfile.fileno(), symfile.fileno())
    function_foo()
    _vmprof.disable()
    assert "function_foo" in  open(symfile.name).read()

def test_enable_disable():
    prof = vmprof.Profiler()
    with prof.measure():
        function_foo()
    stats = prof.get_stats()
    assert stats.functions.keys() == [foo_full_name]
    assert stats.functions.values()[0] > 0

def test_nested_call():
    prof = vmprof.Profiler()
    with prof.measure():
        function_bar()
    stats = prof.get_stats()
    assert len(stats.functions) == 2
    assert stats.function_profile(bar_full_name).keys() == [foo_full_name]

