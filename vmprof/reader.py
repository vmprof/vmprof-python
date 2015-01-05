
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

    def read_object_data(self, reader=None):
        if self.is_virtual:
            return
        self.symbols = read_object(reader, self.name)
        return self.symbols


def read_object(reader, name, repeat=True):
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
        start_addr = int(start_addr, 16)
        symbols.append((start_addr, name))
    symbols.sort()
    if repeat and not symbols:
        return read_object(reader, '/usr/lib/debug' + name, False)
    return symbols


def read_ranges(data):
    ranges = []
    for line in data.splitlines():
        parts = re.split("\s+", line)
        name = parts[-1]
        start, end = parts[0].split('-')
        start = int('0x' + start, 16)
        end = int('0x' + end, 16)
        ranges.append(LibraryData(name, start, end))
    return ranges


def read_sym_file(sym):
    syms = {}
    for line in sym.splitlines():
        if not line.startswith('0x'):
            continue
        addr, name = line.split(': ')
        addr = int(addr, 16)
        name = name.strip()
        syms[(addr, name)] = None

    syms = syms.keys()
    syms.sort()
    return syms


def read_slots(content):
    slots = []
    bottom = 0
    top = 8
    while True:
        data = content[bottom:top]
        bottom = top
        top += 8

        if len(data) < 8:
            break
        val = struct.unpack('Q', data)[0]
        slots.append(val)

    return slots


def read_prof(content): #
    # f = open(fname, 'rb') # to jest czytane do konca
    slots = read_slots(content)
    assert slots[0] == 0 # header count
    i = 2 + slots[1]     # header words
    version = slots[2]
    period = slots[3]

    # parse profile
    pcs = set()
    profiles = []
    while True:
        n = slots[i]
        if n == -1:
            break
        i += 1
        d = slots[i]
        i += 1
        #print 'i = %d, n = %d, d = %d' % (i, n, d)
        assert d <= 2**16, 'stack strace depth too high'
        if slots[i] == 0:
            # end of profile data marker
            i += d
            break
        # Make key out of the stack entries
        stacktrace = []
        for j in range(d):
            pc = slots[i+j]
            #print "read slots[%d+%d] = %d" % (i, j, pc)
            #assert pc != 0
            # Subtract one from caller pc so we map back to call instr.
            if j > 0 and pc > 0:
                pc -= 1
            pcs.add(pc)
            stacktrace.append(pc)
        stacktrace = tuple(stacktrace)
        assert n == 1 # support n != 1 a bit everywhere, notably Profiles()
        profiles.append((stacktrace, n))
        i += d
    #
    # now, read also the symbol map, as a string
    symmap = content[i*8:]
    return period, profiles, symmap
