from __future__ import print_function
import re
import struct
import subprocess
import sys


PY3 = sys.version_info[0] >= 3

WORD_SIZE = struct.calcsize('L')

def read_word(fileobj):
    b = fileobj.read(WORD_SIZE)
    r = int(struct.unpack('l', b)[0])
    return r

def read_string(fileobj):
    lgt = int(struct.unpack('l', fileobj.read(WORD_SIZE))[0])
    return fileobj.read(lgt)

MARKER_STACKTRACE = b'\x01'
MARKER_VIRTUAL_IP = b'\x02'
MARKER_TRAILER = b'\x03'
MARKER_INTERP_NAME = b'\x04'
MARKER_HEADER = b'\x05'

VERSION_BASE = 0
VERSION_THREAD_ID = 1
VERSION_TAG = 2

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

def read_prof(fileobj, virtual_ips_only=False): #
    assert read_word(fileobj) == 0 # header count
    assert read_word(fileobj) == 3 # header size
    assert read_word(fileobj) == 0
    period = read_word(fileobj)
    assert read_word(fileobj) == 0

    virtual_ips = []
    profiles = []
    all = 0
    interp_name = None
    version = 0

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
            trace = []
            if virtual_ips_only:
                fileobj.read(WORD_SIZE * depth)
            else:
                if version >= VERSION_TAG:
                    assert depth & 1 == 0
                    depth = depth // 2
                for j in range(depth):
                    if version >= VERSION_TAG:
                        kind = read_word(fileobj)
                    else:
                        kind = VMPROF_CODE_TAG
                    pc = read_word(fileobj)
                    trace.append(wrap_kind(kind, pc))
            if version >= VERSION_THREAD_ID:
                thread_id, = struct.unpack('l', fileobj.read(WORD_SIZE))
            else:
                thread_id = 0
            profiles.append((trace, 1, thread_id))
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
            all += len(name)
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
