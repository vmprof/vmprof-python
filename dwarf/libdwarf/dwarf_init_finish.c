/*

  Copyright (C) 2000,2002,2003,2004,2005 Silicon Graphics, Inc. All Rights Reserved.
  Portions Copyright (C) 2008-2010 Arxan Technologies, Inc. All Rights Reserved.
  Portions Copyright (C) 2009-2013 David Anderson. All Rights Reserved.
  Portions Copyright (C) 2010-2012 SN Systems Ltd. All Rights Reserved.

  This program is free software; you can redistribute it and/or modify it
  under the terms of version 2.1 of the GNU Lesser General Public License
  as published by the Free Software Foundation.

  This program is distributed in the hope that it would be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

  Further, this software is distributed without any warranty that it is
  free of the rightful claim of any third person regarding infringement
  or the like.  Any license provided herein, whether implied or
  otherwise, applies only to this software file.  Patent licenses, if
  any, provided herein do not apply to combinations of this program with
  other software, or any other product whatsoever.

  You should have received a copy of the GNU Lesser General Public
  License along with this program; if not, write the Free Software
  Foundation, Inc., 51 Franklin Street - Fifth Floor, Boston MA 02110-1301,
  USA.

*/

#include "config.h"
#include "dwarf_incl.h"

#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <stdlib.h>

#include "dwarf_incl.h"
#include "dwarf_harmless.h"

/* For consistency, use the HAVE_LIBELF_H symbol */
#ifdef HAVE_ELF_H
#include <elf.h>
#endif
#ifdef HAVE_LIBELF_H
#include <libelf.h>
#else
#ifdef HAVE_LIBELF_LIBELF_H
#include <libelf/libelf.h>
#endif
#endif

#define DWARF_DBG_ERROR(dbg,errval,retval) \
    _dwarf_error(dbg, error, errval); return(retval);

#define FALSE 0
#define TRUE  1



/* This static is copied to the dbg on dbg init
   so that the static need not be referenced at
   run time, preserving better locality of
   reference.
   Value is 0 means do the string check.
   Value non-zero means do not do the check.
*/
static Dwarf_Small _dwarf_assume_string_in_bounds;
static Dwarf_Small _dwarf_apply_relocs = 1;

/*  Call this after calling dwarf_init but before doing anything else.
    It applies to all objects, not just the current object.  */
int
dwarf_set_reloc_application(int apply)
{
    int oldval = _dwarf_apply_relocs;
    _dwarf_apply_relocs = apply;
    return oldval;
}

int
dwarf_set_stringcheck(int newval)
{
    int oldval = _dwarf_assume_string_in_bounds;

    _dwarf_assume_string_in_bounds = newval;
    return oldval;
}

/* Unifies the basic duplicate/empty testing and section
   data setting to one place. */
static int
get_basic_section_data(Dwarf_Debug dbg,
    struct Dwarf_Section_s *secdata,
    struct Dwarf_Obj_Access_Section_s *doas,
    Dwarf_Half section_index,
    Dwarf_Error* error,
    int duperr, int emptyerr )
{
    /*  There is an elf convention that section index 0  is reserved,
        and that section is always empty.
        Non-elf object formats must honor that by ensuring that
        (when they assign numbers to 'sections' or 'section-like-things')
        they never assign a real section section-number  0 to dss_index. */
    if (secdata->dss_index != 0) {
        DWARF_DBG_ERROR(dbg, duperr, DW_DLV_ERROR);
    }
    if (doas->size == 0) {
        if (emptyerr == 0 ) {
            /* Allow empty section. */
            return DW_DLV_OK;
        }
        /* Know no reason to allow section */
        DWARF_DBG_ERROR(dbg, emptyerr, DW_DLV_ERROR);
    }
    secdata->dss_index = section_index;
    secdata->dss_size = doas->size;
    secdata->dss_addr = doas->addr;
    secdata->dss_link = doas->link;
    secdata->dss_entrysize = doas->entrysize;
    return DW_DLV_OK;
}


static void
add_rela_data( struct Dwarf_Section_s *secdata,
    struct Dwarf_Obj_Access_Section_s *doas,
    Dwarf_Half section_index)
{
    secdata->dss_reloc_index = section_index;
    secdata->dss_reloc_size = doas->size;
    secdata->dss_reloc_entrysize = doas->entrysize;
    secdata->dss_reloc_addr = doas->addr;
    secdata->dss_reloc_symtab = doas->link;
    secdata->dss_reloc_link = doas->link;
}



/*  Used to add the specific information for a debug related section
    Called on each section of interest by section name.
    DWARF_MAX_DEBUG_SECTIONS must be large enough to allow
    that all sections of interest fit in the table.
    */
static int
add_debug_section_info(Dwarf_Debug dbg,
    const char *name,
    struct Dwarf_Section_s *secdata,
    int duperr,int emptyerr,int have_dwarf,
    int *err)
{
    unsigned total_entries = dbg->de_debug_sections_total_entries;
    if (secdata->dss_is_in_use) {
        *err = duperr;
        return DW_DLV_ERROR;
    }
    if (total_entries < DWARF_MAX_DEBUG_SECTIONS) {
        struct Dwarf_dbg_sect_s *debug_section =
            &dbg->de_debug_sections[total_entries];
        secdata->dss_is_in_use = TRUE;
        debug_section->ds_name = name;
        debug_section->ds_secdata = secdata;
        secdata->dss_name = name;
        debug_section->ds_duperr = duperr;
        debug_section->ds_emptyerr = emptyerr;
        debug_section->ds_have_dwarf = have_dwarf;
        ++dbg->de_debug_sections_total_entries;
        return DW_DLV_OK;
    }
    /*  This represents a bug in libdwarf.
        Mis-setup-DWARF_MAX_DEBUG_SECTIONS.
        Or possibly a use of section groups that is
        not supported.  */
    *err = DW_DLE_TOO_MANY_DEBUG;
    return DW_DLV_ERROR;
}


/*  If running this long set of tests is slow
    enough to matter one could set up a local
    tsearch tree with all this content and search
    it instead of this set of sequential tests.
    Or use a switch(){} here with a search tree
    to to turn name into index for the switch(). */
static int
enter_section_in_de_debug_sections_array(Dwarf_Debug dbg,
    const char *scn_name,
    int *err)
{
    int sectionerr = 0;
    /*  Setup the table that contains the basic information about the
        sections that are DWARF related. The entries are very unlikely
        to change very often. */
    if(!strcmp(scn_name,".debug_info")) {
        sectionerr = add_debug_section_info(dbg,".debug_info",
            &dbg->de_debug_info,
            DW_DLE_DEBUG_INFO_DUPLICATE,DW_DLE_DEBUG_INFO_NULL,
            TRUE,err);
        if (sectionerr != DW_DLV_OK) {
            return sectionerr;
        }
        return DW_DLV_OK;
    }
    if(!strcmp(scn_name,".debug_info.dwo")) {
        sectionerr = add_debug_section_info(dbg,".debug_info.dwo",
            &dbg->de_debug_info,
            DW_DLE_DEBUG_INFO_DUPLICATE,DW_DLE_DEBUG_INFO_NULL,
            TRUE,err);
        if (sectionerr != DW_DLV_OK) {
            return sectionerr;
        }
        return DW_DLV_OK;
    }
    if(!strcmp(scn_name,".debug_types")) {
        sectionerr = add_debug_section_info(dbg,".debug_types",
            &dbg->de_debug_types,
            DW_DLE_DEBUG_TYPES_DUPLICATE,DW_DLE_DEBUG_TYPES_NULL,
            TRUE,err);
        if (sectionerr != DW_DLV_OK) {
            return sectionerr;
        }
        return DW_DLV_OK;
    }
    if(!strcmp(scn_name,".debug_types.dwo")) {
        sectionerr = add_debug_section_info(dbg,".debug_types.dwo",
            &dbg->de_debug_types,
            DW_DLE_DEBUG_TYPES_DUPLICATE,DW_DLE_DEBUG_TYPES_NULL,
            TRUE,err);
        if (sectionerr != DW_DLV_OK) {
            return sectionerr;
        }
        return DW_DLV_OK;

    }
    if(!strcmp(scn_name,".debug_abbrev")) {
        sectionerr = add_debug_section_info(dbg,".debug_abbrev",
            &dbg->de_debug_abbrev, /*03*/
            DW_DLE_DEBUG_ABBREV_DUPLICATE,DW_DLE_DEBUG_ABBREV_NULL,
            FALSE,err);
        if (sectionerr != DW_DLV_OK) {
            return sectionerr;
        }
        return DW_DLV_OK;
    }
    if(!strcmp(scn_name,".debug_abbrev.dwo")) {
        sectionerr = add_debug_section_info(dbg,".debug_abbrev.dwo",
            &dbg->de_debug_abbrev, /*03*/
            DW_DLE_DEBUG_ABBREV_DUPLICATE,DW_DLE_DEBUG_ABBREV_NULL,
            FALSE,err);
        if (sectionerr != DW_DLV_OK) {
            return sectionerr;
        }
        return DW_DLV_OK;
    }
    if(!strcmp(scn_name,".debug_aranges")) {
        sectionerr = add_debug_section_info(dbg,".debug_aranges",
            &dbg->de_debug_aranges,
            DW_DLE_DEBUG_ARANGES_DUPLICATE,0,
            FALSE,err);
        if (sectionerr != DW_DLV_OK) {
            return sectionerr;
        }
        return DW_DLV_OK;
    }

    if(!strcmp(scn_name,".debug_line")) {
        sectionerr = add_debug_section_info(dbg,".debug_line",
            &dbg->de_debug_line,
            DW_DLE_DEBUG_LINE_DUPLICATE,0,
            FALSE,err);
        if (sectionerr != DW_DLV_OK) {
            return sectionerr;
        }
        return DW_DLV_OK;
    }
    if(!strcmp(scn_name,".debug_line.dwo")) {
        sectionerr = add_debug_section_info(dbg,".debug_line.dwo",
            &dbg->de_debug_line,
            DW_DLE_DEBUG_LINE_DUPLICATE,0,
            FALSE,err);
        if (sectionerr != DW_DLV_OK) {
            return sectionerr;
        }
        return DW_DLV_OK;
    }
    if(!strcmp(scn_name,".debug_line_str")) {
        sectionerr = add_debug_section_info(dbg,".debug_line_str",
            &dbg->de_debug_line_str,
            DW_DLE_DEBUG_LINE_STR_DUPLICATE,0,
            FALSE,err);
        if (sectionerr != DW_DLV_OK) {
            return sectionerr;
        }
        return DW_DLV_OK;
    }


    if(!strcmp(scn_name,".debug_frame")) {
        sectionerr = add_debug_section_info(dbg,".debug_frame",
            &dbg->de_debug_frame,
            DW_DLE_DEBUG_FRAME_DUPLICATE,0,
            TRUE,err);
        if (sectionerr != DW_DLV_OK) {
            return sectionerr;
        }
        return DW_DLV_OK;
    }
    if(!strcmp(scn_name,".eh_frame")) {
        /* gnu egcs-1.1.2 data */
        sectionerr = add_debug_section_info(dbg,".eh_frame",
            &dbg->de_debug_frame_eh_gnu,
            DW_DLE_DEBUG_FRAME_DUPLICATE,0,
            TRUE,err);
        if (sectionerr != DW_DLV_OK) {
            return sectionerr;
        }
        return DW_DLV_OK;
    }
    if(!strcmp(scn_name,".debug_loc")) {
        sectionerr = add_debug_section_info(dbg,".debug_loc",
            &dbg->de_debug_loc,
            DW_DLE_DEBUG_LOC_DUPLICATE,0,
            FALSE,err);
        if (sectionerr != DW_DLV_OK) {
            return sectionerr;
        }
        return DW_DLV_OK;
    }
    if(!strcmp(scn_name,".debug_loc.dwo")) {
        sectionerr = add_debug_section_info(dbg,".debug_loc.dwo",
            &dbg->de_debug_loc,
            DW_DLE_DEBUG_LOC_DUPLICATE,0,
            FALSE,err);
        if (sectionerr != DW_DLV_OK) {
            return sectionerr;
        }
        return DW_DLV_OK;
    }
    if(!strcmp(scn_name,".debug_pubnames")) {
        sectionerr = add_debug_section_info(dbg,".debug_pubnames",
            &dbg->de_debug_pubnames,
            DW_DLE_DEBUG_PUBNAMES_DUPLICATE,0,
            FALSE,err);
        if (sectionerr != DW_DLV_OK) {
            return sectionerr;
        }
        return DW_DLV_OK;
    }
    if(!strcmp(scn_name,".debug_str")) {
        sectionerr = add_debug_section_info(dbg,".debug_str",
            &dbg->de_debug_str,
            DW_DLE_DEBUG_STR_DUPLICATE,0,
            FALSE,err);
        if (sectionerr != DW_DLV_OK) {
            return sectionerr;
        }
        return DW_DLV_OK;
    }
    if(!strcmp(scn_name,".debug_str.dwo")) {
        sectionerr = add_debug_section_info(dbg,".debug_str.dwo",
            &dbg->de_debug_str,
            DW_DLE_DEBUG_STR_DUPLICATE,0,
            FALSE,err);
        if (sectionerr != DW_DLV_OK) {
            return sectionerr;
        }
        return DW_DLV_OK;
    }
    if(!strcmp(scn_name,".debug_funcnames")) {
        /* SGI IRIX-only. */
        sectionerr = add_debug_section_info(dbg,".debug_funcnames",
            &dbg->de_debug_funcnames,
            /*11*/
            DW_DLE_DEBUG_FUNCNAMES_DUPLICATE,0,
            FALSE,err);
        if (sectionerr != DW_DLV_OK) {
            return sectionerr;
        }
        return DW_DLV_OK;
    }
    if(!strcmp(scn_name,".debug_typenames")) {
        /*  SGI IRIX-only, created years before DWARF3. Content
            essentially identical to .debug_pubtypes.  */
        sectionerr = add_debug_section_info(dbg,".debug_typenames",
            &dbg->de_debug_typenames,
            /*12*/
            DW_DLE_DEBUG_TYPENAMES_DUPLICATE,0,
            FALSE,err);
        if (sectionerr != DW_DLV_OK) {
            return sectionerr;
        }
        return DW_DLV_OK;
    }
    if(!strcmp(scn_name,".debug_pubtypes")) {
        /* Section new in DWARF3.  */
        sectionerr = add_debug_section_info(dbg,".debug_pubtypes",
            &dbg->de_debug_pubtypes,
            /*13*/
            DW_DLE_DEBUG_PUBTYPES_DUPLICATE,0,
            FALSE,err);
        if (sectionerr != DW_DLV_OK) {
            return sectionerr;
        }
        return DW_DLV_OK;
    }
    if(!strcmp(scn_name,".debug_varnames")) {
        /* SGI IRIX-only.  */
        sectionerr = add_debug_section_info(dbg,".debug_varnames",
            &dbg->de_debug_varnames,
            DW_DLE_DEBUG_VARNAMES_DUPLICATE,0,
            FALSE,err);
        if (sectionerr != DW_DLV_OK) {
            return sectionerr;
        }
        return DW_DLV_OK;
    }
    if(!strcmp(scn_name,".debug_weaknames")) {
        /* SGI IRIX-only. */
        sectionerr = add_debug_section_info(dbg,".debug_weaknames",
            &dbg->de_debug_weaknames,
            DW_DLE_DEBUG_WEAKNAMES_DUPLICATE,0,
            FALSE,err);
        if (sectionerr != DW_DLV_OK) {
            return sectionerr;
        }
        return DW_DLV_OK;
    }
    if(!strcmp(scn_name,".debug_macinfo")) {
        sectionerr = add_debug_section_info(dbg,".debug_macinfo",
            &dbg->de_debug_macinfo,
            DW_DLE_DEBUG_MACINFO_DUPLICATE,0,
            FALSE,err);
        if (sectionerr != DW_DLV_OK) {
            return sectionerr;
        }
        return DW_DLV_OK;
    }
    /*  ".debug_macinfo.dwo" is not allowed.  */


    if(!strcmp(scn_name,".debug_macro")) { /* DWARF5 */
        sectionerr = add_debug_section_info(dbg,".debug_macro",
            &dbg->de_debug_macro,
            DW_DLE_DEBUG_MACRO_DUPLICATE,0,
            FALSE,err);
        if (sectionerr != DW_DLV_OK) {
            return sectionerr;
        }
        return DW_DLV_OK;
    }
    if(!strcmp(scn_name,".debug_macro.dwo")) { /* DWARF5 */
        sectionerr = add_debug_section_info(dbg,".debug_macro.dwo",
            &dbg->de_debug_macro,
            DW_DLE_DEBUG_MACRO_DUPLICATE,0,
            FALSE,err);
        if (sectionerr != DW_DLV_OK) {
            return sectionerr;
        }
        return DW_DLV_OK;
    }
    if(!strcmp(scn_name,".debug_ranges")) {
        sectionerr = add_debug_section_info(dbg,".debug_ranges",
            &dbg->de_debug_ranges,
            DW_DLE_DEBUG_RANGES_DUPLICATE,0,
            TRUE,err);
        if (sectionerr != DW_DLV_OK) {
            return sectionerr;
        }
        return DW_DLV_OK;
    }
    /*  No .debug_ranges.dwo allowed. */

    if(!strcmp(scn_name,".debug_str_offsets")) {
        /* New DWARF5 */
        sectionerr = add_debug_section_info(dbg,".debug_str_offsets",
            &dbg->de_debug_str_offsets,
            DW_DLE_DEBUG_STR_OFFSETS_DUPLICATE,0,
            TRUE,err);
        if (sectionerr != DW_DLV_OK) {
            return sectionerr;
        }
        return DW_DLV_OK;
    }
    if(!strcmp(scn_name,".debug_str_offsets.dwo")) {
        /* New DWARF5 */
        sectionerr = add_debug_section_info(dbg,".debug_str_offsets.dwo",
            &dbg->de_debug_str_offsets,
            DW_DLE_DEBUG_STR_OFFSETS_DUPLICATE,0,
            TRUE,err);
        if (sectionerr != DW_DLV_OK) {
            return sectionerr;
        }
        return DW_DLV_OK;
    }
    if(!strcmp(scn_name,".debug_sup")) { /* DWARF5 */
        /* New DWARF5 */
        sectionerr = add_debug_section_info(dbg,".debug_sup",
            &dbg->de_debug_sup,
            DW_DLE_DEBUG_SUP_DUPLICATE,0,
            TRUE,err);
        if (sectionerr != DW_DLV_OK) {
            return sectionerr;
        }
        return DW_DLV_OK;
    }
    /* No .debug_sup.dwo allowed. */

    if(!strcmp(scn_name,".symtab")) {
        sectionerr = add_debug_section_info(dbg,".symtab",
            &dbg->de_elf_symtab,
            DW_DLE_DEBUG_SYMTAB_ERR,0,
            FALSE,err);
        if (sectionerr != DW_DLV_OK) {
            return sectionerr;
        }
        return DW_DLV_OK;
    }
    if(!strcmp(scn_name,".strtab")) {
        sectionerr = add_debug_section_info(dbg,".strtab",
            &dbg->de_elf_strtab,
            DW_DLE_DEBUG_STRTAB_ERR,0,
            FALSE,err);
        if (sectionerr != DW_DLV_OK) {
            return sectionerr;
        }
        return DW_DLV_OK;
    }
    if(!strcmp(scn_name,".debug_addr")) {
        /* New DWARF5 */
        sectionerr = add_debug_section_info(dbg,".debug_addr",
            &dbg->de_debug_addr,
            DW_DLE_DEBUG_ADDR_DUPLICATE,0,
            TRUE,err);
        if (sectionerr != DW_DLV_OK) {
            return sectionerr;
        }
        return DW_DLV_OK;
    }
    /*  No .debug_addr.dwo allowed.  */

    if(!strcmp(scn_name,".gdb_index")) {
        /* gdb added this. */
        sectionerr = add_debug_section_info(dbg,".gdb_index",
            &dbg->de_debug_gdbindex,
            DW_DLE_DUPLICATE_GDB_INDEX,0,
            FALSE,err);
        if (sectionerr != DW_DLV_OK) {
            return sectionerr;
        }
        return DW_DLV_OK;
    }
    if(!strcmp(scn_name,".debug_names")) { /* NEW DWARF5 */
        sectionerr = add_debug_section_info(dbg,".debug_names",
            &dbg->de_debug_names,
            DW_DLE_DEBUG_NAMES_DUPLICATE,0,
            FALSE,err);
        if (sectionerr != DW_DLV_OK) {
            return sectionerr;
        }
        return DW_DLV_OK;
    }
    /* No .debug_names.dwo is allowed. */
    if(!strcmp(scn_name,".debug_cu_index")) {
        /* gdb added this in DW4. It is in  standard DWARF5  */
        sectionerr = add_debug_section_info(dbg,".debug_cu_index",
            &dbg->de_debug_cu_index,
            DW_DLE_DUPLICATE_CU_INDEX,0,
            FALSE,err);
        if (sectionerr != DW_DLV_OK) {
            return sectionerr;
        }
        return DW_DLV_OK;
    }
    if(!strcmp(scn_name,".debug_tu_index")) {
        /* gdb added this in DW4. It is in standard DWARF5 */
        sectionerr = add_debug_section_info(dbg,".debug_tu_index",
            &dbg->de_debug_tu_index,
            DW_DLE_DUPLICATE_TU_INDEX,0,
            FALSE,err);
        if (sectionerr != DW_DLV_OK) {
            return sectionerr;
        }
        return DW_DLV_OK;
    }
    return DW_DLV_NO_ENTRY;
}

static int
is_section_known_already(Dwarf_Debug dbg,
    const char *scn_name,
    unsigned   *found_section_number,
    unsigned    start_number,
    int        *err)
{
    unsigned i = start_number;
    for ( ; i < dbg->de_debug_sections_total_entries; ++i) {
        struct Dwarf_dbg_sect_s *section = &dbg->de_debug_sections[i];
        if (!strcmp(scn_name, section->ds_name)) {
            *found_section_number = i;
            return DW_DLV_OK;
        }
    }
    return DW_DLV_NO_ENTRY;
}

/*  Given an Elf ptr, set up dbg with pointers
    to all the Dwarf data sections.
    Return NULL on error.

    This function is also responsible for determining
    whether the given object contains Dwarf information
    or not.  The test currently used is that it contains
    either a .debug_info or a .debug_frame section.  If
    not, it returns DW_DLV_NO_ENTRY causing dwarf_init() also to
    return DW_DLV_NO_ENTRY.  Earlier, we had thought of using only
    the presence/absence of .debug_info to test, but we
    added .debug_frame since there could be stripped objects
    that have only a .debug_frame section for exception
    processing.
    DW_DLV_NO_ENTRY or DW_DLV_OK or DW_DLV_ERROR

    This does not allow for section-groups in object files,
    for which many .debug_info (and other DWARF) sections may exist.
*/

static int
this_section_dwarf_relevant(const char *scn_name,int type)
{
    /* A small helper function for _dwarf_setup(). */
    if (strncmp(scn_name, ".debug_", 7)
        && strcmp(scn_name, ".eh_frame")
        && strcmp(scn_name, ".symtab")
        && strcmp(scn_name, ".strtab")
        && strcmp(scn_name, ".gdb_index")
        && strncmp(scn_name, ".rela.",6)
        /*  For an object file with incorrect rela section name,
            readelf prints correct debug information,
            as the tool takes the section type instead
            of the section name. Include the incorrect
            section name, until this test uses the section type. */
        && type != SHT_RELA)  {
            /*  Other sections should be ignored, they
                are not relevant for DWARF data. */
            return FALSE;
    }
    /* This is one of ours. */
    return TRUE;
}


static int
_dwarf_setup(Dwarf_Debug dbg, Dwarf_Error * error)
{
    const char *scn_name = 0;
    int foundDwarf = 0;
    struct Dwarf_Obj_Access_Interface_s * obj = 0;

    Dwarf_Endianness endianness;

    /* Table with pointers to debug sections */
    struct Dwarf_Section_s **sections = 0;

    Dwarf_Unsigned section_count = 0;
    Dwarf_Half obj_section_index = 0;

    foundDwarf = FALSE;

    dbg->de_assume_string_in_bounds = _dwarf_assume_string_in_bounds;

    dbg->de_same_endian = 1;
    dbg->de_copy_word = memcpy;
    obj = dbg->de_obj_file;
    endianness = obj->methods->get_byte_order(obj->object);
#ifdef WORDS_BIGENDIAN
    dbg->de_big_endian_object = 1;
    if (endianness == DW_OBJECT_LSB ) {
        dbg->de_same_endian = 0;
        dbg->de_big_endian_object = 0;
        dbg->de_copy_word = _dwarf_memcpy_swap_bytes;
    }
#else /* little endian */
    dbg->de_big_endian_object = 0;
    if (endianness == DW_OBJECT_MSB ) {
        dbg->de_same_endian = 0;
        dbg->de_big_endian_object = 1;
        dbg->de_copy_word = _dwarf_memcpy_swap_bytes;
    }
#endif /* !WORDS_BIGENDIAN */


    /*  The following de_length_size is Not Too Significant. Only used
        one calculation, and an approximate one at that. */
    dbg->de_length_size = obj->methods->get_length_size(obj->object);
    dbg->de_pointer_size = obj->methods->get_pointer_size(obj->object);

    /*  For windows always is 4 ? */
#ifdef WIN32
    dbg->de_pointer_size = 4;
#endif /* WIN32 */

    section_count = obj->methods->get_section_count(obj->object);

    /*  Allocate space to record references to debug sections, that can
        be referenced by RELA sections in the 'sh_info' field. */
    sections = (struct Dwarf_Section_s **)calloc(section_count + 1,
        sizeof(struct Dwarf_Section_s *));
    if (!sections) {
        /* Impossible case, we hope. Give up. */
        _dwarf_error(dbg, error, DW_DLE_SECTION_ERROR);
        return DW_DLV_ERROR;
    }

    /*  We can skip index 0 when considering ELF files, but not other
        object types.  Indeed regardless of the object type we should
        skip section 0 here.
        This is a convention.  We depend on it.
        Non-elf object access code should
        (in itself) understand we will index beginning at 1 and adjust
        itself to deal with this Elf convention.    Without this
        convention various parts of the code in this file won't work correctly.
        A dss_index of 0 must not be used, even though we start at 0
        here.  So the get_section_info() must adapt to the situation
        (the elf version does automatically as a result of Elf having
        a section zero with zero length and an empty name). */
    for (obj_section_index = 0; obj_section_index < section_count;
        ++obj_section_index) {

        struct Dwarf_Obj_Access_Section_s doas;
        int res = DW_DLV_ERROR;
        int err = 0;

        memset(&doas,0,sizeof(doas));
        res = obj->methods->get_section_info(obj->object,
            obj_section_index,
            &doas, &err);
        if (res == DW_DLV_NO_ENTRY){
            free(sections);
            return res;
        } else if (res == DW_DLV_ERROR){
            free(sections);
            DWARF_DBG_ERROR(dbg, err, DW_DLV_ERROR);
        }

        scn_name = doas.name;

        if (!this_section_dwarf_relevant(scn_name,doas.type) ) {
            continue;
        } else {
            /*  Build up the sections table and the
                de_debug* etc pointers in Dwarf_Debug. */
            struct Dwarf_dbg_sect_s *section;

            int found_match = FALSE;
            unsigned initial_start_number = 0;
            unsigned dbg_section_number = 0;
            res = is_section_known_already(dbg,scn_name,
                &dbg_section_number,
                initial_start_number,
                &err);
            if (res == DW_DLV_OK) {
                /* DUPLICATE */
                free(sections);
                DWARF_DBG_ERROR(dbg, DW_DLE_SECTION_DUPLICATION,
                    DW_DLV_ERROR);
            } else if (res == DW_DLV_ERROR) {
                free(sections);
                DWARF_DBG_ERROR(dbg, err, DW_DLV_ERROR);
            }
            /* No entry: new-to-us section, the normal case. */
            res = enter_section_in_de_debug_sections_array(dbg,scn_name,&err);
            if (res == DW_DLV_OK) {
                /*  We just added a new entry in the dbg
                    de_debug_sections array.  So we know its number. */
                unsigned real_start_number =
                    dbg->de_debug_sections_total_entries-1;
                res = is_section_known_already(dbg,scn_name,
                    &dbg_section_number,
                    real_start_number,
                    &err);
                if (res == DW_DLV_OK) {
                    section = &dbg->de_debug_sections[dbg_section_number];
                    res = get_basic_section_data(dbg,
                        section->ds_secdata, &doas,
                        obj_section_index, error,
                        section->ds_duperr,
                        section->ds_emptyerr);
                    if (res != DW_DLV_OK) {
                        free(sections);
                        return res;
                    }
                    sections[obj_section_index] = section->ds_secdata;
                    foundDwarf += section->ds_have_dwarf;
                    found_match = TRUE;
                    /*  Normal section set up.
                        Fall through. */
                }else if (res == DW_DLV_NO_ENTRY) {
                    /*  Some sort of bug in the code here.
                        Should be impossible to get here. */
                    free(sections);
                    DWARF_DBG_ERROR(dbg, DW_DLE_SECTION_ERROR, DW_DLV_ERROR);
                } else {
                    free(sections);
                    DWARF_DBG_ERROR(dbg, err, DW_DLV_ERROR);
                }
            } else if (res == DW_DLV_NO_ENTRY) {
                /*  We get here for relocation sections.
                    Fall through. */
            } else {
                free(sections);
                DWARF_DBG_ERROR(dbg, err, DW_DLV_ERROR);
            }

            if (!found_match) {
                /*  For an object file with incorrect rela section name,
                    the 'readelf' tool, prints correct debug information,
                    as the tool takes the section type instead
                    of the section name. If the current section
                    is a RELA one and the 'sh_info'
                    refers to a debug section, add the relocation data. */
                if (doas.type == SHT_RELA) {
                    if ( doas.info < section_count) {
                        if (sections[doas.info]) {
                            add_rela_data(sections[doas.info],&doas,
                                obj_section_index);
                        }
                    } else {
                        /* Something is wrong with the ELF file. */
                        free(sections);
                        DWARF_DBG_ERROR(dbg, DW_DLE_ELF_SECT_ERR, DW_DLV_ERROR);
                    }
                }
            }
            /* Fetch next section */
        }
    }

    /* Free table with section information. */
    free(sections);
    if (foundDwarf) {
        return DW_DLV_OK;
    }
    return DW_DLV_NO_ENTRY;
}

static int
all_sig8_bits_zero(Dwarf_Sig8 *val)
{
    unsigned u = 0;
    for(  ; u < sizeof(*val); ++u) {
        if (val->signature[u] != 0) {
            return FALSE;
        }
    }
    return TRUE;
}

/*  There is one table per CU and one per TU, and each
    table refers to the associated other DWARF data
    for that CU or TU.
    See DW_SECT_*

    In DWARF4 the type units are in .debug_types
    In DWARF5 the type units are in .debug_info.
*/

static int
load_debugfission_tables(Dwarf_Debug dbg,Dwarf_Error *error)
{
    int i = 0;
    if (dbg->de_debug_cu_index.dss_size ==0 &&
        dbg->de_debug_tu_index.dss_size ==0) {
        /*  This is the normal case.
            No debug fission. Not a .dwp object. */
        return DW_DLV_NO_ENTRY;
    }

    for (i = 0; i < 2; ++i) {
        Dwarf_Xu_Index_Header xuptr = 0;
        struct Dwarf_Section_s* dwsect = 0;
        Dwarf_Unsigned version = 0;
        Dwarf_Unsigned number_of_cols /* L */ = 0;
        Dwarf_Unsigned number_of_CUs /* N */ = 0;
        Dwarf_Unsigned number_of_slots /* M */ = 0;
        Dwarf_Unsigned hashindex = 0;
        const char *secname = 0;
        int res = 0;
        const char *type = 0;

        if (i == 0) {
            dwsect = &dbg->de_debug_cu_index;
            type = "cu";
        } else {
            dwsect = &dbg->de_debug_tu_index;
            type = "tu";
        }
        if ( !dwsect->dss_size ) {
            continue;
        }
        res = dwarf_get_xu_index_header(dbg,type,
            &xuptr,&version,&number_of_cols,
            &number_of_CUs,&number_of_slots,
            &secname,error);
        if (res == DW_DLV_NO_ENTRY) {
            continue;
        }
        if (res != DW_DLV_OK) {
            return res;
        }
        if (i == 0) {
            dbg->de_cu_hashindex_data = xuptr;
        } else {
            dbg->de_tu_hashindex_data = xuptr;
        }
    }
    return DW_DLV_OK;
}

/*
    Use a Dwarf_Obj_Access_Interface to kick things off. All other
    init routines eventually use this one.
    The returned Dwarf_Debug contains a copy of *obj
    the callers copy of *obj may be freed whenever the caller
    wishes.
*/
int
dwarf_object_init(Dwarf_Obj_Access_Interface* obj, Dwarf_Handler errhand,
    Dwarf_Ptr errarg, Dwarf_Debug* ret_dbg,
    Dwarf_Error* error)
{
    Dwarf_Debug dbg = 0;
    int setup_result = DW_DLV_OK;

    dbg = _dwarf_get_debug();
    if (dbg == NULL) {
        DWARF_DBG_ERROR(dbg, DW_DLE_DBG_ALLOC, DW_DLV_ERROR);
    }
    dbg->de_errhand = errhand;
    dbg->de_errarg = errarg;
    dbg->de_frame_rule_initial_value = DW_FRAME_REG_INITIAL_VALUE;
    dbg->de_frame_reg_rules_entry_count = DW_FRAME_LAST_REG_NUM;
#ifdef HAVE_OLD_FRAME_CFA_COL
    /*  DW_FRAME_CFA_COL is really only suitable for old libdwarf frame
        interfaces and its value of 0 there is only usable where
        (as in MIPS) register 0 has no value other than 0 so
        we can use the frame table column 0 for the CFA value
        (and rely on client software to know when 'register 0'
        is the cfa and when to just use a value 0 for register 0).
    */
    dbg->de_frame_cfa_col_number = DW_FRAME_CFA_COL;
#else
    dbg->de_frame_cfa_col_number = DW_FRAME_CFA_COL3;
#endif
    dbg->de_frame_same_value_number = DW_FRAME_SAME_VAL;
    dbg->de_frame_undefined_value_number  = DW_FRAME_UNDEFINED_VAL;

    dbg->de_obj_file = obj;

    setup_result = _dwarf_setup(dbg, error);
    if (setup_result == DW_DLV_OK) {
        int fission_result = load_debugfission_tables(dbg,error);
        /*  In most cases we get
            setup_result == DW_DLV_NO_ENTRY here
            as having debugfission (.dwp objects)
            is fairly rare. */
        if (fission_result == DW_DLV_ERROR) {
            /*  Something is very wrong. */
            setup_result = fission_result;
        }
    }
    if (setup_result != DW_DLV_OK) {
        int freeresult = 0;
        /* We cannot use any _dwarf_setup()
            error here as
            we are freeing dbg, making that error (setup
            as part of dbg) stale.
            Hence we have to make a new error without a dbg.
            But error might be NULL and the init call
            error-handler function might be set.
        */
        int myerr = 0;
        if ( (setup_result == DW_DLV_ERROR) && error ) {
            /*  Preserve our _dwarf_setup error number, but
                this does not apply if error NULL. */
            myerr = dwarf_errno(*error);
            /*  deallocate the soon-stale error pointer. */
            dwarf_dealloc(dbg,*error,DW_DLA_ERROR);
            *error = 0;
        }
        /*  The status we want to return  here is of _dwarf_setup,
            not of the  _dwarf_free_all_of_one_debug(dbg) call.
            So use a local status variable for the free.  */
        freeresult = _dwarf_free_all_of_one_debug(dbg);
        dbg = 0;
        /* DW_DLV_NO_ENTRY not possible in freeresult */
        if (freeresult == DW_DLV_ERROR) {
            /*  Use the _dwarf_setup error number.
                If error is NULL the following will issue
                a message on stderr and abort(), as without
                dbg there is no error-handler function.
                */
            _dwarf_error(NULL,error,DW_DLE_DBG_ALLOC);
            return DW_DLV_ERROR;
        }
        if (setup_result == DW_DLV_ERROR) {
            /*  Use the _dwarf_setup error number.
                If error is NULL the following will issue
                a message on stderr and abort(), as without
                dbg there is no error-handler function.
                */
            _dwarf_error(NULL,error,myerr);
        }
        return setup_result;
    }
    dwarf_harmless_init(&dbg->de_harmless_errors,
        DW_HARMLESS_ERROR_CIRCULAR_LIST_DEFAULT_SIZE);
    *ret_dbg = dbg;
    return DW_DLV_OK;
}


/*  A finish routine that is completely unaware of ELF.

    Frees all memory that was not previously freed by
    dwarf_dealloc.
    Aside frmo certain categories.  */
int
dwarf_object_finish(Dwarf_Debug dbg, Dwarf_Error * error)
{
    int res = _dwarf_free_all_of_one_debug(dbg);
    if (res == DW_DLV_ERROR) {
        DWARF_DBG_ERROR(dbg, DW_DLE_DBG_ALLOC, DW_DLV_ERROR);
    }
    return res;
}


/*  Load the ELF section with the specified index and set its
    dss_data pointer to the memory where it was loaded.  */
int
_dwarf_load_section(Dwarf_Debug dbg,
    struct Dwarf_Section_s *section,
    Dwarf_Error * error)
{
    int res  = DW_DLV_ERROR;
    int err = 0;
    struct Dwarf_Obj_Access_Interface_s *o = 0;

    /* check to see if the section is already loaded */
    if (section->dss_data !=  NULL) {
        return DW_DLV_OK;
    }
    o = dbg->de_obj_file;
    /*  There is an elf convention that section index 0  is reserved,
        and that section is always empty.
        Non-elf object formats must honor that by ensuring that
        (when they assign numbers to 'sections' or 'section-like-things')
        they never assign a real section section-number  0 to dss_index. */
    res = o->methods->load_section(
        o->object, section->dss_index,
        &section->dss_data, &err);
    if (res == DW_DLV_ERROR){
        DWARF_DBG_ERROR(dbg, err, DW_DLV_ERROR);
    }
    if (_dwarf_apply_relocs == 0) {
        return res;
    }
    if (section->dss_reloc_size == 0) {
        return res;
    }
    if (!o->methods->relocate_a_section) {
        return res;
    }
    /*apply relocations */
    res = o->methods->relocate_a_section( o->object, section->dss_index,
        dbg, &err);
    if (res == DW_DLV_ERROR) {
        DWARF_DBG_ERROR(dbg, err, DW_DLV_ERROR);
    }
    return res;
}

/* This is a hack so clients can verify offsets.
   Added April 2005 so that debugger can detect broken offsets
   (which happened in an IRIX  -64 executable larger than 2GB
    using MIPSpro 7.3.1.3 compilers. A couple .debug_pubnames
    offsets were wrong.).
*/
int
dwarf_get_section_max_offsets(Dwarf_Debug dbg,
    Dwarf_Unsigned * debug_info_size,
    Dwarf_Unsigned * debug_abbrev_size,
    Dwarf_Unsigned * debug_line_size,
    Dwarf_Unsigned * debug_loc_size,
    Dwarf_Unsigned * debug_aranges_size,
    Dwarf_Unsigned * debug_macinfo_size,
    Dwarf_Unsigned * debug_pubnames_size,
    Dwarf_Unsigned * debug_str_size,
    Dwarf_Unsigned * debug_frame_size,
    Dwarf_Unsigned * debug_ranges_size,
    Dwarf_Unsigned * debug_typenames_size)
{
    *debug_info_size = dbg->de_debug_info.dss_size;
    *debug_abbrev_size = dbg->de_debug_abbrev.dss_size;
    *debug_line_size = dbg->de_debug_line.dss_size;
    *debug_loc_size = dbg->de_debug_loc.dss_size;
    *debug_aranges_size = dbg->de_debug_aranges.dss_size;
    *debug_macinfo_size = dbg->de_debug_macinfo.dss_size;
    *debug_pubnames_size = dbg->de_debug_pubnames.dss_size;
    *debug_str_size = dbg->de_debug_str.dss_size;
    *debug_frame_size = dbg->de_debug_frame.dss_size;
    *debug_ranges_size = dbg->de_debug_ranges.dss_size;
    *debug_typenames_size = dbg->de_debug_typenames.dss_size;
    return DW_DLV_OK;
}
/*  This adds the new types size (new section) to the output data.
    Oct 27, 2011. */
int
dwarf_get_section_max_offsets_b(Dwarf_Debug dbg,
    Dwarf_Unsigned * debug_info_size,
    Dwarf_Unsigned * debug_abbrev_size,
    Dwarf_Unsigned * debug_line_size,
    Dwarf_Unsigned * debug_loc_size,
    Dwarf_Unsigned * debug_aranges_size,
    Dwarf_Unsigned * debug_macinfo_size,
    Dwarf_Unsigned * debug_pubnames_size,
    Dwarf_Unsigned * debug_str_size,
    Dwarf_Unsigned * debug_frame_size,
    Dwarf_Unsigned * debug_ranges_size,
    Dwarf_Unsigned * debug_typenames_size,
    Dwarf_Unsigned * debug_types_size)
{
    *debug_info_size = dbg->de_debug_info.dss_size;
    *debug_abbrev_size = dbg->de_debug_abbrev.dss_size;
    *debug_line_size = dbg->de_debug_line.dss_size;
    *debug_loc_size = dbg->de_debug_loc.dss_size;
    *debug_aranges_size = dbg->de_debug_aranges.dss_size;
    *debug_macinfo_size = dbg->de_debug_macinfo.dss_size;
    *debug_pubnames_size = dbg->de_debug_pubnames.dss_size;
    *debug_str_size = dbg->de_debug_str.dss_size;
    *debug_frame_size = dbg->de_debug_frame.dss_size;
    *debug_ranges_size = dbg->de_debug_ranges.dss_size;
    *debug_typenames_size = dbg->de_debug_typenames.dss_size;
    *debug_types_size = dbg->de_debug_types.dss_size;
    return DW_DLV_OK;
}


/*  Given a section name, get its size and address */
int
dwarf_get_section_info_by_name(Dwarf_Debug dbg,
    const char *section_name,
    Dwarf_Addr *section_addr,
    Dwarf_Unsigned *section_size,
    Dwarf_Error * error)
{
    struct Dwarf_Obj_Access_Section_s doas;
    struct Dwarf_Obj_Access_Interface_s * obj = 0;
    Dwarf_Unsigned section_count = 0;
    Dwarf_Half section_index = 0;

    *section_addr = 0;
    *section_size = 0;

    obj = dbg->de_obj_file;
    if (NULL == obj) {
        return DW_DLV_NO_ENTRY;
    }

    section_count = obj->methods->get_section_count(obj->object);

    /*  We can skip index 0 when considering ELF files, but not other
        object types. */
    for (section_index = 0; section_index < section_count;
        ++section_index) {
        int err = 0;
        int res = obj->methods->get_section_info(obj->object,
            section_index, &doas, &err);
        if (res == DW_DLV_ERROR) {
            DWARF_DBG_ERROR(dbg, err, DW_DLV_ERROR);
        }

        if (!strcmp(section_name,doas.name)) {
            *section_addr = doas.addr;
            *section_size = doas.size;
            return DW_DLV_OK;
        }
    }

    return DW_DLV_NO_ENTRY;
}

/*  Given a section index, get its size and address */
int
dwarf_get_section_info_by_index(Dwarf_Debug dbg,
    int section_index,
    const char **section_name,
    Dwarf_Addr *section_addr,
    Dwarf_Unsigned *section_size,
    Dwarf_Error * error)
{
    *section_addr = 0;
    *section_size = 0;
    *section_name = NULL;

    /* Check if we have a valid section index */
    if (section_index >= 0 && section_index < dwarf_get_section_count(dbg)) {
        int res = 0;
        int err = 0;
        struct Dwarf_Obj_Access_Section_s doas;
        struct Dwarf_Obj_Access_Interface_s * obj = dbg->de_obj_file;
        if (NULL == obj) {
            return DW_DLV_NO_ENTRY;
        }
        res = obj->methods->get_section_info(obj->object,
            section_index, &doas, &err);
        if (res == DW_DLV_ERROR){
            DWARF_DBG_ERROR(dbg, err, DW_DLV_ERROR);
        }

        *section_addr = doas.addr;
        *section_size = doas.size;
        *section_name = doas.name;
        return DW_DLV_OK;
    }
    return DW_DLV_NO_ENTRY;
}

/*  Get section count */
int
dwarf_get_section_count(Dwarf_Debug dbg)
{
    struct Dwarf_Obj_Access_Interface_s * obj = dbg->de_obj_file;
    if (NULL == obj) {
        return DW_DLV_NO_ENTRY;
    }
    return obj->methods->get_section_count(obj->object);
}

Dwarf_Cmdline_Options dwarf_cmdline_options = {
    FALSE /* Use quiet mode by default. */
};

/* Lets libdwarf reflect a command line option, so we can get details
   of some errors printed using libdwarf-internal information. */
void
dwarf_record_cmdline_options(Dwarf_Cmdline_Options options)
{
    dwarf_cmdline_options = options;
}
