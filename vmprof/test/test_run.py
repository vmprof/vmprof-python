""" Test the actual run
"""
import py
import sys
import tempfile
import gzip
import struct
from datetime import datetime
import pytz

import six

import vmprof
from vmprof.show import PrettyPrinter
from vmprof.reader import (gunzip, BufferTooSmallError, _read_header,
        FileObjWrapper, MARKER_STACKTRACE, MARKER_VIRTUAL_IP,
        MARKER_TRAILER, FileReadError, read_trace,
        VERSION_THREAD_ID, WORD_SIZE, ReaderStatus,
        read_time_and_zone, MARKER_TIME_N_ZONE, assert_error)
from vmshare.binary import read_string, read_word
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
    import time
    for k in range(1000):
        time.sleep(0.001)
    return 1+1


def function_bar():
    return function_foo()


foo_full_name = "py:function_foo:%d:%s" % (function_foo.__code__.co_firstlineno,
                                           function_foo.__code__.co_filename)
bar_full_name = "py:function_bar:%d:%s" % (function_bar.__code__.co_firstlineno,
                                           function_bar.__code__.co_filename)

GZIP = True

def test_basic():
    tmpfile = tempfile.NamedTemporaryFile(delete=False)
    vmprof.enable(tmpfile.fileno())
    function_foo()
    vmprof.disable()
    tmpfile.close()
    if GZIP:
        assert b"function_foo" in gzip.GzipFile(tmpfile.name).read()
    else:
        with open(tmpfile.name, 'rb') as file:
            content = file.read()
            assert b"function_foo" in content

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

def test_start_end_time():
    prof = vmprof.Profiler()
    before_profile = datetime.now(pytz.utc)
    with prof.measure():
        function_foo()
    after_profile = datetime.now(pytz.utc)
    stats = prof.get_stats()
    s = stats.start_time
    e = stats.end_time
    assert before_profile < s and s < after_profile
    assert s < e
    assert e < after_profile and s < after_profile
    assert before_profile < after_profile
    assert before_profile < e


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
    if '__pypy__' in sys.builtin_module_names:
        py.test.skip("not supported on pypy just yet")
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

if GZIP:
    def test_gzip_problem():
        tmpfile = tempfile.NamedTemporaryFile(delete=False)
        vmprof.enable(tmpfile.fileno())
        vmprof._gzip_proc.kill()
        function_foo()
        with py.test.raises(Exception) as exc_info:
            vmprof.disable()
            assert "Error while writing profile" in str(exc_info)
        tmpfile.close()

def read_prof_bit_by_bit(fileobj):
    fileobj = gunzip(fileobj)
    # note that we don't want to use all of this on normal files, since it'll
    # cost us quite a bit in memory and performance and parsing 200M files in
    # CPython is slow (pypy does better, use pypy)
    buf = None
    while True:
        try:
            status = read_header(fileobj, buf)
            break
        except BufferTooSmallError as e:
            buf = e.get_buf()
    finished = False
    buf = None
    while not finished:
        try:
            finished = read_one_marker(fileobj, status, buf)
        except BufferTooSmallError as e:
            buf = e.get_buf()
    return status.period, status.profiles, status.virtual_ips, status.interp_name

def read_one_marker(fileobj, status, buffer_so_far=None):
    fileobj = FileObjWrapper(fileobj, buffer_so_far)
    marker = fileobj.read(1)
    if marker == MARKER_STACKTRACE:
        count = read_word(fileobj)
        # for now
        assert count == 1
        depth = read_word(fileobj)
        assert depth <= 2**16, 'stack strace depth too high'
        trace = read_trace(fileobj, depth, status.version, status.profile_lines)

        if status.version >= VERSION_THREAD_ID:
            thread_id, = struct.unpack('l', fileobj.read(WORD_SIZE))
        else:
            thread_id = 0
        if status.profile_memory:
            mem_in_kb, = struct.unpack('l', fileobj.read(WORD_SIZE))
        else:
            mem_in_kb = 0
        trace.reverse()
        status.profiles.append((trace, 1, thread_id, mem_in_kb))
    elif marker == MARKER_VIRTUAL_IP:
        unique_id = read_word(fileobj)
        name = read_string(fileobj)
        if PY3K:
            name = name.decode()
        status.virtual_ips[unique_id] = name
    elif marker == MARKER_TRAILER:
        return True # finished
    elif marker == MARKER_TIME_N_ZONE:
        read_time_and_zone(fileobj)
    else:
        raise FileReadError("unexpected marker: %d" % ord(marker))
    return False

def read_header(fileobj, buffer_so_far=None):
    fileobj = FileObjWrapper(fileobj, buffer_so_far)
    assert_error(read_word(fileobj) == 0)
    assert_error(read_word(fileobj) == 3)
    assert_error(read_word(fileobj) == 0)
    period = read_word(fileobj)
    assert_error(read_word(fileobj) == 0)
    interp_name, version, profile_memory, profile_lines = _read_header(fileobj)
    return ReaderStatus(interp_name, period, version, None, profile_memory,
                        profile_lines)


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

def test_vmprof_show():
    tmpfile = tempfile.NamedTemporaryFile(delete=False)
    vmprof.enable(tmpfile.fileno())
    function_bar()
    vmprof.disable()
    tmpfile.close()

    pp = PrettyPrinter()
    pp.show(tmpfile.name)

if __name__ == '__main__':
    test_line_profiling()
