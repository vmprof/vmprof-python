
""" Test the actual run
"""

import tempfile
import vmprof
import time


def function_foo():
    t0 = time.time()
    while time.time() - t0 < 0.5:
        pass # busy loop for 0.5s

def function_bar():
    function_foo()

foo_full_name = "py:function_foo:%d:%s" % (function_foo.__code__.co_firstlineno,
                                           function_foo.__code__.co_filename)
bar_full_name = "py:function_bar:%d:%s" % (function_bar.__code__.co_firstlineno,
                                           function_bar.__code__.co_filename)


def test_basic():
    tmpfile = tempfile.NamedTemporaryFile()
    vmprof.enable(tmpfile.fileno())
    function_foo()
    vmprof.disable()
    assert b"function_foo" in  open(tmpfile.name, 'rb').read()

def test_enable_disable():
    prof = vmprof.Profiler()
    with prof.measure():
        function_foo()
    stats = prof.get_stats()
    d = dict(stats.top_profile())
    assert d[foo_full_name] > 0

def test_nested_call():
    prof = vmprof.Profiler()
    with prof.measure():
        function_bar()
    # now jitted, on pypy
    with prof.measure():
        function_bar()
    stats = prof.get_stats()
    tprof = stats.top_profile()
    d = dict(tprof)
    assert d[bar_full_name] > 0
    assert d[foo_full_name] > 0
    for k, v in stats.adr_dict.items():
        if v == bar_full_name:
            bar_adr = k
            break
    names = [stats._get_name(i[0]) for i in stats.function_profile(bar_adr)[0]]
    if len(names) == 1: # cpython
        assert names == [foo_full_name]
    else:
        names.sort()
        assert names[1] == foo_full_name
        assert names[0].startswith('jit:')

