/*

  Copyright (C) 2000,2002,2004,2005,2006 Silicon Graphics, Inc.  All Rights Reserved.
  Portions Copyright (C) 2007-2013 David Anderson. All Rights Reserved.
  Portions Copyright 2012 SN Systems Ltd. All rights reserved.

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
#include <time.h>
#include "dwarf_line.h"


/* FIXME Need to add prologue_end epilogue_begin isa fields. */
static void
print_line_header(Dwarf_Debug dbg)
{
/* Ugly indenting, but makes lines shorter to see them better. */
dwarf_printf(dbg,
"                                                         s b e p e i d\n"
"                                                         t l s r p s i\n"
"                                                         m c e o i a s\n"
" section    op                                       col t k q l l   c\n"
" offset     code               address     file line umn ? ? ? ? ? \n");
}

/* FIXME: print new line values:   prologue_end epilogue_begin isa */
static void
print_line_detail(
    Dwarf_Debug dbg,
    const char *prefix,
    int opcode,
    Dwarf_Unsigned address,
    unsigned long file,
    unsigned long line,
    unsigned long column,
    int is_stmt, int basic_block, int end_sequence,
    int prologue_end, int epilogue_begin, int isa,
    Dwarf_Unsigned discriminator)
{
    dwarf_printf(dbg,
        "%-15s %2d 0x%" DW_PR_XZEROS DW_PR_DUx " "
        "%2lu   %4lu %2lu   %1d %1d %1d",
        prefix,
        (int) opcode,
        (Dwarf_Unsigned) address,
        (unsigned long) file,
        (unsigned long) line,
        (unsigned long) column,
        (int) is_stmt, (int) basic_block, (int) end_sequence);
    if (discriminator || prologue_end || epilogue_begin || isa) {
        dwarf_printf(dbg,
            " %1d", prologue_end);
        dwarf_printf(dbg,
            " %1d", epilogue_begin);
        dwarf_printf(dbg,
            " %1d", isa);
        dwarf_printf(dbg,
            " 0x%" DW_PR_DUx , discriminator);
    }
    dwarf_printf(dbg,
        "\n");
}


/*  return DW_DLV_OK if ok. else DW_DLV_NO_ENTRY or DW_DLV_ERROR
    If err_count_out is non-NULL, this is a special 'check'
    call.  */
static int
_dwarf_internal_printlines(Dwarf_Die die, Dwarf_Error * error,
int * err_count_out, int only_line_header)
{
    /*  This pointer is used to scan the portion of the .debug_line
        section for the current cu. */
    Dwarf_Small *line_ptr = 0;
    Dwarf_Small *orig_line_ptr = 0;

    /*  This points to the last byte of the .debug_line portion for the
        current cu. */
    Dwarf_Small *line_ptr_end = 0;

    /*  Pointer to a DW_AT_stmt_list attribute in case it exists in the
        die. */
    Dwarf_Attribute stmt_list_attr = 0;

    /*  Pointer to DW_AT_comp_dir attribute in die. */
    Dwarf_Attribute comp_dir_attr = 0;

    /*  Pointer to name of compilation directory. */
    Dwarf_Small *comp_dir = NULL;

    /*  Offset into .debug_line specified by a DW_AT_stmt_list
        attribute. */
    Dwarf_Unsigned line_offset = 0;

    struct Line_Table_Prefix_s prefix;


    /*  These are the state machine state variables. */
    Dwarf_Addr address = 0;
    Dwarf_Word file = 1;
    Dwarf_Word line = 1;
    Dwarf_Word column = 0;
    Dwarf_Bool is_stmt = false;
    Dwarf_Bool basic_block = false;
    Dwarf_Bool end_sequence = false;
    Dwarf_Bool prologue_end = false;
    Dwarf_Bool epilogue_begin = false;
    Dwarf_Small isa = 0;
    Dwarf_Unsigned op_index  = 0;
    Dwarf_Unsigned discriminator = 0;


    Dwarf_Sword i=0;
    Dwarf_Word u=0;

    /*  This is the current opcode read from the statement program. */
    Dwarf_Small opcode=0;


    /*  These variables are used to decode leb128 numbers. Leb128_num
        holds the decoded number, and leb128_length is its length in
        bytes. */
    Dwarf_Word leb128_num=0;
    Dwarf_Word leb128_length=0;
    Dwarf_Sword advance_line=0;
    Dwarf_Half attrform = 0;
    /*  This is the operand of the latest fixed_advance_pc extended
        opcode. */
    Dwarf_Half fixed_advance_pc=0;

    /*  In case there are wierd bytes 'after' the line table
        prologue this lets us print something. This is a gcc
        compiler bug and we expect the bytes count to be 12.  */
    Dwarf_Small* bogus_bytes_ptr = 0;
    Dwarf_Unsigned bogus_bytes_count = 0;


    /* The Dwarf_Debug this die belongs to. */
    Dwarf_Debug dbg=0;
    int resattr = DW_DLV_ERROR;
    int lres =    DW_DLV_ERROR;
    int res  =    DW_DLV_ERROR;

    /* ***** BEGIN CODE ***** */

    if (error != NULL) {
        *error = NULL;
    }

    CHECK_DIE(die, DW_DLV_ERROR);
    dbg = die->di_cu_context->cc_dbg;

    res = _dwarf_load_section(dbg, &dbg->de_debug_line,error);
    if (res != DW_DLV_OK) {
        return res;
    }
    if (!dbg->de_debug_line.dss_size) {
        return (DW_DLV_NO_ENTRY);
    }


    resattr = dwarf_attr(die, DW_AT_stmt_list, &stmt_list_attr, error);
    if (resattr != DW_DLV_OK) {
        return resattr;
    }


    /*  The list of relevant FORMs is small.
        DW_FORM_data4, DW_FORM_data8, DW_FORM_sec_offset
    */
    lres = dwarf_whatform(stmt_list_attr,&attrform,error);
    if (lres != DW_DLV_OK) {
        return lres;
    }
    if (attrform != DW_FORM_data4 && attrform != DW_FORM_data8 &&
        attrform != DW_FORM_sec_offset ) {
        _dwarf_error(dbg, error, DW_DLE_LINE_OFFSET_BAD);
        return (DW_DLV_ERROR);
    }
    lres = dwarf_global_formref(stmt_list_attr, &line_offset, error);
    if (lres != DW_DLV_OK) {
        return lres;
    }

    if (line_offset >= dbg->de_debug_line.dss_size) {
        _dwarf_error(dbg, error, DW_DLE_LINE_OFFSET_BAD);
        return (DW_DLV_ERROR);
    }
    orig_line_ptr = dbg->de_debug_line.dss_data;
    line_ptr = dbg->de_debug_line.dss_data + line_offset;
    dwarf_dealloc(dbg, stmt_list_attr, DW_DLA_ATTR);

    /*  If die has DW_AT_comp_dir attribute, get the string that names
        the compilation directory. */
    resattr = dwarf_attr(die, DW_AT_comp_dir, &comp_dir_attr, error);
    if (resattr == DW_DLV_ERROR) {
        return resattr;
    }
    if (resattr == DW_DLV_OK) {
        int cres = DW_DLV_ERROR;
        char *cdir = 0;

        cres = dwarf_formstring(comp_dir_attr, &cdir, error);
        if (cres == DW_DLV_ERROR) {
            return cres;
        } else if (cres == DW_DLV_OK) {
            comp_dir = (Dwarf_Small *) cdir;
        }
    }
    if (resattr == DW_DLV_OK) {
        dwarf_dealloc(dbg, comp_dir_attr, DW_DLA_ATTR);
    }

    dwarf_init_line_table_prefix(&prefix);
    {
        Dwarf_Small *line_ptr_out = 0;
        int dres = dwarf_read_line_table_prefix(dbg,
            line_ptr,dbg->de_debug_line.dss_size - line_offset,
            &line_ptr_out,
            &prefix,
            &bogus_bytes_ptr,
            &bogus_bytes_count,
            error,
            err_count_out);
        if (dres == DW_DLV_ERROR) {
            dwarf_free_line_table_prefix(&prefix);
            return dres;
        }
        if (dres == DW_DLV_NO_ENTRY) {
            dwarf_free_line_table_prefix(&prefix);
            return dres;
        }
        line_ptr_end = prefix.pf_line_ptr_end;
        line_ptr = line_ptr_out;
    }
    if (only_line_header) {
        /* Just checking for header errors, nothing more here.*/
        dwarf_free_line_table_prefix(&prefix);
        return DW_DLV_OK;
    }

    dwarf_printf(dbg,
        "total line info length %ld bytes, "
        "line offset 0x%" DW_PR_XZEROS DW_PR_DUx " %" DW_PR_DSd "\n",
        (long) prefix.pf_total_length,
        (Dwarf_Unsigned) line_offset, (Dwarf_Signed) line_offset);
    dwarf_printf(dbg,
        "line table version %d\n",(int) prefix.pf_version);
    dwarf_printf(dbg,
        "line table length field length %d prologue length %d\n",
        (int)prefix.pf_length_field_length,
        (int)prefix.pf_prologue_length);
    dwarf_printf(dbg,
        "compilation_directory %s\n",
        comp_dir ? ((char *) comp_dir) : "");

    dwarf_printf(dbg,
        "  min instruction length %d\n",
        (int) prefix.pf_minimum_instruction_length);
    dwarf_printf(dbg,
        "  default is stmt        %d\n", (int) prefix.pf_default_is_stmt);
    dwarf_printf(dbg,
        "  line base              %d\n", (int) prefix.pf_line_base);
    dwarf_printf(dbg,
        "  line_range             %d\n", (int) prefix.pf_line_range);
    dwarf_printf(dbg,
        "  opcode base            %d\n", (int) prefix.pf_opcode_base);
    dwarf_printf(dbg,
        "  standard opcode count  %d\n", (int) prefix.pf_std_op_count);

    for (i = 1; i < prefix.pf_opcode_base; i++) {
        dwarf_printf(dbg,
            "  opcode[%2d] length  %d\n", (int) i,
            (int) prefix.pf_opcode_length_table[i - 1]);
    }
    dwarf_printf(dbg,
        "  include directories count %d\n",
        (int) prefix.pf_include_directories_count);
    for (u = 0; u < prefix.pf_include_directories_count; ++u) {
        dwarf_printf(dbg,
            "  include dir[%u] %s\n",
            (int) u, prefix.pf_include_directories[u]);
    }
    dwarf_printf(dbg,
        "  files count            %d\n",
        (int) prefix.pf_files_count);

    for (u = 0; u < prefix.pf_files_count; ++u) {
        struct Line_Table_File_Entry_s *lfile =
            prefix.pf_line_table_file_entries + u;
        Dwarf_Unsigned tlm2 = lfile->lte_last_modification_time;
        Dwarf_Unsigned di = lfile->lte_directory_index;
        Dwarf_Unsigned fl = lfile->lte_length_of_file;

        dwarf_printf(dbg,
            "  file[%u]  %s (file-number: %u) \n",
            (unsigned) u, (char *) lfile->lte_filename,
            (unsigned)(u+1));
        dwarf_printf(dbg,
            "    dir index %d\n", (int) di);
        {
            time_t tt = (time_t) tlm2;

            /* ctime supplies newline */
            dwarf_printf(dbg,
                "    last time 0x%x %s",
                (unsigned) tlm2, ctime(&tt));
        }
        dwarf_printf(dbg,
            "    file length %ld 0x%lx\n",
            (long) fl, (unsigned long) fl);

    }


    {
        Dwarf_Unsigned offset = 0;
        if (bogus_bytes_count > 0) {
            Dwarf_Unsigned wcount = bogus_bytes_count;
            Dwarf_Unsigned boffset = bogus_bytes_ptr - orig_line_ptr;
            dwarf_printf(dbg,
                "*** DWARF CHECK: the line table prologue  header_length "
                " is %" DW_PR_DUu " too high, we pretend it is smaller."
                "Section offset: 0x%" DW_PR_XZEROS DW_PR_DUx
                " (%" DW_PR_DUu ") ***\n",
                wcount, boffset,boffset);
            *err_count_out += 1;
        }
        offset = line_ptr - orig_line_ptr;
        dwarf_printf(dbg,
            "  statement prog offset in section: 0x%"
            DW_PR_XZEROS DW_PR_DUx " (%" DW_PR_DUu ")\n",
            offset, offset);
    }

    /*  Initialize the part of the state machine dependent on the
        prefix.  */
    is_stmt = prefix.pf_default_is_stmt;

    print_line_header(dbg);
    /* Start of statement program.  */
    while (line_ptr < line_ptr_end) {
        int type = 0;

        dwarf_printf(dbg,
            " [0x%06" DW_PR_DSx "] ",
            (Dwarf_Signed) (line_ptr - orig_line_ptr));
        opcode = *(Dwarf_Small *) line_ptr;
        line_ptr++;
        /* 'type' is the output */
        WHAT_IS_OPCODE(type, opcode, prefix.pf_opcode_base,
            prefix.pf_opcode_length_table, line_ptr,
            prefix.pf_std_op_count);
        if (type == LOP_DISCARD) {
            int oc = 0;
            int opcnt = prefix.pf_opcode_length_table[opcode];

            dwarf_printf(dbg,
                "*** DWARF CHECK: DISCARD standard opcode %d "
                "with %d operands: "
                "not understood.", opcode, opcnt);
            *err_count_out += 1;
            for (oc = 0; oc < opcnt; oc++) {
                /*  Read and discard operands we don't
                    understand.
                    Arbitrary choice of unsigned read.
                    Signed read would work as well.  */
                Dwarf_Unsigned utmp2 = 0;

                DECODE_LEB128_UWORD(line_ptr, utmp2);
                dwarf_printf(dbg,
                    " %" DW_PR_DUu
                    " (0x%" DW_PR_XZEROS DW_PR_DUx ")",
                    (Dwarf_Unsigned) utmp2,
                    (Dwarf_Unsigned) utmp2);
            }
            dwarf_printf(dbg,
                "***\n");
            /* Do nothing, necessary ops done */
        } else if (type == LOP_SPECIAL) {
            /*  This op code is a special op in the object, no matter
                that it might fall into the standard op range in this
                compile Thatis, these are special opcodes between
                special_opcode_base and MAX_LINE_OP_CODE.  (including
                special_opcode_base and MAX_LINE_OP_CODE) */
            char special[50];
            Dwarf_Unsigned operation_advance = 0;
            unsigned origop = opcode;

            opcode = opcode - prefix.pf_opcode_base;
            operation_advance = (opcode / prefix.pf_line_range);
            if (prefix.pf_maximum_ops_per_instruction < 2) {
                address = address + (prefix.pf_minimum_instruction_length *
                    operation_advance);
            } else {
                address = address + (prefix.pf_minimum_instruction_length *
                    ((op_index + operation_advance)/
                    prefix.pf_maximum_ops_per_instruction));
                op_index = (op_index +operation_advance)%
                    prefix.pf_maximum_ops_per_instruction;
            }
            line = line + prefix.pf_line_base +
                opcode % prefix.pf_line_range;
            sprintf(special, "Specialop %3u", origop);
            print_line_detail(dbg,special,
                opcode, address, (int) file, line, column,
                is_stmt, basic_block, end_sequence,
                prologue_end, epilogue_begin, isa,discriminator);
            basic_block = false;
            prologue_end = false;
            epilogue_begin = false;
            discriminator = 0;
        } else if (type == LOP_STANDARD) {
            switch (opcode) {

            case DW_LNS_copy:{
                print_line_detail(dbg,"DW_LNS_copy",
                    opcode, address, file, line,
                    column, is_stmt, basic_block,
                    end_sequence, prologue_end,
                    epilogue_begin, isa,discriminator);
                basic_block = false;
                prologue_end = false;
                epilogue_begin = false;
                discriminator = 0;
                }
                break;

            case DW_LNS_advance_pc:{
                Dwarf_Unsigned utmp2 = 0;

                DECODE_LEB128_UWORD(line_ptr, utmp2);
                dwarf_printf(dbg,
                    "DW_LNS_advance_pc val %"
                    DW_PR_DSd " 0x%"
                    DW_PR_XZEROS DW_PR_DUx "\n",
                    (Dwarf_Signed) (Dwarf_Word) utmp2,
                    (Dwarf_Unsigned) (Dwarf_Word) utmp2);
                leb128_num = (Dwarf_Word) utmp2;
                address = address +
                    prefix.pf_minimum_instruction_length * leb128_num;
                }
                break;
            case DW_LNS_advance_line:{
                Dwarf_Signed stmp = 0;

                DECODE_LEB128_SWORD(line_ptr, stmp);
                advance_line = (Dwarf_Sword) stmp;
                dwarf_printf(dbg,
                    "DW_LNS_advance_line val %" DW_PR_DSd " 0x%"
                    DW_PR_XZEROS DW_PR_DSx "\n",
                    (Dwarf_Signed) advance_line,
                    (Dwarf_Signed) advance_line);
                line = line + advance_line;
                }
                break;

            case DW_LNS_set_file:{
                Dwarf_Unsigned utmp2 = 0;

                DECODE_LEB128_UWORD(line_ptr, utmp2);
                file = (Dwarf_Word) utmp2;
                dwarf_printf(dbg,
                    "DW_LNS_set_file  %ld\n", (long) file);
                }
                break;
            case DW_LNS_set_column:{
                Dwarf_Unsigned utmp2 = 0;

                DECODE_LEB128_UWORD(line_ptr, utmp2);
                column = (Dwarf_Word) utmp2;
                dwarf_printf(dbg,
                    "DW_LNS_set_column val %" DW_PR_DSd " 0x%"
                    DW_PR_XZEROS DW_PR_DSx "\n",
                    (Dwarf_Signed) column, (Dwarf_Signed) column);
                }
                break;
            case DW_LNS_negate_stmt:{
                is_stmt = !is_stmt;
                dwarf_printf(dbg,
                    "DW_LNS_negate_stmt\n");
                }
                break;
            case DW_LNS_set_basic_block:{
                dwarf_printf(dbg,
                    "DW_LNS_set_basic_block\n");
                basic_block = true;
                }
                break;

            case DW_LNS_const_add_pc:{
                opcode = MAX_LINE_OP_CODE - prefix.pf_opcode_base;
                if (prefix.pf_maximum_ops_per_instruction < 2) {
                    Dwarf_Unsigned operation_advance =
                        (opcode / prefix.pf_line_range);
                    address = address +
                        prefix.pf_minimum_instruction_length *
                            operation_advance;
                } else {
                    Dwarf_Unsigned operation_advance =
                        (opcode / prefix.pf_line_range);
                    address = address + prefix.pf_minimum_instruction_length *
                        ((op_index + operation_advance)/
                        prefix.pf_maximum_ops_per_instruction);
                    op_index = (op_index +operation_advance)%
                        prefix.pf_maximum_ops_per_instruction;
                }

                dwarf_printf(dbg,
                    "DW_LNS_const_add_pc new address 0x%"
                    DW_PR_XZEROS DW_PR_DSx "\n",
                    (Dwarf_Signed) address);
                }
                break;
            case DW_LNS_fixed_advance_pc:{
                READ_UNALIGNED(dbg, fixed_advance_pc, Dwarf_Half,
                    line_ptr, sizeof(Dwarf_Half));
                line_ptr += sizeof(Dwarf_Half);
                address = address + fixed_advance_pc;
                dwarf_printf(dbg,
                    "DW_LNS_fixed_advance_pc val %" DW_PR_DSd
                    " 0x%" DW_PR_XZEROS DW_PR_DSx
                    " new address 0x%" DW_PR_XZEROS DW_PR_DSx "\n",
                    (Dwarf_Signed) fixed_advance_pc,
                    (Dwarf_Signed) fixed_advance_pc,
                    (Dwarf_Signed) address);
                op_index = 0;
                }
                break;
            case DW_LNS_set_prologue_end:{
                prologue_end = true;
                dwarf_printf(dbg,
                    "DW_LNS_set_prologue_end set true.\n");
                }
                break;
                /* New in DWARF3 */
            case DW_LNS_set_epilogue_begin:{
                epilogue_begin = true;
                dwarf_printf(dbg,
                    "DW_LNS_set_epilogue_begin set true.\n");
                }
                break;

                /* New in DWARF3 */
            case DW_LNS_set_isa:{
                Dwarf_Unsigned utmp2;

                DECODE_LEB128_UWORD(line_ptr, utmp2);
                isa = utmp2;
                dwarf_printf(dbg,
                    "DW_LNS_set_isa new value 0x%"
                    DW_PR_XZEROS DW_PR_DUx ".\n",
                    (Dwarf_Unsigned) utmp2);
                if (isa != utmp2) {
                    /*  The value of the isa did not fit in our
                        local so we record it wrong. declare an
                        error. */
                    dwarf_free_line_table_prefix(&prefix);
                    _dwarf_error(dbg, error,
                        DW_DLE_LINE_NUM_OPERANDS_BAD);
                    return (DW_DLV_ERROR);
                }
                }
                break;
            } /* end switch */
        } else if (type == LOP_EXTENDED) {
            Dwarf_Unsigned utmp3 = 0;
            Dwarf_Word instr_length = 0;
            Dwarf_Small ext_opcode = 0;

            DECODE_LEB128_UWORD(line_ptr, utmp3);
            instr_length = (Dwarf_Word) utmp3;
            ext_opcode = *(Dwarf_Small *) line_ptr;
            line_ptr++;
            switch (ext_opcode) {

            case DW_LNE_end_sequence:{
                end_sequence = true;

                print_line_detail(dbg,"DW_LNE_end_sequence extended",
                    opcode, address, file, line,
                    column, is_stmt, basic_block,
                    end_sequence, prologue_end,
                    epilogue_begin, isa,discriminator);

                address = 0;
                file = 1;
                line = 1;
                column = 0;
                is_stmt = prefix.pf_default_is_stmt;
                basic_block = false;
                end_sequence = false;
                prologue_end = false;
                epilogue_begin = false;
                isa = 0;
                discriminator = 0;
                op_index = 0;
                }
                break;
            case DW_LNE_set_address:{
                READ_UNALIGNED(dbg, address, Dwarf_Addr,
                    line_ptr,
                    die->di_cu_context->cc_address_size);

                line_ptr += die->di_cu_context->cc_address_size;
                dwarf_printf(dbg,
                    "DW_LNE_set_address address 0x%"
                    DW_PR_XZEROS DW_PR_DUx "\n",
                    (Dwarf_Unsigned) address);

                op_index = 0;
                }
                break;
            case DW_LNE_define_file:{
                Dwarf_Unsigned di = 0;
                Dwarf_Unsigned tlm = 0;
                Dwarf_Unsigned fl = 0;

                Dwarf_Small *fn = (Dwarf_Small *) line_ptr;
                line_ptr = line_ptr + strlen((char *) line_ptr) + 1;
                di = _dwarf_decode_u_leb128(line_ptr,
                    &leb128_length);
                line_ptr = line_ptr + leb128_length;
                tlm = _dwarf_decode_u_leb128(line_ptr,
                    &leb128_length);
                line_ptr = line_ptr + leb128_length;
                fl = _dwarf_decode_u_leb128(line_ptr,
                    &leb128_length);
                line_ptr = line_ptr + leb128_length;

                dwarf_printf(dbg,
                    "DW_LNE_define_file %s \n", fn);
                dwarf_printf(dbg,
                    "    dir index %d\n", (int) di);
                {
                    time_t tt3 = (time_t) tlm;

                    /* ctime supplies newline */
                    dwarf_printf(dbg,
                        "    last time 0x%x %s",
                        (unsigned) tlm, ctime(&tt3));
                }
                dwarf_printf(dbg,
                    "    file length %ld 0x%lx\n",
                    (long) fl, (unsigned long) fl);

                }
                break;
            case DW_LNE_set_discriminator:{
                /* new in DWARF4 */
                Dwarf_Unsigned utmp2 = 0;

                DECODE_LEB128_UWORD(line_ptr, utmp2);
                discriminator = (Dwarf_Word) utmp2;
                dwarf_printf(dbg,
                    "DW_LNE_set_discriminator 0x%"
                    DW_PR_XZEROS DW_PR_DUx "\n",utmp2);
                }
                break;

            default:{
                /*  This is an extended op code we do not know about,
                    other than we know now many bytes it is
                    (and the op code and the bytes of operand). */

                Dwarf_Unsigned remaining_bytes = instr_length -1;
                if (instr_length < 1 || remaining_bytes > DW_LNE_LEN_MAX) {
                    dwarf_free_line_table_prefix(&prefix);
                    _dwarf_error(dbg, error,
                        DW_DLE_LINE_EXT_OPCODE_BAD);
                    return (DW_DLV_ERROR);
                }
                dwarf_printf(dbg,
                    "DW_LNE extended op 0x%x ",ext_opcode);
                dwarf_printf(dbg,
                    "Bytecount: %" DW_PR_DUu , (Dwarf_Unsigned)instr_length);
                if (remaining_bytes > 0) {
                    dwarf_printf(dbg,
                        " linedata: 0x");
                    while (remaining_bytes > 0) {
                        dwarf_printf(dbg,
                            "%02x",(unsigned char)(*(line_ptr)));
                        line_ptr++;
                        remaining_bytes--;
                    }
                }
                dwarf_printf(dbg,
                    "\n");
            }
            break;
            } /* Dnd switch */

        }
    }

    dwarf_free_line_table_prefix(&prefix);
    return (DW_DLV_OK);
}

/*  This is support for dwarfdump: making it possible
    for clients wanting line detail info on stdout
    to get that detail without including internal libdwarf
    header information.
    Caller passes in compilation unit DIE.
    The _dwarf_ version is obsolete (though supported for
    compatibility).
    The dwarf_ version is preferred.
    The functions are intentionally identical: having
    _dwarf_print_lines call dwarf_print_lines might
    better emphasize they are intentionally identical, but
    that seemed slightly silly given how short the functions are.
    Interface adds error_count (output value) February 2009.  */
int
dwarf_print_lines(Dwarf_Die die, Dwarf_Error * error,int *error_count)
{
    int only_line_header = 0;
    int res = _dwarf_internal_printlines(die, error,
        error_count,
        only_line_header);
    if (res != DW_DLV_OK) {
        return res;
    }
    return res;
}
int
_dwarf_print_lines(Dwarf_Die die, Dwarf_Error * error)
{
    int only_line_header = 0;
    int err_count = 0;
    int res = _dwarf_internal_printlines(die, error,
        &err_count,
        only_line_header);
    /* No way to get error count back in this interface */
    if (res != DW_DLV_OK) {
        return res;
    }
    return res;
}

/* The check is in case we are not printing full line data,
   this gets some of the issues noted with .debug_line,
   but not all. Call dwarf_print_lines() to get all issues.
   Intended for apps like dwarfdump.
*/
void
dwarf_check_lineheader(Dwarf_Die die, int *err_count_out)
{
    Dwarf_Error err;
    int only_line_header = 1;
    _dwarf_internal_printlines(die, &err,err_count_out,
        only_line_header);
    return;
}

