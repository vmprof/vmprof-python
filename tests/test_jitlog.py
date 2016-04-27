import struct, py
from vmprof import reader
from vmprof.log import constants as const
from vmprof.log import marks
from vmprof.log.objects import (FlatOp, TraceForest, Trace,
        MergePoint)
from vmprof.binary import (encode_addr, encode_str, encode_s64,
    encode_u64)
from tests.test_reader import FileObj
from vmprof.reader import (read_one_marker, FileReadError, read_header,
    FileObjWrapper)

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
    name = struct.pack("<i", 3) + "zAz"
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
    assert forest.traces[0xFFAA].get_stage('noopt').ops[0].core_dump == (4, 'DEADBEEF')

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

def test_serialize_op():
    forest = TraceForest(1)
    trace = Trace(forest, 'loop', 200, 0)
    trace.start_mark(const.MARK_TRACE_OPT)
    trace.add_instr(FlatOp(0, 'INT_ADD', ['i1','i2'], '?'))
    dict = trace._serialize()
    stage = dict['stages']['opt']
    assert len(stage['ops']) == 1

def test_serialize_debug_merge_point():
    forest = TraceForest(1)
    trace = Trace(forest, 'loop', 0, 0)
    trace.start_mark(const.MARK_TRACE_OPT)
    trace.add_instr(FlatOp(0, 'INT_ADD', ['i1','i2'], 'i3'))
    trace.add_instr(FlatOp(1, 'INT_SUB', ['i1','i2'], 'i4'))
    trace.add_instr(FlatOp(2, 'INT_MUL', ['i1','i2'], 'i5'))
    trace.add_instr(MergePoint([(0x1,'/x.py'),
                                (0x2,2),
                                (0x4, 4),
                                (0x8, 'funcname'),
                                (0x10, 'LOAD_FAST')]))
    dict = trace._serialize()
    stage = dict['stages']['opt']
    assert len(stage['ops']) == 3
    assert len(stage['merge_points']) == 1 + 1
    merge_points = stage['merge_points']
    assert merge_points.keys()[0] == 3
    assert merge_points['first'] == 3
    assert merge_points[3][0] == {
            'filename': '/x.py',
            'lineno': 2,
            'scope': 'funcname',
            'index': 4,
            'opcode': 'LOAD_FAST'
           }

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

