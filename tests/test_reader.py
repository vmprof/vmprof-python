
import struct, py
from vmprof import reader
from vmprof import jitlog
from vmprof.reader import (read_one_marker, FileReadError, read_header,
    MARKER_HEADER, BufferTooSmallError, FileObjWrapper, ReaderStatus)

class FileObj(object):
    def __init__(self, lst=None):
        self.s = b''
        if lst is None:
            return
        for item in lst:
            if isinstance(item, int):
                item = struct.pack('l', item)
            self.write(item)

    def read(self, count):
        if self.s:
            s = self.s[:count]
            self.s = self.s[count:]
            return s # might be incomplete
        return b''

    def write(self, s):
        self.s += s

def test_fileobj():
    f = FileObj()
    f.write(b'foo')
    f.write(b'bar')
    assert f.read(2) == b'fo'
    assert f.read(2) == b'ob'
    assert f.read(4) == b'ar'
    assert f.read(1) == b''

def test_fileobj_wrapper():
    f1 = FileObj([b"123456"])
    fw = FileObjWrapper(f1)
    assert fw.read(4) == b"1234"
    exc = py.test.raises(BufferTooSmallError, fw.read, 4)
    f1.write(b"789")
    fw = FileObjWrapper(f1, exc.value.get_buf())
    assert fw.read(3) == b'123'
    assert fw.read(4) == b'4567'
    assert fw.read(2) == b'89'

def test_read_header():
    f = FileObj()
    f.write(struct.pack("l", 13))
    py.test.raises(FileReadError, read_header, f)
    f = FileObj([0, 3, 0, 100, 0, MARKER_HEADER])
    exc = py.test.raises(BufferTooSmallError, read_header, f)
    f.write(struct.pack("!h", 13))
    f.write(struct.pack("b", 9))
    f.write(b"foointerp")
    status = read_header(f, exc.value.get_buf())
    assert status.version == 13
    assert status.interp_name == "foointerp"

def test_jitlog_numbering():
    name_numbers = set()
    for name in dir(reader):
        if name.startswith('MARKER'):
            number = getattr(reader, name)
            assert number not in name_numbers
            name_numbers.add(number)
    for name in dir(jitlog):
        if name.startswith('MARKER'):
            number = getattr(jitlog, name)
            assert number not in name_numbers
            name_numbers.add(number)

def default_resop_meta(self):
    pass

def test_read_resop():
    fobj = FileObj([b"\x11\xff\x00\x04\x00\x00\x00call\x11\x00\xfe\x02\x00\x00\x00me"])
    fw = FileObjWrapper(fobj)
    status = ReaderStatus('pypy', 0.001, '0')
    forest = jitlog.TraceForest()
    forest.parse(fw, fw.read(1))
    assert forest.resops[0xff] == 'call'
    forest.parse(fw, fw.read(1))
    assert forest.resops[0xfe00] == 'me'

def test_asm_addr():
    addr1 = struct.pack("l", 0xAFFEAFFE)
    addr2 = struct.pack("l", 0xFEEDFEED)
    unique_id = struct.pack("l", 0xABCDEF)
    name = struct.pack("<i", 3) + "zzz"


    fobj = FileObj([b"\x16\x04\x00\x00\x00loop" + unique_id + name, # start a loop
                    b"\x14", addr1, addr2])
    fw = FileObjWrapper(fobj)
    forest = jitlog.TraceForest()
    for i in range(2):
        forest.parse(fw, fw.read(1))
    assert forest.traces[0xABCDEF].addrs == (0xAFFEAFFE, 0xFEEDFEED)

def test_asm_positions():
    unique_id = struct.pack("l", 0xFFAA)
    name = struct.pack("<i", 3) + "zAz"
    descr_nmr = struct.pack("l", 0)
    fobj = FileObj([b"\x11\xff\x00\x04\x00\x00\x00fire\x11\x00\xfe\x02\x00\x00\x00on",
                    b"\x16\x04\x00\x00\x00loop" + unique_id + name, # start a loop
                    b"\x10\x05\x00\x00\x00i1,i2", # input args
                    b"\x13\xff\x00\x10\x00\x00\x00i3,i2,i1,descr()" + descr_nmr, # resop
                    b"\x15\x04\x00\x08\x00\x00\x00DEADBEEF", # resop
                    ])
    fw = FileObjWrapper(fobj)
    forest = jitlog.TraceForest()
    for i in range(6):
        forest.parse(fw, fw.read(1))
    assert forest.traces[0xFFAA].inputargs == ['i1','i2']
    assert str(forest.traces[0xFFAA].get_stage('noopt').ops[0]) == 'i3 = fire(i2, i1, @descr())'
    assert forest.traces[0xFFAA].get_stage('noopt').ops[0].core_dump == (4, 'DEADBEEF')

def test_patch_asm():
    addr1 = struct.pack("l", 64)
    addr2 = struct.pack("l", 127)
    unique_id = struct.pack("l", 0x0)
    name = struct.pack("<i", 0) + ""

    addr_len = struct.pack("<i", 8)
    fobj = FileObj([b"\x11\xff\x00\x06\x00\x00\x00python",
                    b"\x18\x04\x00\x00\x00loop" + unique_id + name, # start a loop
                    b"\x14", addr1, addr2,
                    b"\x13\xff\x00\x05\x00\x00\x00i3,de" + "\x78" + "\x00" * 7, # resop
                    b"\x15\x00\x00\x40\x00\x00\x00", b"\x00" * 64, # machine code
                    b"\x19", 64+56, b'\x08\x00\x00\x00', b'\x00\xFF' * 4, # patch
                   ])
    fw = FileObjWrapper(fobj)
    forest = jitlog.TraceForest()
    for i in range(6):
        forest.parse(fw, fw.read(1))
    assert str(forest.traces[0].get_stage('asm').ops[0]) == 'i3 = python(, @de)'
    # TODO assert forest.traces[0].get_core_dump() == '\x00' * 56 + '\x00\xFF' * 4

def test_patch_asm_timeval():
    forest = jitlog.TraceForest()
    trace = jitlog.Trace(forest, 'bridge', 0, 0, '')
    trace.start_mark(jitlog.MARKER_JITLOG_TRACE_ASM, 0)
    trace.set_addr_bounds(0, 9)
    trace.add_instr(0, 'hello', ['world'], None, None)
    trace.get_stage('asm').get_last_op().set_core_dump(0, 'abcdef')
    trace.add_instr(0, 'add', ['i1', 'i2'], 'i3', None)
    trace.get_stage('asm').get_last_op().set_core_dump(6, 'a312')
    forest.patch_memory(4, '4321', 1)
    trace.get_core_dump(0) == "abcdefa312"
    trace.get_core_dump(1) == "abcd432112"

