/*
  Copyright (C) 2000-2006 Silicon Graphics, Inc.  All Rights Reserved.
  Portions Copyright 2007-2010 Sun Microsystems, Inc. All rights reserved.
  Portions Copyright 2009-2011 SN Systems Ltd. All rights reserved.
  Portions Copyright 2008-2011 David Anderson. All rights reserved.

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
#include "naming.h"
#include "dwconf.h"
#include "esb.h"
#include "uri.h"
#include <ctype.h>

#include "print_sections.h"

/*
    Print line number information:
        [line] [address] <new statement>
        new basic-block
        filename
*/


static void
print_source_intro(Dwarf_Die cu_die)
{
    Dwarf_Off off = 0;
    int ores = dwarf_dieoffset(cu_die, &off, &err);

    if (ores == DW_DLV_OK) {
        printf("Source lines (from CU-DIE at .debug_info offset 0x%"
            DW_PR_XZEROS DW_PR_DUx "):\n",
            (Dwarf_Unsigned) off);
    } else {
        printf("Source lines (for the CU-DIE at unknown location):\n");
    }
}

static void
record_line_error(const char *where, Dwarf_Error err)
{
    char tmp_buff[500];
    if (check_lines && checking_this_compiler()) {
        snprintf(tmp_buff, sizeof(tmp_buff),
            "Error getting line details calling %s dwarf error is %s",
            where,dwarf_errmsg(err));
        DWARF_CHECK_ERROR(lines_result,tmp_buff);
    }
}

extern void
print_line_numbers_this_cu(Dwarf_Debug dbg, Dwarf_Die cu_die)
{
    Dwarf_Signed linecount = 0;
    Dwarf_Line *linebuf = NULL;
    Dwarf_Signed i = 0;
    Dwarf_Addr pc = 0;
    Dwarf_Unsigned lineno = 0;
    Dwarf_Unsigned column = 0;
    Dwarf_Error err = 0;

    Dwarf_Bool newstatement = 0;
    Dwarf_Bool lineendsequence = 0;
    Dwarf_Bool new_basic_block = 0;
    int lres = 0;
    int sres = 0;
    int ares = 0;
    int lires = 0;
    int cores = 0;
    int line_errs = 0;

    Dwarf_Bool SkipRecord = FALSE;

    current_section_id = DEBUG_LINE;

    /* line_flag is TRUE */

    if (do_print_dwarf) {
        printf("\n.debug_line: line number info for a single cu\n");
    } else {
        /* We are checking, not printing. */
        Dwarf_Half tag = 0;
        int tres = dwarf_tag(cu_die, &tag, &err);
        if (tres != DW_DLV_OK) {
            /*  Something broken here. */
            print_error(dbg,"Unable to see CU DIE tag "
                "though we could see it earlier. Something broken.",
                tres,err);
            return;
        } else if (tag == DW_TAG_type_unit) {
            /*  Not checking since type units missing
                address range in CU header. */
            return;
        }
    }
    if (verbose > 1) {
        int errcount = 0;
        print_source_intro(cu_die);
        print_one_die(dbg, cu_die,
            /* print_information= */ 1,
            /* indent level */0,
            /* srcfiles= */ 0, /* cnt= */ 0,
            /* ignore_die_stack= */TRUE);
        DWARF_CHECK_COUNT(lines_result,1);
        lres = dwarf_print_lines(cu_die, &err,&errcount);
        if (errcount > 0) {
            DWARF_ERROR_COUNT(lines_result,errcount);
            DWARF_CHECK_COUNT(lines_result,(errcount-1));
        }
        if (lres == DW_DLV_ERROR) {
            print_error(dbg, "dwarf_srclines details", lres, err);
        }
        return;
    }

    if (check_lines && checking_this_compiler()) {
        DWARF_CHECK_COUNT(lines_result,1);
        dwarf_check_lineheader(cu_die,&line_errs);
        if (line_errs > 0) {
            DWARF_CHECK_ERROR_PRINT_CU();
            DWARF_ERROR_COUNT(lines_result,line_errs);
            DWARF_CHECK_COUNT(lines_result,(line_errs-1));
        }
    }
    lres = dwarf_srclines(cu_die, &linebuf, &linecount, &err);
    if (lres == DW_DLV_ERROR) {
        /* Do not terminate processing */
        if (check_decl_file) {
            DWARF_CHECK_COUNT(decl_file_result,1);
            DWARF_CHECK_ERROR2(decl_file_result,"dwarf_srclines",
                dwarf_errmsg(err));
            record_dwarf_error = FALSE;  /* Clear error condition */
        } else {
            print_error(dbg, "dwarf_srclines", lres, err);
        }
    } else if (lres == DW_DLV_NO_ENTRY) {
        /* no line information is included */
    } else {
        char *padding;
        struct esb_s lastsrc;
        esb_constructor(&lastsrc);
        /* Padding for a nice layout */
        padding = line_print_pc ? "            " : "";
        if (do_print_dwarf) {
            print_source_intro(cu_die);
            if (verbose) {
                print_one_die(dbg, cu_die,
                    /* print_information= */ TRUE,
                    /* indent_level= */ 0,
                    /* srcfiles= */ 0, /* cnt= */ 0,
                    /* ignore_die_stack= */TRUE);
            }
            /* Check if print of <pc> address is needed. */
            printf("\n");
            printf("%sNS new statement, BB new basic block, "
                "ET end of text sequence\n",padding);
            printf("%sPE prologue end, EB epilogue begin\n",padding);
            printf("%sIA=val ISA number, DI=val discriminator value\n",
                padding);
            if (line_print_pc) {
                printf("<pc>        ");
            }
            printf("[row,col] "
                "NS BB ET PE EB IS= DI= uri: \"filepath\"\n");
        }
        for (i = 0; i < linecount; i++) {
            Dwarf_Line line = linebuf[i];
            string filename = 0;
            int nsres = 0;
            Dwarf_Bool found_line_error = FALSE;
            Dwarf_Bool has_is_addr_set = FALSE;
            char *where = NULL;

            if (check_decl_file && checking_this_compiler()) {
                /* A line record with addr=0 was detected */
                if (SkipRecord) {
                    /* Skip records that do not have ís_addr_set' */
                    ares = dwarf_line_is_addr_set(line, &has_is_addr_set, &err);
                    if (ares == DW_DLV_OK && has_is_addr_set) {
                        SkipRecord = FALSE;
                    }
                    else {
                        /*  Keep ignoring records until we have
                            one with 'is_addr_set' */
                        continue;
                    }
                }
            }

            if (check_lines && checking_this_compiler()) {
                DWARF_CHECK_COUNT(lines_result,1);
            }
            filename = "<unknown>";
            sres = dwarf_linesrc(line, &filename, &err);
            if (sres == DW_DLV_ERROR) {
                /* Do not terminate processing */
                where = "dwarf_linesrc()";
                record_line_error(where,err);
                found_line_error = TRUE;
            }

            ares = dwarf_lineaddr(line, &pc, &err);

            if (ares == DW_DLV_ERROR) {
                /* Do not terminate processing */
                where = "dwarf_lineaddr()";
                record_line_error(where,err);
                found_line_error = TRUE;
            }
            if (ares == DW_DLV_NO_ENTRY) {
                pc = 0;
            }
            lires = dwarf_lineno(line, &lineno, &err);
            if (lires == DW_DLV_ERROR) {
                /* Do not terminate processing */
                where = "dwarf_lineno()";
                record_line_error(where,err);
                found_line_error = TRUE;
            }
            if (lires == DW_DLV_NO_ENTRY) {
                lineno = -1LL;
            }
            cores = dwarf_lineoff_b(line, &column, &err);
            if (cores == DW_DLV_ERROR) {
                /* Do not terminate processing */
                where = "dwarf_lineoff()";
                record_line_error(where,err);
                found_line_error = TRUE;
            }
            if (cores == DW_DLV_NO_ENTRY) {
                /*  Zero was always the correct default, meaning
                    the left edge. DWARF2/3/4 spec sec 6.2.2 */
                column = 0;
            }

            /*  Process any possible error condition, though
                we won't be at the first such error. */
            if (check_decl_file && checking_this_compiler()) {
                DWARF_CHECK_COUNT(decl_file_result,1);
                if (found_line_error) {
                    DWARF_CHECK_ERROR2(decl_file_result,where,dwarf_errmsg(err));
                } else if (do_check_dwarf) {
                    /*  Check the address lies with a valid [lowPC:highPC]
                        in the .text section*/
                    if (IsValidInBucketGroup(pRangesInfo,pc)) {
                        /* Valid values; do nothing */
                    } else {
                        /*  At this point may be we are dealing with
                            a linkonce symbol. The problem we have here
                            is we have consumed the deug_info section
                            and we are dealing just with the records
                            from the .debug_line, so no PU_name is
                            available and no high_pc. Traverse the linkonce
                            table if try to match the pc value with
                            one of those ranges.
                        */
                        if (check_lines && checking_this_compiler()) {
                            DWARF_CHECK_COUNT(lines_result,1);
                        }
                        if (FindAddressInBucketGroup(pLinkonceInfo,pc)){
                            /* Valid values; do nothing */
                        } else {
                            /*  The SN Systems Linker generates
                                line records
                                with addr=0, when dealing with linkonce
                                symbols and no stripping */
                            if (pc) {
                                char addr_tmp[100];
                                if (check_lines && checking_this_compiler()) {
                                    snprintf(addr_tmp,sizeof(addr_tmp),
                                        ".debug_line: Address"
                                        " 0x%" DW_PR_XZEROS DW_PR_DUx
                                        " outside a valid .text range",pc);
                                    DWARF_CHECK_ERROR(lines_result,
                                        addr_tmp);
                                }
                            } else {
                                SkipRecord = TRUE;
                            }
                        }
                    }
                    /*  Check the last record for the .debug_line,
                        the one created by DW_LNE_end_sequence,
                        is the same as the high_pc
                        address for the last known user program
                        unit (PU) */
                    if ((i + 1 == linecount) &&
                        seen_PU_high_address) {
                        /*  Ignore those PU that have been stripped
                            by the linker; their low_pc values are
                            set to -1 (snc linker only) */
                        /*  It is perfectly sensible for a compiler
                            to leave a few bytes of NOP or other stuff
                            after the last instruction in a subprogram,
                            for cache-alignment or other purposes, so
                            a mismatch here is not necessarily
                            an error.  */

                        if (check_lines && checking_this_compiler()) {
                            DWARF_CHECK_COUNT(lines_result,1);
                            if ((pc != PU_high_address) &&
                                (PU_base_address != elf_max_address)) {
                                char addr_tmp[100];
                                snprintf(addr_tmp,sizeof(addr_tmp),
                                    ".debug_line: Address"
                                    " 0x%" DW_PR_XZEROS DW_PR_DUx
                                    " may be incorrect"
                                    " as DW_LNE_end_sequence address",pc);
                                DWARF_CHECK_ERROR(lines_result,
                                    addr_tmp);
                            }
                        }
                    }
                }
            }

            /* Display the error information */
            if (found_line_error || record_dwarf_error) {
                if (check_verbose_mode && PRINTING_UNIQUE) {
                    /* Print the record number for better error description */
                    printf("Record = %"  DW_PR_DUu
                        " Addr = 0x%" DW_PR_XZEROS DW_PR_DUx
                        " [%4" DW_PR_DUu ",%2" DW_PR_DUu "] '%s'\n",
                        i, pc,lineno,column,filename);
                    /* The compilation unit was already printed */
                    if (!check_decl_file) {
                        PRINT_CU_INFO();
                    }
                }
                record_dwarf_error = FALSE;
                /* Due to a fatal error, skip current record */
                if (found_line_error) {
                    continue;
                }
            }
            if (do_print_dwarf) {
                /* Check if print of <pc> address is needed. */
                if (line_print_pc) {
                    printf("0x%" DW_PR_XZEROS DW_PR_DUx "  ", pc);
                }
                printf("[%4" DW_PR_DUu ",%2" DW_PR_DUu "]", lineno, column);
            }

            nsres = dwarf_linebeginstatement(line, &newstatement, &err);
            if (nsres == DW_DLV_OK) {
                if (newstatement && do_print_dwarf) {
                    printf(" %s","NS");
                }
            } else if (nsres == DW_DLV_ERROR) {
                print_error(dbg, "linebeginstatment failed", nsres, err);
            }
            nsres = dwarf_lineblock(line, &new_basic_block, &err);
            if (nsres == DW_DLV_OK) {
                if (new_basic_block && do_print_dwarf) {
                    printf(" %s","BB");
                }
            } else if (nsres == DW_DLV_ERROR) {
                print_error(dbg, "lineblock failed", nsres, err);
            }
            nsres = dwarf_lineendsequence(line, &lineendsequence, &err);
            if (nsres == DW_DLV_OK) {
                if (lineendsequence && do_print_dwarf) {
                    printf(" %s", "ET");
                }
            } else if (nsres == DW_DLV_ERROR) {
                print_error(dbg, "lineblock failed", nsres, err);
            }
            if (do_print_dwarf) {
                Dwarf_Bool prologue_end = 0;
                Dwarf_Bool epilogue_begin = 0;
                Dwarf_Unsigned isa = 0;
                Dwarf_Unsigned discriminator = 0;
                int disres = dwarf_prologue_end_etc(line,
                    &prologue_end,&epilogue_begin,
                    &isa,&discriminator,&err);
                if (disres == DW_DLV_ERROR) {
                    print_error(dbg, "dwarf_prologue_end_etc() failed",
                        disres, err);
                }
                if (prologue_end) {
                    printf(" PE");
                }
                if (epilogue_begin) {
                    printf(" EB");
                }
                if (isa) {
                    printf(" IS=0x%" DW_PR_DUx, isa);
                }
                if (discriminator) {
                    printf(" DI=0x%" DW_PR_DUx, discriminator);
                }
            }


            if (i > 0 &&  verbose < 3  &&
                strcmp(filename,esb_get_string(&lastsrc)) == 0) {
                /* Do not print name. */
            } else {
                struct esb_s urs;
                esb_constructor(&urs);
                esb_append(&urs, " uri: \"");
                translate_to_uri(filename,&urs);
                esb_append(&urs,"\"");
                if (do_print_dwarf) {
                    printf("%s",esb_get_string(&urs));
                }
                esb_destructor(&urs);
                esb_empty_string(&lastsrc);
                esb_append(&lastsrc,filename);
            }
            if (sres == DW_DLV_OK) {
                dwarf_dealloc(dbg, filename, DW_DLA_STRING);
            }
            if (do_print_dwarf) {
                printf("\n");
            }
        }
        esb_destructor(&lastsrc);
        dwarf_srclines_dealloc(dbg, linebuf, linecount);
    }
}
