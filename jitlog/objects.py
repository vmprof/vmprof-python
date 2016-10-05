import os
import sys
import struct
import argparse
from collections import defaultdict
from jitlog import constants as const, merge_point

PY3 = sys.version_info[0] >= 3

class FlatOp(object):
    def __init__(self, opnum, opname, args, result,
                 descr=None, descr_number=None, failargs=None):
        self.opnum = opnum
        self.opname = opname
        self.args = args
        self.result = result
        self.descr = descr
        self.descr_number = descr_number
        self.core_dump = None
        self.failargs = failargs
        self.index = -1
        self.linkid = -1 # a unique id that is generated from the descr_number

    def get_name(self):
        return self.opname

    def getindex(self):
        return self.index

    def is_debug(self):
        return False

    def has_descr(self, descr=None):
        if not descr:
            return self.descr is not None
        return descr == self.descr_number

    def is_stitched(self):
        return False

    def get_descr_nmr(self):
        return self.descr_number

    def is_guard(self):
        return "guard" in self.opname

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
            descr = ', @' + str(descr)
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

class MergePoint(FlatOp):
    def __init__(self, values):
        assert isinstance(values, dict)
        self.values = values

    def get_name(self):
        return ""

    def is_debug(self):
        return True

    def get_scope(self):
        scope = const.MP_SCOPE[0]
        if scope in self.values:
            return self.values[scope]
        return ""

    def get_source_line(self):
        filename = None
        lineno = None
        for sem_type, value in self.values.items():
            if sem_type == const.MP_FILENAME[0]:
                filename = value
            if sem_type == const.MP_LINENO[0]:
                lineno = value
        if filename is None or lineno is None:
            return 0, None
        return lineno, filename

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

class Stage(object):
    def __init__(self, mark, timeval):
        self.mark = mark
        self.ops = []
        self.merge_points = []
        self.timeval = timeval
        self.stitch_points = []

    def get_last_op(self):
        if len(self.ops) == 0:
            return None
        return self.ops[-1]

    def get_op(self, i):
        if i < 0 or len(self.ops) <= i:
            return None
        return self.ops[i]

    def append_op(self, op):
        op.index = len(self.ops)
        if isinstance(op, MergePoint):
            self.merge_points.append(op)
        else:
            self.ops.append(op)

    def get_ops(self):
        return self.ops

    def get_merge_points(self):
        return self.merge_points

    def get_opcount(self):
        return len(self.ops)

    def is_asm(self):
        return self.mark == 'asm'

    def __repr__(self):
        return 'Stage("%s", %d ops)' % (self.mark, self.get_opcount())

from collections import namedtuple

class TraceLink(namedtuple('TraceLink', 'origin target')):
    pass

class Trace(object):
    def __init__(self, forest, trace_type, tick, unique_id, jd_name=None):
        self.forest = forest
        self.jd_name = jd_name
        self.type = trace_type
        self.inputargs = []
        assert self.type in ('loop', 'bridge')
        self.unique_id = unique_id
        self.stages = {}
        self.last_mark = None
        self.addrs = (-1,-1)
        # this saves a quadrupel for each
        self.my_patches = None
        self.counter = 0
        self.point_counters = {}
        self.merge_point_files = defaultdict(list)
        self.descr_number = 0 # the descr this trace is attached to
        # the trace links that lead to this trace
        self.links_up = []
        # the links leading from this trace
        self.links = []

    def link(self, op, target):
        self.links.append(TraceLink(PointInTrace(self, op), PointInTrace(target, None)))

    def backward_link(self, orig):
        """ connect parent with this trace. orig is the point
        in the parent trace, enter the first op in this trace """
        assert isinstance(orig, PointInTrace)
        enter = PointInTrace(self, None)
        self.links_up.append(TraceLink(orig, enter))

    def get_id(self):
        """ Return the unique id for that trace object """
        return self.unique_id

    def add_up_enter_count(self, count):
        self.counter += count

    def get_counter_points(self):
        d = {0: self.counter}
        d.update(self.point_counters)
        return d

    def get_stitched_descr_number(self):
        return self.descr_number

    def get_parent(self):
        for link in self.links_up:
            if link.origin and link.target.op is None:
                return link.origin.trace
        return None

    def get_first_merge_point(self):
        stage = self.get_stage('opt')
        if stage:
            mps = stage.get_merge_points()
            if len(mps) != 0:
                return mps[0]
        return None

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
        return self.stages.get(type, None)

    def start_mark(self, mark):
        mark_name = 'noopt'
        if mark == const.MARK_TRACE_OPT:
            mark_name = 'opt'
        elif mark == const.MARK_TRACE_ASM:
            mark_name = 'asm'
        else:
            assert mark == const.MARK_TRACE
            if self.last_mark == mark_name:
                # NOTE unrolling
                #
                # this case means that the optimizer has been invoked
                # twice (see compile_loop in rpython/jit/metainterp/compile.py)
                # and the loop was unrolled in between.
                #
                # we just return here, which means the following ops will just append the loop
                # ops to the preamble ops to the current stage!
                return
        self.last_mark = mark_name
        assert mark_name is not None
        if mark_name in self.stages:
            return self.stages[mark_name]
        tick = self.forest.timepos
        stage = Stage(mark_name, tick)
        self.stages[mark_name] = stage
        return stage

    def get_last_stage(self):
        return self.stages.get(self.last_mark, None)

    def set_core_dump_to_last_op(self, rel_pos, dump):
        assert self.last_mark is not None
        flatop = self.get_stage(self.last_mark).get_last_op()
        flatop.set_core_dump(rel_pos, dump)

    def add_instr(self, op):
        stage = self.get_stage(self.last_mark)
        stage.append_op(op)
        if stage.is_asm():
            if op.has_descr():
                stage.stitch_points.append(PointInTrace(self, op))
                nmr = op.get_descr_nmr()
                if nmr == 0x0:
                    sys.stderr.write("descr in trace %s should not be 0x0\n" % self)
                else:
                    dict = self.forest.descr_nmr_to_point_in_trace
                    # a label could already reside in that position
                    if nmr not in dict:
                        dict[nmr] = PointInTrace(self, op)
                    else:
                        pass
                        #sys.stderr.write("duplicate descr: 0x%x\n" % nmr)

                if op.get_name() == "label":
                    self.forest.labels[nmr] = PointInTrace(self, op)
                if op.get_name() == "jump":
                    self.forest.jumps[nmr] = PointInTrace(self, op)

        if op.get_name() == "increment_debug_counter":
            prev_op = stage.get_op(op.index-1)
            # look for the previous operation, it is a label saved
            # in descr_number_to_point_in_trace
            if prev_op:
                descr_number = prev_op.get_descr_nmr()
                pit = self.forest.get_point_in_trace_by_descr(descr_number)
                pit.set_inc_op(op)

        if isinstance(op, MergePoint):
            lineno, filename = op.get_source_line()
            if filename:
                self.merge_point_files[filename].append(lineno)

    def is_bridge(self):
        return self.type == 'bridge'

    def set_inputargs(self, args):
        self.inputargs = args

    def set_addr_bounds(self, a, b):
        self.addrs = (a,b)
        if a in self.forest.addrs:
            sys.stderr.write("jit log sets address bounds to a location another trace already is resident of\n")
        self.forest.addrs[a] = self

    def is_assembled(self):
        """ return True if the jit log indicated to have assembled this trace """
        return self.addrs[0] != -1

    def get_addrs(self):
        return tuple(self.addrs)

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
        for i, op in enumerate(stage.get_ops()):
            if start <= i <= end:
                dump = op.get_core_dump(self.addrs[0], self.my_patches, timeval)
                core_dump.append(dump)
        return ''.join(core_dump)

    def get_name(self):
        stage = self.get_stage('opt')
        if not stage:
            pass
        return 'unknown'

    def get_failing_guard(self):
        """ return the resoperation (FlatOp) for the descr this trace is attached to """
        if len(self.links_up) > 0:
            for link in self.links_up:
                origin = link.origin
                if origin and origin.op:
                    return origin.op
        return None

    def __repr__(self):
        return 'Trace(%d, 0x%x, %d)' % (self.unique_id, id(self), len(self.stages))

def iter_ranges(numbers):
    if len(numbers) == 0:
        raise StopIteration
    numbers.sort()
    first = numbers[0]
    last = numbers[0]
    for pos, i in enumerate(numbers[1:]):
        if (i - first) > 50:
            yield range(first, last+1)
            if pos+1 < len(numbers):
                last = i
                first = i
            else:
                raise StopIteration
        else:
            last = i
    yield range(first, last+1)

class PointInTrace(object):
    def __init__(self, trace, op):
        self.trace = trace
        self.op = op
        self.inc_op = None

    def enters_the_trace(self):
        return self.op is None

    def get_operation(self):
        return self.op

    def set_inc_op(self, op):
        if self.inc_op is None:
            self.inc_op = op

    def add_up_enter_count(self, count):
        if not self.inc_op:
            return False# this is a label!
        counters = self.trace.point_counters
        i = self.inc_op.index
        c = counters.get(i, 0)
        counters[i] = c + count
        return True

    def __repr__(self):
        return "PointInTrace(%s, op %s)" % (self.trace, self.op)

def decode_source(source_bytes):
    # copied from _bootstrap_external.py
    """Decode bytes representing source code and return the string.
    Universal newline support is used in the decoding.
    """
    import _io
    import tokenize  # To avoid bootstrap issues.
    source_bytes_readline = _io.BytesIO(source_bytes).readline
    encoding = tokenize.detect_encoding(source_bytes_readline)
    newline_decoder = _io.IncrementalNewlineDecoder(None, True)
    return newline_decoder.decode(source_bytes.decode(encoding[0]))

def read_python_source(file):
    with open(file, 'rb') as fd:
        data = fd.read()
        if PY3:
            data = decode_source(data)
        return data

class TraceForest(object):
    def __init__(self, version, is_32bit=False, machine=None):
        self.word_size = 4 if is_32bit else 8
        self.version = version
        self.machine = machine
        self.roots = []
        self.traces = {}
        self.addrs = {}
        self.last_trace = None
        self.resops = {}
        self.timepos = 0
        self.patches = []
        self.stitches = {}
        self.filepath = None
        # a mapping from source file name -> {lineno: (indent, line)}
        self.source_lines = defaultdict(dict)
        self.descr_nmr_to_point_in_trace = {}
        self.exc = None # holds an exception object if an error occured
        self.labels = {}
        self.jumps = {}
        self.redirect_descrs = {}
        self.linkid = 0

    def next_link_id(self):
        linkid = self.linkid
        self.linkid += 1
        return linkid

    def exception_raised(self):
        return self.exc

    def unlink_jitlog(self):
        if self.filepath and os.path.exists(self.filepath):
            os.unlink(self.filepath)
            self.filepath = None

    def get_point_in_trace_by_descr(self, descr):
        assert isinstance(descr, int)
        return self.descr_nmr_to_point_in_trace.get(descr, None)

    def get_source_line(self, filename, lineno):
        lines = self.source_lines.get(filename, None)
        if not lines:
            return None, None
        return lines.get(lineno, (None, None))

    def copy_and_add_source_code_tags(self):
        with open(self.filepath, "ab") as fd:
            blob = self.encode_source_code_lines()
            fd.write(blob)

    def extract_source_code_lines(self):
        if len(self.source_lines) > 0:
            # uploading the jitlog more than once, will append source code
            # to the jitlog, thus it will yield many duplicates. If there
            # is already source code attached, we skip this step
            return False
        file_contents = {}
        for _, trace in self.traces.items():
            for file, lines in trace.merge_point_files.items():
                if file not in file_contents:
                    if not os.path.exists(file):
                        continue
                    code = read_python_source(file)
                    file_contents[file] = code.splitlines()

                split_lines = file_contents[file]
                saved_lines = self.source_lines[file]
                for int_range in iter_ranges(lines):
                    for r in int_range:
                        line = split_lines[r-1]
                        data = line.lstrip()
                        diff = len(line) - len(data)
                        indent = diff
                        for i in range(0, diff):
                            if line[i] == '\t':
                                indent += 7
                        saved_lines[r] = (indent, data)
        return True

    def get_trace(self, id):
        return self.traces.get(id, None)

    def get_trace_by_addr(self, addr):
        return self.addrs.get(addr, None)

    def get_trace_by_id(self, id):
        return self.traces.get(id, None)

    def add_trace(self, trace_type, unique_id, trace_nmr, jd_name=None):
        """ Create a new trace object and attach it to the forest """
        trace = Trace(self, trace_type, self.timepos, unique_id, jd_name)
        trace.stamp = len(self.traces)
        self.traces[unique_id] = trace
        self.last_trace = trace
        return trace

    def stitch_bridge(self, descr_number, addr_to):
        assert isinstance(descr_number, int)
        bridge = self.get_trace_by_addr(addr_to)
        if bridge is None:
            raise Exception("bridge is None")
        #assert bridge.descr_number == 0, "a bridge can only be stitched once"
        #bridge.descr_number = descr_number
        # TODO remove this
        self.stitches[descr_number] = bridge.unique_id
        assert bridge is not None, ("no trace to be found for addr 0x%x" % addr_to)
        point_in_trace = self.get_point_in_trace_by_descr(descr_number)
        if not point_in_trace:
            sys.stderr.write("link to trace of descr 0x%x not found!\n" % descr_number)
        else:
            trace = point_in_trace.trace
            op = point_in_trace.op
            trace.link(op, bridge)
            bridge.backward_link(point_in_trace)

    def get_stitch_target(self, descr_number):
        assert isinstance(descr_number, int)
        return self.stitches.get(descr_number)

    def patch_memory(self, addr, content, timeval):
        self.patches.append((timeval, addr, content))

    def time_tick(self):
        self.timepos += 1

    def is_jitlog_marker(self, marker):
        if len(marker) == 0:
            return False
        assert len(marker) == 1
        return const.MARK_JITLOG_START <= marker <= const.MARK_JITLOG_END

    def encode_source_code_lines(self):
        marks = []
        for filename, lines in self.source_lines.items():
            marks.append(const.MARK_SOURCE_CODE)
            data = filename
            if PY3:
                data = data.encode('utf-8')
            marks.append(struct.pack('<I', len(data)))
            marks.append(data)

            marks.append(struct.pack('<H', len(lines)))
            for lineno, (indent, line) in lines.items():
                marks.append(struct.pack('<HBI', lineno, indent, len(line)))
                marks.append(line.encode('utf-8'))
        return b''.join(marks)

    def add_source_code_line(self, filename, lineno, indent, line):
        dict = self.source_lines[filename]
        if lineno in dict:
            sys.stderr.write("dup source code. %s line %d\n" % (filename, lineno))
        dict[lineno] = (indent, line)

    def add_tmp_callback(self, descr_nmr, uid=-1):
        self.redirect_descrs[descr_nmr] = uid

    def redirect_assembler(self, descr_number, new_descr_number, trace_id):
        assert isinstance(descr_number, int)
        self.stitches[descr_number] = trace_id
        if descr_number not in self.redirect_descrs:
            pass
        else:
            del self.redirect_descrs[descr_number]
        # setup the object properties
        trace = self.get_trace_by_id(trace_id)
        if trace is None:
            sys.stderr.write("could not redirect_assembler 0x%x 0x%x id 0x%x\n" % (descr_number, new_descr_number, trace_id))
            return
        point_in_trace = self.get_point_in_trace_by_descr(descr_number)
        if not point_in_trace:
            sys.stderr.write("redirect call assembler: link to trace of descr 0x%x not found trace %d!\n" % (descr_number, trace_id))
        else:
            parent = point_in_trace.trace
            op = point_in_trace.op
            parent.link(op, trace)
            trace.backward_link(point_in_trace)


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
