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
    return RPYTHON_TAGS.get(frame.name, 'C')

def rpython_tagger(stacktrace):
    # the logic to determine the tag for a specific stacktrace is as follow:
    #
    #   - by default, the tag is "C"
    #
    #   - if the frame is inside the special <JIT> lib, the tag is "JIT"
    #
    #   - if the frame is one of the special funcs listed in RPYTHON_TAGS, it
    #     uses the corresponding tag
    #
    #   - "WARMUP" frames propagates towards the top: i.e., the stacktrace
    #     [pypy_g_resume_in_blackhole, f, g] it is tagged as "WARMUP" because
    #     "g" is above pypy_g_resume_in_blackhole
    #
    #   - "WARMUP" frames stop to propagate as soon as we encounter a new JIT
    #     or virtual frame; i.e., [pypy_g_resume_in_blackhole, f, jit:loop1] -
    #     is tagged as "JIT", even if there is a "WARMUP" frame below
    #
    # Although this logic might seem complicate, it is needed to count GC
    # collections triggered by blackhole to be counted as WARMUP instead of GC
    #
    tag = 'C'
    for frame in stacktrace:
        newtag = tag_frame(frame)
        if frame.is_virtual or newtag == 'JIT':
            # reset the potential "WARMUP" status
            tag = newtag
        elif tag == 'WARMUP':
            # all the frames below a WARMUP frame count as warmup as well
            pass
        else:
            tag = newtag
    return tag


def cpython_tagger(stacktrace):
    return 'C'


def tagger_for_tests(stacktrace):
    """
    Tag frames whose name starts with jit: as "JIT"
    """
    topmost = stacktrace[-1]
    if topmost.name.startswith('jit:'):
        return 'JIT'
    return 'C'
