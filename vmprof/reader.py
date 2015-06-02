
import re
import struct
import subprocess
import sys


PY3 = sys.version_info[0] >= 3


class LibraryData(object):
    def __init__(self, name, start, end, is_virtual=False, symbols=None):
        self.name = name
        self.start = start
        self.end = end
        self.is_virtual = is_virtual
        if symbols is None:
            symbols = []
        self.symbols = symbols

    def read_object_data(self, start_addr=0, reader=None):
        if self.is_virtual:
            return
        self.symbols = read_object(reader, self.name, start_addr)
        return self.symbols

    def get_symbols_from(self, cached_lib):
        symbols = []
        for (addr, name) in cached_lib.symbols:
            symbols.append((addr - cached_lib.start + self.start, name))
        self.symbols = symbols

    def __repr__(self):
        return '<Library data for %s, ranges %x-%x>' % (self.name, self.start,
                                                        self.end)


def read_object(reader, name, lib_start_addr, repeat=True):
    if PY3 and isinstance(name, bytes):
        name = name.decode('utf-8')
    if reader is None:
        try:
            out = subprocess.check_output('nm -n "%s" 2>/dev/null' % name, shell=True)
            if PY3:
                out = out.decode('latin1')
        except subprocess.CalledProcessError:
            out = ''
    else:
        out = reader(name)
    lines = out.splitlines()
    symbols = []
    for line in lines:
        parts = line.split()
        if len(parts) != 3:
            continue
        start_addr, tp, name = parts
        if tp in ('t', 'T') and not name.startswith('__gcmap'):
            start_addr = int(start_addr, 16) + lib_start_addr
            symbols.append((start_addr, name))
    symbols.sort()
    if repeat and not symbols:
        return read_object(reader, '/usr/lib/debug' + name, lib_start_addr,
                           False)
    return symbols


def read_ranges(data):
    if PY3 and isinstance(data, bytes):
        data = data.decode('latin1')
    ranges = []
    for line in data.splitlines():
        parts = re.split("\s+", line)
        name = parts[-1]
        start, end = parts[0].split('-')
        start = int('0x' + start, 16)
        end = int('0x' + end, 16)
        if name: # don't map anonymous memory, JIT code will be there
            ranges.append(LibraryData(name, start, end))
    return ranges

def read_word(fileobj):
    b = fileobj.read(8)
    r = int(struct.unpack('Q', b)[0])
    return r

def read_string(fileobj):
    lgt = int(struct.unpack('Q', fileobj.read(8))[0])
    return fileobj.read(lgt)

MARKER_STACKTRACE = b'\x01'
MARKER_VIRTUAL_IP = b'\x02'
MARKER_TRAILER = b'\x03'
MARKER_INTERP_NAME = b'\x04'

def read_prof(fileobj, virtual_ips_only=False): #
    assert read_word(fileobj) == 0 # header count
    assert read_word(fileobj) == 3 # header size
    assert read_word(fileobj) == 0 # version?
    period = read_word(fileobj)
    assert read_word(fileobj) == 0

    virtual_ips = []
    profiles = []
    interp_name = None

    while True:
        marker = fileobj.read(1)
        if marker == MARKER_STACKTRACE:
            count = read_word(fileobj)
            # for now
            assert count == 1
            depth = read_word(fileobj)
            assert depth <= 2**16, 'stack strace depth too high'
            trace = []
            if virtual_ips_only:
                fileobj.read(8 * depth)
            else:
                for j in range(depth):
                    pc = read_word(fileobj)
                    if j > 0 and pc > 0:
                        pc -= 1
                    trace.append(pc)
                profiles.append((trace, 1))
        elif marker == MARKER_INTERP_NAME:
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
            if not virtual_ips_only:
                symmap = read_ranges(fileobj.read())
            break
        else:
            assert not marker
            symmap = []
            break
    virtual_ips.sort() # I think it's sorted, but who knows
    if virtual_ips_only:
        return virtual_ips
    return period, profiles, virtual_ips, symmap, interp_name
