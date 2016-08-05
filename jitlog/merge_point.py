from jitlog import constants as const
from vmshare.binary import read_string, read_le_u64

class MergePointDecoder(object):
    def __init__(self, sem_type):
        self.sem_type = sem_type
        self.last_prefix = None

    def set_prefix(self, prefix):
        self.last_prefix = prefix

class IntMergePointDecoder(MergePointDecoder):
    def decode(self, fileobj):
        assert fileobj.read(1) == b'\x00'
        return read_le_u64(fileobj)

class StrMergePointDecoder(MergePointDecoder):
    def decode(self, fileobj):
        type = fileobj.read(1)
        if type == b'\xef':
            return self.last_prefix[:]
        string = read_string(fileobj, True)
        if type == b'\x00':
            return self.last_prefix + string
        else:
            assert type == b'\xff'
            return string

def get_decoder(sem_type, gen_type, version):
    assert 0 <= sem_type <= const.MP_OPCODE[0]
    if gen_type == "s":
        return StrMergePointDecoder(sem_type)
    else:
        assert gen_type == "i"
        return IntMergePointDecoder(sem_type)

