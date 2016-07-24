
""" Test the actual run
"""

import py
import sys
import tempfile

import six

import vmprof
from vmprof.reader import read_prof_bit_by_bit
from vmprof.stats import Stats

if sys.version_info.major == 3:
    xrange = range
    PY3K = True
else:
    PY3K = False

if '__pypy__' in sys.builtin_module_names:
    COUNT = 100000
else:
    COUNT = 10000
    
def function_foo():
    for k in range(1000):
        l = [a for a in xrange(COUNT)]
    return l


def function_bar():
    return function_foo()


foo_full_name = "py:function_foo:%d:%s" % (function_foo.__code__.co_firstlineno,
                                           function_foo.__code__.co_filename)
bar_full_name = "py:function_bar:%d:%s" % (function_bar.__code__.co_firstlineno,
                                           function_bar.__code__.co_filename)


def test_basic():
    tmpfile = tempfile.NamedTemporaryFile(delete=False)
    vmprof.enable(tmpfile.fileno())
    function_foo()
    vmprof.disable()
    tmpfile.close()
    assert b"function_foo" in open(tmpfile.name, 'rb').read()

def test_read_bit_by_bit():
    tmpfile = tempfile.NamedTemporaryFile(delete=False)
    vmprof.enable(tmpfile.fileno())
    function_foo()
    vmprof.disable()
    tmpfile.close()
    with open(tmpfile.name, 'rb') as f:
        period, profiles, virtual_symbols, interp_name = read_prof_bit_by_bit(f)
        stats = Stats(profiles, virtual_symbols, interp_name)
        stats.get_tree()

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

    if '__pypy__' in sys.builtin_module_names:
        names.sort()
        assert len([x for x in names if str(x).startswith('jit:')]) > 0
        assert len([x for x in names if x == foo_full_name]) == 1
    else:
        if sys.version_info.major == 2:
            assert names == [foo_full_name]
        else:
            assert foo_full_name in names
    t = stats.get_tree()
    while 'function_bar' not in t.name:
        t = t['']
    assert len(t.children) == 1
    assert 'function_foo' in t[''].name
    if PY3K:
        assert len(t[''].children) == 1
        assert '<listcomp>' in t[''][''].name
    else:
        assert len(t[''].children) == 0

def test_multithreaded():
    if '__pypy__' in sys.builtin_module_names or PY3K:
        py.test.skip("not supported on pypy and python3 just yet")
    import threading
    finished = []

    def f():
        for k in range(1000):
            l = [a for a in xrange(COUNT)]
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
    cur_id = list(all_ids)[0]
    assert len(all_ids) in (3, 4) # maybe 0
    lgt1 = len([x[2] for x in stats.profiles if x[2] == cur_id])
    total = len(stats.profiles)
    # between 33-10% and 33+10% is within one profile
    # this is too close of a call - thread scheduling can leave us
    # unlucky, especially on badly behaved systems
    # assert (0.23 * total) <= lgt1 <= (0.43 * total)
    assert len(finished) == 3

def test_memory_measurment():
    if not sys.platform.startswith('linux') or '__pypy__' in sys.builtin_module_names:
        py.test.skip("unsupported platform")
    def function_foo():
        all = []
        for k in range(1000):
            all.append([a for a in xrange(COUNT)])
        return all


    def function_bar():
        return function_foo()
    prof = vmprof.Profiler()
    with prof.measure(memory=True):
        function_bar()

    s = prof.get_stats()


def test_line_profiling():
    tmpfile = tempfile.NamedTemporaryFile(delete=False)
    vmprof.enable(tmpfile.fileno(), lines=True)  # enable lines profiling
    function_foo()
    vmprof.disable()
    tmpfile.close()

    def walk(tree):
        assert len(tree.lines) >= len(tree.children)

        for v in six.itervalues(tree.children):
                walk(v)

    with open(tmpfile.name, 'rb') as f:
        period, profiles, virtual_symbols, interp_name = read_prof_bit_by_bit(f)
        stats = Stats(profiles, virtual_symbols, interp_name)
        walk(stats.get_tree())



if __name__ == '__main__':
    test_line_profiling()
