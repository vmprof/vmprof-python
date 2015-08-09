from collections import namedtuple

Frame = namedtuple('Frame', ['name', 'is_virtual'])


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
        node = self.root
        for frame in stacktrace:
            node = node[frame]
        node.self_count += count


class StackFrameNode(object):

    def __init__(self, frame):
        self.frame = frame
        self.self_count = 0
        self.children = {}

    def cumulative_count(self):
        res = self.self_count
        for child in self.children.values():
            res += child.cumulative_count()
        return res

    def __getitem__(self, frame):
        if isinstance(frame, str):
            frame = Frame(frame, False)
        #
        try:
            return self.children[frame]
        except KeyError:
            child = StackFrameNode(frame)
            self.children[frame] = child
            return child

    def __repr__(self):
        if self.children:
            children = '...'
        else:
            children = '{}'
        s = 'StackFrameNode(%r, self_count=%d, children=%s)'
        return s % (self.frame, self.self_count, children)

    def pprint(self, indent=0):
        print '%s%s: %s' % (' '*indent, self.frame.name, self.self_count)
        for child in self.children.values():
            child.pprint(indent=indent+2)
