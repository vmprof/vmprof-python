


MARKER_JITLOG_INPUT_ARGS = b'\x10'
MARKER_JITLOG_RESOP_META = b'\x11'
MARKER_JITLOG_RESOP = b'\x12'
MARKER_JITLOG_RESOP_DESCR = b'\x13'
MARKER_JITLOG_ASM_ADDR = b'\x14'
MARKER_JITLOG_ASM = b'\x15'
MARKER_JITLOG_TRACE = b'\x16'
MARKER_JITLOG_TRACE_OPT = b'\x17'
MARKER_JITLOG_TRACE_ASM = b'\x18'
MARKER_JITLOG_PATCH = b'\x19'

class FlatOp(object):
    def __init__(self, opnum, opname, args, result, descr):
        self.opnum = opnum
        self.opname = opname
        self.args = args
        self.result = result
        self.descr = descr
        self.core_dump = None

    def set_core_dump(self, rel_pos, core_dump):
        self.core_dump = (rel_pos, core_dump)

    def get_core_dump(self, base_addr, patches, timeval):
        coredump = self.core_dump[1][:]
        for timepos, addr, content in patches:
            if timeval < timepos:
                continue # do not apply the patch
            op_off = self.core_dump[0]
            patch_start = (addr - base_addr) - op_off 
            patch_end = patch_start + len(content)
            content_end = len(content)-1
            if patch_end >= len(coredump):
                patch_end = len(coredump)
                content_end = patch_end - patch_start
            coredump = coredump[:patch_start] + content[:content_end] + coredump[patch_end:]
        return coredump

    def __repr__(self):
        suffix = ''
        if self.result is not None:
            suffix = "%s = " % self.result
        descr = self.descr
        if descr is None:
            descr = ''
        else:
            descr = ', @' + descr
        return '%s%s(%s%s)' % (suffix, self.opname,
                                ', '.join(self.args), descr)

class Trace(object):
    def __init__(self, forest, trace_type, tick):
        self.forest = forest
        self.type = trace_type
        assert self.type in ('loop', 'bridge')
        self.ops = []
        self.creation_tick = tick
        # this saves a quadrupel for each
        self.my_patches = None

    def is_bridge(self):
        return self.type == 'bridge'

    def set_inputargs(self, args):
        self.inputargs = args

    def set_addr_bounds(self, a, b):
        self.addrs = (a,b)

    def add_instr(self, opnum, opname, args, result, descr):
        self.ops.append(FlatOp(opnum, opname, args, result, descr))

    def contains_patch(self, addr):
        if self.addrs is None:
            return False
        return self.addrs[0] <= addr <= self.addrs[1]

    def get_core_dump(self, timeval=-1, opslice=(0,-1)):
        if timeval == -1:
            timeval = 2**31-1 # a very high number
        if self.my_patches is None:
            self.my_patches = []
            for patch in self.forest.patches:
                patch_time, addr, content = patch
                if self.contains_patch(addr):
                    self.my_patches.append(patch)

        core_dump = []
        start,end = opslice
        if end == -1:
            end = len(opslice)
        for i, op in enumerate(self.ops[start:end]):
            dump = op.get_core_dump(self.addrs[0], self.my_patches, timeval)
            core_dump.append(dump)
        return ''.join(core_dump)

class TraceTree(object):
    pass

class TraceForest(object):
    def __init__(self):
        self.roots = []
        self.traces = []
        self.last_trace = None
        self.resops = {}
        self.timepos = 0
        self.patches = []

    def add_trace(self, trace_type, marker):
        tree = Trace(self, trace_type, self.timepos)
        self.traces.append(tree)
        if marker == MARKER_JITLOG_TRACE:
            self.time_tick()
        return tree

    def patch_memory(self, addr, content, timeval):
        self.patches.append((timeval, addr, content))

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
            rel_pos = fileobj.read_le_u16()
            dump = fileobj.read_string()
            flatop = trace.ops[-1]
            flatop.set_core_dump(rel_pos, dump)
        elif marker == MARKER_JITLOG_PATCH:
            addr = fileobj.read_le_addr()
            dump = fileobj.read_string()
            self.patch_memory(addr, dump, self.timepos)

