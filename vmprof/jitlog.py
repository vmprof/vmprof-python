from vmprof.binary import (read_word, read_string,
        read_le_u16, read_le_addr, read_le_u64)
import struct
import argparse
from collections import defaultdict

MARK_JITLOG_INPUT_ARGS = b'\x10'
MARK_JITLOG_RESOP_META = b'\x11'
MARK_JITLOG_RESOP = b'\x12'
MARK_JITLOG_RESOP_DESCR = b'\x13'
MARK_JITLOG_ASM_ADDR = b'\x14'
MARK_JITLOG_ASM = b'\x15'
MARK_JITLOG_TRACE = b'\x16'
MARK_JITLOG_TRACE_OPT = b'\x17'
MARK_JITLOG_TRACE_ASM = b'\x18'
MARK_JITLOG_STITCH_BRIDGE= b'\x19'
MARK_JITLOG_COUNTER = b'\x20'
MARK_JITLOG_HEADER = b'\x23'
MARK_JITLOG_DEBUG_MERGE_POINT = b'\x24'
JITLOG_END = b'\x25' # do not prefix this one with MARKER, it is not used in the jitlog

JITLOG_MIN_VERSION = 1
JITLOG_VERSION = 1

def read_jitlog(filename):
    with open(str(filename), 'rb') as fileobj:
        forest = TraceForest()

        is_jit_log = fileobj.read(1) == MARK_JITLOG_HEADER
        version = ord(fileobj.read(1)) | (ord(fileobj.read(1)) << 8)
        assert is_jit_log, "Missing header. %s might not be a jitlog!" % filename
        assert version >= JITLOG_MIN_VERSION, \
                "Version does not match. Log is version %d%d is not satisfied" % \
                    (version, JITLOG_VERSION)
        while True:
            marker = fileobj.read(1)
            if marker == '':
                break # end of file!
            assert forest.is_jitlog_marker(marker), \
                    "marker unkown: 0x%x at pos 0x%x" % (ord(marker), fileobj.tell())
            forest.parse(fileobj, marker)
        return forest

class FlatOp(object):
    def __init__(self, opnum, opname, args, result, descr=None, descr_number=None):
        self.opnum = opnum
        self.opname = opname
        self.args = args
        self.result = result
        self.descr = descr
        self.descr_number = descr_number
        self.core_dump = None

    def has_descr(self, descr=None):
        if not descr:
            return self.descr is not None
        return descr == self.descr_number

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

    def pretty_print(self):
        suffix = ''
        if self.result is not None and self.result != '?':
            suffix = "%s = " % self.result
        descr = self.descr
        if descr is None:
            descr = ''
        else:
            descr = ', @' + descr
        return '%s%s(%s%s)' % (suffix, self.opname,
                                ', '.join(self.args), descr)

    def _serialize(self, trace_dict):
        dict = { 'num': self.opnum,
                 'args': self.args }
        if self.result:
            dict['res'] = self.result
        if self.descr:
            dict['descr'] = self.descr
        if self.core_dump:
             dict['dump'] = self.core_dump
        if self.descr_number:
             dict['descr_number'] = hex(self.descr_number)
        return dict

class MergePoint(FlatOp):
    def __init__(self, filename, lineno, enclosed, index, opname):
        self.filename = filename
        self.lineno = lineno
        self.enclosed = enclosed
        self.index = index
        self.opname = opname

    def has_descr(self, descr=None):
        return False

    def set_core_dump(self, rel_pos, core_dump):
        raise NotImplementedError

    def get_core_dump(self, base_addr, patches, timeval):
        raise NotImplementedError

    def __repr__(self):
        return 'debug_merge_point(xxx)'

    def pretty_print(self):
        return 'impl me debug merge point'

    def _serialize(self, trace_dict):
        dict = {}
        for prop in ['filename', 'lineno', 'enclosed', 'index', 'opname']:
            dict[prop] = getattr(self, prop)
        index = len(trace_dict['ops'])
        merge_points = trace_dict['merge_points']
        merge_points[index].append(dict)
        if len(merge_points) == 1:
            # fast access for the first debug merge point!
            merge_points['first'] = index
        return None

class Stage(object):
    def __init__(self, mark, timeval):
        self.mark = mark
        self.ops = []
        self.timeval = timeval

    def get_last_op(self):
        if len(self.ops) == 0:
            return None
        return self.ops[-1]

    def get_ops(self):
        return self.ops

class Trace(object):
    def __init__(self, forest, trace_type, tick, unique_id):
        self.forest = forest
        self.type = trace_type
        self.inputargs = []
        assert self.type in ('loop', 'bridge')
        self.unique_id = unique_id
        self.stages = {}
        self.last_mark = None
        self.addrs = (-1,-1)
        # this saves a quadrupel for each
        self.my_patches = None
        self.bridges = []
        self.descr_numbers = set()
        self.counter = 0

    def pretty_print(self, args):
        stage = self.stages.get(args.stage, None)
        if not stage:
            return ""
        resop = []

        for op in stage.ops:
            resop.append(op.pretty_print())

        return '\n'.join(resop)

    def get_stage(self, type):
        assert type is not None
        return self.stages[type]

    def stitch_bridge(self, timeval, descr_number, addr_to):
        self.bridges.append((timeval, descr_number, addr_to))

    def start_mark(self, mark, tick=0):
        mark_name = 'noopt'
        if mark == MARK_JITLOG_TRACE_OPT:
            mark_name = 'opt'
        elif mark == MARK_JITLOG_TRACE_ASM:
            mark_name = 'asm'
        self.last_mark = mark_name
        assert mark_name is not None
        self.stages[mark_name] = Stage(mark_name, tick)

    def set_core_dump_to_last_op(self, rel_pos, dump):
        assert self.last_mark is not None
        flatop = self.get_stage(self.last_mark).get_last_op()
        flatop.set_core_dump(rel_pos, dump)

    def add_instr(self, op):
        if op.has_descr():
            self.descr_numbers.add(op.descr_number)
        ops = self.get_stage(self.last_mark).get_ops()
        ops.append(op)

    def is_bridge(self):
        return self.type == 'bridge'

    def set_inputargs(self, args):
        self.inputargs = args

    def set_addr_bounds(self, a, b):
        self.addrs = (a,b)

    def contains_addr(self, addr):
        return self.addrs[0] <= addr <= self.addrs[1]

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
        stage = self.get_stage('asm')
        if not stage:
            return None # no core dump!
        for i, op in enumerate(stage.get_ops()[start:end]):
            dump = op.get_core_dump(self.addrs[0], self.my_patches, timeval)
            core_dump.append(dump)
        return ''.join(core_dump)

    def _serialize(self):
        bridges = []
        for bridge in self.bridges:
            bridges.append({ 'time': bridge[0],
                             'descr_number': hex(bridge[1]),
                             'target': hex(bridge[2]),
                           })
        stages = {}
        dict = { 'unique_id': hex(self.unique_id),
                 'type': self.type,
                 'args': self.inputargs,
                 'stages': stages,
                 'bridges': bridges,
                 'counter': self.counter,
               }

        for markname, stage in self.stages.items():
            ops = []
            # merge points is a dict mapping from index -> merge_points
            stage_dict = { 'ops': ops, 'tick': stage.timeval, 'merge_points': defaultdict(list) }
            stages[markname] = stage_dict
            for op in stage.ops:
                result = op._serialize(stage_dict)
                if result:
                    ops.append(result)
        import pdb; pdb.set_trace()
        if self.addrs != (-1,-1):
            dict['addr'] = (hex(self.addrs[0]), hex(self.addrs[1]))
        return dict


class TraceForest(object):
    def __init__(self, keep_data=True):
        self.roots = []
        self.traces = {}
        self.last_trace = None
        self.resops = {}
        self.timepos = 0
        self.patches = []
        self.keep = keep_data

    def get_trace(self, id):
        return self.traces.get(id, None)

    def add_trace(self, marker, trace_type, unique_id):
        trace = Trace(self, trace_type, self.timepos, unique_id)
        self.traces[unique_id] = trace
        if marker == MARK_JITLOG_TRACE:
            self.time_tick()
        return trace

    def stitch_bridge(self, descr_number, addr_to, timeval):
        for tid, trace in self.traces.items():
            if descr_number in trace.descr_numbers:
                trace.stitch_bridge(timeval, descr_number, addr_to)
                break
        else:
            raise NotImplementedError

    def patch_memory(self, addr, content, timeval):
        self.patches.append((timeval, addr, content))

    def time_tick(self):
        self.timepos += 1

    def is_jitlog_marker(self, marker):
        if marker == '':
            return False
        assert len(marker) == 1
        return MARK_JITLOG_INPUT_ARGS <= marker <= JITLOG_END

    def parse(self, fileobj, marker):
        trace = self.last_trace
        if marker == MARK_JITLOG_TRACE or \
           marker == MARK_JITLOG_TRACE_OPT or \
           marker == MARK_JITLOG_TRACE_ASM:
            trace_type = read_string(fileobj, True)
            unique_id = read_le_addr(fileobj)
            if trace_type == 'bridge':
                unique_id = read_le_addr(fileobj)
            # XXX remove name, there is no such thing as a name for
            # the loop. but extract it from debug merge point
            name = read_string(fileobj, True)
            if self.keep:
                if unique_id not in self.traces:
                    print("adding new trace with unique_id:", hex(unique_id))
                    trace = self.add_trace(marker, trace_type, unique_id)
                else:
                    print("found      trace with unique_id:", hex(unique_id))
                    trace = self.traces[unique_id]
                trace.start_mark(marker, self.timepos)
                self.last_trace = trace
                self.time_tick()
        elif marker == MARK_JITLOG_INPUT_ARGS:
            argnames = read_string(fileobj, True).split(',')
            if self.keep:
                trace.set_inputargs(argnames)
        elif marker == MARK_JITLOG_ASM_ADDR:
            addr1 = read_le_addr(fileobj)
            addr2 = read_le_addr(fileobj)
            if self.keep:
                trace.set_addr_bounds(addr1, addr2)
        elif marker == MARK_JITLOG_RESOP_META:
            assert len(self.resops) == 0
            count = read_le_u16(fileobj)
            for i in range(count):
                opnum = read_le_u16(fileobj)
                opname = read_string(fileobj, True)
                if self.keep:
                    self.resops[opnum] = opname
        elif marker == MARK_JITLOG_RESOP or \
             marker == MARK_JITLOG_RESOP_DESCR:
            opnum = read_le_u16(fileobj)
            args = read_string(fileobj, True).split(',')
            descr_number = -1
            if marker == MARK_JITLOG_RESOP_DESCR:
                descr_number = read_le_addr(fileobj)
            descr = None
            if not self.keep:
                return
            if marker == MARK_JITLOG_RESOP_DESCR:
                descr = args[-1]
                args = args[:-1]
            result = args[0]
            args = args[1:]
            if opnum not in self.resops:
                assert False, "opnum is not known: " + str(opnum) + \
                              " at binary pos " + str(hex(fileobj.tell()))
            opname = self.resops[opnum]
            op = FlatOp(opnum, opname, args, result, descr, descr_number)
            trace.add_instr(op)
        elif marker == MARK_JITLOG_ASM:
            rel_pos = read_le_u16(fileobj)
            dump = read_string(fileobj, True)
            if self.keep:
                trace.set_core_dump_to_last_op(rel_pos, dump)
        elif marker == MARK_JITLOG_STITCH_BRIDGE:
            descr_number = read_le_addr(fileobj)
            addr_tgt = read_le_addr(fileobj)
            if self.keep:
                self.stitch_bridge(descr_number, addr_tgt, self.timepos)
        elif marker == MARK_JITLOG_COUNTER:
            ident = read_le_addr(fileobj)
            count = read_le_addr(fileobj)
            trace = self.get_trace(ident)
            trace.counter += count
        elif marker == MARK_JITLOG_DEBUG_MERGE_POINT:
            filename = read_string(fileobj, True)
            lineno = read_le_u16(fileobj)
            enclosed = read_string(fileobj, True)
            index = read_le_u64(fileobj)
            opname = read_string(fileobj, True)
            trace.add_instr(MergePoint(filename, lineno, enclosed, index, opname))
        else:
            assert False, (marker, fileobj.tell())

    def _serialize(self):
        return {
            'resops': self.resops,
            'traces': [trace._serialize() for trace in self.traces.values()],
            'forest': None,
        }

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("jitlog")
    parser.add_argument("--stage", default='asm', help='Which stage should be outputted to stdout')
    args = parser.parse_args()

    trace_forest = read_jitlog(args.jitlog)
    print(trace_forest)
    stage = args.stage
    for _, trace in trace_forest.traces.items():
        text = trace.pretty_print(args)
        print(text)

if __name__ == '__main__':
    main()
