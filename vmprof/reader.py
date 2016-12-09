from __future__ import print_function
import re
import os
import struct
import subprocess
import sys
from six.moves import xrange
import io
import gzip
import datetime

from vmshare.binary import read_word, read_string, read_words, read_timeval, read_timezone

PY3  = sys.version_info[0] >= 3
WORD_SIZE = struct.calcsize('L')

def read_trace(fileobj, depth, version, profile_lines=False):
    if version == VERSION_TAG:
        assert depth & 1 == 0
        depth = depth // 2
        kinds_and_pcs = read_words(fileobj, depth * 2)
        # kinds_and_pcs is a list of [kind1, pc1, kind2, pc2, ...]
        return [wrap_kind(kinds_and_pcs[i], kinds_and_pcs[i+1])
                for i in xrange(0, len(kinds_and_pcs), 2)]
    else:
        trace = read_words(fileobj, depth)

        if profile_lines:
            for i in xrange(0, len(trace), 2):
                # In the line profiling mode even items in the trace are line numbers.
                # Every line number corresponds to the following frame, represented by an address.
                trace[i] = -trace[i]
        return trace


MARKER_STACKTRACE = b'\x01'
MARKER_VIRTUAL_IP = b'\x02'
MARKER_TRAILER = b'\x03'
MARKER_INTERP_NAME = b'\x04'
MARKER_HEADER = b'\x05'
MARKER_TIME_N_ZONE = b'\x06'
MARKER_META = b'\x07'


VERSION_BASE = 0
VERSION_THREAD_ID = 1
VERSION_TAG = 2
VERSION_MEMORY = 3
VERSION_MODE_AWARE = 4
VERSION_DURATION = 5

PROFILE_MEMORY = 1
PROFILE_LINES = 2

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
    if kind == VMPROF_ASSEMBLER_TAG:
        return AssemblerCode(pc)
    elif kind == VMPROF_JITTED_TAG:
        return JittedCode(pc)
    assert kind == VMPROF_CODE_TAG
    return pc


def gunzip(fileobj):
    is_gzipped = fileobj.read(2) == b'\037\213'
    fileobj.seek(-2, os.SEEK_CUR)
    if is_gzipped:
        fileobj = io.BufferedReader(gzip.GzipFile(fileobj=fileobj))
    return fileobj


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

def _read_header(fileobj, consume_mark=True):
    if consume_mark:
        marker = fileobj.read(1)
        assert_error(marker == MARKER_HEADER, "expected header")
    version, = struct.unpack("!h", fileobj.read(2))

    if version >= VERSION_MODE_AWARE:
        mode = ord(fileobj.read(1))
        profile_memory = (mode & PROFILE_MEMORY) != 0
        profile_lines = (mode & PROFILE_LINES) != 0
    else:
        profile_memory = version == VERSION_MEMORY
        profile_lines = False

    lgt = ord(fileobj.read(1))
    interp_name = fileobj.read(lgt)
    if PY3:
        interp_name = interp_name.decode()
    return interp_name, version, profile_memory, profile_lines

def read_time_and_zone(fileobj):
    return datetime.datetime.fromtimestamp(
        read_timeval(fileobj)/10.0**6,
        read_timezone(fileobj)
    )

def read_prof(fileobj, virtual_ips_only=False):
    fileobj = gunzip(fileobj)

    assert read_word(fileobj) == 0 # header count
    assert read_word(fileobj) == 3 # header size
    assert read_word(fileobj) == 0
    period = read_word(fileobj)
    assert read_word(fileobj) == 0

    virtual_ips = []
    profiles = []
    interp_name = None
    start_time = None
    end_time = None
    version = 0
    profile_memory = False
    profile_lines = False
    meta = {}

    while True:
        marker = fileobj.read(1)
        if marker == MARKER_HEADER:
            assert not version, "multiple headers"
            interp_name, version, profile_memory, profile_lines = \
                    _read_header(fileobj, consume_mark=False)
        elif marker == MARKER_META:
            key = read_string(fileobj)
            value = read_string(fileobj)
            assert not key in meta, "key duplication, %s already present" % (key,)
            meta[key] = value
        elif marker == MARKER_TIME_N_ZONE:
            start_time = read_time_and_zone(fileobj)
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
                trace = read_trace(fileobj, depth, version, profile_lines)
            if version >= VERSION_THREAD_ID:
                thread_id, = struct.unpack('l', fileobj.read(WORD_SIZE))
            else:
                thread_id = 0
            if profile_memory:
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
            if version >= VERSION_DURATION:
                end_time = read_time_and_zone(fileobj)
            break
        else:
            assert not marker, (fileobj.tell(), repr(marker))
            break
    virtual_ips.sort() # I think it's sorted, but who knows
    if virtual_ips_only:
        return virtual_ips
    return period, profiles, virtual_ips, interp_name, meta, start_time, end_time
