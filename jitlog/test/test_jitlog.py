import struct, py, sys
from vmprof import reader
from jitlog import constants as const
from jitlog import marks
from jitlog.objects import (FlatOp, TraceForest, Trace,
        MergePoint, PointInTrace, iter_ranges)
from vmprof.binary import (encode_addr, encode_str, encode_s64,
    encode_u64)
from vmprof.test.test_reader import FileObj
from vmprof.reader import (read_one_marker, FileReadError, read_header,
    FileObjWrapper)
import base64

PY3 = sys.version_info[0] >= 3

def construct_forest(fileobj):
    version = 1
    forest = TraceForest(version)
    try:
        while True:
            marker = fileobj.read(1)
            read = marks.get_reader(version, marker)
            trace = forest.last_trace
            read(forest, trace, fileobj)
            forest.time_tick()
    except:
        pass
    return forest

def test_read_resop():
    fobj = FileObj([const.MARK_RESOP_META + b"\x02\x00",
                    b"\xff\x00\x04\x00\x00\x00call",
                    b"\x00\xfe\x02\x00\x00\x00me"])
    fw = FileObjWrapper(fobj)
    forest = construct_forest(fw)
    assert forest.resops[0xff] == 'call'
    assert forest.resops[0xfe00] == 'me'

def test_asm_addr():
    fobj = FileObj([const.MARK_START_TRACE, encode_s64(0x15), encode_str('loop'), encode_s64(0),
                    const.MARK_TRACE, encode_s64(0x15),
                    const.MARK_ASM_ADDR, encode_addr(0xAFFE), encode_addr(0xFEED)
                   ])
    fw = FileObjWrapper(fobj)
    forest = construct_forest(fw)
    assert forest.traces[0x15].addrs == (0xAFFE, 0xFEED)

def test_asm_positions():
    unique_id = struct.pack("l", 0xFFAA)
    name = struct.pack("<i", 3) + b"zAz"
    descr_nmr = struct.pack("l", 0)
    fobj = FileObj([const.MARK_RESOP_META, b"\x02\x00",
                    b"\xff\x00\x04\x00\x00\x00fire\x00\xfe\x02\x00\x00\x00on",
                    const.MARK_START_TRACE, encode_s64(0xffaa), encode_str('loop'), encode_s64(0),
                    const.MARK_TRACE, encode_s64(0xffaa),
                    const.MARK_INPUT_ARGS, b"\x05\x00\x00\x00i1,i2", # input args
                    const.MARK_RESOP_DESCR, b"\xff\x00\x10\x00\x00\x00i3,i2,i1,descr()" + descr_nmr, # resop
                    const.MARK_ASM, b"\x04\x00\x08\x00\x00\x00DEADBEEF", # resop
                    ])
    fw = FileObjWrapper(fobj)
    forest = construct_forest(fw)
    assert forest.traces[0xFFAA].inputargs == ['i1','i2']
    assert str(forest.traces[0xFFAA].get_stage('noopt').ops[0]) == 'i3 = fire(i2, i1, @descr())'
    assert forest.traces[0xFFAA].get_stage('noopt').ops[0].core_dump == (4, b'DEADBEEF')

#def test_patch_asm():
#    addr1 = struct.pack("l", 64)
#    addr2 = struct.pack("l", 127)
#    unique_id = struct.pack("l", 0x0)
#    name = struct.pack("<i", 0) + ""
#
#    addr_len = struct.pack("<i", 8)
#    fobj = FileObj([b"\x11\xff\x00\x06\x00\x00\x00python",
#                    b"\x18\x04\x00\x00\x00loop" + unique_id + name, # start a loop
#                    b"\x14", addr1, addr2,
#                    b"\x13\xff\x00\x05\x00\x00\x00i3,de" + "\x78" + "\x00" * 7, # resop
#                    b"\x15\x00\x00\x40\x00\x00\x00", b"\x00" * 64, # machine code
#                    b"\x19", 64+56, b'\x08\x00\x00\x00', b'\x00\xFF' * 4, # patch
#                   ])
#    fw = FileObjWrapper(fobj)
#    forest = TraceForest(1)
#    for i in range(6):
#        forest.parse(fw, fw.read(1))
#    assert str(forest.traces[0].get_stage('asm').ops[0]) == 'i3 = python(, @de)'
#    # TODO assert forest.traces[0].get_core_dump() == '\x00' * 56 + '\x00\xFF' * 4

def test_patch_asm_timeval():
    forest = TraceForest(1)
    trace = Trace(forest, 'bridge', 0, 0)
    trace.start_mark(const.MARK_TRACE_ASM)
    trace.set_addr_bounds(0, 9)
    trace.add_instr(FlatOp(0, 'hello', ['world'], None, None))
    trace.get_stage('asm').get_last_op().set_core_dump(0, 'abcdef')
    trace.add_instr(FlatOp(0, 'add', ['i1', 'i2'], 'i3', None))
    trace.get_stage('asm').get_last_op().set_core_dump(6, 'a312')
    forest.patch_memory(4, '4321', 1)
    trace.get_core_dump(0) == "abcdefa312"
    trace.get_core_dump(1) == "abcd432112"

def test_counters():
    descr_nmr = encode_addr(10)

    addr_len = struct.pack("<i", 8)
    fobj = FileObj([const.MARK_RESOP_META + b"\x01\x00\xff\x00", encode_str("python"),
                    const.MARK_START_TRACE, encode_s64(0xffaa), encode_str('loop'), encode_s64(0),
                    const.MARK_TRACE, encode_s64(0xffaa),
                    const.MARK_INPUT_ARGS, encode_str("i1,i2"), # input args
                    const.MARK_RESOP_DESCR, b"\xff\x00", encode_str("i3,i2,i1,descr()") + descr_nmr, # resop
                    const.MARK_ASM, b"\x04\x00", encode_str("DEADBEEF"), # coredump
                    const.MARK_ASM_ADDR, encode_addr(0xabcdef), encode_addr(0xabcdff),
                    const.MARK_JITLOG_COUNTER, encode_addr(0xabcdef), 15,
                    const.MARK_JITLOG_COUNTER, encode_addr(0xabcdef), 0,
                    const.MARK_JITLOG_COUNTER, encode_addr(0xabcdef), 15,
                    const.MARK_JITLOG_COUNTER, encode_addr(0xabcfff), 5, # not counted to 0xabcdef
                   ])
    fw = FileObjWrapper(fobj)
    forest = construct_forest(fw)

    forest.get_trace_by_addr(0xabcdef).counter == 30

def test_merge_point_extract_source_code():
    forest = TraceForest(1)
    trace = forest.add_trace('loop', 0)
    trace.start_mark(const.MARK_TRACE_OPT)
    trace.add_instr(MergePoint({0x1:'jitlog/test/data/code.py', 0x2: 2}))
    trace.add_instr(FlatOp(0, 'INT_ADD', ['i1','i2'], 'i3'))
    forest.extract_source_code_lines()
    assert forest.source_lines['jitlog/test/data/code.py'][2] == (4, b'return a + b')

def test_merge_point_extract_multiple_lines():
    forest = TraceForest(1)
    trace = forest.add_trace('loop', 0)
    trace.start_mark(const.MARK_TRACE_OPT)
    trace.add_instr(MergePoint({0x1: 'jitlog/test/data/code.py', 0x2: 5}))
    trace.add_instr(FlatOp(0, 'INT_MUL', ['i1','i2'], 'i3'))
    trace.add_instr(MergePoint({0x1: 'jitlog/test/data/code.py', 0x2: 7}))
    forest.extract_source_code_lines()
    assert forest.source_lines['jitlog/test/data/code.py'][5] == (4, b'c = a * 2')
    assert forest.source_lines['jitlog/test/data/code.py'][6] == (8, b'd = c * 3')
    assert forest.source_lines['jitlog/test/data/code.py'][7] == (4, b'return d + 5')

def test_merge_point_duplicate_source_lines():
    forest = TraceForest(1)
    trace = forest.add_trace('loop', 0)
    trace.start_mark(const.MARK_TRACE_OPT)
    trace.add_instr(MergePoint({0x1: 'jitlog/test/data/code.py', 0x2: 5}))
    trace.add_instr(MergePoint({0x1: 'jitlog/test/data/code.py', 0x2: 5}))
    trace.add_instr(MergePoint({0x1: 'jitlog/test/data/code.py', 0x2: 5}))
    trace.add_instr(MergePoint({0x1: 'jitlog/test/data/code.py', 0x2: 5}))
    forest.extract_source_code_lines()
    assert forest.source_lines['jitlog/test/data/code.py'][5] == (4, b'c = a * 2')
    assert len(forest.source_lines['jitlog/test/data/code.py']) == 1

def test_merge_point_encode():
    forest = TraceForest(1)
    trace = forest.add_trace('loop', 0)
    trace.start_mark(const.MARK_TRACE_OPT)
    trace.add_instr(MergePoint({0x1:'jitlog/test/data/code.py', 0x2: 5}))
    trace.add_instr(FlatOp(0, 'INT_MUL', ['i1','i2'], 'i3'))
    trace.add_instr(MergePoint({0x1:'jitlog/test/data/code.py', 0x2: 7}))
    trace.add_instr(MergePoint({0x1:'jitlog/test/data/code2.py', 0x2: 3}))
    forest.extract_source_code_lines()
    binary = trace.forest.encode_source_code_lines()
    parta = b'\x22\x19\x00\x00\x00jitlog/test/data/code2.py' \
            b'\x01\x00' \
            b'\x03\x00\x07\x13\x00\x00\x00self.unique = False'
    partb = b'\x22\x18\x00\x00\x00jitlog/test/data/code.py' \
            b'\x03\x00' \
            b'\x05\x00\x04\x09\x00\x00\x00c = a * 2' \
            b'\x06\x00\x08\x09\x00\x00\x00d = c * 3' \
            b'\x07\x00\x04\x0c\x00\x00\x00return d + 5'
    equals = binary == parta + partb
    if not equals:
        assert binary == partb + parta

def test_iter_ranges():
    r = lambda a,b: list(range(a,b))
    if PY3:
        r = range
    assert list(iter_ranges([])) == []
    assert list(iter_ranges([1])) == [r(1,2)]
    assert list(iter_ranges([5,7])) == [r(5,8)]
    assert list(iter_ranges([14,25,100])) == [r(14,26),r(100,101)]
    assert list(iter_ranges([-1,2])) == [r(-1,2+1)]
    assert list(iter_ranges([0,1,100,101,102,300,301])) == [r(0,2),r(100,103),r(300,302)]

def test_read_jitlog_counter():
    forest = TraceForest(1)
    ta = forest.add_trace('loop', 1)
    ta.start_mark(const.MARK_TRACE_ASM)
    op = FlatOp(0, 'hello', '', '?', 0, 2)
    ta.add_instr(op)
    tb = forest.add_trace('bridge', 22)
    fw = FileObjWrapper(FileObj([encode_addr(0x0), b'l', encode_u64(20)]))
    assert marks.read_jitlog_counter(forest, None, fw) == False, \
            "must not find trace"
    fw = FileObjWrapper(FileObj([encode_addr(1), b'e', encode_u64(145),
                                 encode_addr(2), b'l', encode_u64(45),
                                 encode_addr(22), b'b', encode_u64(100),
                                ]))
    # read the entry, the label, and the bridge
    assert marks.read_jitlog_counter(forest, None, fw) == True
    assert marks.read_jitlog_counter(forest, None, fw) == True
    assert marks.read_jitlog_counter(forest, None, fw) == True
    assert ta.counter == 145
    assert ta.point_counters[0] == 45
    assert tb.counter == 100

def test_point_in_trace():
    forest = TraceForest(1)
    trace = forest.add_trace('loop', 0)
    trace.start_mark(const.MARK_TRACE_ASM)
    op = FlatOp(0, 'hello', '', '?', 0, 0)
    trace.add_instr(op)
    trace.add_up_enter_count(10)
    point_in_trace = forest.get_point_in_trace_by_descr(0)
    point_in_trace.add_up_enter_count(20)

    assert trace.counter == 10
    assert trace.point_counters[0] == 20

class FakeOp(object):
    def __init__(self, i):
        self.index = i

def test_counter_points():
    forest = TraceForest(1)
    trace = forest.add_trace('loop', 0)
    d = trace.get_counter_points()
    assert d['enter'] == 0
    assert len(d) == 1
    trace.counter = 100
    d = trace.get_counter_points()
    assert d['enter'] == 100
    assert len(d) == 1
    pit = PointInTrace(trace, FakeOp(10))
    pit.add_up_enter_count(55)
    d = trace.get_counter_points()
    assert d[10] == 55
    assert len(d) == 2

