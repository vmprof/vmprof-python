from __future__ import print_function
import os
import struct
import sys
from six.moves import xrange
import io
import gzip
import datetime

PY3  = sys.version_info[0] >= 3


MARKER_STACKTRACE = b'\x01'
MARKER_VIRTUAL_IP = b'\x02'
MARKER_TRAILER = b'\x03'
MARKER_INTERP_NAME = b'\x04' # do not use! deprecated
MARKER_HEADER = b'\x05'
MARKER_TIME_N_ZONE = b'\x06'
MARKER_META = b'\x07'
MARKER_NATIVE_SYMBOLS = b'\x08'


VERSION_BASE = 0
VERSION_THREAD_ID = 1
VERSION_TAG = 2
VERSION_MEMORY = 3
VERSION_MODE_AWARE = 4
VERSION_DURATION = 5
VERSION_TIMESTAMP = 6

PROFILE_MEMORY = 1
PROFILE_LINES = 2
PROFILE_NATIVE = 4
PROFILE_RPYTHON = 8

VMPROF_CODE_TAG = 1
VMPROF_BLACKHOLE_TAG = 2
VMPROF_JITTED_TAG = 3
VMPROF_JITTING_TAG = 4
VMPROF_GC_TAG = 5
VMPROF_ASSEMBLER_TAG = 6
VMPROF_NATIVE_TAG = 7


class AssemblerCode(int):
    pass

class JittedCode(int):
    pass

class NativeCode(int):
    pass

def wrap_kind(kind, pc):
    if kind == VMPROF_ASSEMBLER_TAG:
        return AssemblerCode(pc)
    elif kind == VMPROF_JITTED_TAG:
        return JittedCode(pc)
    elif kind == VMPROF_NATIVE_TAG:
        return NativeCode(pc)
    assert kind == VMPROF_CODE_TAG
    return pc

def gunzip(fileobj):
    is_gzipped = fileobj.read(2) == b'\037\213'
    fileobj.seek(-2, os.SEEK_CUR)
    if is_gzipped:
        fileobj = io.BufferedReader(gzip.GzipFile(fileobj=fileobj))
    return fileobj

class ReaderStatus(object):
    def __init__(self, interp_name, period, version, previous_virtual_ips=None,
                 profile_memory=False, profile_lines=False):
        if previous_virtual_ips is not None:
            self.virtual_ips = previous_virtual_ips
        else:
            self.virtual_ips = {}
        self.profiles = []
        self.interp_name = interp_name
        self.period = period
        self.version = version
        self.profile_memory = profile_memory
        self.profile_lines = profile_lines

class FileReadError(Exception):
    pass

def assert_error(condition, error="malformed file"):
    if not condition:
        raise FileReadError(error)

class LogReader(object):
    # NOTE be sure to carry along changes in src/symboltable.c for
    # native symbol resolution if something changes in this function
    def __init__(self, fileobj, state):
        self.fileobj = fileobj
        self.state = state
        self.word_size = None
        self.addr_size = None
        self.setup()

    def setup(self):
        pass

    def detect_file_sizes(self):
        self.fileobj.seek(0, os.SEEK_SET)
        firstbytes = self.read(8)
        three = '\x03' if not PY3 else 3
        little = None
        if firstbytes[4] == three:
            little = True
            self.setup_once(little_endian=little, word_size=4, addr_size=4)
        elif firstbytes[7] == three:
            little = False
            self.setup_once(little_endian=little, word_size=4, addr_size=4)
        else:
            firstbytes = self.read(8)
            if firstbytes[0] == three:
                little = True
                self.setup_once(little_endian=little, word_size=8, addr_size=8)
            elif firstbytes[7] == three:
                little = False
                self.setup_once(little_endian=little, word_size=8, addr_size=8)
            else:
                raise NotImplementedError("could not determine word and addr size")

        # determine if it is windows 64 bit
        # even though it migt be a 64bit log, teh addr_size is now 4
        if self.addr_size == 4:
            # read further
            self.fileobj.seek(0, os.SEEK_SET)
            self.read(4*4)
            windows64 = self.read_word() == 1
            if windows64:
                self.setup_once(little_endian=little, word_size=4, addr_size=8)

        self.fileobj.seek(0, os.SEEK_SET)

    def setup_once(self, little_endian, word_size, addr_size):
        self.little_endian = little_endian
        assert self.little_endian, "big endian profile are not supported"
        self.word_size = word_size
        self.addr_size = addr_size

    def read_static_header(self):
        r = self.read_word()
        assert r == 0 # header count
        r = self.read_word()
        assert r == 3 # header size
        r = self.read_word()
        assert r == 0
        self.state.period = self.read_word()
        r = self.read_word()
        assert r in (0, 1)

    def read_header(self):
        s = self.state
        fileobj = self.fileobj
        s.version, = struct.unpack("!h", fileobj.read(2))

        if s.version >= VERSION_MODE_AWARE:
            mode = ord(fileobj.read(1))
            s.profile_memory = (mode & PROFILE_MEMORY) != 0
            s.profile_lines = (mode & PROFILE_LINES) != 0
            s.profile_rpython = (mode & PROFILE_RPYTHON) != 0
        else:
            s.profile_memory = s.version == VERSION_MEMORY
            s.profile_lines = False
            s.profile_rpython = False

        lgt = ord(fileobj.read(1))
        s.interp_name = fileobj.read(lgt)
        if s.interp_name == b'pypy':
            s.profile_rpython = True
        if PY3:
            s.interp_name = s.interp_name.decode()

    def read_addr(self):
        if self.addr_size == 8:
            return struct.unpack('<q', self.fileobj.read(8))[0]
        elif self.addr_size == 4:
            return struct.unpack('<l', self.fileobj.read(4))[0]
        else:
            raise NotImplementedError("did not implement size %d" % self.size)

    def read_word(self):
        if self.word_size == 8:
            return struct.unpack('<q', self.fileobj.read(8))[0]
        elif self.word_size == 4:
            return struct.unpack('<l', self.fileobj.read(4))[0]
        else:
            raise NotImplementedError("did not implement size %d" % self.size)

    def read(self, count):
        return self.fileobj.read(count)

    def read_string(self):
        cnt = self.read_word()
        bytes = self.read(cnt)
        if PY3:
            return bytes.decode('utf-8')
        return bytes

    def read_trace(self, depth):
        if self.state.profile_rpython:
            assert depth & 1 == 0
            depth = depth // 2
            kinds_and_pcs = self.read_addresses(depth * 2)
            # kinds_and_pcs is a list of [kind1, pc1, kind2, pc2, ...]
            return [wrap_kind(kinds_and_pcs[i], kinds_and_pcs[i+1])
                    for i in xrange(0, len(kinds_and_pcs), 2)]
        else:
            trace = self.read_addresses(depth)

            if self.state.profile_lines:
                for i in xrange(0, len(trace), 2):
                    # In the line profiling mode even items in the trace are line numbers.
                    # Every line number corresponds to the following frame, represented by an address.
                    trace[i] = -trace[i]
            return trace

    def read_addresses(self, count):
        addrs = []
        for i in range(count):
            addr = self.read_addr()
            if addr > 0 and addr & 1 == 1:
                addrs.append(NativeCode(addr))
            else:
                addrs.append(addr)
        return addrs

    def read_s64(self):
        return struct.unpack('q', self.fileobj.read(8))[0]

    def read_time_and_zone(self):
        return datetime.datetime.fromtimestamp(
            self.read_timeval()/10.0**6, self.read_timezone())

    def read_timeval(self):
        tv_sec = self.read_s64()
        tv_usec = self.read_s64()
        return tv_sec * 10**6 + tv_usec

    def read_timezone(self):
        timezone = self.read(8).strip(b'\x00')
        # we should use pytz and parse iso8601 if we really support time zones
        return None

    def read_all(self):
        s = self.state
        fileobj = self.fileobj

        self.detect_file_sizes()
        self.read_static_header()

        while True:
            marker = fileobj.read(1)
            if marker == MARKER_HEADER:
                assert not s.version, "multiple headers"
                self.read_header()
            elif marker == MARKER_META:
                key = self.read_string()
                value = self.read_string()
                assert not key in s.meta, "key duplication, %s already present" % (key,)
                s.meta[key] = value
            elif marker == MARKER_TIME_N_ZONE:
                s.start_time = self.read_time_and_zone()
            elif marker == MARKER_STACKTRACE:
                count = self.read_word()
                # for now
                assert count == 1
                depth = self.read_word()
                assert depth <= 2**16, 'stack strace depth too high'
                trace = self.read_trace(depth)
                thread_id = 0
                mem_in_kb = 0
                if s.version >= VERSION_THREAD_ID:
                    thread_id = self.read_addr()
                if s.profile_memory:
                    mem_in_kb = self.read_addr()
                trace.reverse()
                self.add_trace(trace, 1, thread_id, mem_in_kb)
            elif marker == MARKER_VIRTUAL_IP or marker == MARKER_NATIVE_SYMBOLS:
                unique_id = self.read_addr()
                name = self.read_string()
                self.add_virtual_ip(marker, unique_id, name)
            elif marker == MARKER_TRAILER:
                #if not virtual_ips_only:
                #    symmap = read_ranges(fileobj.read())
                if s.version >= VERSION_DURATION:
                    s.end_time = self.read_time_and_zone()
                break
            else:
                assert not marker, (fileobj.tell(), repr(marker))
                break

        self.finished_reading_profile()

    def finished_reading_profile(self):
        self.state.virtual_ips.sort() # I think it's sorted, but who knows

    def add_virtual_ip(self, marker, unique_id, name):
        self.state.virtual_ips.append((unique_id, name))

    def add_trace(self, trace, trace_count, thread_id, mem_in_kb):
        self.state.profiles.append((trace, trace_count, thread_id, mem_in_kb))

class LogReaderDumpNative(LogReader):
    def setup(self):
        self.dedup = set()

    def finished_reading_profile(self):
        import _vmprof
        if not hasattr(_vmprof, 'resolve_addr'):
            # windows does not implement that!
            return

        resolve_addr = _vmprof.resolve_addr
        from _vmprof import resolve_addr
        LogReader.finished_reading_profile(self)
        if len(self.dedup) == 0:
            return
        self.fileobj.seek(0, os.SEEK_END)
        # must match '<lang>:<name>:<line>:<file>'
        # 'n' has been chosen as lang here, because the symbol
        # can be generated from several languages (e.g. C, C++, ...)

        for addr in self.dedup:
            bytelist = [b"\x08"]
            result = resolve_addr(addr)
            if result is None:
                name, lineno, srcfile = None, 0, None
            else:
                name, lineno, srcfile = result
            if not name:
                name = "<native symbol 0x%x>" % addr
            if not srcfile:
                srcfile = "-"
            string = "n:%s:%d:%s" % (name, lineno, srcfile)
            bytestring = string.encode('utf-8')
            bytelist.append(struct.pack("P", addr))
            bytelist.append(struct.pack("l", len(bytestring)))
            bytelist.append(bytestring)
            self.fileobj.write(b"".join(bytelist))

    def add_virtual_ip(self, marker, unique_id, name):
        pass # do nothing, no need to save this data

    def add_trace(self, trace, trace_count, thread_id, mem_in_kb):
        for addr in trace:
            if addr not in self.dedup:
                self.dedup.add(addr)

class ReaderState(object):
    pass

class LogReaderState(ReaderState):
    def __init__(self):
        self.virtual_ips = []
        self.profiles = []
        self.interp_name = None
        self.start_time = None
        self.end_time = None
        self.version = 0
        self.profile_memory = False
        self.profile_lines = False
        self.meta = {}
        self.little_endian = True
        self.period = 0

def _read_prof(fileobj, virtual_ips_only=False):
    fileobj = gunzip(fileobj)

    state = LogReaderState()
    reader = LogReader(fileobj, state)
    reader.read_all()

    if virtual_ips_only:
        return state.virtual_ips
    return state

class FdWrapper(object):
    """ This wrapper behaves like a file object. Could not find
        an stdlib API function that creates such an object without
        closing it.
    """
    def __init__(self, fd):
        self.fd = fd

    def write(self, bytes):
        return os.write(self.fd, bytes)

    def read(self, n):
        return os.read(self.fd, n)

    def seek(self, pos, how):
        return os.lseek(self.fd, pos, how)

    def tell(self):
        return os.lseek(self.fd, 0, os.SEEK_CUR)
