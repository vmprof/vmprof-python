import sys
from jitlog import constants as const, marks
from jitlog.objects import TraceForest
from vmshare.binary import read_string
from io import BytesIO

JITLOG_MIN_VERSION = 1
JITLOG_VERSION = 1

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
    is_jit_log = fileobj.read(1) == const.MARK_JITLOG_HEADER
    version = ord(fileobj.read(1)) | (ord(fileobj.read(1)) << 8)
    is_32bit = ord(fileobj.read(1))
    machine = read_string(fileobj, True)
    forest = TraceForest(version, is_32bit, machine)
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
            read(forest, trace, fileobj)
            forest.time_tick()
        except KeyError as e:
            forest.exc = e
            break
        except ParseException as e:
            forest.exc = e
            break
        except Exception as e:
            msg = "failed at %s with marker %s with exc %s" %\
                    (hex(fileobj.tell()), marker, str(e))
            forest.exc = ParseException(msg)
            break

    return forest

