import re

RE_PART = re.compile(r'\.part\.[0-9]+$') # matches things like .part.1 in function names

RPYTHON_TAGS = {
    'pypy_g_resume_in_blackhole': 'WARMUP',
    'pypy_g_MetaInterp__compile_and_run_once': 'WARMUP',
    'pypy_g_ResumeGuardDescr__trace_and_compile_from_bridge': 'WARMUP',
    'pypy_g_IncrementalMiniMarkGC_major_collection_step': 'GC:MAJOR',
    'pypy_g_IncrementalMiniMarkGC_minor_collection': 'GC:MINOR',

}

def tag_frame(frame):
    if frame.lib and frame.lib.name == '<JIT>':
        return 'JIT'
    name = frame.name
    name = RE_PART.sub('', name) # remove the .part.* suffix
    return RPYTHON_TAGS.get(name, 'C')

def is_gc(tag):
    return tag.startswith('GC:')

def is_warmup(tag):
    return tag == 'WARMUP'

def rpython_tagger(curtag, frame):
    # the logic to determine the tag for a specific stacktrace is as follow:
    #
    #   - if the frame is inside the special <JIT> lib, the tag is "JIT"
    #
    #   - if the frame is one of the special funcs listed in RPYTHON_TAGS, it
    #     uses the corresponding tag
    #
    #   - GC and WARMUP frames propagates towards the top: i.e., the stacktrace
    #     [pypy_g_resume_in_blackhole, f, g] it is tagged as "WARMUP" because
    #     "g" is above pypy_g_resume_in_blackhole
    #
    #   - propagation stops as soon as we encounter a new JIT
    #     or virtual frame; i.e., [pypy_g_resume_in_blackhole, f, jit:loop1] -
    #     is tagged as "JIT", even if there is a "WARMUP" frame below
    #
    #   - WARMUP tags "win" over GC tags: i.e., if there is a GC frame above a
    #     WARMUP frame, the stacktrace is still tagged as WARMUP
    #
    newtag = tag_frame(frame)
    if frame.is_virtual or newtag in ('JIT', 'WARMUP'):
        # reset the tag, stopping the propagation of oldtag. Note that GC is
        # not listed (because WARMUP "win" over GC).
        return newtag
    elif is_warmup(curtag) or is_gc(curtag):
        # propagate the current tag
        return curtag
    else:
        # normal case
        return newtag


def cpython_tagger(curtag, frame):
    return 'C'


def tagger_for_tests(curtag, frame):
    """
    Tag frames whose name starts with jit: as "JIT"
    """
    if frame.name.startswith('jit:'):
        return 'JIT'
    return 'C'
