import sys
from collections import namedtuple, Counter

Frame = namedtuple('Frame', ['name', 'is_virtual'])

class TickCounter(Counter):

    def total(self):
        return sum(self.itervalues())

    def _shortrepr(self, label):
        tags = ['%s: %s' % (tag, count) for tag, count in sorted(self.iteritems())]
        return '%s{%s}' % (label, ', '.join(tags))


class SymbolicStackTrace(object):

    def __init__(self, stacktrace, addrspace):
        self.frames = []
        for addr in stacktrace:
            name, addr, is_virtual, lib = addrspace.lookup(addr)
            frame = Frame(name, is_virtual)
            self.frames.append(frame)

    def __repr__(self):
        frames = ', '.join([frame.name for frame in self.frames])
        return '[%s]' % frames

    def __getitem__(self, i):
        return self.frames[i]

    def __len__(self):
        return len(self.frames)


class CallGraph(object):

    def __init__(self):
        self.root = StackFrameNode(Frame('<all>', False))

    def add_stacktrace(self, stacktrace, count):
        tag = get_tag(stacktrace[-1])
        #
        topmost_virtual_node = None
        node = self.root
        for frame in stacktrace:
            node.cumulative_ticks[tag] += count
            node = node[frame]
            if frame.is_virtual:
                topmost_virtual_node = node
        #
        # now node is the topmost node, fix its values
        node.self_ticks[tag] += count
        node.cumulative_ticks[tag] += count
        if topmost_virtual_node:
            topmost_virtual_node.virtual_ticks[tag] += count

def get_tag(frame):
    # XXX this is a hack only for tests
    if frame.name.startswith('jit:'):
        return 'JIT'
    else:
        return 'C'


class StackFrameNode(object):
    """
    Represent a function frame on the stack.

    Frames can be virtual or non-virtual:

      - virtual frames correspond to python-level functions (or other
        high-level languages, in case vmprof is used to profile other
        languages than Python).

      - non-virtual frames are the actual C frames which are found on the CPU
        stack

    Conceptually, virtual frames are never on the top of the stack, they
    simply act as the parent of the non-virtual frames which actually execute
    them. For example, in case of PyPy, the "py:foo" virtual frame might be
    executed by e.g. pyframe_execute_frame, or by one or more JITted code (or
    both).

    The "ticks" count how often the frame has been sampled on the stack:

      - self_ticks: the time spent executing this specific frame, i.e. the
        number of times the frame was on the top of the stack

      - cumulative_ticks: the time spent on this frame + the time spent on all
        its descendants

      - virtual_ticks: for virtual frames only, the time spent executing this
        specific virtual frame: note that virtual frames are never on top of
        the stack, so this accounts for the time spent in C and/or JIT frames
        such as pyframe_execute_frame etc. It is equivalent to the
        cumulative_ticks if we take into account only the stacktraces which do
        not contain any other virtual frame on top of the current one.

    For example, suppose to have a python function ``foo`` which calls
    ``bar``, we might have a call graph like this (virtual frames are py:foo
    and py:bar)::

    +--py:bar virtual-ticks-+ +-------------py:foo virtual-ticks--------------+
    |                       | |                                               |
    v                       v v                                               v

    +=======================+
    | pyframe_execute_frame | +-self-ticks----+ +-self-ticks--+ +-self-ticks--+
    +=======================+ | pyframe_      | | JIT loop #1 | | JIT loop #2 |
    |        py:bar         | | execute_frame | |             | |             |
    +=======================+ v               v v             v v             v
    |     CALL_FUNCTION     |
    +=======================+==================+===============+===============+
    |          pyframe_execute_frame           |  JIT loop #1  |  JIT loop #2  |
    +==========================================+===============+===============+
    |                                 py:foo                                   |
    +==========================================================================+
    """

    def __init__(self, frame):
        self.frame = frame
        self.children = {}
        self.tag = get_tag(frame)
        #
        self.self_ticks = TickCounter()
        self.cumulative_ticks = TickCounter()
        if self.is_virtual:
            self.virtual_ticks = TickCounter()
        else:
            self.virtual_ticks = None

    @property
    def is_virtual(self):
        return self.frame.is_virtual

    def __getitem__(self, frame):
        if isinstance(frame, str):
            # lookup by symbol name is read-only
            name = frame
            frame = self._lookup_children(name)
            if frame is None:
                raise KeyError(name)
        #
        try:
            return self.children[frame]
        except KeyError:
            child = StackFrameNode(frame)
            self.children[frame] = child
            return child

    def _lookup_children(self, name):
        for child in self.children:
            if child.name == name:
                return child
        return None

    def __repr__(self):
        return '<StackFrameNode %s>' % self._shortrepr()

    def _shortrepr(self):
        res = '%s: %s %s' % (self.frame.name,
                             self.self_ticks._shortrepr('self'),
                             self.cumulative_ticks._shortrepr('cumulative'))
        if self.is_virtual:
            res += ' ' + self.virtual_ticks._shortrepr('virtual')
        return res

    def pprint(self, indent=0, stream=None):
        if stream is None:
            stream = sys.stdout
        #
        print >> stream, '%s%s' % (' '*indent, self._shortrepr())
        for child in self.children.values():
            child.pprint(indent=indent+2, stream=stream)
