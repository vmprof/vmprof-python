#include "symboltable.h"

#include <stdio.h>

#define LOG printf

#ifdef __APPLE__

#include <mach-o/loader.h>
#include <mach-o/nlist.h>
#include <mach-o/stab.h>
#include <mach-o/dyld.h>
#include <mach-o/dyld_images.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <mach/message.h>
#include <mach/kern_return.h>
#include <mach/task_info.h>
#include <sys/types.h>
#include <unistd.h>
#include <dlfcn.h>

void dump_all_known_symbols(int fd) {
    kern_return_t kr;
    task_t task;
    mach_vm_address_t addr;
    mach_vm_size_t vmsize;
    vm_region_top_info_data_t topinfo;
    mach_msg_type_number_t count;
    memory_object_name_t obj;
    const struct mach_header_64 * hdr;
    const struct symtab_command *sc;
    const struct nlist_64 * file_symtbl;
    const struct load_command *lc;
    int ret = 0;
    pid_t pid;

    pid = getpid();
    kr = task_for_pid(mach_task_self(), pid, &task);
    if (kr != KERN_SUCCESS) {
        goto teardown;
    }

    addr = 0;

    do {
        // extract the top info using vm_region
        count = VM_REGION_TOP_INFO_COUNT;
        vmsize = 0;
        kr = mach_vm_region(task, &addr, &vmsize, VM_REGION_TOP_INFO,
                          (vm_region_info_t)&topinfo, &count, &obj);
        if (kr == KERN_SUCCESS) {
            vm_address_t start = addr, end = addr + vmsize;
            // dladdr now gives the path of the shared object
            Dl_info info;
            if (dladdr((const void*)start, &info) == 0) {
                // could not find image containing start
                addr += vmsize;
                continue;
            }
            hdr = (struct mach_header_64*)info.dli_fbase;
            if (hdr->magic != MH_MAGIC_64) {
                addr += vmsize;
                continue;
            }

            if (hdr->cputype != CPU_TYPE_X86_64) {
                addr += vmsize;
                continue;
            }

            lc = (const struct load_command *)(hdr + 1);

            LOG(" mach-o hdr %s has %d commands\n", info.dli_fname, hdr->ncmds);
            for (int j = 0; j < hdr->ncmds; j++) {
                if (lc->cmd == LC_SYMTAB) {
                    LOG(" cmd %d/%d is LC_SYMTAB\n", j, hdr->ncmds);
                    sc = (const struct symtab_command*) lc;
                    const char * strtbl = (const char*)((const char*)hdr + sc->stroff);
                    struct nlist_64 * l = (struct nlist_64*)((const char*)hdr + sc->symoff);
                    printf(" symtab has %d syms\n", sc->nsyms);
                    for (int s = 0; s < sc->nsyms; s++) {
                        //printf("  sym %d/%d\n", s, sc->nsyms);
                        struct nlist_64 * entry = &l[s];
                        uint32_t t = entry->n_type;
                        if ((t & N_FUN) && (t & N_TYPE) != N_UNDF) {
                            uint32_t off = entry->n_un.n_strx;
                            if (off >= sc->strsize || off == 0) {
                                continue;
                            }
                            //LOG("  %d off i%d %s\n", off, s, &strtbl[off]);
                        }
                    }
                }
                lc = (const struct load_command *)((char *)lc + lc->cmdsize);
            }
            addr = addr + vmsize;
        } else if (kr != KERN_INVALID_ADDRESS) {
            goto teardown;
        }
    } while (kr == KERN_SUCCESS);

teardown:
    if (task != MACH_PORT_NULL) {
        mach_port_deallocate(mach_task_self(), task);
    }
    //return ret;
}
//    const struct mach_header_64 * hdr;
//    const struct symtab_command *sc;
//    const struct nlist_64 * file_symtbl;
//    const struct load_command *lc;
//    int image_count = 0;
//    intptr_t slide;
//    //
//    image_count = _dyld_image_count();
//    for (int i = 0; i < image_count; i++) {
//        const char * image_name = _dyld_get_image_name(i);
//        hdr = (const struct mach_header_64*)_dyld_get_image_header(i);
//        slide = _dyld_get_image_vmaddr_slide(i);
//        LOG("searching mach-o image %s %x %p\n", image_name, slide, hdr);
//}
#endif

#ifdef __unix__
void dump_all_known_symbols(int fd) {
    //write(fd, buf, size);
    xxx
}
#endif
