import re

class Tagger(object):
    """
    Base class to tag stacktraces
    """

    @staticmethod
    def get(interp_name):
        if interp_name == 'cpython':
            return CPythonTagger()
        elif interp_name == 'pypy':
            return RPythonTagger()
        elif interp_name == 'test':
            return TaggerForTests()
        else:
            raise ValueError("Unknown interpreter: %s" % interp_name)

    def tag(self, stacktrace):
        """
        Assign a tag to each frame in the stacktrace.

        Return a list of tags (in the same order as in stacktrace) and the tag
        of the topmost frame.
        """
        tag = 'C'
        tags = []
        for frame in stacktrace:
            tag = self.tag_single_frame(tag, frame)
            tags.append(tag)
        return tags, tag

    def tag_single_frame(self, curtag, frame):
        """
        Return the tag for this specific frame, where ``curtag`` is the tag of the
        caller
        """
        raise NotImplementedError


    def _tag_C_frame(self, curtag, frame):
        if frame.lib and frame.lib.is_external:
            return 'EXT'
        elif curtag == 'EXT' and frame.lib and frame.lib.is_libc:
            return 'EXT'
        else:
            return 'C'

class CPythonTagger(Tagger):
    """
    Tag everything as "C"
    """

    def tag_single_frame(self, curtag, frame):
        return self._tag_C_frame(curtag, frame)


class TaggerForTests(Tagger):
    """
    Tag frames whose name starts with jit: as "JIT"
    """

    def tag_single_frame(self, curtag, frame):
        if frame.name.startswith('jit:'):
            return 'JIT'
        return 'C'


class RPythonTagger(Tagger):
    """
    Tag RPython/PyPy stacktraces.

    The logic to tag a specific frame is as follows:
    
      - if the frame is inside the special <JIT> lib, the tag is "JIT"
    
      - if the frame is one of the special funcs listed in RPYTHON_TAGS, it
        uses the corresponding tag
    
      - GC and WARMUP frames propagates towards the top: i.e., the stacktrace
        [pypy_g_resume_in_blackhole, f, g] it is tagged as "WARMUP" because
        "g" is above pypy_g_resume_in_blackhole
    
      - propagation stops as soon as we encounter a new JIT
        or virtual frame; i.e., [pypy_g_resume_in_blackhole, f, jit:loop1] -
        is tagged as "JIT", even if there is a "WARMUP" frame below
    
      - WARMUP tags "win" over GC tags: i.e., if there is a GC frame above a
        WARMUP frame, the stacktrace is still tagged as WARMUP
    """

    # matches things like .part.1 in function names
    RE_PART = re.compile(r'\.part\.[0-9]+$')

    RPYTHON_TAGS = {
        'pypy_g_resume_in_blackhole': 'WARMUP',
        'pypy_g_MetaInterp__compile_and_run_once': 'WARMUP',
        'pypy_g_ResumeGuardDescr__trace_and_compile_from_bridge': 'WARMUP',
        #
        'pypy_g_IncrementalMiniMarkGC_gc_step_until': 'GC:MAJOR',
        'pypy_g_IncrementalMiniMarkGC_major_collection_step': 'GC:MAJOR',
        'pypy_g_IncrementalMiniMarkGC_minor_collection': 'GC:MINOR',
    }

    def is_gc(self, tag):
        return tag.startswith('GC:')

    def is_warmup(self, tag):
        return tag == 'WARMUP'

    def _tag_frame(self, frame):
        if frame.lib and frame.lib.name == '<JIT>':
            return 'JIT'
        name = frame.name
        name = self.RE_PART.sub('', name) # remove the .part.* suffix
        return self.RPYTHON_TAGS.get(name, 'C')

    def tag_single_frame(self, curtag, frame):
        newtag = self._tag_frame(frame)
        if newtag == 'C':
            # propagate EXT in case it calls libc
            newtag = self._tag_C_frame(curtag, frame)
        #
        if frame.is_virtual or newtag in ('JIT', 'EXT'):
            # reset the tag, stopping the propagation of oldtag
            return newtag
        elif self.is_warmup(curtag):
            return 'WARMUP'
        elif self.is_gc(curtag):
            # GC frames during blackholing count as WARMUP (WARMUP "wins" over GC)
            if self.is_warmup(newtag):
                return 'WARMUP'
            else:
                return curtag
        else:
            # normal case
            return newtag

