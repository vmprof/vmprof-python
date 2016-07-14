from __future__ import print_function
from vmprof.log import constants as const
from vmprof.log import merge_point
from vmprof.log.objects import FlatOp, MergePoint
from vmprof.binary import (read_word, read_string,
        read_le_u16, read_le_addr, read_le_u64,
        read_le_s64, read_bytes, read_byte,
        read_char)
import base64
import sys
import re

VERSIONS = {}

def get_reader(version, mark):
    return VERSIONS[version][mark]

def version(*versions):
    def func(parser):
        for ver in versions:
            if ver not in VERSIONS:
                VERSIONS[ver] = {}
            parsers = VERSIONS[ver]
            #
            name = parser.__name__
            assert name.startswith("read_")
            name = "MARK_" + name[5:].upper()
            mark = getattr(const, name, -1)
            assert mark not in parsers, "%s for version %d has already been defined" % (parser.__name__, ver)
            parsers[mark] = parser
        return parser
    return func

@version(1)
def read_resop_meta(forest, trace, fileobj):
    assert len(forest.resops) == 0
    count = read_le_u16(fileobj)
    for i in range(count):
        opnum = read_le_u16(fileobj)
        opname = read_string(fileobj, True)
        forest.resops[opnum] = opname

@version(1)
def read_start_trace(forest, trace, fileobj):
    trace_id = read_le_s64(fileobj)
    trace_type = read_string(fileobj, True)
    trace_nmr = read_le_s64(fileobj)
    #
    assert trace_id not in forest.traces
    forest.add_trace(trace_type, trace_id)
    assert trace_id in forest.traces

@version(1)
def read_trace(forest, trace, fileobj):
    assert trace is not None
    trace_id = read_le_s64(fileobj)
    assert trace_id == trace.unique_id
    trace.start_mark(const.MARK_TRACE)

@version(1)
def read_trace_opt(forest, trace, fileobj):
    assert trace is not None
    trace_id = read_le_s64(fileobj)
    assert trace_id == trace.unique_id
    trace.start_mark(const.MARK_TRACE_OPT)

@version(1)
def read_trace_asm(forest, trace, fileobj):
    assert trace is not None
    trace_id = read_le_s64(fileobj)
    assert trace_id == trace.unique_id
    trace.start_mark(const.MARK_TRACE_ASM)

@version(1)
def read_input_args(forest, trace, fileobj):
    assert trace is not None
    argnames = read_string(fileobj, True).split(',')
    trace.set_inputargs(argnames)

@version(1)
def read_resop(forest, trace, fileobj):
    assert trace is not None
    opnum = read_le_u16(fileobj)
    args = read_string(fileobj, True).split(',')
    result = args[0]
    args = args[1:]
    assert opnum in forest.resops, "opnum is not known: " + str(opnum) + \
                  " at binary pos " + str(hex(fileobj.tell()))
    opname = forest.resops[opnum]
    op = FlatOp(opnum, opname, args, result, None, -1)
    trace.add_instr(op)

TOKEN_REGEX = re.compile("TargetToken\((\d+)\)")

@version(1)
def read_resop_descr(forest, trace, fileobj):
    assert trace is not None
    opnum = read_le_u16(fileobj)
    args = read_string(fileobj, True).split(',')
    descr_number = read_le_addr(fileobj)
    descr = args[-1]
    result = args[0]
    args = args[1:-1]
    assert opnum in forest.resops, "opnum is not known: " + str(opnum) + \
                  " at binary pos " + str(hex(fileobj.tell()))
    opname = forest.resops[opnum]
    op = FlatOp(opnum, opname, args, result, descr, descr_number)
    trace.add_instr(op)

@version(1)
def read_asm_addr(forest, trace, fileobj):
    assert trace is not None
    addr1 = read_le_addr(fileobj)
    addr2 = read_le_addr(fileobj)
    trace.set_addr_bounds(addr1, addr2)

@version(1)
def read_asm(forest, trace, fileobj):
    assert trace is not None
    rel_pos = read_le_u16(fileobj)
    dump = read_bytes(fileobj)
    trace.set_core_dump_to_last_op(rel_pos, dump)

@version(1)
def read_init_merge_point(forest, trace, fileobj):
    count = read_le_u16(fileobj)
    types = []
    for i in range(count):
        sem_type = read_byte(fileobj)
        gen_type = read_char(fileobj)
        d = merge_point.get_decoder(sem_type, gen_type, forest.version)
        types.append(d)
    stage = trace.get_last_stage()
    assert stage is not None
    stage.merge_point_types = types

@version(1)
def read_common_prefix(forest, trace, fileobj):
    index = ord(fileobj.read(1))
    prefix = read_string(fileobj, True)
    stage = trace.get_last_stage()
    stage.merge_point_types[index].set_prefix(prefix)

@version(1)
def read_merge_point(forest, trace, fileobj):
    assert trace is not None
    stage = trace.get_last_stage()
    assert stage is not None
    #
    values = { decoder.sem_type : decoder.decode(fileobj)
               for decoder in stage.merge_point_types }
    trace.add_instr(MergePoint(values))

@version(1)
def read_stitch_bridge(forest, trace, fileobj):
    descr_number = read_le_addr(fileobj)
    addr_tgt = read_le_addr(fileobj)
    forest.stitch_bridge(descr_number, addr_tgt)

@version(1)
def read_jitlog_counter(forest, trace, fileobj):
    addr = read_le_addr(fileobj)
    type = read_char(fileobj)
    count = read_le_u64(fileobj)
    # entry: gets the globally numbered addr of the loop
    # bridge: gets the addr of the fail descr
    # label: gets the addr of the loop token
    trace = forest.get_trace_by_id(addr)
    if trace:
        trace.add_up_enter_count(count)
        return True
    else:
        if type == 'e':
            # it can happen that jitlog counters are present,
            # even though there is no trace to be found.
            # vm starts up (bootstrapping creates 1-2 trace),
            # jitlog is enabled afterwards
            return False
        assert type == 'b' or type == 'l'
        point_in_trace = forest.get_point_in_trace_by_descr(addr)
        if point_in_trace:
            if type == 'b':
                point_in_trace.trace.add_up_enter_count(count)
            else:
                point_in_trace.add_up_enter_count(count)
            return True
    print("trace with 0x%x (type '%c' was executed %d times" \
          " but was not recorded in the log" % (addr, type, count), file=sys.stderr)
    return False

@version(1)
def read_abort_trace(forest, trace, fileobj):
    trace_id = read_le_u64(fileobj)
    # TODO?

@version(1)
def read_source_code(forest, trace, fileobj):
    filename = read_string(fileobj, True)
    count = read_le_u16(fileobj)
    for i in range(count):
        lineno = read_le_u16(fileobj)
        indent = read_byte(fileobj)
        text = read_string(fileobj, True)
        forest.add_source_code_line(filename, lineno, indent, text)
