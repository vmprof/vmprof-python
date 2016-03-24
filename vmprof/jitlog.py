from vmprof.binary import (read_word, read_string,
        read_le_u16, read_le_addr)

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

    def _serialize(self):
        dict = { 'num': self.opnum,
                 'args': self.args }
        if self.result:
            dict['res'] = self.result
        if self.descr:
            dict['descr'] = self.descr
        if self.core_dump:
             dict['dump'] = self.core_dump
        return dict

class Trace(object):
    def __init__(self, forest, trace_type, tick, unique_id, name):
        self.forest = forest
        self.type = trace_type
        assert self.type in ('loop', 'bridge')
        self.unique_id = unique_id
        self.name = name
        self.ops = None
        self.addrs = (-1,-1)
        # this saves a quadrupel for each
        self.my_patches = None

    def start_mark(self, mark, tick=0):
        mark_name = 'noopt'
        if mark == MARKER_JITLOG_TRACE_OPT:
            mark_name = 'opt'
        elif mark == MARKER_JITLOG_TRACE_ASM:
            mark_name = 'asm'
        if not self.ops:
            self.ops = []
        self.ops.append((mark_name, [], tick))

    def set_core_dump_to_last_op(self, rel_pos, dump):
        ops = self.ops[-1][1]
        flatop = ops[-1]
        flatop.set_core_dump(rel_pos, dump)

    def add_instr(self, opnum, opname, args, result, descr):
        ops = self.ops[-1][1]
        ops.append(FlatOp(opnum, opname, args, result, descr))

    def is_bridge(self):
        return self.type == 'bridge'

    def set_inputargs(self, args):
        self.inputargs = args

    def set_addr_bounds(self, a, b):
        self.addrs = (a,b)

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
        ops = None
        for mark, operations, tick in self.ops:
            if mark == 'asm':
                ops = operations
        if not ops:
            return None # no core dump!
        for i, op in enumerate(ops[start:end]):
            dump = op.get_core_dump(self.addrs[0], self.my_patches, timeval)
            core_dump.append(dump)
        return ''.join(core_dump)

    def _serialize(self):
        dict = { 'unique_id': hex(self.unique_id),
                 'name': self.name,
                 'type': self.type,
                 'args': self.inputargs,
                 'stages': {
                     markname : { 'ops': [ op._serialize() for op in ops ], \
                                  'tick': tick, } \
                     for markname, ops, tick in self.ops
                 }
               }
        if self.addrs != (-1,-1):
            dict['addr'] = (hex(self.addrs[0]), hex(self.addrs[1]))
        return dict


class TraceTree(object):
    pass

class TraceForest(object):
    def __init__(self, keep_data=True):
        self.roots = []
        self.traces = {}
        self.last_trace = None
        self.resops = {}
        self.timepos = 0
        self.patches = []
        self.keep = keep_data

    def add_trace(self, marker, trace_type, unique_id, name):
        trace = Trace(self, trace_type, self.timepos, unique_id, name)
        self.traces[unique_id] = trace
        if marker == MARKER_JITLOG_TRACE:
            self.time_tick()
        return trace

    def patch_memory(self, addr, content, timeval):
        self.patches.append((timeval, addr, content))

    def time_tick(self):
        self.timepos += 1

    def is_jitlog_marker(self, marker):
        if marker == '':
            return False
        return marker in (b'\x10\x11\x12'
                          b'\x13\x14\x15'
                          b'\x16\x17\x18\x19')

    def parse(self, fileobj, marker):
        trace = self.last_trace
        if marker == MARKER_JITLOG_TRACE or \
           marker == MARKER_JITLOG_TRACE_OPT or \
           marker == MARKER_JITLOG_TRACE_ASM:
            trace_type = read_string(fileobj, True)
            unique_id = read_le_addr(fileobj)
            name = read_string(fileobj, True)
            if self.keep:
                if unique_id not in self.traces:
                    trace = self.add_trace(marker, trace_type, unique_id, name)
                else:
                    trace = self.traces[unique_id]
                trace.start_mark(marker, self.timepos)
                self.last_trace = trace
                self.time_tick()
        elif marker == MARKER_JITLOG_INPUT_ARGS:
            argnames = read_string(fileobj, True).split(',')
            if self.keep:
                trace.set_inputargs(argnames)
        elif marker == MARKER_JITLOG_ASM_ADDR:
            addr1 = read_le_addr(fileobj)
            addr2 = read_le_addr(fileobj)
            if self.keep:
                trace.set_addr_bounds(addr1, addr2)
        elif marker == MARKER_JITLOG_RESOP_META:
            opnum = read_le_u16(fileobj)
            opname = read_string(fileobj, True)
            if self.keep:
                self.resops[opnum] = opname
        elif marker == MARKER_JITLOG_RESOP or \
             marker == MARKER_JITLOG_RESOP_DESCR:
            opnum = read_le_u16(fileobj)
            args = read_string(fileobj, True).split(',')
            descr = None
            if not self.keep:
                return
            if marker == MARKER_JITLOG_RESOP_DESCR:
                descr = args[-1]
                args = args[:-1]
            result = args[0]
            args = args[1:]
            trace.add_instr(opnum, self.resops[opnum], args, result, descr)
        elif marker == MARKER_JITLOG_ASM:
            rel_pos = read_le_u16(fileobj)
            dump = read_string(fileobj, True)
            if self.keep:
                trace.set_core_dump_to_last_op(rel_pos, dump)
        elif marker == MARKER_JITLOG_PATCH:
            addr = read_le_addr(fileobj)
            dump = read_string(fileobj, True)
            if self.keep:
                self.patch_memory(addr, dump, self.timepos)
        else:
            assert False, (marker, fileobj.tell())

    def _serialize(self):
        return {
            'resops': self.resops,
            'traces': [trace._serialize() for trace in self.traces.values()],
            'forest': None,
        }

