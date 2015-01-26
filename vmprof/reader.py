
import re
import commands
import struct


class LibraryData(object):
    def __init__(self, name, start, end, is_virtual=False, symbols=None):
        self.name = name
        self.start = start
        self.end = end
        self.is_virtual = is_virtual
        self.symbols = symbols

    def read_object_data(self, start_addr=0, reader=None):
        if self.is_virtual:
            return
        self.symbols = read_object(reader, self.name, start_addr)
        return self.symbols

    def __repr__(self):
        return '<Library data for %s, ranges %x-%x>' % (self.name, self.start,
                                                        self.end)


def read_object(reader, name, lib_start_addr, repeat=True):
    if reader is None:
        out = commands.getoutput('nm -n "%s"' % name)
    else:
        out = reader(name)
    lines = out.splitlines()
    symbols = []
    for line in lines:
        parts = line.split()
        if len(parts) != 3:
            continue
        start_addr, _, name = parts
        start_addr = int(start_addr, 16) + lib_start_addr
        symbols.append((start_addr, name))
    symbols.sort()
    if repeat and not symbols:
        return read_object(reader, '/usr/lib/debug' + name, lib_start_addr,
                           False)
    return symbols


def read_ranges(data):
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
    return struct.unpack('Q', fileobj.read(8))[0]

def read_string(fileobj):
    len = struct.unpack('Q', fileobj.read(8))[0]
    return fileobj.read(len)

MARKER_STACKTRACE = '\x01'
MARKER_VIRTUAL_IP = '\x02'
MARKER_TRAILER = '\x03'

def read_prof(fileobj): #
    # f = open(fname, 'rb') # to jest czytane do konca
    assert read_word(fileobj) == 0 # header count
    assert read_word(fileobj) == 3 # header size
    assert read_word(fileobj) == 0 # version?
    period = read_word(fileobj)
    assert read_word(fileobj) == 0

    virtual_ips = {}
    profiles = []

    while True:
        marker = fileobj.read(1)
        if marker == MARKER_STACKTRACE:
            count = read_word(fileobj)
            # for now
            assert count == 1
            depth = read_word(fileobj)
            assert depth <= 2**16, 'stack strace depth too high'
            trace = []
            for j in range(depth):
                pc = read_word(fileobj)
                if j > 0 and pc > 0:
                    pc -= 1
                trace.append(pc)
            profiles.append(trace)
        elif marker == MARKER_VIRTUAL_IP:
            unique_id = read_word(fileobj)
            name = read_string(fileobj)
            virtual_ips[unique_id] = name
        elif marker == MARKER_TRAILER:
            symmap = read_ranges(fileobj.read())
            return period, profiles, virtual_ips, symmap
