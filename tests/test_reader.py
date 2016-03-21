
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
    from vmprof.jitlog import MARKER_JITLOG_RESOP_META

    fobj = FileObj([b"\x11\xff\x00\x04\x00\x00\x00call\x11\x00\xfe\x02\x00\x00\x00me"])
    fw = FileObjWrapper(fobj)
    status = ReaderStatus('pypy', 0.001, '0')
    read_one_marker(fw, status)
    assert status.forest.resops[0xff] == 'call'
    read_one_marker(fw, status)
    assert status.forest.resops[0xfe00] == 'me'

def test_asm_addr():
    addr1 = struct.pack("l", 0xAFFEAFFE)
    addr2 = struct.pack("l", 0xFEEDFEED)

    fobj = FileObj([b"\x16\x04\x00\x00\x00loop", # start a loop
                    b"\x14", addr1, addr2])
    fw = FileObjWrapper(fobj)
    status = ReaderStatus('pypy', 0.001, '0')
    for i in range(2):
        read_one_marker(fw, status)
    assert status.forest.trees[0].addrs == (0xAFFEAFFE, 0xFEEDFEED)

def test_asm_positions():
    fobj = FileObj([b"\x11\xff\x00\x04\x00\x00\x00fire\x11\x00\xfe\x02\x00\x00\x00on",
                    b"\x16\x04\x00\x00\x00loop", # start a loop
                    b"\x10\x05\x00\x00\x00i1,i2", # input args
                    b"\x13\xff\x00\x10\x00\x00\x00i3,i2,i1,descr()", # resop
                    b"\x15\x08\x00\x00\x00DEADBEEF", # resop
                    ])
    fw = FileObjWrapper(fobj)
    status = ReaderStatus('pypy', 0.001, '0')
    for i in range(6):
        read_one_marker(fw, status)
    assert status.forest.trees[0].inputargs == ['i1','i2']
    assert str(status.forest.trees[0].ops[0]) == 'i3 = fire(i2, i1, @descr())'
    assert status.forest.trees[0].ops[0].core_dump == 'DEADBEEF'




