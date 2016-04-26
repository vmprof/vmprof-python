from vmprof.log import constants as const
from vmprof.binary import (read_word, read_string,
        read_le_u16, read_le_addr, read_le_u64,
        read_le_s64)

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
    from vmprof.jitlog import FlatOp
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

@version(1)
def read_resop_descr(forest, trace, fileobj):
    from vmprof.jitlog import FlatOp
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



#        elif marker == const.MARK_TRACE or \
#           marker == const.MARK_TRACE_OPT or \
#           marker == const.MARK_TRACE_ASM:
#            trace_id = read_le_s64(fileobj)
#            print("marking trace", trace_id, hex(fileobj.tell()))
#            assert trace_id in self.traces
#            self.last_trace.start_mark(marker, self.timepos)

#        elif marker == const.MARK_TRACE or \
#           marker == const.MARK_TRACE_OPT or \
#           marker == const.MARK_TRACE_ASM:
#            trace_id = read_le_s64(fileobj)
#            print("marking trace", trace_id, hex(fileobj.tell()))
#            assert trace_id in self.traces
#            self.last_trace.start_mark(marker, self.timepos)
#        elif marker == const.MARK_ASM_ADDR:
#            addr1 = read_le_addr(fileobj)
#            addr2 = read_le_addr(fileobj)
#            if self.keep:
#                trace.set_addr_bounds(addr1, addr2)
#                trace.forest.addrs[addr1] = trace
#        elif marker == const.MARK_ASM:
#            rel_pos = read_le_u16(fileobj)
#            dump = read_string(fileobj, True)
#            if self.keep:
#                trace.set_core_dump_to_last_op(rel_pos, dump)
#        elif marker == const.MARK_STITCH_BRIDGE:
#            descr_number = read_le_addr(fileobj)
#            addr_tgt = read_le_addr(fileobj)
#            if self.keep:
#                self.stitch_bridge(descr_number, addr_tgt, self.timepos)
#        elif marker == const.MARK_JITLOG_COUNTER:
#            addr = read_le_addr(fileobj)
#            count = read_le_addr(fileobj)
#            trace = self.get_trace_by_addr(addr)
#            trace.counter += count
#        elif marker == const.MARK_MERGE_POINT:
#            filename = read_string(fileobj, True)
#            lineno = read_le_u16(fileobj)
#            enclosed = read_string(fileobj, True)
#            index = read_le_u64(fileobj)
#            opname = read_string(fileobj, True)
#            trace.add_instr(MergePoint(filename, lineno, enclosed, index, opname))
#        elif marker == const.MARK_ABORT_TRACE:
#            trace_id = read_le_u64(fileobj)
#            # TODO
#        else:
#            assert False, (marker, fileobj.tell())
