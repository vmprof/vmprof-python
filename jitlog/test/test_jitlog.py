import struct, py, sys
import pytest
from vmprof import reader
from jitlog import constants as const
from jitlog import marks
from jitlog.parser import ParseException
from jitlog.parser import _parse_jitlog, ParseContext
from jitlog.objects import (FlatOp, TraceForest, Trace,
        MergePoint, PointInTrace, iter_ranges)
from vmshare.binary import (encode_str, encode_le_u64, encode_le_u32)
from vmprof.test.test_reader import FileObj
from vmprof.test.test_run import FileObjWrapper, BufferTooSmallError
import vmprof

PY3 = sys.version_info[0] >= 3

def construct_forest(fileobj, version=1, forest=None):
    if forest is None:
        forest = TraceForest(version)
    ctx = ParseContext(forest)
    try:
        while True:
            marker = fileobj.read(1)
            read = marks.get_reader(version, marker)
            trace = forest.last_trace
            read(ctx, trace, fileobj)
            forest.time_tick()
    except BufferTooSmallError:
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
    fobj = FileObj([const.MARK_START_TRACE, encode_le_u64(0x15), encode_str('loop'), encode_le_u64(0),
                    const.MARK_TRACE, encode_le_u64(0x15),
                    const.MARK_ASM_ADDR, encode_le_u64(0xAFFE), encode_le_u64(0xFEED)
                   ])
    fw = FileObjWrapper(fobj)
    forest = construct_forest(fw)
    trace = forest.get_trace(0x15)
    assert trace.addrs == (0xAFFE, 0xFEED)
    assert trace.jd_name == None

def test_asm_positions():
    name = struct.pack("<i", 3) + b"zAz"
    descr_nmr = encode_le_u64(0)
    fobj = FileObj([const.MARK_RESOP_META, b"\x02\x00",
                    b"\xff\x00\x04\x00\x00\x00fire\x00\xfe\x02\x00\x00\x00on",
                    const.MARK_START_TRACE, encode_le_u64(0xffaa), encode_str('loop'), encode_le_u64(0),
                    const.MARK_TRACE, encode_le_u64(0xffaa),
                    const.MARK_INPUT_ARGS, b"\x05\x00\x00\x00i1,i2", # input args
                    const.MARK_RESOP_DESCR, b"\xff\x00\x10\x00\x00\x00i3,i2,i1,descr()" + descr_nmr, # resop
                    const.MARK_ASM, b"\x04\x00\x08\x00\x00\x00DEADBEEF", # resop
                    ])
    fw = FileObjWrapper(fobj)
    forest = construct_forest(fw)
    assert forest.traces[0xFFAA].inputargs == ['i1','i2']
    assert str(forest.traces[0xFFAA].get_stage('noopt').ops[0]) == 'i3 = fire(i2, i1, @descr())'
    assert forest.traces[0xFFAA].get_stage('noopt').ops[0].core_dump == (4, b'DEADBEEF')

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
    descr_nmr = encode_le_u64(10)
    addr_len = struct.pack("<i", 8)
    fobj = FileObj([const.MARK_RESOP_META + b"\x01\x00\xff\x00", encode_str("python"),
                    const.MARK_START_TRACE, encode_le_u64(0xffaa), encode_str('loop'), encode_le_u64(0),
                    const.MARK_TRACE, encode_le_u64(0xffaa),
                    const.MARK_INPUT_ARGS, encode_str("i1,i2"), # input args
                    const.MARK_RESOP_DESCR, b"\xff\x00", encode_str("i3,i2,i1,descr()") + descr_nmr, # resop
                    const.MARK_ASM, b"\x04\x00", encode_str("DEADBEEF"), # coredump
                    const.MARK_ASM_ADDR, encode_le_u64(0xabcdef), encode_le_u64(0xabcdff),
                    const.MARK_JITLOG_COUNTER, encode_le_u64(0xabcdef), b'l', encode_le_u64(15),
                    const.MARK_JITLOG_COUNTER, encode_le_u64(0xabcdef), b'l', encode_le_u64(0),
                    const.MARK_JITLOG_COUNTER, encode_le_u64(0xabcdef), b'l', encode_le_u64(15),
                    const.MARK_JITLOG_COUNTER, encode_le_u64(0xabcfff), b'l', encode_le_u64(5), # not counted to 0xabcdef
                   ])
    fw = FileObjWrapper(fobj)
    forest = construct_forest(fw)

    forest.get_trace_by_addr(0xabcdef).counter == 30

def test_merge_point_extract_source_code():
    forest = TraceForest(1)
    trace = forest.add_trace('loop', 0, 0)
    trace.start_mark(const.MARK_TRACE_OPT)
    trace.add_instr(MergePoint({0x1:'jitlog/test/data/code.py', 0x2: 2}))
    trace.add_instr(FlatOp(0, 'INT_ADD', ['i1','i2'], 'i3'))
    forest.extract_source_code_lines()
    assert forest.source_lines['jitlog/test/data/code.py'][2] == (4, 'return a + b')

def test_merge_point_extract_multiple_lines():
    forest = TraceForest(1)
    trace = forest.add_trace('loop', 0, 0)
    trace.start_mark(const.MARK_TRACE_OPT)
    trace.add_instr(MergePoint({0x1: 'jitlog/test/data/code.py', 0x2: 5}))
    trace.add_instr(FlatOp(0, 'INT_MUL', ['i1','i2'], 'i3'))
    trace.add_instr(MergePoint({0x1: 'jitlog/test/data/code.py', 0x2: 7}))
    forest.extract_source_code_lines()
    assert forest.source_lines['jitlog/test/data/code.py'][5] == (4, 'c = a * 2')
    assert forest.source_lines['jitlog/test/data/code.py'][6] == (8, 'd = c * 3')
    assert forest.source_lines['jitlog/test/data/code.py'][7] == (4, 'return d + 5')

def test_merge_point_duplicate_source_lines():
    forest = TraceForest(1)
    trace = forest.add_trace('loop', 0, 0)
    trace.start_mark(const.MARK_TRACE_OPT)
    trace.add_instr(MergePoint({0x1: 'jitlog/test/data/code.py', 0x2: 5}))
    trace.add_instr(MergePoint({0x1: 'jitlog/test/data/code.py', 0x2: 5}))
    trace.add_instr(MergePoint({0x1: 'jitlog/test/data/code.py', 0x2: 5}))
    trace.add_instr(MergePoint({0x1: 'jitlog/test/data/code.py', 0x2: 5}))
    forest.extract_source_code_lines()
    assert forest.source_lines['jitlog/test/data/code.py'][5] == (4, 'c = a * 2')
    assert len(forest.source_lines['jitlog/test/data/code.py']) == 1

def test_add_source_code_lines_to_forest():
    forest = TraceForest(1)
    forest.add_source_code_line("x.py", 12, 12, "x = 1")
    forest.add_source_code_line("x.py", 13, 12, "y = 1")

def test_merge_point_encode():
    forest = TraceForest(1)
    trace = forest.add_trace('loop', 0, 0)
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
    ta = forest.add_trace('loop', 1, 0)
    ta.start_mark(const.MARK_TRACE_ASM)
    op = FlatOp(0, 'hello', '', '?', 0, 2)
    ta.add_instr(op)
    op2 = FlatOp(0, 'increment_debug_counter', '', '?', 0, 2)
    ta.add_instr(op2)
    tb = forest.add_trace('bridge', 22, 101)
    fw = FileObjWrapper(FileObj([encode_le_u64(0x0), b'l', encode_le_u64(20)]))
    assert marks.read_jitlog_counter(ParseContext(forest), None, fw) == False, \
            "must not find trace"
    fw = FileObjWrapper(FileObj([encode_le_u64(1), b'e', encode_le_u64(145),
                                 encode_le_u64(2), b'l', encode_le_u64(45),
                                 encode_le_u64(22), b'b', encode_le_u64(100),
                                ]))
    # read the entry, the label, and the bridge
    assert marks.read_jitlog_counter(ParseContext(forest), None, fw) == True
    assert marks.read_jitlog_counter(ParseContext(forest), None, fw) == True
    assert marks.read_jitlog_counter(ParseContext(forest), None, fw) == True
    assert ta.counter == 145
    assert ta.point_counters[1] == 45
    assert tb.counter == 100

def test_point_in_trace():
    forest = TraceForest(1)
    trace = forest.add_trace('loop', 0, 0)
    trace.start_mark(const.MARK_TRACE_ASM)
    op = FlatOp(0, 'hello', '', '?', 0, 1)
    trace.add_instr(op)
    trace.add_up_enter_count(10)
    point_in_trace = forest.get_point_in_trace_by_descr(1)
    point_in_trace.set_inc_op(FakeOp(1))
    point_in_trace.add_up_enter_count(20)

    assert trace.counter == 10
    assert trace.point_counters[1] == 20

class FakeOp(object):
    def __init__(self, i):
        self.index = i

def test_counter_points():
    forest = TraceForest(1)
    trace = forest.add_trace('loop', 0, 0)
    d = trace.get_counter_points()
    assert d[0] == 0
    assert len(d) == 1
    trace.counter = 100
    d = trace.get_counter_points()
    assert d[0] == 100
    assert len(d) == 1
    pit = PointInTrace(trace, FakeOp(10))
    assert not pit.add_up_enter_count(55)
    pit.set_inc_op(FakeOp(11))
    assert pit.add_up_enter_count(55)
    d = trace.get_counter_points()
    assert d[11] == 55
    assert len(d) == 2

def test_32bit_log_header():
    fobj = FileObj([const.MARK_JITLOG_HEADER+ b"\x01\x00\x01"+\
                    encode_str('ppc64le')])
    forest = _parse_jitlog(fobj)
    assert forest.version == 1
    assert forest.word_size == 4
    assert forest.machine == 'ppc64le'

def test_32bit_read_trace():
    fobj = FileObj([const.MARK_JITLOG_HEADER+ b"\x01\x00\x01"+encode_str('s390x'),
                    const.MARK_START_TRACE, encode_le_u32(0x15), encode_str('loop'), encode_le_u32(0),
                   ])
    forest = _parse_jitlog(fobj)
    assert forest.version == 1
    assert forest.word_size == 4
    assert len(forest.traces) == 1
    assert forest.machine == 's390x'

def test_v2_start_trace():
    fobj = FileObj([const.MARK_START_TRACE,
            encode_le_u64(0x15),
            encode_str('loop'),
            encode_le_u64(0),
            encode_str('jd_is_a_hippy'),
            ])
    fw = FileObjWrapper(fobj)
    forest = construct_forest(fw, version=2)
    assert forest.get_trace(0x15).jd_name == 'jd_is_a_hippy'

def test_exception_recover():
    # weird file provided, fails without returning anything
    fobj = FileObj([0x0])
    with pytest.raises(ParseException):
        _parse_jitlog(fobj)

    # incomplete log, bails and adds exception
    fobj = FileObj([const.MARK_JITLOG_HEADER,
                    b'\x01\x00\x00', encode_str('x86_64'),
                   b'\x00'
                   ])
    f = _parse_jitlog(fobj)
    assert hasattr(f, 'exc')
    assert "marker unknown" in f.exc.args[0]

    # some valid data, but most of it missing
    fobj = FileObj([const.MARK_JITLOG_HEADER,
                    b'\x01\x00\x00', encode_str('x86_64'),
                    const.MARK_START_TRACE, encode_le_u64(0xffaa), encode_str('loop'), encode_le_u64(0),
                    const.MARK_TRACE, encode_le_u64(0xffaa),
                    const.MARK_START_TRACE # uff, trace ends here, data missing
                   ])
    f = _parse_jitlog(fobj)
    assert len(f.traces) == 1
    assert hasattr(f, 'exc')

def test_v3_redirect_assembler():
    # prepare a forest that already got two traces,
    # the first with call assembler and the target of
    # call assembler already included. then a read mark
    # redirect assembler is emulated.
    forest = TraceForest(3)
    trace = forest.add_trace('loop', 0, 0)
    trace.start_mark(const.MARK_TRACE_ASM)
    op = FlatOp(0, 'call_assembler_i', '', 'i0', 0, 15)
    trace.add_instr(op)
    #
    trace2 = forest.add_trace('loop', 16, 0)
    trace2.start_mark(const.MARK_TRACE_ASM)
    trace2.set_addr_bounds(42,44)
    #
    fobj = FileObj([const.MARK_REDIRECT_ASSEMBLER,
                    encode_le_u64(15),
                    encode_le_u64(17),
                    encode_le_u64(16),
                    ])
    fw = FileObjWrapper(fobj)
    forest = construct_forest(fw, forest=forest)
    asm = forest.get_trace(16)
    parent = forest.get_trace(0)
    assert asm.get_parent() == parent
    assert len(parent.links) == 1

def test_failing_guard():
    forest = TraceForest(3)
    trace = forest.add_trace('loop', 0, 0)
    trace.start_mark(const.MARK_TRACE_ASM)
    op = FlatOp(0, 'gurad_true', '', 'i0', 0, 15)
    trace.add_instr(op)
    #
    trace2 = forest.add_trace('bridge', 16, 0)
    trace2.start_mark(const.MARK_TRACE_OPT)
    trace2.set_addr_bounds(42,44)
    #
    forest.stitch_bridge(15, 42)
    assert trace2.get_failing_guard() == op

