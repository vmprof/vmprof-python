import sys
import struct
import traceback
from jitlog import constants as const, marks
from jitlog.objects import TraceForest
from vmshare.binary import read_string
from io import BytesIO
from collections import defaultdict

JITLOG_MIN_VERSION = 1
JITLOG_VERSION = 1

class ParseContext(object):
    def __init__(self, forest):
        self.descrs = defaultdict(list)
        self.forest = forest
        self.word_size = self.forest.word_size

    def read_le_addr(self, fileobj):
        b = fileobj.read(self.word_size)
        if self.word_size == 4:
            res = int(struct.unpack('I', b)[0])
        else:
            res = int(struct.unpack('Q', b)[0])
        return res


class ParseException(Exception):
    pass

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
    """ JitLog is parsed. if an exception is raised after
    the header has been read, it is saved in the field 'exc' on
    the forest returned.

    This allows to parse an incomplete log file an still display
    everything that was readable!
    If a ParseException is directly raised, this is an error that cannot
    be recovered from!
    """
    try:
        is_jit_log = fileobj.read(1) == const.MARK_JITLOG_HEADER
        version = ord(fileobj.read(1)) | (ord(fileobj.read(1)) << 8)
        is_32bit = ord(fileobj.read(1))
        machine = read_string(fileobj, True)
        forest = TraceForest(version, is_32bit, machine)
        ctx = ParseContext(forest)
    except Exception:
        raise ParseException("Header malformed")
    #
    if not is_jit_log:
        raise ParseException("Missing header. Provided input might not be a jitlog!")
    if version < JITLOG_MIN_VERSION:
        raise ParseException("Version %d is not supported" % version)
    while True:
        marker = fileobj.read(1)
        if len(marker) == 0:
            break # end of file!
        if not forest.is_jitlog_marker(marker):
            msg = "marker unknown: 0x%x at pos 0x%x" % \
                      (ord(marker), fileobj.tell())
            forest.exc = ParseException(msg)
            break
        trace = forest.last_trace
        try:
            read = marks.get_reader(version, marker)
            read(ctx, trace, fileobj)
            forest.time_tick()
        except KeyError as e:
            forest.exc = e
            break
        except ParseException as e:
            forest.exc = e
            break
        except Exception as e:
            exc_type, exc_value, exc_traceback = sys.exc_info()
            tb = traceback.extract_tb(exc_traceback, limit=3)
            msg = "failed at 0x%x with marker %s with exc \"%s\". trace back: \"%s\"" %\
                    (fileobj.tell(), marker, str(e), tb)
            forest.exc = ParseException(msg)
            break

    return forest

