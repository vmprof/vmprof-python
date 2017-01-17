#include "trampoline.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <dlfcn.h>
#include <errno.h>
#include <stdint.h>
#include <unistd.h>
#if __APPLE__
#include <mach-o/dyld.h>
#endif
#include "machine.h"

#define PAGE_ALIGNED(a,size) (void*)(((uintptr_t)a) & ~0xfff) 

/*
 * The trampoline works the following way:
 *
 * cpython_vmprof_PyEval_EvalFrameEx called 'tramp' in the following
 *
 *          +--- PyEval_Loop ---+
 *     +----| jmp page          | <-- patched, original bits moved to page
 *     |    | asm instr 1       | <+- label PyEval
 *     |    | asm instr 2       |  |
 *     |    | ...               |  |
 *     |    +-------------------+  |
 *     |                           |
 *     +--->+--- page ----------+  |
 *          | push rbp          | <-- copied from PyEval_Loop
 *          | mov rsp -> rbp    |  |
 *          | ...               |  |
 *          | push rdi          | <-- save the frame, custom method
 *          | jmp PyEval        |--+
 *          +-------------------+
 */

static
int g_patched = 0;

static char * g_trampoline = NULL;
// the machine code size copied over from the callee
static int g_trampoline_length;

void _jmp_to(char * a, uintptr_t addr, int call) {

    // TODO 32-bit

    // moveabsq <addr>, <reg>
    a[0] = 0x48; // REX.W
    if (call) {
        a[1] = 0xb8; // %rax
    } else {
        a[1] = 0xba; // %rdx
    }
    a[2] = addr & 0xff;
    a[3] = (addr >> 8) & 0xff;
    a[4] = (addr >> 16) & 0xff;
    a[5] = (addr >> 24) & 0xff;
    a[6] = (addr >> 32) & 0xff;
    a[7] = (addr >> 40) & 0xff;
    a[8] = (addr >> 48) & 0xff;
    a[9] = (addr >> 56) & 0xff;

    if (call) {
        a[10] = 0xff;
        a[11] = 0xd0;
    } else {
        a[10] = 0xff;
        a[11] = 0xe2;
    }
}

int vmp_find_frameobj_on_stack(const char * callee_name) {
    return 0;
    char * callee_addr = (char*)dlsym(RTLD_DEFAULT, callee_name);
    struct ud u;
    const struct ud_operand * op1;
    const struct ud_operand * op2;
    int off = 0;
    int bytes = 0;
    int scan_bytes = 100;
    char * ptr = callee_addr;

    //asm("int $3");
    // 1) copy the instructions that should be redone in the trampoline
    while (bytes < scan_bytes) {
        int res = vmp_machine_code_instr_length(ptr, &u);
        if (res == 0) {
            return -1;
        }
        enum ud_mnemonic_code code = ud_insn_mnemonic(&u);
        printf("%s, %s\n", ud_lookup_mnemonic(code), ud_insn_asm(&u));
        //if (code == UD_Ipsubq || code == UD_Ivpsubq) {
        //    op1 = ud_insn_opr(&u, 0);
        //    op2 = ud_insn_opr(&u, 1);
        //    if (op1->type == UD_OP_IMM && op2->type == UD_OP_REG) {
        //        //off = op->lval.udword;
        //        //const char * c = ud_reg_tab[op->base - UD_R_AL];
        //    }
        //    asm("int $3");
        //}
        bytes += res;
        ptr += res;
    }
    return -10;
}

// a hilarious typo, tramp -> trump :)
int _redirect_trampoline_and_back(char * callee, char * trump) {

    char * trump_first_byte = trump;
    int needed_bytes = 12;
    int bytes = 0;
    char * ptr = callee;
    struct ud u;
    const struct ud_operand * op1;
    const struct ud_operand * op2;
    int off = 0;

    // 1) copy the instructions that should be redone in the trampoline
    while (bytes < needed_bytes) {
        int res = vmp_machine_code_instr_length(ptr, &u);
        enum ud_mnemonic_code code = ud_insn_mnemonic(&u);
        //if (code == UD_Ipsubq || code == UD_Ivpsubq) {
        //    op1 = ud_insn_opr(&u, 0);
        //    op2 = ud_insn_opr(&u, 1);
        //    if (op1->type == UD_OP_IMM && op2->type == UD_OP_REG) {
        //        //off = op->lval.udword;
        //        //const char * c = ud_reg_tab[op->base - UD_R_AL];
        //    }
        //    asm("int $3");
        //}
        if (res == 0) {
            return 1;
        }
        bytes += res;
        ptr += res;
    }
    g_trampoline_length = bytes;

    // 2) initiate the first few instructions of the eval loop
    (void)memcpy(trump, callee, bytes);
    // TODO 32bit
    _jmp_to(trump+bytes+6, (uintptr_t)callee+bytes, 0);

    // 3) overwrite the first few bytes of callee to jump to tramp
    // callee must call back 
    _jmp_to(callee, (uintptr_t)trump, 0);

    return 0;
}


int vmp_patch_callee_trampoline(const char * callee_name)
{
    void ** callee_addr = (void**)dlsym(RTLD_DEFAULT, callee_name);
    int result;
    int pagesize = sysconf(_SC_PAGESIZE);
    errno = 0;

    result = mprotect(PAGE_ALIGNED(callee_addr, pagesize), pagesize*2, PROT_READ|PROT_WRITE);
    if (result != 0) {
        fprintf(stderr, "read|write protecting callee_addr\n");
        return -1;
    }
    // create a new page and set it all of it writable
    char * page = (char*)mmap(NULL, pagesize, PROT_READ|PROT_WRITE|PROT_EXEC,
                              MAP_ANONYMOUS | MAP_PRIVATE, 0, 0);
    if (page == NULL) {
        return -1;
    }

    char * a = (char*)callee_addr;
    if (_redirect_trampoline_and_back(a, page) != 0) {
        return -1;
    }

    result = mprotect(PAGE_ALIGNED(callee_addr, pagesize), pagesize*2, PROT_READ|PROT_EXEC);
    if (result != 0) {
        fprintf(stderr, "read|exec protecting callee addr\n");
        return -1;
    }
    // revert, the page should not be writable any more now!
    result = mprotect((void*)page, pagesize, PROT_READ|PROT_EXEC);
    if (result != 0) {
        fprintf(stderr, "read|exec protecting tramp\n");
        return -1;
    }

    g_trampoline = page;

    return 0;
}

int vmp_unpatch_callee_trampoline(const char * callee_name)
{
    if (!g_patched) {
        return -1;
    }

    void ** callee_addr = (void**)dlsym(RTLD_DEFAULT, callee_name);
    int result;
    int pagesize = sysconf(_SC_PAGESIZE);
    errno = 0;

    result = mprotect(PAGE_ALIGNED(callee_addr, pagesize), pagesize*2, PROT_READ|PROT_WRITE);
    if (result != 0) {
        fprintf(stderr, "read|write protecting callee_addr\n");
        return 1;
    }

    // copy back as if nothing ever happened!!
    (void)memcpy(callee_addr, g_trampoline, g_trampoline_length);

    result = mprotect(PAGE_ALIGNED(callee_addr, pagesize), pagesize*2, PROT_READ|PROT_EXEC);
    if (result != 0) {
        fprintf(stderr, "read|exec protecting callee addr\n");
        return 1;
    }

    munmap(g_trampoline, pagesize);
    g_trampoline = NULL;
    g_trampoline_length = 0;

    return 0;
}
