from vmprof.callgraph import Frame
from vmprof.tagger import rpython_tagger

class FakeLib(object):
    def __init__(self, name):
        self.name = name

FakeLib.libc = FakeLib('libc')
FakeLib.JIT = FakeLib('<JIT>')

class TestRPythonTagger:

    def stack(self, *args):
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

    def test_basic(self):
        stacktrace = self.stack('main', 'f', 'g')
        tag = rpython_tagger(stacktrace)
        assert tag == 'C'

    def test_jit(self):
        stacktrace = self.stack('main', 'f', 'jit:loop')
        tag = rpython_tagger(stacktrace)
        assert tag == 'JIT'
        #
        stacktrace = self.stack('main', 'f', 'jit:loop', 'g')
        tag = rpython_tagger(stacktrace)
        assert tag == 'C'

    def test_gc(self):
        stacktrace = self.stack('main', 'f',
                                'pypy_g_IncrementalMiniMarkGC_major_collection_step')
        tag = rpython_tagger(stacktrace)
        assert tag == 'GC:MAJOR'
        #
        stacktrace = self.stack('main', 'f',
                                'pypy_g_IncrementalMiniMarkGC_minor_collection.part.1')
        tag = rpython_tagger(stacktrace)
        assert tag == 'GC:MINOR'

    def test_warmup(self):
        stacktrace = self.stack('main', 'f', 'pypy_g_resume_in_blackhole')
        tag = rpython_tagger(stacktrace)
        assert tag == 'WARMUP'

    def test_gc_propagate(self):
        stacktrace = self.stack('main', 'f',
                                'pypy_g_IncrementalMiniMarkGC_major_collection_step',
                                'brk')
        tag = rpython_tagger(stacktrace)
        assert tag == 'GC:MAJOR'

    def test_warmup_propagate(self):
        # the frames g and h still counts as warmup, as they are above a warmup frame
        stacktrace = self.stack('main', 'f', 'pypy_g_resume_in_blackhole', 'g', 'h')
        tag = rpython_tagger(stacktrace)
        assert tag == 'WARMUP'
        #
        # this is valid in particular for GC frames
        stacktrace = self.stack('main', 'f',
                                'pypy_g_resume_in_blackhole',
                                'pypy_g_IncrementalMiniMarkGC_minor_collection')
        tag = rpython_tagger(stacktrace)
        assert tag == 'WARMUP'
        #
        # a WARMUP on top of GC counts as WARMUP ("WARMUP wins")
        stacktrace = self.stack('main', 'f',
                                'pypy_g_IncrementalMiniMarkGC_minor_collection',
                                'pypy_g_resume_in_blackhole')
        tag = rpython_tagger(stacktrace)
        assert tag == 'WARMUP'

    def test_warmup_reset(self):
        # if, for any reason, during warmup we re-enter the interpreter or the
        # JIT, the stacktrace no longer counts as warmup
        stacktrace = self.stack('main', 'f',
                                'pypy_g_resume_in_blackhole',
                                'py:main', 'g')
        tag = rpython_tagger(stacktrace)
        assert tag == 'C'
        #
        stacktrace = self.stack('main', 'f',
                                'pypy_g_resume_in_blackhole',
                                'jit:loop')
        tag = rpython_tagger(stacktrace)
        assert tag == 'JIT'
