
""" Test the actual run
"""

import py
import tempfile
import vmprof
import time
import sys


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

def test_multithreaded():
    if '__pypy__' in sys.builtin_module_names:
        py.test.skip("not supported on pypy just yet")
    import threading
    finished = []

    def f():
        t0 = time.time()
        while time.time() - t0 < 1.5:
            pass # busy loop
        finished.append("foo")

    threads = [threading.Thread(target=f), threading.Thread(target=f)]
    prof = vmprof.Profiler()
    with prof.measure():
        for t in threads:
            t.start()
        f()
        for t in threads:
            t.join()
    
    stats = prof.get_stats()
    all_ids = set([x[2] for x in stats.profiles])
    cur_id = threading.currentThread().ident
    assert all_ids == set([threading.currentThread().ident,
                           threads[0].ident, threads[1].ident])
    lgt1 = len([x[2] for x in stats.profiles if x[2] == cur_id])
    total = len(stats.profiles)
    # between 33-10% and 33+10% is within one profile
    assert (0.23 * total) <= lgt1 <= (0.43 * total)
    assert len(finished) == 3
