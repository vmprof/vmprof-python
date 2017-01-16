#include "symboltable.h"

#include <stdio.h>
#include "_vmprof.h"

#ifdef _PY_TEST
#define LOG(...) printf(__VA_ARGS__)
#else
#define LOG(...)
#endif

static
void _write_address_and_name(int fd, uint64_t e, const char * sym) {
    struct str {
        long addr;
        long size;
        char str[512];
    } s;
    s.addr = e;
    /* must mach '<lang>:<name>:<line>:<file>'
     * 'n' has been chosen as lang here, because the symbol
     * can be generated from several languages (e.g. C, C++, ...)
     */
    s.str[0] = 'n';
    s.str[1] = ':';
    s.size = strlen(sym);
    if (s.size > 256) {
        s.size = 256;
    }
    (void)memcpy(s.str + 2, sym, s.size);
    // line number and filename missing
    (void)memcpy(s.str + 2 + s.size, ":0:-", 4);
    s.size += 4;
    (void)write(fd, "\x08", 1); // MARKER_NATIVE_SYMBOLS
    (void)write(fd, &s, sizeof(long)+sizeof(long)+s.size);
}

#ifdef __APPLE__

#include <mach-o/loader.h>
#include <mach-o/nlist.h>
#include <mach-o/stab.h>
#include <mach-o/dyld.h>
#include <mach-o/dyld_images.h>

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
        LOG("searching mach-o image %s %llx\n", image_name, (void*)hdr);
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
        for (int j = 0; j < hdr->ncmds; j++, (lc = (const struct load_command *)((char *)lc + lc->cmdsize))) {
            if (lc->cmd == LC_SYMTAB) {
                LOG(" cmd %d/%d is LC_SYMTAB\n", j, hdr->ncmds);
                sc = (const struct symtab_command*) lc;
                // skip if symtab entry is not populated
                if (sc->symoff == 0) {
                    LOG("symoff == 0\n");
                    continue;
                } else if (sc->stroff == 0) {
                    LOG("stroff == 0\n");
                    continue;
                } else if (sc->nsyms == 0) {
                    LOG("nsym == 0\n");
                    continue;
                } else if (sc->strsize == 0) {
                    LOG("strsize == 0\n");
                    continue;
                }
                const char * strtbl = (const char*)((const char*)hdr + sc->stroff);
                struct nlist_64 * l = (struct nlist_64*)((const char*)hdr + sc->symoff);
                LOG(" symtab has %d syms\n", sc->nsyms);
                for (int s = 0; s < sc->nsyms; s++) {
                    struct nlist_64 * entry = &l[s];
                    uint32_t t = entry->n_type;
                    if ((N_STAB & t) & N_FUN) {
                        uint32_t off = entry->n_un.n_strx;
                        if (off >= sc->strsize || off == 0) {
                            continue;
                        }
                        const char * sym = &strtbl[off];
                        printf("---> %s %x\n", sym, entry->n_type);
                        uint64_t e = entry->n_value;
                        _write_address_and_name(fd, e, sym);
                    }
                }
            }
        }
    }
}
#elif defined(__unix__)

#include <link.h>

#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <string.h>
#include <elf.h>
#include <sys/auxv.h>
#include <link.h>
#include <sys/mman.h>
#include <bits/wordsize.h>

#if __WORDSIZE == 64
#define ELF_R_SYM ELF64_R_SYM
#define ELF_ST_BIND ELF64_ST_BIND
#define ELF_ST_TYPE ELF64_ST_TYPE
#elif __WORDSIZE == 32
#define ELF_R_SYM ELF32_R_SYM
#define ELF_ST_BIND ELF32_ST_BIND
#define ELF_ST_TYPE ELF32_ST_TYPE
#else
#error "unsupported word size"
#endif

#define F_SYMTAB 0x00001
#define F_STRTAB 0x00002
#define F_RELA   0x00004

typedef struct _stab {
    int flags;
    char * strtab;
    ElfW(Xword) strtab_size;

    ElfW(Sym) * symtab;
    ElfW(Xword) symtab_size;

    ElfW(Rela) *rela;
    ElfW(Xword) rela_size;
    ElfW(Xword) rela_ent;
} stab_t;

int _load_info_from(ElfW(Addr) base, stab_t * t, const ElfW(Phdr) * phdr) {
    ElfW(Dyn) *dyn;
    int result = 0;

    for (dyn = (ElfW(Dyn) *)(base + phdr->p_vaddr); dyn->d_tag; dyn++) {
        if (dyn->d_tag == DT_SYMTAB) {
            t->symtab = (ElfW(Sym)*)dyn->d_un.d_ptr;
            result |= F_SYMTAB;
        } else if (dyn->d_tag == DT_SYMENT) {
            t->symtab_size = dyn->d_un.d_val;
        } else if (dyn->d_tag == DT_STRTAB) {
            t->strtab = (char *)dyn->d_un.d_ptr;
            result |= F_STRTAB;
        } else if (dyn->d_tag == DT_STRSZ) {
            t->strtab_size = dyn->d_un.d_val;
        } else if (dyn->d_tag == DT_RELA) {
            t->rela = (ElfW(Rela)*)dyn->d_un.d_ptr;
            result |= F_RELA;
        } else if (dyn->d_tag == DT_RELASZ) {
            t->rela_size = dyn->d_un.d_val;
        } else if (dyn->d_tag == DT_RELAENT) {
            t->rela_ent = dyn->d_un.d_val;
        }
    }
    t->flags = result;
    return ((F_SYMTAB | F_STRTAB) & result) != 0;
}

static int _dump_symbols2(ElfW(Addr) base, stab_t * table, int fd) {
    ElfW(Rela) *rela;
    ElfW(Rela) *relaend;
    ElfW(Sym) * sym;

    if (table->flags & F_RELA) {
        // yeah, go ahead, relocation could be found!!
        relaend = (ElfW(Rela)*)((char*)table->rela + table->rela_size);
        for (rela = table->rela; rela < relaend; rela++) {
            sym = &table->symtab[ELF_R_SYM(rela->r_info)];
            if (ELF_ST_TYPE(sym->st_info) != STT_FUNC) {
                continue;
            }
            char * name = table->strtab + sym->st_name;
            if (strlen(name) <= 0) {
                continue;
            }
            uint64_t addr = base + rela->r_offset;
            //LOG("%s\n", name);
            _write_address_and_name(fd, addr, name);
        }
    }
    return 0;
}

static int iter_shared_objects(struct dl_phdr_info *info, size_t size, void *data) {
    int fd = *((int*)data);
    uint16_t phentsize = getauxval(AT_PHENT);
    LOG("shared object %s\n", info->dlpi_name);
    stab_t table;
    int r;

    int16_t phnum = info->dlpi_phnum;
    const ElfW(Phdr) * phdr = info->dlpi_phdr;
    ElfW(Addr) base = info->dlpi_addr;
    for (int i = 0; i < phnum; i++, (phdr = (ElfW(Phdr) *)((char *)phdr + phentsize))) {
        if (phdr->p_type != PT_DYNAMIC) {
            continue;
        }
        r = _load_info_from(base, &table, phdr);
        if (!r) {
            continue;
        }
        (void)_dump_symbols2(base, &table, fd);
    }
    return 0;
}

void dump_all_known_symbols(int fd) {
    (void)dl_iterate_phdr(iter_shared_objects, (void*)&fd);
}
#else
// other platforms than linux & mac os x
void dump_all_known_symbols(int fd) {
    // oh, nothing to do!! a not supported platform
}
#endif
