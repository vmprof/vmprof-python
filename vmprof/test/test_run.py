""" Test the actual run
"""
import os
import pytest
import sys
import tempfile
import time
import gzip
import time
import pytz
import vmprof
import six
from cffi import FFI
from datetime import datetime
import requests
from vmshare.service import Service, ServiceException
from vmprof.show import PrettyPrinter
from vmprof.profiler import read_profile
from vmprof.reader import (gunzip, MARKER_STACKTRACE, MARKER_VIRTUAL_IP,
        MARKER_TRAILER, FileReadError, VERSION_THREAD_ID,
        MARKER_TIME_N_ZONE, assert_error,
        MARKER_META, MARKER_NATIVE_SYMBOLS)
from vmshare.binary import read_string, read_word, read_addr
from vmprof.stats import Stats

IS_PYPY = '__pypy__' in sys.builtin_module_names

class BufferTooSmallError(Exception):
    def get_buf(self):
        return b"".join(self.args[0])

class FileObjWrapper(object):
    def __init__(self, fileobj, buffer_so_far=None):
        self._fileobj = fileobj
        self._buf = []
        self._buffer_so_far = buffer_so_far
        self._buffer_pos = 0

    def read(self, count):
        if self._buffer_so_far is not None:
            if self._buffer_pos + count >= len(self._buffer_so_far):
                s = self._buffer_so_far[self._buffer_pos:]
                s += self._fileobj.read(count - len(s))
                self._buffer_so_far = None
            else:
                s = self._buffer_so_far[self._buffer_pos:self._buffer_pos + count]
                self._buffer_pos += count
        else:
            s = self._fileobj.read(count)
        self._buf.append(s)
        if len(s) < count:
            raise BufferTooSmallError(self._buf)
        return s


if sys.version_info.major == 3:
    xrange = range
    PY3K = True
else:
    PY3K = False
if hasattr(os, 'uname') and os.uname()[4] == 'ppc64le':
    PPC64LE = True
else:
    PPC64LE = False

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


def functime_foo(t=0.05, insert=False):
    if (insert):
        vmprof.insert_real_time_thread()
    sleep_retry_eintr(t)


def functime_bar(t=0.05, remove=False):
    if (remove):
        vmprof.remove_real_time_thread()
    sleep_retry_eintr(t)


def sleep_retry_eintr(t):
    start = time.time()
    remaining = t
    while remaining > 0:
        time.sleep(remaining)
        elapsed = time.time() - start
        remaining = t - elapsed


foo_full_name = "py:function_foo:%d:%s" % (function_foo.__code__.co_firstlineno,
                                           function_foo.__code__.co_filename)
bar_full_name = "py:function_bar:%d:%s" % (function_bar.__code__.co_firstlineno,
                                           function_bar.__code__.co_filename)

foo_time_name = "py:functime_foo:%d:%s" % (functime_foo.__code__.co_firstlineno,
                                           functime_foo.__code__.co_filename)
bar_time_name = "py:functime_bar:%d:%s" % (functime_bar.__code__.co_firstlineno,
                                           functime_bar.__code__.co_filename)

GZIP = False

@pytest.mark.skipif("sys.platform == 'win32'")
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
    stats = read_profile(tmpfile.name)
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
    before_profile = datetime.now()
    if sys.platform == 'win32':
        # it seems that the windows implementation of vmp_write_time_now
        # is borken, and cuts of some micro second precision.
        import time
        time.sleep(1)
    with prof.measure():
        function_foo()
    after_profile = datetime.now()
    stats = prof.get_stats()
    s = stats.start_time
    e = stats.end_time
    assert before_profile <= s and s <= after_profile
    assert s <= e
    assert e <= after_profile and s <= after_profile
    assert before_profile <= after_profile
    assert before_profile <= e

@pytest.mark.skipif("sys.platform == 'win32'")
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
        # assert len([x for x in names if str(x).startswith('jit:')]) > 0 # XXX re-enable this!
        assert len([x for x in names if x == foo_full_name]) == 1
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
        pytest.skip("not supported on pypy just yet")
    if sys.platform == 'win32':
        pytest.skip("skip in windows for now") # XXX
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
    if sys.platform == 'darwin':
        # on travis CI, these mac builds sometimes fail because of scheduling
        # issues. Having only 1 thread id is legit, which means that
        # only one thread has been interrupted. (Usually 2 are at least in this list)
        assert len(all_ids) >= 1
    else:
        assert len(all_ids) in (3, 4) # maybe 0

    #cur_id = list(all_ids)[0]
    #lgt1 = len([x[2] for x in stats.profiles if x[2] == cur_id])
    #total = len(stats.profiles)
    # between 33-10% and 33+10% is within one profile
    # this is too close of a call - thread scheduling can leave us
    # unlucky, especially on badly behaved systems
    # assert (0.23 * total) <= lgt1 <= (0.43 * total)
    assert len(finished) == 3

def test_memory_measurment():
    if not sys.platform.startswith('linux') or '__pypy__' in sys.builtin_module_names:
        pytest.skip("unsupported platform")
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

    prof.get_stats()


@pytest.mark.skipif("sys.platform == 'win32'")
def test_vmprof_real_time():
    prof = vmprof.Profiler()
    with prof.measure(real_time=True):
        functime_foo()
    stats = prof.get_stats()
    tprof = stats.top_profile()
    d = dict(tprof)
    assert d[foo_time_name] > 0


@pytest.mark.skipif("'__pypy__' in sys.builtin_module_names")
@pytest.mark.skipif("sys.platform == 'win32'")
@pytest.mark.parametrize("insert_foo,remove_bar", [
    (False, False),
    (False,  True),
    ( True, False),
    ( True,  True),
])
def test_vmprof_real_time_threaded(insert_foo, remove_bar):
    import threading
    prof = vmprof.Profiler()
    wait = 0.5
    thread = threading.Thread(target=functime_foo, args=[wait, insert_foo])
    with prof.measure(period=0.25, real_time=True):
        thread.start()
        functime_bar(wait, remove_bar)
        thread.join()
    stats = prof.get_stats()
    tprof = stats.top_profile()
    d = dict(tprof)
    assert insert_foo == (foo_time_name in d)
    assert remove_bar != (bar_time_name in d)


@pytest.mark.skipif("'__pypy__' in sys.builtin_module_names")
@pytest.mark.skipif("sys.platform == 'win32'")
@pytest.mark.parametrize("insert_foo,remove_bar", [
    (False, False),
    (False,  True),
    ( True, False),
    ( True,  True),
])
def test_insert_other_real_time_thread(insert_foo, remove_bar):
    import threading
    prof = vmprof.Profiler()
    wait = 0.5
    # This test is the same as above, except that we manually add/remove
    # all threads explicitly by id from the main thread.
    thread = threading.Thread(target=functime_foo, args=[wait, False])
    with prof.measure(period=0.25, real_time=True):
        thread.start()
        if insert_foo:
            vmprof.insert_real_time_thread(thread.ident)
        if remove_bar:
            vmprof.remove_real_time_thread(threading.current_thread().ident)
        functime_bar(wait, False)
        thread.join()
    stats = prof.get_stats()
    tprof = stats.top_profile()
    d = dict(tprof)
    assert insert_foo == (foo_time_name in d)
    assert remove_bar != (bar_time_name in d)


@pytest.mark.skipif("'__pypy__' in sys.builtin_module_names")
@pytest.mark.skipif("sys.platform == 'win32'")
@pytest.mark.skip("seems to crash")
def test_vmprof_real_time_many_threads():
    import threading
    prof = vmprof.Profiler()
    wait = 0.5

    # 12 is chosen to force multiple reallocs of the thread_size_step.
    n_threads = 12
    threads = []
    for _ in range(n_threads):
        thread = threading.Thread(target=functime_foo, args=[wait, True])
        threads.append(thread)

    with prof.measure(period=0.1, real_time=True):
        for thread in threads:
            thread.start()
        functime_bar(wait, False)
        for thread in threads:
            thread.join()
    stats = prof.get_stats()
    tprof = stats.top_profile()
    d = dict(tprof)
    assert foo_time_name in d
    assert bar_time_name in d


if GZIP:
    def test_gzip_problem():
        tmpfile = tempfile.NamedTemporaryFile(delete=False)
        vmprof.enable(tmpfile.fileno())
        vmprof._gzip_proc.kill()
        vmprof._gzip_proc.wait()
        # ensure that the gzip process really tries to write
        # to the gzip proc that was killed
        function_foo()
        with pytest.raises(Exception) as exc_info:
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
            thread_id = read_addr(fileobj)
        else:
            thread_id = 0
        if status.profile_memory:
            mem_in_kb = read_addr(fileobj)
        else:
            mem_in_kb = 0
        trace.reverse()
        status.profiles.append((trace, 1, thread_id, mem_in_kb))
    elif marker == MARKER_VIRTUAL_IP or marker == MARKER_NATIVE_SYMBOLS:
        unique_id = read_addr(fileobj)
        name = read_string(fileobj)
        if PY3K:
            name = name.decode()
        status.virtual_ips[unique_id] = name
    elif marker == MARKER_META:
        read_string(fileobj)
        read_string(fileobj)
        # TODO save the for the tests?
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


@pytest.mark.skipif("IS_PYPY")
def test_line_profiling():
    tmpfile = tempfile.NamedTemporaryFile(delete=False)
    vmprof.enable(tmpfile.fileno(), lines=True, native=False)  # enable lines profiling
    function_foo()
    vmprof.disable()
    tmpfile.close()

    def walk(tree):
        assert len(tree.lines) >= len(tree.children)

        for v in six.itervalues(tree.children):
                walk(v)

    stats = read_profile(tmpfile.name)
    walk(stats.get_tree())

def test_vmprof_show():
    tmpfile = tempfile.NamedTemporaryFile(delete=False)
    vmprof.enable(tmpfile.fileno())
    function_bar()
    vmprof.disable()
    tmpfile.close()

    pp = PrettyPrinter()
    pp.show(tmpfile.name)

@pytest.mark.skipif("sys.platform == 'win32' or sys.platform == 'darwin'")
class TestNative(object):
    def setup_class(cls):
        ffi = FFI()
        ffi.cdef("""
        void native_gzipgzipgzip(void);
        """)
        source = """
        #include "zlib.h"
        unsigned char input[100];
        unsigned char output[100];
        void native_gzipgzipgzip(void) {
            z_stream defstream;
            defstream.zalloc = Z_NULL;
            defstream.zfree = Z_NULL;
            defstream.opaque = Z_NULL;
            defstream.next_in = input; // input char array
            defstream.next_out = output; // output char array

            deflateInit(&defstream, Z_DEFAULT_COMPRESSION);
            int i = 0;
            while (i < 10000) {
                defstream.avail_in = 100;
                defstream.avail_out = 100;
                deflate(&defstream, Z_FINISH);
                i++;
            }
            deflateEnd(&defstream);
        }
        """
        libs = []
        if sys.platform.startswith('linux'):
            libs.append('z')
        # trick: compile with _CFFI_USE_EMBEDDING=1 which will not define Py_LIMITED_API
        ffi.set_source("vmprof.test._test_native_gzip", source, include_dirs=['src'],
                       define_macros=[('_CFFI_USE_EMBEDDING',1),('_PY_TEST',1)], libraries=libs,
                       extra_compile_args=['-Werror', '-g', '-O0'])

        ffi.compile(verbose=True)
        from vmprof.test import _test_native_gzip as clib
        cls.lib = clib.lib
        cls.ffi = clib.ffi

    @pytest.mark.skipif("IS_PYPY")
    def test_gzip_call(self):
        p = vmprof.Profiler()
        with p.measure(native=True):
            for i in range(1000):
                self.lib.native_gzipgzipgzip();
        stats = p.get_stats()
        top = stats.get_top(stats.profiles)
        pp = PrettyPrinter()
        pp._print_tree(stats.get_tree())
        def walk(parent):
            if parent is None or len(parent.children) == 0:
                return False

            if 'n:native_gzipgzipgzip:' in parent.name:
                return True

            for child in parent.children.values():
                if 'n:native_gzipgzipgzip:' in child.name:
                    p = float(child.count) / parent.count
                    assert p >= 0.1 # usually bigger than 0.4
                    return True
                else:
                    found = walk(child)
                    if found:
                        return True

        parent = stats.get_tree()
        assert walk(parent)

    def test_is_enabled(self):
        assert vmprof.is_enabled() == False
        tmpfile = tempfile.NamedTemporaryFile(delete=False)
        vmprof.enable(tmpfile.fileno())
        assert vmprof.is_enabled() == True
        vmprof.disable()
        assert vmprof.is_enabled() == False

    def test_get_profile_path(self):
        assert vmprof.get_profile_path() == None
        tmpfile = tempfile.NamedTemporaryFile(delete=False)
        vmprof.enable(tmpfile.fileno())
        if not vmprof.get_profile_path() == tmpfile.name:
            with open(vmprof.get_profile_path(), 'rb') as fd1:
                with open(tmpfile.name, "rb") as fd2:
                    assert fd1.read() == fd2.read()
        vmprof.disable()
        assert vmprof.get_profile_path() == None

    def test_get_runtime(self):
        p = vmprof.Profiler()
        with p.measure():
            time.sleep(2.5)
        stats = p.get_stats()
        micros = stats.get_runtime_in_microseconds()
        ts = stats.end_time - stats.start_time
        print(ts)
        assert 2500000 <= micros <= 3000000

if __name__ == '__main__':
    test_line_profiling()
