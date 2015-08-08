from collections import namedtuple

class Frame(object):
    def __init__(self, name, tag=None):
        self.name = name
        self.tag = tag

    def __repr__(self):
        if self.tag:
            return 'Frame(%r, tag=%r)' % (self.name, self.tag)
        return 'Frame(%r)' % self.name

    def __eq__(self, other):
        return self.__dict__ == other.__dict__

    def __hash__(self):
        return hash(self.__dict__)



class SymbolicStackTrace(object):

    def __init__(self, stacktrace, addrspace):
        self.frames = []
        for addr in stacktrace:
            name, addr, is_virtual, lib = addrspace.lookup(addr)
            frame = Frame(name)
            if is_virtual:
                frame.tag = 'py'
            self.frames.append(frame)

    def __repr__(self):
        frames = ', '.join([frame.name for frame in self.frames])
        return '[%s]' % frames

    def __getitem__(self, i):
        return self.frames[i]

    def __len__(self):
        return len(self.frames)
