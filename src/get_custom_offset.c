// XXX: this should be part of _vmprof (the CPython extension), not vmprof
// (the library)

#define UNW_LOCAL_ONLY
#include <libunwind.h>

static void *tramp_start, *tramp_end;

void vmprof_set_tramp_range(void *start, void *end) {
    tramp_start = start;
    tramp_end = end;
}

int custom_sanity_check()
{
    return 1;
}

static ptrdiff_t vmprof_unw_get_custom_offset(void* ip, unw_cursor_t *cp) {
    // XXX remove hardcoded addresses
    if (ip >= tramp_start && ip <= tramp_end) {
        void *bp;
        void *sp;
        
        /* This is a stage2 trampoline created by hotpatch:

               push   %rbx
               push   %rbp
               mov    %rsp,%rbp
               and    $0xfffffffffffffff0,%rsp   // make sure the stack is aligned
               movabs $0x7ffff687bb10,%rbx
               callq  *%rbx
               leaveq 
               pop    %rbx
               retq   

           the stack layout is like this:

               +-----------+                      high addresses
               | ret addr  |
               +-----------+
               | saved rbx |   start of the function frame
               +-----------+
               | saved rbp |
               +-----------+
               | ........  |   <-- rbp
               +-----------+                      low addresses

           So, the trampoline frame starts at rbp+16, and the return address,
           is at rbp+24.  The vmprof API requires us to return the offset of
           the frame relative to sp, hence we have this weird computation.

           XXX (antocuni): I think we could change the API to return directly
           the frame address instead of the offset; however, this require a
           change in the PyPy code too
        */

        unw_get_reg (cp, UNW_REG_SP, (unw_word_t*)&sp);
        unw_get_reg (cp, UNW_X86_64_RBP, (unw_word_t*)&bp);
        return bp+16+8-sp;
    }
	return -1;
}

static long vmprof_write_header_for_jit_addr(void **result, long n,
											 void *ip, int max_depth)
{
	return n;
}
