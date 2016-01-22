
import struct, py
from vmprof.reader import read_one_marker, FileReadError, read_header,\
    MARKER_HEADER, BufferTooSmallError, FileObjWrapper

class FileObj(object):
    def __init__(self, lst=None):
        self.s = ''
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
        return ''

    def write(self, s):
        self.s += s

def test_fileobj():
    f = FileObj()
    f.write('foo')
    f.write('bar')
    assert f.read(2) == 'fo'
    assert f.read(2) == 'ob'
    assert f.read(4) == 'ar'
    assert f.read(1) == ''

def test_fileobj_wrapper():
    f1 = FileObj(["123456"])
    fw = FileObjWrapper(f1)
    assert fw.read(4) == "1234"
    exc = py.test.raises(BufferTooSmallError, fw.read, 4)
    f1.write("789")
    fw = FileObjWrapper(f1, exc.value.get_buf())
    assert fw.read(3) == '123'
    assert fw.read(4) == '4567'
    assert fw.read(2) == '89'

def test_read_header():
    f = FileObj()
    f.write(struct.pack("l", 13))
    py.test.raises(FileReadError, read_header, f)
    f = FileObj([0, 3, 0, 100, 0, MARKER_HEADER])
    exc = py.test.raises(BufferTooSmallError, read_header, f)
    f.write(struct.pack("!h", 13))
    f.write(struct.pack("b", 9))
    f.write("foointerp")
    status = read_header(f, exc.value.get_buf())
    assert status.version == 13
    assert status.interp_name == "foointerp"
