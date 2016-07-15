
import struct, py
from vmprof import reader
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
    f.write(struct.pack("b", 3))
    f.write(struct.pack("b", 9))
    f.write(b"foointerp")
    status = read_header(f, exc.value.get_buf())
    assert status.version == 13
    assert status.profile_lines == True
    assert status.profile_memory == True
    assert status.interp_name == "foointerp"

