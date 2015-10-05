from vmprof.callgraph import Frame
from vmprof.tagger import RPythonTagger

class FakeLib(object):
    def __init__(self, name, is_external=False, is_libc=False):
        self.name = name
        self.is_libc = is_libc
        self.is_external = is_external

FakeLib.libc = FakeLib('libc', is_libc=True)
FakeLib.JIT = FakeLib('<JIT>')
FakeLib.libz = FakeLib('libz', is_external=True)

class TestRPythonTagger:

    tagger = RPythonTagger()

    def get_stacktrace(self, *args):
        stacktrace = []
        for arg in args:
            is_virtual = False
            lib = FakeLib.libc
            if arg.startswith('py:'):
                is_virtual = True
            elif arg.startswith('jit:'):
                lib = FakeLib.JIT
            elif arg.startswith('z:'):
                lib = FakeLib.libz
            frame = Frame(arg, 0x00, lib, is_virtual)
            stacktrace.append(frame)
        return stacktrace

    def tag(self, *args):
        stacktrace = self.get_stacktrace(*args)
        tags, topmost_tag = self.tagger.tag(stacktrace)
        return topmost_tag

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

    def test_external(self):
        tag = self.tag('a', 'z:foo')
        assert tag == 'EXT'

    def test_external_calling_libc(self):
        # libc functions called from an EXT lib count as EXT as well. Else, we
        # risk to miscount things like malloc and qsort, which are part of the
        # EXT
        tag = self.tag('a', 'z:foo', 'malloc')
        assert tag == 'EXT'

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
        # JIT, or call an EXT lib, the stacktrace no longer counts as warmup
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
        #
        tag = self.tag('main',
                       'pypy_g_resume_in_blackhole',
                       'z:foo')
        assert tag == 'EXT'


    def test_taglist(self):
        stacktrace = self.get_stacktrace(
            'main',                                            # C
            'pypy_g_resume_in_blackhole',                      # WARMUP
            'f',                                               # WARMUP
            'pypy_g_IncrementalMiniMarkGC_minor_collection',   # WARMUP
            'g',                                               # WARMUP
            'py:main',                                         # C
            'h',                                               # C
            'pypy_g_IncrementalMiniMarkGC_minor_collection',   # GC:MINOR
            'k')                                               # GC:MINOR
        tags, topmost_tag = self.tagger.tag(stacktrace)
        assert tags == ['C', 'WARMUP', 'WARMUP', 'WARMUP', 'WARMUP',
                        'C', 'C', 'GC:MINOR', 'GC:MINOR']
