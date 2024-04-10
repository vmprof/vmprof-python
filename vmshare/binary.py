import sys
import struct
import array
import pytz

WORD_SIZE = struct.calcsize('L')
if sys.maxsize == 2**63-1:
    ADDR_SIZE = 8
    ADDR_CHAR = 'q'
else:
    ADDR_SIZE = 4
    ADDR_CHAR = 'l'

def read_word(fileobj):
    """Read a single long from `fileobj`."""
    b = fileobj.read(WORD_SIZE)
    #do not use UNPACK_CHAR here
    r = int(struct.unpack('l', b)[0])
    return r

def read_addr(fileobj):
    return struct.unpack(ADDR_CHAR, fileobj.read(ADDR_SIZE))[0]

def read_addresses(fileobj, count):
    """Read `addresses` longs from `fileobj`."""
    r = array.array(ADDR_CHAR)
    b = fileobj.read(ADDR_SIZE * count)
    r.frombytes(b)

    return r

def read_byte(fileobj):
    value = fileobj.read(1)
    return value[0]

def read_char(fileobj):
    value = fileobj.read(1)
    return chr(value[0])

def read_bytes(fileobj):
    lgt = int(struct.unpack('<i', fileobj.read(4))[0])
    return fileobj.read(lgt)

def read_string(fileobj, little_endian=False):
    if little_endian:
        lgt = int(struct.unpack('<i', fileobj.read(4))[0])
        data = fileobj.read(lgt)
        data = data.decode('utf-8')
        return data
    else:
        lgt = int(struct.unpack('l', fileobj.read(WORD_SIZE))[0])
    return fileobj.read(lgt)

def read_le_u16(fileobj):
    return int(struct.unpack('<H', fileobj.read(2))[0])

def read_le_u64(fileobj):
    return int(struct.unpack('<Q', fileobj.read(8))[0])

def read_s64(fileobj):
    return int(struct.unpack('q', fileobj.read(8))[0])

def read_le_s64(fileobj):
    return int(struct.unpack('<q', fileobj.read(8))[0])

def read_timeval(fileobj):
    tv_sec = read_s64(fileobj)
    tv_usec = read_s64(fileobj)
    return tv_sec * 10**6 + tv_usec

def read_timezone(fileobj):
    timezone = fileobj.read(8).strip(b'\x00')
    timezone = timezone.decode('ascii')
    if timezone:
        return pytz.timezone(timezone)
    return None

def encode_le_u16(value):
    return struct.pack('<H', value)

def encode_le_s32(value):
    return struct.pack('<i', value)

def encode_le_u32(value):
    return struct.pack('<I', value)

def encode_le_s64(value):
    return struct.pack('<q', value)

def encode_le_u64(value):
    return struct.pack('<Q', value)

def encode_str(val):
    val = val.encode()
    return struct.pack("<i", len(val)) + val


