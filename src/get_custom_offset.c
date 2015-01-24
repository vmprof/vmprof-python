#define UNW_LOCAL_ONLY
#include <libunwind.h>

static ptrdiff_t vmprof_unw_get_custom_offset(void* ip, unw_cursor_t *cp) {
    // XXX remove hardcoded addresses
    if (ip >= 0x0000000040000000 && ip <= 0x0000000040000017) {
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
