import sys
from jitlog import constants as const, marks
from jitlog.objects import TraceForest
from vmshare.binary import read_string
from io import BytesIO

JITLOG_MIN_VERSION = 1
JITLOG_VERSION = 1

def read_jitlog_data(filename):
    with open(str(filename), 'rb') as fileobj:
        return fileobj.read()

def parse_jitlog(filename, data=None):
    if data is None:
        data = read_jitlog_data(filename)
    fileobj = BytesIO(data)
    f = _parse_jitlog(fileobj)
    f.filepath = filename
    return f

def _parse_jitlog(fileobj):
    is_jit_log = fileobj.read(1) == const.MARK_JITLOG_HEADER
    version = ord(fileobj.read(1)) | (ord(fileobj.read(1)) << 8)
    is_32bit = ord(fileobj.read(1))
    machine = read_string(fileobj, True)
    forest = TraceForest(version, is_32bit, machine)
    assert is_jit_log, "Missing header. Data might not be a jitlog!"
    assert version >= JITLOG_MIN_VERSION, \
            "Version does not match. Log is version %d%d is not satisfied" % \
                (version, JITLOG_VERSION)
    while True:
        marker = fileobj.read(1)
        if len(marker) == 0:
            break # end of file!
        assert forest.is_jitlog_marker(marker), \
                "marker unkown: 0x%x at pos 0x%x" % (ord(marker), fileobj.tell())
        trace = forest.last_trace
        try:
            read = marks.get_reader(version, marker)
            read(forest, trace, fileobj)
            forest.time_tick()
        except KeyError:
            print("failed at", hex(fileobj.tell()), "with marker", marker)
            raise

    return forest

