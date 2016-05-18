from __future__ import print_function
import re
import struct
import array
import subprocess
import sys
from itertools import islice
from six.moves import zip as izip

PY3 = sys.version_info[0] >= 3

WORD_SIZE = struct.calcsize('L')

def read_word(fileobj):
    """Read a single long from `fileobj`."""
    b = fileobj.read(WORD_SIZE)
    r = int(struct.unpack('l', b)[0])
    return r

def read_words(fileobj, nwords):
    """Read `nwords` longs from `fileobj`."""
    try:
        # Do we have real file object?
        fileobj.fileno()
    except (AttributeError, NotImplementedError):
        # If not, use slow version
        b = fileobj.read(WORD_SIZE * nwords)
        r = [int(w) for w in struct.unpack('l' * nwords, b)]
    else:
        r = array.array('l')
        r.fromfile(fileobj, nwords)
    return r

def read_string(fileobj):
    lgt = int(struct.unpack('l', fileobj.read(WORD_SIZE))[0])
    return fileobj.read(lgt)

def read_trace(fileobj, depth, version):
    if version == VERSION_TAG:
        assert depth & 1 == 0
        depth = depth // 2
        pcs_and_kinds = read_words(fileobj, depth * 2)
        kinds = islice(pcs_and_kinds, 0, None, 2)
        pcs   = islice(pcs_and_kinds, 1, None, 2)
        return [wrap_kind(kind, pc) for kind, pc in izip(kinds, pcs)]
    else:
        return read_words(fileobj, depth)

MARKER_STACKTRACE = b'\x01'
MARKER_VIRTUAL_IP = b'\x02'
MARKER_TRAILER = b'\x03'
MARKER_INTERP_NAME = b'\x04'
MARKER_HEADER = b'\x05'

VERSION_BASE = 0
VERSION_THREAD_ID = 1
VERSION_TAG = 2
VERSION_MEMORY = 3

VMPROF_CODE_TAG = 1
VMPROF_BLACKHOLE_TAG = 2
VMPROF_JITTED_TAG = 3
VMPROF_JITTING_TAG = 4
VMPROF_GC_TAG = 5
VMPROF_ASSEMBLER_TAG = 6

class AssemblerCode(int):
    pass

class JittedCode(int):
    pass

def wrap_kind(kind, pc):
    cls = {
        VMPROF_ASSEMBLER_TAG: AssemblerCode,
        VMPROF_JITTED_TAG: JittedCode,
        VMPROF_CODE_TAG: lambda x: x,
    }[kind]
    return cls(pc)

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

class ReaderStatus(object):
    def __init__(self, interp_name, period, version, previous_virtual_ips=None):
        if previous_virtual_ips is not None:
            self.virtual_ips = previous_virtual_ips
        else:
            self.virtual_ips = {}
        self.profiles = []
        self.interp_name = interp_name
        self.period = period
        self.version = version

class FileReadError(Exception):
    pass

def assert_error(condition, error="malformed file"):
    if not condition:
        raise FileReadError(error)

def read_header(fileobj, buffer_so_far=None):
    fileobj = FileObjWrapper(fileobj, buffer_so_far)
    assert_error(read_word(fileobj) == 0)
    assert_error(read_word(fileobj) == 3)
    assert_error(read_word(fileobj) == 0)
    period = read_word(fileobj)
    assert_error(read_word(fileobj) == 0)
    marker = fileobj.read(1)
    assert_error(marker == MARKER_HEADER, "expected header")
    version, = struct.unpack("!h", fileobj.read(2))
    lgt = ord(fileobj.read(1))
    interp_name = fileobj.read(lgt)
    if PY3:
        interp_name = interp_name.decode()
    return ReaderStatus(interp_name, period, version)

def read_one_marker(fileobj, status, buffer_so_far=None):
    fileobj = FileObjWrapper(fileobj, buffer_so_far)
    marker = fileobj.read(1)
    if marker == MARKER_STACKTRACE:
        count = read_word(fileobj)
        # for now
        assert count == 1
        depth = read_word(fileobj)
        assert depth <= 2**16, 'stack strace depth too high'
        trace = read_trace(fileobj, depth, status.version)
        if status.version >= VERSION_THREAD_ID:
            thread_id, = struct.unpack('l', fileobj.read(WORD_SIZE))
        else:
            thread_id = 0
        if status.version == VERSION_MEMORY:
            mem_in_kb, = struct.unpack('l', fileobj.read(WORD_SIZE))
        else:
            mem_in_kb = 0
        trace.reverse()
        status.profiles.append((trace, 1, thread_id, mem_in_kb))
    elif marker == MARKER_VIRTUAL_IP:
        unique_id = read_word(fileobj)
        name = read_string(fileobj)
        if PY3:
            name = name.decode()
        status.virtual_ips[unique_id] = name
    elif marker == MARKER_TRAILER:
        return True # finished
    else:
        raise FileReadError("unexpected marker: %d" % ord(marker))
    return False

def read_prof_bit_by_bit(fileobj):
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

def read_prof(fileobj, virtual_ips_only=False): #
    assert read_word(fileobj) == 0 # header count
    assert read_word(fileobj) == 3 # header size
    assert read_word(fileobj) == 0
    period = read_word(fileobj)
    assert read_word(fileobj) == 0

    virtual_ips = []
    profiles = []
    interp_name = None
    version = 0

    # Optimization: Save a lot of memory by explicitly caching IP integers.
    # This works around the fact that integers, albeit equivalent, are usually
    # allocated to different actual memory objects by Python.
    integer_cache = {}

    while True:
        marker = fileobj.read(1)
        if marker == MARKER_HEADER:
            assert not version, "multiple headers"
            version, = struct.unpack("!h", fileobj.read(2))
            lgt = ord(fileobj.read(1))
            interp_name = fileobj.read(lgt)
            if PY3:
                interp_name = interp_name.decode()
        elif marker == MARKER_STACKTRACE:
            count = read_word(fileobj)
            # for now
            assert count == 1
            depth = read_word(fileobj)
            assert depth <= 2**16, 'stack strace depth too high'
            if virtual_ips_only:
                fileobj.read(WORD_SIZE * depth)
                trace = []
            else:
                trace = read_trace(fileobj, depth, version)
                trace = [integer_cache.setdefault(x, x) for x in trace]
            if version >= VERSION_THREAD_ID:
                thread_id, = struct.unpack('l', fileobj.read(WORD_SIZE))
            else:
                thread_id = 0
            if version == VERSION_MEMORY:
                mem_in_kb, = struct.unpack('l', fileobj.read(WORD_SIZE))
            else:
                mem_in_kb = 0
            trace.reverse()
            profiles.append((trace, 1, thread_id, mem_in_kb))
        elif marker == MARKER_INTERP_NAME:
            assert not version, "multiple headers"
            assert not interp_name, "Dual interpreter name header"
            lgt = ord(fileobj.read(1))
            interp_name = fileobj.read(lgt)
            if PY3:
                interp_name = interp_name.decode()
        elif marker == MARKER_VIRTUAL_IP:
            unique_id = read_word(fileobj)
            name = read_string(fileobj)
            if PY3:
                name = name.decode()
            virtual_ips.append((unique_id, name))
        elif marker == MARKER_TRAILER:
            #if not virtual_ips_only:
            #    symmap = read_ranges(fileobj.read())
            break
        else:
            assert not marker, (fileobj.tell(), repr(marker))
            break
    virtual_ips.sort() # I think it's sorted, but who knows
    if virtual_ips_only:
        return virtual_ips
    return period, profiles, virtual_ips, interp_name
