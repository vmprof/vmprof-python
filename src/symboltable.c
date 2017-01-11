#include "symboltable.h"

#include <stdio.h>
#include "_vmprof.h"

#ifdef _PY_TEST
#define LOG printf
#else
#define LOG
#endif

#ifdef __APPLE__

#include <mach-o/loader.h>
#include <mach-o/nlist.h>
#include <mach-o/stab.h>
#include <mach-o/dyld.h>
#include <mach-o/dyld_images.h>

void write_address_and_name(int fd, uint64_t e, const char * sym) {
    struct str {
        long addr;
        long size;
        char str[256];
    } s;
    s.addr = e;
    s.size = strlen(sym);
    if (s.size > 256) {
        s.size = 256;
    }
    (void)memcpy(s.str, sym, s.size);
    (void)write(fd, "\x08", 1); // MARKER_NATIVE_SYMBOLS as char[1]
    (void)write(fd, &s, sizeof(long)+sizeof(long)+s.size);
}

void dump_all_known_symbols(int fd) {
    const struct mach_header_64 * hdr;
    const struct symtab_command *sc;
    const struct nlist_64 * file_symtbl;
    const struct load_command *lc;
    int image_count = 0;

    image_count = _dyld_image_count();
    for (int i = 0; i < image_count; i++) {
        const char * image_name = _dyld_get_image_name(i);
        hdr = (const struct mach_header_64*)_dyld_get_image_header(i);
        LOG("searching mach-o image %s %llx\n", image_name, hdr);
        if (hdr->magic != MH_MAGIC_64) {
            continue;
        }
        uint32_t ft = hdr->filetype;
        if (ft == MH_DYLIB) {
            // TODO handle dylibs gracefully
            continue;
        }

        if (hdr->cputype != CPU_TYPE_X86_64) {
            continue;
        }

        lc = (const struct load_command *)(hdr + 1);

        LOG(" mach-o hdr has %d commands\n", hdr->ncmds);
        for (int j = 0; j < hdr->ncmds; j++) {
            if (lc->cmd == LC_SYMTAB) {
                LOG(" cmd %d/%d is LC_SYMTAB\n", j, hdr->ncmds);
                sc = (const struct symtab_command*) lc;
                const char * strtbl = (const char*)((const char*)hdr + sc->stroff);
                struct nlist_64 * l = (struct nlist_64*)((const char*)hdr + sc->symoff);
                LOG(" symtab has %d syms\n", sc->nsyms);
                for (int s = 0; s < sc->nsyms; s++) {
                    struct nlist_64 * entry = &l[s];
                    uint32_t t = entry->n_type;
                    if (t & N_EXT) {
                        uint32_t off = entry->n_un.n_strx;
                        if (off >= sc->strsize || off == 0) {
                            continue;
                        }
                        const char * sym = &strtbl[off];
                        uint64_t e = entry->n_value;
                        write_address_and_name(fd, e, sym);
                    }
                }
            }
            lc = (const struct load_command *)((char *)lc + lc->cmdsize);
        }
    }
}
#elif defined(__unix__)
void dump_all_known_symbols(int fd) {
    //write(fd, buf, size);
    xxx
}
#else
// other platforms than linux & mac os x
void dump_all_known_symbols(int fd) {
    // oh, nothing to do!! not supported platform
}
#endif
