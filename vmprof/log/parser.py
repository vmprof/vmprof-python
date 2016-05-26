import sys
from vmprof.log import constants as const, marks
from vmprof.log.objects import TraceForest
if sys.version_info[0] >= 3:
    from io import StringIO
else:
    from StringIO import StringIO

JITLOG_MIN_VERSION = 1
JITLOG_VERSION = 1

def read_jitlog_data(filename):
    with open(str(filename), 'rb') as fileobj:
        return fileobj.read()

def parse_jitlog(filename, data=None):
    if data is None:
        data = read_jitlog_data(filename)
    fileobj = StringIO(data)
    f = _parse_jitlog(fileobj)
    f.filepath = filename
    return f

def _parse_jitlog(fileobj):
    is_jit_log = fileobj.read(1) == const.MARK_JITLOG_HEADER
    version = ord(fileobj.read(1)) | (ord(fileobj.read(1)) << 8)
    forest = TraceForest(version)
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

