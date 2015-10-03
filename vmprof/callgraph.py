import sys
from collections import namedtuple, Counter
from vmprof.tagger import Tagger

Frame = namedtuple('Frame', ['name', 'addr', 'lib', 'is_virtual'])

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
            frame = Frame(name, addr, lib, is_virtual)
            self.frames.append(frame)

    def __repr__(self):
        frames = ', '.join([frame.name for frame in self.frames])
        return '[%s]' % frames

    def __getitem__(self, i):
        return self.frames[i]

    def __len__(self):
        return len(self.frames)


def unpack_frame_name(frame):
    name = frame.name
    filename = None
    line = None
    if frame.is_virtual:
        parts = frame.name.split(':')
        if len(parts) == 4:
            assert parts[0] == 'py'
            name = parts[1]
            line = parts[2]
            filename = parts[3]
    elif frame.lib:
        filename = frame.lib.name
    return name, filename, line



def remove_jit_hack(stacktrace):
    # The current version of vmprof uses an evil hack to mark virtual JIT
    # frames, surrounding them with two dummy addresses (JITSTACK_START and
    # JITSTACK_END). Suppose to have a function "a" containing a loop, and
    # inside the loop we call "b" which calls "c"; the stacktraces look like
    # this (bottom-to-top):
    #
    #     py:a
    #     call_assembler
    #     JITSTACK_START  # sentinel
    #     py:a            # duplicate
    #     py:b
    #     py:c
    #     JITSTACK_END
    #     jit:loop#1
    #
    # Note that py:a is specified twice: this is because the JIT codemap
    # correctly maps jit:loop#1 to "a->b->c", but this is wrong because there
    # should be only ONE python-level frame corresponding to "a"; entering the
    # JIT does not create a new python-level frame, it just "reuses" the
    # existing one.
    #
    # Note also that _vmprof emits stacktraces top-to-bottom, which means that
    # JITSTACK_START and JITSTACK_END are reversed (that's why START==0x02 and
    # END==0x01)
    #
    # This function removes the sentinels and the spurious, duplicate virtual
    # frame; the stacktrace above is translated to this:
    #
    #    py:a
    #    call_assembler
    #    py:b
    #    py:c
    #    jit:loop#1
    #
    # I claim that this hack is completely unnecessary, as we have better ways
    # to determine whether a particular address is JITted or not. I think that
    # we should just get rid of it at the _vmprof level, and directly
    # generated "non-hacked" stacktraces like the one above. In the meantime,
    # we manually remove the hack using this function.
    #
    def is_virtual(addr):
        return addr & 0x7000000000000000
    
    JITSTACK_START = 0x02
    JITSTACK_END = 0x01
    #
    new_stacktrace = []
    remove_next = False
    last_virtual = None
    for addr in stacktrace:
        if addr == JITSTACK_START:
            remove_next = True
            continue
        elif addr == JITSTACK_END:
            continue
        elif remove_next and addr == last_virtual:
            # remove this frame
            remove_next = False
        else:
            new_stacktrace.append(addr)
            remove_next = False
            if is_virtual(addr):
                last_virtual = addr
    return new_stacktrace


class CallGraph(object):

    def __init__(self, interp_name):
        self.tagger = Tagger.get(interp_name)
        self.root = StackFrameNode(Frame('<all>', 0, None, is_virtual=False))

    @classmethod
    def from_profiles(cls, interp_name, addrspace, profiles):
        callgraph = cls(interp_name)
        for profile in profiles:
            stacktrace, count, _ = profile
            # _vmprof produces stacktraces which are ordered from the top-most
            # frame to the bottom. add_stacktrace expects them in the opposite
            # order, so we need to reverse it first
            stacktrace.reverse()
            stacktrace = remove_jit_hack(stacktrace)
            stacktrace = SymbolicStackTrace(stacktrace, addrspace)
            callgraph.add_stacktrace(stacktrace, count)
        return callgraph

    def add_stacktrace(self, stacktrace, count):
        tags, topmost_tag = self.tagger.tag(stacktrace)
        #
        topmost_virtual_node = None
        node = self.root
        for frame, tag in zip(stacktrace, tags):
            node.cumulative_ticks[topmost_tag] += count
            node = node[frame]
            node.tag = tag
            if frame.is_virtual:
                topmost_virtual_node = node
        #
        # now node is the topmost node, fix its values
        node.self_ticks[tag] += count
        node.cumulative_ticks[tag] += count
        if topmost_virtual_node:
            topmost_virtual_node.virtual_ticks[tag] += count

    def get_virtual_root(self):
        """
        Transform the raw tree into a tree which contains only the virtual nodes
        """
        vroot = self.root.clone()
        for vchild in self.root.get_virtuals():
            vroot.children[vchild.frame] = vchild
        return vroot


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
        self.tag = None
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

    def clone(self):
        """
        Shallow clone: it returns a copy of itself, but with no children
        """
        res = StackFrameNode(self.frame)
        res.tag = self.tag
        res.self_ticks = self.self_ticks.copy()
        res.cumulative_ticks = self.cumulative_ticks.copy()
        if self.virtual_ticks is None:
            res.virtual_ticks = None
        else:
            res.virtual_ticks = self.virtual_ticks.copy()
        return res

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
        name = self.frame.name
        res = '%s: %s %s' % (name,
                             self.self_ticks._shortrepr('self'),
                             self.cumulative_ticks._shortrepr('cumulative'))
        if self.is_virtual:
            res += ' ' + self.virtual_ticks._shortrepr('virtual')
        return res

    def _get_children(self):
        def key(node):
            # virtual frames first, then order by total cumulative ticks, then
            # by name
            total_ticks = sum(node.cumulative_ticks.values())
            return (-node.is_virtual, -total_ticks, node.frame.name)
        children = self.children.values()
        children.sort(key=key)
        return children

    def pprint(self, indent=0, stream=None):
        if stream is None:
            stream = sys.stdout
        #
        print >> stream, '%s%s' % (' '*indent, self._shortrepr())
        for child in self._get_children():
            child.pprint(indent=indent+2, stream=stream)

    def serialize(self):
        """
        Turn the Python object into a dict which can be json-serialized
        """
        children = self._get_children()
        children = [child.serialize() for child in children]
        #
        name, filename, line = unpack_frame_name(self.frame)
        return dict(name = name,
                    filename = filename,
                    line = line,
                    is_virtual = self.is_virtual,
                    tag = self.tag,
                    self_ticks = self.self_ticks,
                    cumulative_ticks = self.cumulative_ticks,
                    virtual_ticks = self.virtual_ticks,
                    children = children,
                )


    def get_virtuals(self):
        """
        Return a list of virtual-only nodes to be attached to a parent node.
        """
        vchildren = []
        for child in self.children.itervalues():
            vchildren += child.get_virtuals()
        #
        if self.is_virtual:
            selfnode = self.clone()
            for vchild in vchildren:
                selfnode.children[vchild.frame] = vchild
            return [selfnode]
        #
        return self._merge(vchildren)

    def _merge(self, nodes):
        byframe = {}
        for node in nodes:
            try:
                old_node = byframe[node.frame]
            except KeyError:
                byframe[node.frame] = node
            else:
                old_node.self_ticks.update(node.self_ticks)
                old_node.cumulative_ticks.update(node.cumulative_ticks)
                old_node.virtual_ticks.update(node.virtual_ticks)
        #
        return byframe.values()
