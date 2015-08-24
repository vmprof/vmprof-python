/*
  Copyright (C) 2000,2004,2005 Silicon Graphics, Inc.  All Rights Reserved.
  Portions Copyright (C) 2007-2012 David Anderson. All Rights Reserved.
  Portions Copyright (C) 2011-2012 SN Systems Ltd. All Rights Reserved

  This program is free software; you can redistribute it and/or modify it
  under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation.

  This program is distributed in the hope that it would be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

  Further, this software is distributed without any warranty that it is
  free of the rightful claim of any third person regarding infringement
  or the like.  Any license provided herein, whether implied or
  otherwise, applies only to this software file.  Patent licenses, if
  any, provided herein do not apply to combinations of this program with
  other software, or any other product whatsoever.

  You should have received a copy of the GNU General Public License along
  with this program; if not, write the Free Software Foundation, Inc., 51
  Franklin Street - Fifth Floor, Boston MA 02110-1301, USA.

*/




#include "globals.h"
#include "print_reloc.h"

#define DW_SECTION_REL_DEBUG_NUM      9  /* Number of sections */
/* .debug_abbrev should never have a relocation applied to it as it
   never refers to another section! (other sections refer to
   .debug_abbrev) */

#define DW_SECTNAME_RELA_DEBUG          ".rela.debug_"
#define DW_SECTNAME_RELA_DEBUG_INFO     ".rela.debug_info"
#define DW_SECTNAME_RELA_DEBUG_LINE     ".rela.debug_line"
#define DW_SECTNAME_RELA_DEBUG_PUBNAMES ".rela.debug_pubnames"
#define DW_SECTNAME_RELA_DEBUG_ABBREV   ".rela.debug_abbrev"
#define DW_SECTNAME_RELA_DEBUG_ARANGES  ".rela.debug_aranges"
#define DW_SECTNAME_RELA_DEBUG_FRAME    ".rela.debug_frame"
#define DW_SECTNAME_RELA_DEBUG_LOC      ".rela.debug_loc"
#define DW_SECTNAME_RELA_DEBUG_RANGES   ".rela.debug_ranges"
#define DW_SECTNAME_RELA_DEBUG_TYPES    ".rela.debug_types"

#define DW_SECTNAME_REL_DEBUG          ".rel.debug_"
#define DW_SECTNAME_REL_DEBUG_INFO     ".rel.debug_info"
#define DW_SECTNAME_REL_DEBUG_LINE     ".rel.debug_line"
#define DW_SECTNAME_REL_DEBUG_PUBNAMES ".rel.debug_pubnames"
#define DW_SECTNAME_REL_DEBUG_ABBREV   ".rel.debug_abbrev"
#define DW_SECTNAME_REL_DEBUG_ARANGES  ".rel.debug_aranges"
#define DW_SECTNAME_REL_DEBUG_FRAME    ".rel.debug_frame"
#define DW_SECTNAME_REL_DEBUG_LOC      ".rel.debug_loc"
#define DW_SECTNAME_REL_DEBUG_RANGES   ".rel.debug_ranges"
#define DW_SECTNAME_REL_DEBUG_TYPES    ".rel.debug_types"


#define STRING_FOR_DUPLICATE " duplicate"
#define STRING_FOR_NULL      " null"

static char *sectnames[] = {
    DW_SECTNAME_REL_DEBUG_INFO,
    DW_SECTNAME_REL_DEBUG_LINE,
    DW_SECTNAME_REL_DEBUG_PUBNAMES,
    DW_SECTNAME_REL_DEBUG_ABBREV,
    DW_SECTNAME_REL_DEBUG_ARANGES,
    DW_SECTNAME_REL_DEBUG_FRAME,
    DW_SECTNAME_REL_DEBUG_LOC,
    DW_SECTNAME_REL_DEBUG_RANGES,
    DW_SECTNAME_REL_DEBUG_TYPES,
};
static char *sectnamesa[] = {
    DW_SECTNAME_RELA_DEBUG_INFO,
    DW_SECTNAME_RELA_DEBUG_LINE,
    DW_SECTNAME_RELA_DEBUG_PUBNAMES,
    DW_SECTNAME_RELA_DEBUG_ABBREV,
    DW_SECTNAME_RELA_DEBUG_ARANGES,
    DW_SECTNAME_RELA_DEBUG_FRAME,
    DW_SECTNAME_RELA_DEBUG_LOC,
    DW_SECTNAME_RELA_DEBUG_RANGES,
    DW_SECTNAME_RELA_DEBUG_TYPES,
};

static char *error_msg_duplicate[] = {
    DW_SECTNAME_REL_DEBUG_INFO STRING_FOR_DUPLICATE,
    DW_SECTNAME_REL_DEBUG_LINE STRING_FOR_DUPLICATE,
    DW_SECTNAME_REL_DEBUG_PUBNAMES STRING_FOR_DUPLICATE,
    DW_SECTNAME_REL_DEBUG_ABBREV STRING_FOR_DUPLICATE,
    DW_SECTNAME_REL_DEBUG_ARANGES STRING_FOR_DUPLICATE,
    DW_SECTNAME_REL_DEBUG_FRAME STRING_FOR_DUPLICATE,
    DW_SECTNAME_REL_DEBUG_LOC STRING_FOR_DUPLICATE,
    DW_SECTNAME_REL_DEBUG_RANGES STRING_FOR_DUPLICATE,
    DW_SECTNAME_REL_DEBUG_TYPES STRING_FOR_DUPLICATE,
};

static char *error_msg_null[] = {
    DW_SECTNAME_REL_DEBUG_INFO STRING_FOR_NULL,
    DW_SECTNAME_REL_DEBUG_LINE STRING_FOR_NULL,
    DW_SECTNAME_REL_DEBUG_PUBNAMES STRING_FOR_NULL,
    DW_SECTNAME_REL_DEBUG_ABBREV STRING_FOR_NULL,
    DW_SECTNAME_REL_DEBUG_ARANGES STRING_FOR_NULL,
    DW_SECTNAME_REL_DEBUG_FRAME STRING_FOR_NULL,
    DW_SECTNAME_REL_DEBUG_LOC STRING_FOR_NULL,
    DW_SECTNAME_REL_DEBUG_RANGES STRING_FOR_NULL,
    DW_SECTNAME_REL_DEBUG_TYPES STRING_FOR_NULL,
};

/*  Include Section type, to be able to deal with all the
    Elf32_Rel, Elf32_Rela, Elf64_Rel, Elf64_Rela relocation types */
#define SECT_DATA_SET(x,t,n) {                                      \
    if (sect_data[(x)].buf != NULL) {                               \
        print_error(dbg, error_msg_duplicate[(x)], DW_DLV_OK, err); \
    }                                                               \
    if ((data = elf_getdata(scn, 0)) == NULL || data->d_size == 0) {\
        print_error(dbg, error_msg_null[(x)],DW_DLV_OK, err);       \
    }                                                               \
    sect_data[(x)].buf = data -> d_buf;                             \
    sect_data[(x)].size = data -> d_size;                           \
    sect_data[(x)].type = t;                                        \
    sect_data[(x)].name = n;                                        \
    }
/* Record the relocation table name information */
static const char **reloc_type_names = NULL;
static Dwarf_Small number_of_reloc_type_names = 0;

/* Set the relocation names based on the machine type */
static void
set_relocation_table_names(Dwarf_Small machine_type)
{
    reloc_type_names = 0;
    number_of_reloc_type_names = 0;
    switch (machine_type) {
    case EM_MIPS:
#ifdef DWARF_RELOC_MIPS
        reloc_type_names = reloc_type_names_MIPS;
        number_of_reloc_type_names =
            sizeof(reloc_type_names_MIPS) / sizeof(char *);
#endif /* DWARF_RELOC_MIPS */
        break;
    case EM_PPC:
#ifdef DWARF_RELOC_PPC
        reloc_type_names = reloc_type_names_PPC;
        number_of_reloc_type_names =
            sizeof(reloc_type_names_PPC) / sizeof(char *);
#endif /* DWARF_RELOC_PPC */
        break;
    case EM_PPC64:
#ifdef DWARF_RELOC_PPC64
        reloc_type_names = reloc_type_names_PPC64;
        number_of_reloc_type_names =
            sizeof(reloc_type_names_PPC64) / sizeof(char *);
#endif /* DWARF_RELOC_PPC64 */
        break;
    case EM_ARM:
#ifdef DWARF_RELOC_ARM
        reloc_type_names = reloc_type_names_ARM;
        number_of_reloc_type_names =
            sizeof(reloc_type_names_ARM) / sizeof(char *);
#endif /* DWARF_RELOC_ARM */
        break;
    case EM_X86_64:
#ifdef DWARF_RELOC_X86_64
        reloc_type_names = reloc_type_names_X86_64;
        number_of_reloc_type_names =
            sizeof(reloc_type_names_X86_64) / sizeof(char *);
#endif /* DWARF_RELOC_X86_64 */
        break;
    default:
        /* We don't have others covered. */
        reloc_type_names = 0;
        number_of_reloc_type_names = 0;
        break;
  }
}

/*
    Return valid reloc type names.
    If buf is used, it is static, so beware: it
    will be overwritten by the next call.
*/
static const char *
get_reloc_type_names(int index)
{
    static char buf[100];
    const char *retval = 0;

    if (index < 0 || index >= number_of_reloc_type_names) {
        sprintf(buf, "reloc type %d unknown", (int) index);
        retval = buf;
    } else {
        retval = reloc_type_names[index];
    }
    return retval;
}

#ifndef HAVE_ELF64_GETEHDR
#define Elf64_Addr  long
#define Elf64_Word  unsigned long
#define Elf64_Xword unsigned long
#define Elf64_Sym   long
#endif

static struct {
    Dwarf_Small *buf;
    Dwarf_Unsigned size;
    Dwarf_Bool display; /* Display reloc if TRUE */
    const char *name;         /* Section name */
    Elf64_Xword type;   /* To cover 32 and 64 records types */
} sect_data[DW_SECTION_REL_DEBUG_NUM];


typedef size_t indx_type;

typedef struct {
    indx_type indx;
    char *name;
    Elf32_Addr value;
    Elf32_Word size;
    int type;
    int bind;
    unsigned char other;
    Elf32_Half shndx;
} SYM;


typedef struct {
    indx_type indx;
    char *name;
    Elf64_Addr value;
    Elf64_Xword size;
    int type;
    int bind;
    unsigned char other;
    unsigned short shndx;
} SYM64;

static void print_reloc_information_64(int section_no,
    Dwarf_Small * buf,
    Dwarf_Unsigned size,
    Elf64_Xword type,
    char **scn_names,int scn_names_count);
static void print_reloc_information_32(int section_no,
    Dwarf_Small * buf,
    Dwarf_Unsigned size,
    Elf64_Xword type,
    char **scn_names,int scn_names_count);
static SYM *readsyms(Elf32_Sym * data, size_t num, Elf * elf,
    Elf32_Word link);
static SYM64 *read_64_syms(Elf64_Sym * data, size_t num, Elf * elf,
    Elf64_Word link);
static void *get_scndata(Elf_Scn * fd_scn, size_t * scn_size);
static void print_relocinfo_64(Dwarf_Debug dbg, Elf * elf);
static void print_relocinfo_32(Dwarf_Debug dbg, Elf * elf);

static SYM   *sym_data;
static SYM64 *sym_data_64;
static unsigned long   sym_data_entry_count;
static unsigned long   sym_data_64_entry_count;

typedef struct {
    indx_type index;
    char *name_rel;     /* .rel.debug_* names  */
    char *name_rela;    /* .rela.debug_* names */
} REL_INFO;

/*  If the incoming scn_name is known, record the name
    in our reloc section names table.
    For a given (debug) section there can be a .rel or a .rela,
    not both.
    The name-to-index in this table is fixed, invariant.
    It has to match other tables like
*/
static int
get_reloc_section(Dwarf_Debug dbg,
    Elf_Scn *scn,
    char *scn_name,
    Elf64_Word sh_type)
{
    static REL_INFO rel_info[DW_SECTION_REL_DEBUG_NUM] = {
    {/*0*/ DW_SECTION_REL_DEBUG_INFO,
    DW_SECTNAME_REL_DEBUG_INFO,
    DW_SECTNAME_RELA_DEBUG_INFO},

    {/*1*/ DW_SECTION_REL_DEBUG_LINE,
    DW_SECTNAME_REL_DEBUG_LINE,
    DW_SECTNAME_RELA_DEBUG_LINE},

    {/*2*/ DW_SECTION_REL_DEBUG_PUBNAMES,
    DW_SECTNAME_REL_DEBUG_PUBNAMES,
    DW_SECTNAME_RELA_DEBUG_PUBNAMES},

    {/*3*/ DW_SECTION_REL_DEBUG_ABBREV,
    DW_SECTNAME_REL_DEBUG_ABBREV,
    DW_SECTNAME_RELA_DEBUG_ABBREV},

    {/*4*/ DW_SECTION_REL_DEBUG_ARANGES,
    DW_SECTNAME_REL_DEBUG_ARANGES,
    DW_SECTNAME_RELA_DEBUG_ARANGES},

    {/*5*/ DW_SECTION_REL_DEBUG_FRAME,
    DW_SECTNAME_REL_DEBUG_FRAME,
    DW_SECTNAME_RELA_DEBUG_FRAME},

    {/*6*/ DW_SECTION_REL_DEBUG_LOC,
    DW_SECTNAME_REL_DEBUG_LOC,
    DW_SECTNAME_RELA_DEBUG_LOC},

    {/*7*/ DW_SECTION_REL_DEBUG_RANGES,
    DW_SECTNAME_REL_DEBUG_RANGES,
    DW_SECTNAME_RELA_DEBUG_RANGES},

    {/*8*/ DW_SECTION_REL_DEBUG_TYPES,
    DW_SECTNAME_REL_DEBUG_TYPES,
    DW_SECTNAME_RELA_DEBUG_TYPES},
    };

    Elf_Data *data;
    int index;
    for (index = 0; index < DW_SECTION_REL_DEBUG_NUM; ++index) {
        const char *n = rel_info[index].name_rel;
        const char *na = rel_info[index].name_rela;
        if (strcmp(scn_name, n) == 0) {
            SECT_DATA_SET(rel_info[index].index,sh_type,n)
            return TRUE;
        }
        if (strcmp(scn_name, na) == 0) {
            SECT_DATA_SET(rel_info[index].index,sh_type,na)
            return TRUE;
        }
    }
    return FALSE;
}

void
print_relocinfo(Dwarf_Debug dbg, unsigned reloc_map)
{
    Elf *elf;
    char *endr_ident;
    int is_64bit;
    int res;
    int i;

    for (i = 0; i < DW_SECTION_REL_DEBUG_NUM; i++) {
        sect_data[i].display = reloc_map & (1 << i);
        sect_data[i].buf = 0;
        sect_data[i].size = 0;
        sect_data[i].type = SHT_NULL;
    }
    res = dwarf_get_elf(dbg, &elf, &err);
    if (res != DW_DLV_OK) {
        print_error(dbg, "dwarf_get_elf error", res, err);
    }
    if ((endr_ident = elf_getident(elf, NULL)) == NULL) {
        print_error(dbg, "DW_ELF_GETIDENT_ERROR", res, err);
    }
    is_64bit = (endr_ident[EI_CLASS] == ELFCLASS64);
    if (is_64bit) {
        print_relocinfo_64(dbg, elf);
    } else {
        print_relocinfo_32(dbg, elf);
    }
}

static void
print_relocinfo_64(Dwarf_Debug dbg, Elf * elf)
{
#ifdef HAVE_ELF64_GETEHDR
    Elf_Scn *scn = NULL;
    Elf64_Ehdr *ehdr64 = 0;
    Elf64_Shdr *shdr64 = 0;
    char *scn_name = 0;
    int i = 0;
    Elf64_Sym *sym_64 = 0;
    char **scn_names = 0;
    int scn_names_cnt = 0;

    ehdr64 = elf64_getehdr(elf);
    if (ehdr64 == NULL) {
        print_error(dbg, "DW_ELF_GETEHDR_ERROR", DW_DLV_OK, err);
    }

    /*  Make the section name array big enough
        that we don't need to check for overrun in the loop. */
    scn_names = (char **)calloc(ehdr64->e_shnum + 1, sizeof(char *));

    while ((scn = elf_nextscn(elf, scn)) != NULL) {

        shdr64 = elf64_getshdr(scn);
        if (shdr64 == NULL) {
            print_error(dbg, "DW_ELF_GETSHDR_ERROR", DW_DLV_OK, err);
        }
        scn_name = elf_strptr(elf, ehdr64->e_shstrndx, shdr64->sh_name);
        if (scn_name  == NULL) {
            print_error(dbg, "DW_ELF_STRPTR_ERROR", DW_DLV_OK, err);
        }

        if (scn_names) {
            /* elf_nextscn() skips section with index '0' */
            scn_names[++scn_names_cnt] = scn_name;
        }
        if (shdr64->sh_type == SHT_SYMTAB) {
            size_t sym_size = 0;
            size_t count = 0;

            sym_64 = (Elf64_Sym *) get_scndata(scn, &sym_size);
            if (sym_64 == NULL) {
                print_error(dbg, "no symbol table data", DW_DLV_OK,
                    err);
            }
            count = sym_size / sizeof(Elf64_Sym);
            sym_64++;
            free(sym_data_64);
            sym_data_64 = read_64_syms(sym_64, count, elf, shdr64->sh_link);
            sym_data_64_entry_count = count;
            if (sym_data_64  == NULL) {
                print_error(dbg, "problem reading symbol table data",
                    DW_DLV_OK, err);
            }
        } else if (!get_reloc_section(dbg,scn,scn_name,shdr64->sh_type)) {
            continue;
        }
    }                           /* while */

    /* Set the relocation names based on the machine type */
    set_relocation_table_names(ehdr64->e_machine);

    for (i = 0; i < DW_SECTION_REL_DEBUG_NUM; i++) {
        if (sect_data[i].display &&
            sect_data[i].buf != NULL &&
            sect_data[i].size > 0) {
            print_reloc_information_64(i, sect_data[i].buf,
                sect_data[i].size, sect_data[i].type,scn_names,
                scn_names_cnt);
        }
    }

    if (scn_names) {
        free(scn_names);
        scn_names = 0;
        scn_names_cnt = 0;
    }
#endif
}

static void
print_relocinfo_32(Dwarf_Debug dbg, Elf * elf)
{
    Elf_Scn *scn = NULL;
    Elf32_Ehdr *ehdr32 = 0;
    Elf32_Shdr *shdr32 = 0;
    char *scn_name = 0;
    int i = 0;
    Elf32_Sym  *sym = 0;
    char **scn_names = 0;
    int scn_names_cnt = 0;

    ehdr32 = elf32_getehdr(elf);
    if (ehdr32 == NULL) {
        print_error(dbg, "DW_ELF_GETEHDR_ERROR", DW_DLV_OK, err);
    }

    /*  Make the section name array big enough
        that we don't need to check for overrun in the loop. */
    scn_names = (char **)calloc(ehdr32->e_shnum + 1, sizeof(char *));

    while ((scn = elf_nextscn(elf, scn)) != NULL) {
        shdr32 = elf32_getshdr(scn);
        if (shdr32 == NULL) {
            print_error(dbg, "DW_ELF_GETSHDR_ERROR", DW_DLV_OK, err);
        }
        scn_name = elf_strptr(elf, ehdr32->e_shstrndx, shdr32->sh_name);
        if (scn_name == NULL) {
            print_error(dbg, "DW_ELF_STRPTR_ERROR", DW_DLV_OK, err);
        }

        if (scn_names) {
            /* elf_nextscn() skips section with index '0' */
            scn_names[++scn_names_cnt] = scn_name;
        }

        if (shdr32->sh_type == SHT_SYMTAB) {
            size_t sym_size = 0;
            size_t count = 0;

            sym = (Elf32_Sym *) get_scndata(scn, &sym_size);
            if (sym == NULL) {
                print_error(dbg, "no symbol table data", DW_DLV_OK,
                    err);
            }
            count = sym_size / sizeof(Elf32_Sym);
            sym++;
            free(sym_data);
            sym_data = readsyms(sym, count, elf, shdr32->sh_link);
            sym_data_entry_count = count;
            if (sym_data  == NULL) {
                print_error(dbg, "problem reading symbol table data",
                    DW_DLV_OK, err);
            }
        } else if (!get_reloc_section(dbg,scn,scn_name,shdr32->sh_type)) {
            continue;
        }
    }  /* End while. */

    /* Set the relocation names based on the machine type */
    set_relocation_table_names(ehdr32->e_machine);

    for (i = 0; i < DW_SECTION_REL_DEBUG_NUM; i++) {
        if (sect_data[i].display &&
            sect_data[i].buf != NULL &&
            sect_data[i].size > 0) {
            print_reloc_information_32(i, sect_data[i].buf,
                sect_data[i].size,sect_data[i].type,scn_names,
                scn_names_cnt);
        }
    }

    if (scn_names) {
        free(scn_names);
        scn_names = 0;
        scn_names_cnt = 0;
    }
}

#if HAVE_ELF64_R_INFO
#ifndef ELF64_R_TYPE
#define ELF64_R_TYPE(x) 0       /* FIXME */
#endif
#ifndef ELF64_R_SYM
#define ELF64_R_SYM(x) 0        /* FIXME */
#endif
#ifndef ELF64_ST_TYPE
#define ELF64_ST_TYPE(x) 0      /* FIXME */
#endif
#ifndef ELF64_ST_BIND
#define ELF64_ST_BIND(x) 0      /* FIXME */
#endif
#endif /* HAVE_ELF64_R_INFO */


static void
print_reloc_information_64(int section_no, Dwarf_Small * buf,
    Dwarf_Unsigned size, Elf64_Xword type,
    char **scn_names, int scn_names_count)
{
    /* Include support for Elf64_Rel and Elf64_Rela */
    Dwarf_Unsigned add = 0;
    Dwarf_Half rel_size = SHT_RELA == type ?
        sizeof(Elf64_Rela) : sizeof(Elf64_Rel);
    Dwarf_Unsigned off = 0;

    printf("\n%s:\n", type== SHT_RELA?sectnamesa[section_no]:
        sectnames[section_no]);

    /* Print some headers and change the order for better reading */
    printf("Offset     Addend     %-26s Index   Symbol Name\n","Reloc Type");

#if HAVE_ELF64_GETEHDR
    for (off = 0; off < size; off += rel_size) {
#if HAVE_ELF64_R_INFO
        /* This works for the Elf64_Rel in linux */
        Elf64_Rel *p = (Elf64_Rel *) (buf + off);
        char *name = 0;
        if (sym_data ) {
            size_t index = ELF64_R_SYM(p->r_info) - 1;
            if (index < sym_data_entry_count) {
                name = sym_data[index].name;
            }
        } else if (sym_data_64) {
            size_t index = ELF64_R_SYM(p->r_info) - 1;
            if (index < sym_data_64_entry_count) {
                name = sym_data_64[index].name;
            }
        }

        /*  When the name is not available, use the
            section name as a reference for the name.*/
        if (!name || !name[0]) {
            size_t index = ELF64_R_SYM(p->r_info) - 1;
            if (index < sym_data_64_entry_count) {
                SYM64 *sym_64 = &sym_data_64[index];
                if (sym_64->type == STT_SECTION &&
                    sym_64->shndx < scn_names_count) {
                    name = scn_names[sym_64->shndx];
                }
            }
        }
        if (!name || !name[0]) {
            name = "<no name>";
        }

        if (SHT_RELA == type) {
            Elf64_Rela *pa = (Elf64_Rela *)p;
            add = pa->r_addend;
        }

        printf("0x%08lx 0x%08lx %-26s <%5ld> %s\n",
            (unsigned long int) (p->r_offset),
            (unsigned long int) (add),
            get_reloc_type_names(ELF64_R_TYPE(p->r_info)),
            (long)ELF64_R_SYM(p->r_info),
            name);
#else
        /*  sgi/mips -64 does not have r_info in the 64bit relocations,
            but seperate fields, with 3 types, actually. Only one of
            which prints here, as only one really used with dwarf */
        Elf64_Rel *p = (Elf64_Rel *) (buf + off);
        char *name = 0;
        if (sym_data ) {
            size_t index = p->r_sym - 1;
            if (index < sym_data_entry_count) {
                name = sym_data[index].name;
            }
        } else if (sym_data_64) {
            size_t index = p->r_sym - 1;
            if (index < sym_data_64_entry_count) {
                name = sym_data_64[index].name;
            }
        }
        if (!name || !name[0]) {
            name = "<no name>";
        }
        printf("%5" DW_PR_DUu " %-26s <%3ld> %s\n",
            (Dwarf_Unsigned) (p->r_offset),
            get_reloc_type_names(p->r_type),
            (long)p->r_sym,
            name);
#endif
    }
#endif /* HAVE_ELF64_GETEHDR */
}

static void
print_reloc_information_32(int section_no, Dwarf_Small * buf,
   Dwarf_Unsigned size, Elf64_Xword type, char **scn_names,
   int scn_names_count)
{
    /*  Include support for Elf32_Rel and Elf32_Rela */
    Dwarf_Unsigned add = 0;
    Dwarf_Half rel_size = SHT_RELA == type ?
        sizeof(Elf32_Rela) : sizeof(Elf32_Rel);
    Dwarf_Unsigned off = 0;

    printf("\n%s:\n", type== SHT_RELA?sectnamesa[section_no]:
        sectnames[section_no]);


    /* Print some headers and change the order for better reading. */
    printf("Offset     Addend     %-26s Index   Symbol Name\n","Reloc Type");

    for (off = 0; off < size; off += rel_size) {
        Elf32_Rel *p = (Elf32_Rel *) (buf + off);
        char *name = 0;

        if (sym_data) {
            size_t index = ELF32_R_SYM(p->r_info) - 1;
            if (index < sym_data_entry_count) {
                name = sym_data[index].name;
            }
        }

        /*  When the name is not available, use the
            section name as a reference for the name. */
        if (!name || !name[0]) {
            size_t index = ELF32_R_SYM(p->r_info) - 1;
            if (index < sym_data_entry_count) {
                SYM *sym = &sym_data[index];
                if (sym->type == STT_SECTION&&
                    sym->shndx < scn_names_count) {
                    name = scn_names[sym->shndx];
                }
            }
        }
        if (!name || !name[0]) {
            name = "<no name>";
        }
        if (SHT_RELA == type) {
            Elf32_Rela *pa = (Elf32_Rela *)p;
            add = pa->r_addend;
        }
        printf("0x%08lx 0x%08lx %-26s <%5ld> %s\n",
            (unsigned long int) (p->r_offset),
            (unsigned long int) (add),
            get_reloc_type_names(ELF32_R_TYPE(p->r_info)),
            (long)ELF32_R_SYM(p->r_info),
            name);
    }
}

static SYM *
readsyms(Elf32_Sym * data, size_t num, Elf * elf, Elf32_Word link)
{
    SYM *s, *buf;
    indx_type i;

    buf = (SYM *) calloc(num, sizeof(SYM));
    if (buf == NULL) {
        return NULL;
    }
    s = buf; /* save pointer to head of array */
    for (i = 1; i < num; i++, data++, buf++) {
        buf->indx = i;
        buf->name = (char *) elf_strptr(elf, link, data->st_name);
        buf->value = data->st_value;
        buf->size = data->st_size;
        buf->type = ELF32_ST_TYPE(data->st_info);
        buf->bind = ELF32_ST_BIND(data->st_info);
        buf->other = data->st_other;
        buf->shndx = data->st_shndx;
    }   /* end for loop */
    return (s);
}

static SYM64 *
read_64_syms(Elf64_Sym * data, size_t num, Elf * elf, Elf64_Word link)
{
#ifdef HAVE_ELF64_GETEHDR

    SYM64 *s, *buf;
    indx_type i;

    buf = (SYM64 *) calloc(num, sizeof(SYM64));
    if (buf == NULL) {
        return NULL;
    }
    s = buf;                    /* save pointer to head of array */
    for (i = 1; i < num; i++, data++, buf++) {
        buf->indx = i;
        buf->name = (char *) elf_strptr(elf, link, data->st_name);
        buf->value = data->st_value;
        buf->size = data->st_size;
        buf->type = ELF64_ST_TYPE(data->st_info);
        buf->bind = ELF64_ST_BIND(data->st_info);
        buf->other = data->st_other;
        buf->shndx = data->st_shndx;
    }                           /* end for loop */
    return (s);
#else
    return 0;
#endif /* HAVE_ELF64_GETEHDR */
}

static void *
get_scndata(Elf_Scn * fd_scn, size_t * scn_size)
{
    Elf_Data *p_data;

    p_data = 0;
    if ((p_data = elf_getdata(fd_scn, p_data)) == 0 ||
        p_data->d_size == 0) {
        return NULL;
    }
    *scn_size = p_data->d_size;
    return (p_data->d_buf);
}

/* Cleanup of malloc space (some of the pointers will be 0 here)
   so dwarfdump looks 'clean' to a malloc checker.
*/
void
clean_up_syms_malloc_data()
{
    free(sym_data);
    sym_data = 0;
    free(sym_data_64);
    sym_data_64 = 0;
    sym_data_64_entry_count = 0;
    sym_data_entry_count = 0;
}
