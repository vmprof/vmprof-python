from vmprof.callgraph import Frame
from vmprof.tagger import rpython_tagger

class FakeLib(object):
    def __init__(self, name):
        self.name = name

FakeLib.libc = FakeLib('libc')
FakeLib.JIT = FakeLib('<JIT>')

class TestRPythonTagger:

    tagger = staticmethod(rpython_tagger)

    def get_stacktrace(self, args):
        stacktrace = []
        for arg in args:
            is_virtual = False
            lib = FakeLib.libc
            if arg.startswith('py:'):
                is_virtual = True
            elif arg.startswith('jit:'):
                lib = FakeLib.JIT
            frame = Frame(arg, 0x00, lib, is_virtual)
            stacktrace.append(frame)
        return stacktrace

    def tag(self, *args):
        stacktrace = self.get_stacktrace(args)
        tag = 'C'
        for frame in stacktrace:
            tag = self.tagger(tag, frame)
        return tag

    def test_basic(self):
        tag = self.tag('main', 'f', 'g')
        assert tag == 'C'

    def test_jit(self):
        tag = self.tag('main', 'f', 'jit:loop')
        assert tag == 'JIT'
        #
        tag = self.tag('main', 'f', 'jit:loop', 'g')
        assert tag == 'C'

    def test_gc(self):
        tag = self.tag('main', 'f', 'pypy_g_IncrementalMiniMarkGC_major_collection_step')
        assert tag == 'GC:MAJOR'
        #
        tag = self.tag('main', 'f', 'pypy_g_IncrementalMiniMarkGC_minor_collection.part.1')
        assert tag == 'GC:MINOR'

    def test_warmup(self):
        tag = self.tag('main', 'f', 'pypy_g_resume_in_blackhole')
        assert tag == 'WARMUP'

    def test_gc_propagate(self):
        tag = self.tag('main',
                       'pypy_g_IncrementalMiniMarkGC_major_collection_step',
                       'brk')
        assert tag == 'GC:MAJOR'

    def test_warmup_propagate(self):
        # the frames g and h still counts as warmup, as they are above a warmup frame
        tag = self.tag('main',
                       'pypy_g_resume_in_blackhole',
                       'g',
                       'h')
        assert tag == 'WARMUP'
        #
        # this is valid in particular for GC frames
        tag = self.tag('main',
                       'pypy_g_resume_in_blackhole',
                       'pypy_g_IncrementalMiniMarkGC_minor_collection')
        assert tag == 'WARMUP'
        #
        # a WARMUP on top of GC counts as WARMUP ("WARMUP wins")
        tag = self.tag('main',
                       'pypy_g_IncrementalMiniMarkGC_minor_collection',
                       'pypy_g_resume_in_blackhole')
        assert tag == 'WARMUP'

    def test_warmup_reset(self):
        # if, for any reason, during warmup we re-enter the interpreter or the
        # JIT, the stacktrace no longer counts as warmup
        tag = self.tag('main',
                       'pypy_g_resume_in_blackhole',
                       'py:main',
                       'g')
        assert tag == 'C'
        #
        tag = self.tag('main',
                       'pypy_g_resume_in_blackhole',
                       'jit:loop')
        assert tag == 'JIT'
