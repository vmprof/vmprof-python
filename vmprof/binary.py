import sys, struct

WORD_SIZE = struct.calcsize('L')
PY3 = sys.version_info[0] >= 3

def read_word(fileobj):
    b = fileobj.read(WORD_SIZE)
    r = int(struct.unpack('l', b)[0])
    return r

def read_string(fileobj, little_endian=False):
    if little_endian:
        lgt = int(struct.unpack('<i', fileobj.read(4))[0])
        data = fileobj.read(lgt)
        if PY3:
            data = data.decode()
        return data
    else:
        lgt = int(struct.unpack('l', fileobj.read(WORD_SIZE))[0])
    return fileobj.read(lgt)

def read_le_addr(fileobj):
    b = fileobj.read(WORD_SIZE)
    return int(struct.unpack('l', b)[0])

def read_le_u16(fileobj):
    return int(struct.unpack('<H', fileobj.read(2))[0])


