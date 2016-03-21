


MARKER_JITLOG_INPUT_ARGS = b'\x10'
MARKER_JITLOG_RESOP_META = b'\x11'
MARKER_JITLOG_RESOP = b'\x12'
MARKER_JITLOG_RESOP_DESCR = b'\x13'
MARKER_JITLOG_ASM_ADDR = b'\x14'
MARKER_JITLOG_ASM = b'\x15'
MARKER_JITLOG_TRACE = b'\x16'
MARKER_JITLOG_TRACE_OPT = b'\x17'
MARKER_JITLOG_TRACE_ASM = b'\x18'
MARKER_JITLOG_TRACE_PATCH = b'\x19'

class FlatOp(object):
    def __init__(self, opnum, opname, args, result, descr):
        self.opnum = opnum
        self.opname = opname
        self.args = args
        self.result = result
        self.descr = descr
        self.core_dump = None

    def set_core_dump(self, core_dump):
        self.core_dump = core_dump

    def __repr__(self):
        if self.result != '':
            suffix = "%s =" % self.result
        descr = self.descr
        if descr is None:
            descr = ''
        else:
            descr = ', @' + descr
        return '%s %s(%s%s)' % (suffix, self.opname,
                                ', '.join(self.args), descr)

class Trace(object):
    def __init__(self, trace_type, tick):
        self.type = trace_type
        assert self.type in ('loop', 'bridge')
        self.ops = []
        self.creation_tick = tick

    def is_bridge(self):
        return self.type == 'bridge'

    def set_inputargs(self, args):
        self.inputargs = args

    def set_addr_bounds(self, a, b):
        self.addrs = (a,b)

    def add_instr(self, opnum, opname, args, result, descr):
        self.ops.append(FlatOp(opnum, opname, args, result, descr))


class TraceTree(object):
    pass

class TraceForest(object):
    def __init__(self):
        self.roots = []
        self.trees = []
        self.last_trace = None
        self.resops = {}
        self.timepos = 0

    def add_trace(self, trace_type, marker):
        tree = Trace(trace_type, self.timepos)
        self.trees.append(tree)
        if marker == MARKER_JITLOG_TRACE:
            self.time_tick()
        return tree

    def time_tick(self):
        self.timepos += 1

    def is_jitlog_marker(self, marker):
        return marker in (b'\x10\x11\x12'
                          b'\x13\x14\x15'
                          b'\x16\x17\x18\x19')

    def parse(self, fileobj, marker):
        trace = self.last_trace
        if marker == MARKER_JITLOG_TRACE or \
           marker == MARKER_JITLOG_TRACE_OPT or \
           marker == MARKER_JITLOG_TRACE_ASM:
            trace_type = fileobj.read_string()
            trace = self.add_trace(trace_type, marker)
            self.last_trace = trace
        elif marker == MARKER_JITLOG_INPUT_ARGS:
            argnames = fileobj.read_string().split(',')
            trace.set_inputargs(argnames)
        elif marker == MARKER_JITLOG_ASM_ADDR:
            addr1 = fileobj.read_le_addr()
            addr2 = fileobj.read_le_addr()
            trace.set_addr_bounds(addr1, addr2)
        elif marker == MARKER_JITLOG_RESOP_META:
            opnum = fileobj.read_le_u16()
            opname = fileobj.read_string()
            self.resops[opnum] = opname
        elif marker == MARKER_JITLOG_RESOP or \
             marker == MARKER_JITLOG_RESOP_DESCR:
            opnum = fileobj.read_le_u16()
            args = fileobj.read_string().split(',')
            descr = None
            if marker == MARKER_JITLOG_RESOP_DESCR:
                descr = args[-1]
                args = args[:-1]
            result = args[0]
            args = args[1:]
            trace.add_instr(opnum, self.resops[opnum], args, result, descr)
        elif marker == MARKER_JITLOG_ASM:
            dump = fileobj.read_string()
            flatop = trace.ops[-1]
            flatop.set_core_dump(dump)

