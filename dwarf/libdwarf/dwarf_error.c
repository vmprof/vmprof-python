/*

  Copyright (C) 2000-2005 Silicon Graphics, Inc. All Rights Reserved.
  Portions Copyright (C) 2008-2014 David Anderson.  All Rights Reserved.

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
#ifdef HAVE_ELF_H
#include <elf.h>
#endif

#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdlib.h>

/* Array to hold string representation of errors. Any time a
   define is added to the list in libdwarf.h, a string should be
   added to this Array
*/

const char *_dwarf_errmsgs[] = {

    "No error (0)\n",
    "DW_DLE_VMM 1 dwarf format/library version mismatch",
    "DW_DLE_MAP 2 memory map failure",
    "DW_DLE_LEE 3 libelf error",
    "DW_DLE_NDS 4 no debug section",
    "DW_DLE_NLS    5  no line section ",
    "DW_DLE_ID     6  invalid descriptor for query ",
    "DW_DLE_IOF    7  I/O failure ",
    "DW_DLE_MAF    8  memory allocation failure ",
    "DW_DLE_IA     9  invalid argument ",
    "DW_DLE_MDE    10  mangled debugging entry ",
    "DW_DLE_MLE    11  mangled line number entry ",
    "DW_DLE_FNO    12  file not open ",
    "DW_DLE_FNR    13  file not a regular file ",
    "DW_DLE_FWA    14  file open with wrong access ",
    "DW_DLE_NOB    15  not an object file ",
    "DW_DLE_MOF    16  mangled object file header ",
    "DW_DLE_EOLL   17  end of location list entries ",
    "DW_DLE_NOLL   18  no location list section ",
    "DW_DLE_BADOFF 19  Invalid offset ",
    "DW_DLE_EOS    20  end of section  ",
    "DW_DLE_ATRUNC 21  abbreviations section appears truncated",
    "DW_DLE_BADBITC  22  Address size passed to dwarf bad",

    "DW_DLE_DBG_ALLOC 23 Unable to malloc a Dwarf_Debug structure",
    "DW_DLE_FSTAT_ERROR 24 The file fd passed to dwarf_init "
        "cannot be fstat()ed",
    "DW_DLE_FSTAT_MODE_ERROR 25 The file mode bits do not "
        "indicate that the file being opened via "
        "dwarf_init() is a normal file",
    "DW_DLE_INIT_ACCESS_WRONG 26 A call to dwarf_init had an "
        "access of other than DW_DLC_READ",
    "DW_DLE_ELF_BEGIN_ERROR 27 a call to "
        "elf_begin(... ELF_C_READ_MMAP... ) failed",
    "DW_DLE_ELF_GETEHDR_ERROR 28 a call to "
        "elf32_getehdr() or elf64_getehdr() failed",
    "DW_DLE_ELF_GETSHDR_ERROR 29 a call to "
        "elf32_getshdr() or elf64_getshdr() failed",
    "DW_DLE_ELF_STRPTR_ERROR 30 a call to "
        "elf_strptr() failed trying to get a section name",
    "DW_DLE_DEBUG_INFO_DUPLICATE 31  Only one .debug_info  "
        "section is allowed",
    "DW_DLE_DEBUG_INFO_NULL 32 .debug_info section present but "
        "elf_getdata() failed or section is zero-length",
    "DW_DLE_DEBUG_ABBREV_DUPLICATE 33 Only one .debug_abbrev  "
        "section is allowed",
    "DW_DLE_DEBUG_ABBREV_NULL 34 .debug_abbrev section present but "
        "elf_getdata() failed or section is zero-length",
    "DW_DLE_DEBUG_ARANGES_DUPLICATE 35 Only one .debug_aranges  "
        "section is allowed",
    "DW_DLE_DEBUG_ARANGES_NULL 36 .debug_aranges section present but "
        "elf_getdata() failed or section is zero-length",
    "DW_DLE_DEBUG_LINE_DUPLICATE 37 Only one .debug_line  "
        "section is allowed",
    "DW_DLE_DEBUG_LINE_NULL (38) .debug_line section present but "
        "elf_getdata() failed or section is zero-length",
    "DW_DLE_DEBUG_LOC_DUPLICATE (39) Only one .debug_loc  "
        "section is allowed",
    "DW_DLE_DEBUG_LOC_NULL (40) .debug_loc section present but "
        "elf_getdata() failed or section is zero-length",
    "DW_DLE_DEBUG_MACINFO_DUPLICATE (41) Only one .debug_macinfo  "
        "section is allowed",
    "DW_DLE_DEBUG_MACINFO_NULL (42) .debug_macinfo section present but "
        "elf_getdata() failed or section is zero-length",
    "DW_DLE_DEBUG_PUBNAMES_DUPLICATE (43) Only one .debug_pubnames  "
        "section is allowed",
    "DW_DLE_DEBUG_PUBNAMES_NULL (44) .debug_pubnames section present but "
        "elf_getdata() failed or section is zero-length",
    "DW_DLE_DEBUG_STR_DUPLICATE (45)  Only one .debug_str  "
        "section is allowed",
    "DW_DLE_DEBUG_STR_NULL (46) .debug_str section present but "
        "elf_getdata() failed or section is zero-length",
    "DW_DLE_CU_LENGTH_ERROR (47)",
    "DW_DLE_VERSION_STAMP_ERROR (48)",
    "DW_DLE_ABBREV_OFFSET_ERROR (49)",
    "DW_DLE_ADDRESS_SIZE_ERROR (50)",
    "DW_DLE_DEBUG_INFO_PTR_NULL (51)",
    "DW_DLE_DIE_NULL (52)",
    "DW_DLE_STRING_OFFSET_BAD (53)",
    "DW_DLE_DEBUG_LINE_LENGTH_BAD (54)",
    "DW_DLE_LINE_PROLOG_LENGTH_BAD (55)",
    "DW_DLE_LINE_NUM_OPERANDS_BAD",
    "DW_DLE_LINE_SET_ADDR_ERROR",
    "DW_DLE_LINE_EXT_OPCODE_BAD",
    "DW_DLE_DWARF_LINE_NULL",
    "DW_DLE_INCL_DIR_NUM_BAD",
    "DW_DLE_LINE_FILE_NUM_BAD",
    "DW_DLE_ALLOC_FAIL",
    "DW_DLE_NO_CALLBACK_FUNC",
    "DW_DLE_SECT_ALLOC",
    "DW_DLE_FILE_ENTRY_ALLOC",
    "DW_DLE_LINE_ALLOC",
    "DW_DLE_FPGM_ALLOC",
    "DW_DLE_INCDIR_ALLOC",
    "DW_DLE_STRING_ALLOC",
    "DW_DLE_CHUNK_ALLOC",
    "DW_DLE_BYTEOFF_ERR",
    "DW_DLE_CIE_ALLOC",
    "DW_DLE_FDE_ALLOC",
    "DW_DLE_REGNO_OVFL",
    "DW_DLE_CIE_OFFS_ALLOC",
    "DW_DLE_WRONG_ADDRESS",
    "DW_DLE_EXTRA_NEIGHBORS",
    "DW_DLE_WRONG_TAG",
    "DW_DLE_DIE_ALLOC",
    "DW_DLE_PARENT_EXISTS",
    "DW_DLE_DBG_NULL",
    "DW_DLE_DEBUGLINE_ERROR",
    "DW_DLE_DEBUGFRAME_ERROR",
    "DW_DLE_DEBUGINFO_ERROR",
    "DW_DLE_ATTR_ALLOC",
    "DW_DLE_ABBREV_ALLOC",
    "DW_DLE_OFFSET_UFLW",
    "DW_DLE_ELF_SECT_ERR",
    "DW_DLE_DEBUG_FRAME_LENGTH_BAD",
    "DW_DLE_FRAME_VERSION_BAD",
    "DW_DLE_CIE_RET_ADDR_REG_ERROR",
    "DW_DLE_FDE_NULL",
    "DW_DLE_FDE_DBG_NULL",
    "DW_DLE_CIE_NULL",
    "DW_DLE_CIE_DBG_NULL",
    "DW_DLE_FRAME_TABLE_COL_BAD",
    "DW_DLE_PC_NOT_IN_FDE_RANGE",
    "DW_DLE_CIE_INSTR_EXEC_ERROR",
    "DW_DLE_FRAME_INSTR_EXEC_ERROR",
    "DW_DLE_FDE_PTR_NULL",
    "DW_DLE_RET_OP_LIST_NULL",
    "DW_DLE_LINE_CONTEXT_NULL",
    "DW_DLE_DBG_NO_CU_CONTEXT",
    "DW_DLE_DIE_NO_CU_CONTEXT",
    "DW_DLE_FIRST_DIE_NOT_CU",
    "DW_DLE_NEXT_DIE_PTR_NULL",
    "DW_DLE_DEBUG_FRAME_DUPLICATE  Only one .debug_frame  "
        "section is allowed",
    "DW_DLE_DEBUG_FRAME_NULL  .debug_frame section present but "
        "elf_getdata() failed or section is zero-length",
    "DW_DLE_ABBREV_DECODE_ERROR",
    "DW_DLE_DWARF_ABBREV_NULL",
    "DW_DLE_ATTR_NULL",
    "DW_DLE_DIE_BAD",
    "DW_DLE_DIE_ABBREV_BAD",
    "DW_DLE_ATTR_FORM_BAD",
    "DW_DLE_ATTR_NO_CU_CONTEXT",
    "DW_DLE_ATTR_FORM_SIZE_BAD",
    "DW_DLE_ATTR_DBG_NULL",
    "DW_DLE_BAD_REF_FORM",
    "DW_DLE_ATTR_FORM_OFFSET_BAD",
    "DW_DLE_LINE_OFFSET_BAD",
    "DW_DLE_DEBUG_STR_OFFSET_BAD",
    "DW_DLE_STRING_PTR_NULL",
    "DW_DLE_PUBNAMES_VERSION_ERROR",
    "DW_DLE_PUBNAMES_LENGTH_BAD",
    "DW_DLE_GLOBAL_NULL",
    "DW_DLE_GLOBAL_CONTEXT_NULL",
    "DW_DLE_DIR_INDEX_BAD",
    "DW_DLE_LOC_EXPR_BAD",
    "DW_DLE_DIE_LOC_EXPR_BAD",
    "DW_DLE_ADDR_ALLOC",
    "DW_DLE_OFFSET_BAD",
    "DW_DLE_MAKE_CU_CONTEXT_FAIL",
    "DW_DLE_REL_ALLOC",
    "DW_DLE_ARANGE_OFFSET_BAD",
    "DW_DLE_SEGMENT_SIZE_BAD (135) Size of a segment selector should usually be less than 8 (bytes).",
    "DW_DLE_ARANGE_LENGTH_BAD",
    "DW_DLE_ARANGE_DECODE_ERROR",
    "DW_DLE_ARANGES_NULL",
    "DW_DLE_ARANGE_NULL",
    "DW_DLE_NO_FILE_NAME",
    "DW_DLE_NO_COMP_DIR",
    "DW_DLE_CU_ADDRESS_SIZE_BAD",
    "DW_DLE_INPUT_ATTR_BAD",
    "DW_DLE_EXPR_NULL",
    "DW_DLE_BAD_EXPR_OPCODE",
    "DW_DLE_EXPR_LENGTH_BAD",
    "DW_DLE_MULTIPLE_RELOC_IN_EXPR",
    "DW_DLE_ELF_GETIDENT_ERROR",
    "DW_DLE_NO_AT_MIPS_FDE",
    "DW_DLE_NO_CIE_FOR_FDE",
    "DW_DLE_DIE_ABBREV_LIST_NULL",
    "DW_DLE_DEBUG_FUNCNAMES_DUPLICATE",
    "DW_DLE_DEBUG_FUNCNAMES_NULL .debug_funcnames section present but "
        "elf_getdata() failed or section is zero-length",
    "DW_DLE_DEBUG_FUNCNAMES_VERSION_ERROR",
    "DW_DLE_DEBUG_FUNCNAMES_LENGTH_BAD",
    "DW_DLE_FUNC_NULL",
    "DW_DLE_FUNC_CONTEXT_NULL",
    "DW_DLE_DEBUG_TYPENAMES_DUPLICATE",
    "DW_DLE_DEBUG_TYPENAMES_NULL .debug_typenames section present but "
        "elf_getdata() failed or section is zero-length",
    "DW_DLE_DEBUG_TYPENAMES_VERSION_ERROR",
    "DW_DLE_DEBUG_TYPENAMES_LENGTH_BAD",
    "DW_DLE_TYPE_NULL",
    "DW_DLE_TYPE_CONTEXT_NULL",
    "DW_DLE_DEBUG_VARNAMES_DUPLICATE",
    "DW_DLE_DEBUG_VARNAMES_NULL .debug_varnames section present but "
        "elf_getdata() failed or section is zero-length",
    "DW_DLE_DEBUG_VARNAMES_VERSION_ERROR",
    "DW_DLE_DEBUG_VARNAMES_LENGTH_BAD",
    "DW_DLE_VAR_NULL",
    "DW_DLE_VAR_CONTEXT_NULL",
    "DW_DLE_DEBUG_WEAKNAMES_DUPLICATE",
    "DW_DLE_DEBUG_WEAKNAMES_NULL .debug_weaknames section present but "
        "elf_getdata() failed or section is zero-length",

    "DW_DLE_DEBUG_WEAKNAMES_VERSION_ERROR",
    "DW_DLE_DEBUG_WEAKNAMES_LENGTH_BAD",
    "DW_DLE_WEAK_NULL",
    "DW_DLE_WEAK_CONTEXT_NULL (175)",
    "DW_DLE_LOCDESC_COUNT_WRONG (176)",
    "DW_DLE_MACINFO_STRING_NULL (177)",
    "DW_DLE_MACINFO_STRING_EMPTY (178)",
    "DW_DLE_MACINFO_INTERNAL_ERROR_SPACE (179)",
    "DW_DLE_MACINFO_MALLOC_FAIL (180)",
    "DW_DLE_DEBUGMACINFO_ERROR (181)",
    "DW_DLE_DEBUG_MACRO_LENGTH_BAD (182)",
    "DW_DLE_DEBUG_MACRO_MAX_BAD (183)",
    "DW_DLE_DEBUG_MACRO_INTERNAL_ERR (184)",
    "DW_DLE_DEBUG_MACRO_MALLOC_SPACE (185)",
    "DW_DLE_DEBUG_MACRO_INCONSISTENT (186)",
    "DW_DLE_DF_NO_CIE_AUGMENTATION(187)",
    "DW_DLE_DF_REG_NUM_TOO_HIGH(188)",
    "DW_DLE_DF_MAKE_INSTR_NO_INIT(189)",
    "DW_DLE_DF_NEW_LOC_LESS_OLD_LOC(190)",
    "DW_DLE_DF_POP_EMPTY_STACK(191)",
    "DW_DLE_DF_ALLOC_FAIL(192)",
    "DW_DLE_DF_FRAME_DECODING_ERROR(193)",
    "DW_DLE_DEBUG_LOC_SECTION_SHORT(194)",
    "DW_DLE_FRAME_AUGMENTATION_UNKNOWN(195)",
    "DW_DLE_PUBTYPE_CONTEXT(196)",
    "DW_DLE_DEBUG_PUBTYPES_LENGTH_BAD(197)",
    "DW_DLE_DEBUG_PUBTYPES_VERSION_ERROR(198)",
    "DW_DLE_DEBUG_PUBTYPES_DUPLICATE(199)",
    "DW_DLE_FRAME_CIE_DECODE_ERROR(200)",
    "DW_DLE_FRAME_REGISTER_UNREPRESENTABLE(201)",
    "DW_DLE_FRAME_REGISTER_COUNT_MISMATCH(202)",
    "DW_DLE_LINK_LOOP(203)",
    "DW_DLE_STRP_OFFSET_BAD(204)",
    "DW_DLE_DEBUG_RANGES_DUPLICATE(205)",
    "DW_DLE_DEBUG_RANGES_OFFSET_BAD(206)",
    "DW_DLE_DEBUG_RANGES_MISSING_END(207)",
    "DW_DLE_DEBUG_RANGES_OUT_OF_MEM(208)",
    "DW_DLE_DEBUG_SYMTAB_ERR(209)",
    "DW_DLE_DEBUG_STRTAB_ERR(210)",
    "DW_DLE_RELOC_MISMATCH_INDEX(211)",
    "DW_DLE_RELOC_MISMATCH_RELOC_INDEX(212)",
    "DW_DLE_RELOC_MISMATCH_STRTAB_INDEX(213)",
    "DW_DLE_RELOC_SECTION_MISMATCH(214)",
    "DW_DLE_RELOC_SECTION_MISSING_INDEX(215)",
    "DW_DLE_RELOC_SECTION_LENGTH_ODD(216)",
    "DW_DLE_RELOC_SECTION_PTR_NULL(217)",
    "DW_DLE_RELOC_SECTION_MALLOC_FAIL(218)",
    "DW_DLE_NO_ELF64_SUPPORT(219)",
    "DW_DLE_MISSING_ELF64_SUPPORT(220)",
    "DW_DLE_ORPHAN_FDE(221)",
    "DW_DLE_DUPLICATE_INST_BLOCK(222)",
    "DW_DLE_BAD_REF_SIG8_FORM(223)",
    "DW_DLE_ATTR_EXPRLOC_FORM_BAD(224)",
    "DW_DLE_FORM_SEC_OFFSET_LENGTH_BAD(225)",
    "DW_DLE_NOT_REF_FORM(226)",
    "DW_DLE_DEBUG_FRAME_LENGTH_NOT_MULTIPLE(227)",
    "DW_DLE_REF_SIG8_NOT_HANDLED (228)",
    "DW_DLE_DEBUG_FRAME_POSSIBLE_ADDRESS_BOTCH (229)",
    "DW_DLE_LOC_BAD_TERMINATION (230) the last location operator in an expression is missing some associated data, an operator ended too soon",
    "DW_DLE_SYMTAB_SECTION_LENGTH_ODD (231) so doing relocations seems unsafe",
    "DW_DLE_RELOC_SECTION_SYMBOL_INDEX_BAD (232) so doing a relocation seems unsafe",
    "DW_DLE_RELOC_SECTION_RELOC_TARGET_SIZE_UNKNOWN (233) so doing a relocation is unsafe",
    "DW_DLE_SYMTAB_SECTION_ENTRYSIZE_ZERO(234)",
    "DW_DLE_LINE_NUMBER_HEADER_ERROR (235), a line number program header seems incomplete (perhaps the header_length is wrong?).",
    "DW_DLE_DEBUG_TYPES_NULL (236)",
    "DW_DLE_DEBUG_TYPES_DUPLICATE (237)",
    "DW_DLE_DEBUG_TYPES_ONLY_DWARF4 (238) DW4 and DW5 have types CUs",
    "DW_DLE_DEBUG_TYPEOFFSET_BAD (239)",
    "DW_DLE_GNU_OPCODE_ERROR (240)",
    "DW_DLE_DEBUGPUBTYPES_ERROR (241), could not create pubtypes section",
    "DW_DLE_AT_FIXUP_NULL (242)",
    "DW_DLE_AT_FIXUP_DUP (243)",
    "DW_DLE_BAD_ABINAME (244)",
    "DW_DLE_TOO_MANY_DEBUG(245), too many .debug_* sections present somehow",
    "DW_DLE_DEBUG_STR_OFFSETS_DUPLICATE(246)",
    "DW_DLE_SECTION_DUPLICATION(247)",
    "DW_DLE_SECTION_ERROR(248)",
    "DW_DLE_DEBUG_ADDR_DUPLICATE(249)",
    "DW_DLE_DEBUG_CU_UNAVAILABLE_FOR_FORM(250)",
    "DW_DLE_DEBUG_FORM_HANDLING_INCOMPLETE(251)",
    "DW_DLE_NEXT_DIE_PAST_END(252)",
    "DW_DLE_NEXT_DIE_WRONG_FORM(253)",
    "DW_DLE_NEXT_DIE_NO_ABBREV_LIST(254)",
    "DW_DLE_NESTED_FORM_INDIRECT_ERROR(255)",
    "DW_DLE_CU_DIE_NO_ABBREV_LIST(256)",
    "DW_DLE_MISSING_NEEDED_DEBUG_ADDR_SECTION(257)",
    "DW_DLE_ATTR_FORM_NOT_ADDR_INDEX(258)",
    "DW_DLE_ATTR_FORM_NOT_STR_INDEX(259)",
    "DW_DLE_DUPLICATE_GDB_INDEX(260)",
    "DW_DLE_ERRONEOUS_GDB_INDEX_SECTION(261)",
    "DW_DLE_GDB_INDEX_COUNT_ERROR(262)",
    "DW_DLE_GDB_INDEX_COUNT_ADDR_ERROR(263)",
    "DW_DLE_GDB_INDEX_CUVEC_ERROR(264)",
    "DW_DLE_GDB_INDEX_INDEX_ERROR(264)"
    "DW_DLE_GDB_INDEX_CUVEC_ERROR(265)",
    "DW_DLE_DUPLICATE_CU_INDEX(266)",
    "DW_DLE_DUPLICATE_TU_INDEX(267)",
    "DW_DLE_XU_TYPE_ARG_ERROR(268)",
    "DW_DLE_XU_IMPOSSIBLE_ERROR(269)",
    "DW_DLE_XU_NAME_COL_ERROR(270)",
    "DW_DLE_XU_HASH_ROW_ERROR(271)",
    "DW_DLE_XU_HASH_INDEX_ERROR(272)",
    "DW_DLE_FAILSAFE_ERRVAL(273)",
    "DW_DLE_ARANGE_ERROR(274) producer problem in object generation",
    "DW_DLE_PUBNAMES_ERROR(275) producer problem in object generation",
    "DW_DLE_FUNCNAMES_ERROR(276) producer problem in object generation",
    "DW_DLE_TYPENAMES_ERROR(277) producer problem in object generation",
    "DW_DLE_VARNAMES_ERROR(278) producer problem in object generation",
    "DW_DLE_WEAKNAMES_ERROR(279) producer problem in object generation",
    "DW_DLE_RELOCS_ERROR(280) producer problem in object generation",
    "DW_DLE_DW_DLE_ATTR_OUTSIDE_SECTION(281)",
    "DW_DLE_FISSION_INDEX_WRONG(282)",
    "DW_DLE_FISSION_VERSION_ERROR(283)",
    "DW_DLE_NEXT_DIE_LOW_ERROR(284) corrupted DIE tree",
    "DW_DLE_CU_UT_TYPE_ERROR(285) not a valid DW_UT_* value",
    "DW_DLE_NO_SUCH_SIGNATURE_FOUND(286) CU signature not in the index",
    "DW_DLE_SIGNATURE_SECTION_NUMBER_WRONG(287) libdwarf software error",
    "DW_DLE_ATTR_FORM_NOT_DATA8(288) wanted an 8 byte signature",
    "DW_DLE_SIG_TYPE_WRONG_STRING (289) expected tu or cu",
    "DW_DLE_MISSING_REQUIRED_TU_OFFSET_HASH(290) is a broken dwp package file",
    "DW_DLE_MISSING_REQUIRED_CU_OFFSET_HASH(291) is a broken dwp package file",
    "DW_DLE_DWP_MISSING_DWO_ID(292)",
    "DW_DLE_DWP_SIBLING_ERROR(293)",
    "DW_DLE_DEBUG_FISSION_INCOMPLETE(294)",
    "DW_DLE_FISSION_SECNUM_ERR(295) internal libdwarf error",
    "DW_DLE_DEBUG_MACRO_DUPLICATE(296)",
    "DW_DLE_DEBUG_NAMES_DUPLICATE(297)",
    "DW_DLE_DEBUG_LINE_STR_DUPLICATE(298)",
    "DW_DLE_DEBUG_SUP_DUPLICATE(299)",
};




/*  This function performs error handling as described in the
    libdwarf consumer document section 3.  Dbg is the Dwarf_debug
    structure being processed.  Error is a pointer to the pointer
    to the error descriptor that will be returned.  Errval is an
    error code listed in dwarf_error.h.

    If the malloc arena is exhausted we return a pointer to
    a special static error record.  This special singleton
    is mostly ignored by dwarf_dealloc().
    Users should not be storing Dwarf_Error pointers
    for long so this singleton is only going to cause
    confusion when callers try to save an out-of-memory
    Dwarf_Error pointer.
    The _dwarf_failsafe_error is intended to
    be an improvement over an abort() call.
    The failsafe means we will not abort due to
    a Dwarf_Error struct creation.
*/
void
_dwarf_error(Dwarf_Debug dbg, Dwarf_Error * error, Dwarf_Sword errval)
{
    Dwarf_Error errptr;

    /*  Allow NULL dbg on entry, since sometimes that can happen and we
        want to report the upper-level error, not this one. */
    if (error != NULL) {
        /*  If dbg is NULL, use the alternate error struct. However,
            this will overwrite the earlier error. */
        if (dbg != NULL) {
            errptr =
                (Dwarf_Error) _dwarf_get_alloc(dbg, DW_DLA_ERROR, 1);
            if (errptr == NULL) {
                errptr = &_dwarf_failsafe_error;
                errptr->er_static_alloc = 1;
            }
        } else {
            /*  We have no dbg to work with. dwarf_init failed. We hack
                up a special area. */
            errptr = _dwarf_special_no_dbg_error_malloc();
            if (errptr == NULL) {
                errptr = &_dwarf_failsafe_error;
                errptr->er_static_alloc = 1;
            }
        }

        errptr->er_errval = errval;
        *error = errptr;
        return;
    }

    if (dbg != NULL && dbg->de_errhand != NULL) {
        errptr = (Dwarf_Error) _dwarf_get_alloc(dbg, DW_DLA_ERROR, 1);
        if (errptr == NULL) {
            errptr = &_dwarf_failsafe_error;
            errptr->er_static_alloc = 1;
        }
        errptr->er_errval = errval;
        dbg->de_errhand(errptr, dbg->de_errarg);
        return;
    }
    fprintf(stderr,
        "abort() in libdwarf. No error argument, no handler.\n");
    abort();
}


Dwarf_Unsigned
dwarf_errno(Dwarf_Error error)
{
    if (error == NULL) {
        return (0);
    }

    return (error->er_errval);
}


/*
*/
char *
dwarf_errmsg(Dwarf_Error error)
{
    if (error == NULL) {
        return "Dwarf_Error is NULL";
    }

    if (error->er_errval >=
        (Dwarf_Signed)(sizeof(_dwarf_errmsgs) / sizeof(char *))) {
        return "Dwarf_Error value out of range";
    }

    return ((char *) _dwarf_errmsgs[error->er_errval]);
}
