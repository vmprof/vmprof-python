
""" Test the actual run
"""

import tempfile
import _vmprof
import vmprof

def function_foo():
    for i in range(100000000):
        pass

def function_bar():
    function_foo()

foo_full_name = "py:function_foo:%d:%s" % (function_foo.__code__.co_firstlineno,
                                           function_foo.__code__.co_filename)
bar_full_name = "py:function_bar:%d:%s" % (function_bar.__code__.co_firstlineno,
                                           function_bar.__code__.co_filename)


def test_basic():
    tmpfile = tempfile.NamedTemporaryFile()
    _vmprof.enable(tmpfile.fileno())
    function_foo()
    _vmprof.disable()
    assert "function_foo" in  open(tmpfile.name).read()

def test_enable_disable():
    prof = vmprof.Profiler()
    with prof.measure():
        function_foo()
    stats = prof.get_stats()
    assert stats.top_profile()[-1][0] == foo_full_name
    assert stats.top_profile()[-1][1] > 0

def test_nested_call():
    prof = vmprof.Profiler()
    with prof.measure():
        function_bar()
    stats = prof.get_stats()
    tprof = stats.top_profile()
    assert tprof[-2][0] == bar_full_name
    assert tprof[-1][0] == foo_full_name
    for k, v in stats.adr_dict.iteritems():
        if v == bar_full_name:
            bar_adr = k
            break
    assert stats._get_name(stats.function_profile(bar_adr)[0][0][0]) == foo_full_name

