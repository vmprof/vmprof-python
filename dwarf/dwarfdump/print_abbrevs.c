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

/*  The address of the Free Software Foundation is
    Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
    Boston, MA 02110-1301, USA.
    SGI has moved from the Crittenden Lane address.
*/

#include "globals.h"
#include "naming.h"
#include "dwconf.h"
#include "esb.h"

#include "print_sections.h"


/* The following relevent for one specific Linker. */
#define SNLINKER_MAX_ATTRIB_COUNT  16

/* Print data in .debug_abbrev
   This is inherently unsafe as it assumes there
   are no byte sequences in .debug_abbrev other than
   legal abbrev sequences.  But the Dwarf spec
   does not promise that. The spec only promises
   that any bytes at an offset referred to from
   .debug_info are legal sequences.
*/
extern void
print_abbrevs(Dwarf_Debug dbg)
{
    Dwarf_Abbrev ab;
    Dwarf_Unsigned offset = 0;
    Dwarf_Unsigned length = 0;
    Dwarf_Unsigned abbrev_entry_count = 0;
    /* Maximum defined tag is 0xffff, DW_TAG_hi_user. */
    Dwarf_Half tag = 0;
    Dwarf_Half attr = 0;
    Dwarf_Signed form = 0;
    Dwarf_Off off = 0;
    Dwarf_Unsigned i = 0;
    const char * child_name = 0;
    Dwarf_Unsigned abbrev_num = 1;
    Dwarf_Signed child_flag = 0;
    int abres = 0;
    int tres = 0;
    int acres = 0;
    Dwarf_Unsigned abbrev_code = 0;

    current_section_id = DEBUG_ABBREV;

    if (do_print_dwarf) {
        printf("\n.debug_abbrev\n");
    }
    while ((abres = dwarf_get_abbrev(dbg, offset, &ab,
        &length, &abbrev_entry_count,
        &err)) == DW_DLV_OK) {

        if (abbrev_entry_count == 0) {
            /* Simple innocuous zero : null abbrev entry */
            if (dense) {
                printf("<%" DW_PR_DUu "><0x%" DW_PR_XZEROS DW_PR_DUx "><%"
                    DW_PR_DSd "><%s>\n",
                    abbrev_num,
                    offset,
                    (Dwarf_Signed) /* abbrev_code */ 0,
                    "null .debug_abbrev entry");
            } else {
                printf("<%5" DW_PR_DUu "><0x%" DW_PR_XZEROS DW_PR_DUx
                    "><code: %3" DW_PR_DSd "> %-20s\n",
                    abbrev_num,
                    offset,
                    (Dwarf_Signed) /* abbrev_code */ 0,
                    "null .debug_abbrev entry");
            }

            offset += length;
            ++abbrev_num;
            dwarf_dealloc(dbg, ab, DW_DLA_ABBREV);
            continue;
        }
        tres = dwarf_get_abbrev_tag(ab, &tag, &err);
        if (tres != DW_DLV_OK) {
            dwarf_dealloc(dbg, ab, DW_DLA_ABBREV);
            print_error(dbg, "dwarf_get_abbrev_tag", tres, err);
        }
        tres = dwarf_get_abbrev_code(ab, &abbrev_code, &err);
        if (tres != DW_DLV_OK) {
            dwarf_dealloc(dbg, ab, DW_DLA_ABBREV);
            print_error(dbg, "dwarf_get_abbrev_code", tres, err);
        }
        if (dense) {
            printf("<%" DW_PR_DUu "><0x%" DW_PR_XZEROS  DW_PR_DUx
                "><%" DW_PR_DSd "><%s>",
                abbrev_num,
                offset, abbrev_code,
                get_TAG_name(tag,dwarf_names_print_on_error));
        }
        else {
            printf("<%5" DW_PR_DUu "><0x%" DW_PR_XZEROS DW_PR_DUx "><code: %3"
                DW_PR_DSd "> %-20s",
                abbrev_num,
                offset, abbrev_code,
                get_TAG_name(tag,dwarf_names_print_on_error));
        }
        /* Process specific TAGs specially. */
        tag_specific_checks_setup(tag,0);
        ++abbrev_num;
        acres = dwarf_get_abbrev_children_flag(ab, &child_flag, &err);
        if (acres == DW_DLV_ERROR) {
            dwarf_dealloc(dbg, ab, DW_DLA_ABBREV);
            print_error(dbg, "dwarf_get_abbrev_children_flag", acres,
                err);
        }
        if (acres == DW_DLV_NO_ENTRY) {
            child_flag = 0;
        }
        child_name = get_children_name(child_flag,
            dwarf_names_print_on_error);
        if (dense)
            printf(" %s", child_name);
        else
            printf("        %s\n", child_name);
        /*  Abbrev just contains the format of a die, which debug_info
            then points to with the real data. So here we just print the
            given format. */
        for (i = 0; i < abbrev_entry_count; i++) {
            int aeres = 0;

            aeres =
                dwarf_get_abbrev_entry(ab, i, &attr, &form, &off, &err);
            if (aeres == DW_DLV_ERROR) {
                dwarf_dealloc(dbg, ab, DW_DLA_ABBREV);
                print_error(dbg, "dwarf_get_abbrev_entry", aeres, err);
            }
            if (aeres == DW_DLV_NO_ENTRY) {
                attr = -1LL;
                form = -1LL;
            }
            if (dense) {
                printf(" <%ld>%s<%s>", (unsigned long) off,
                    get_AT_name(attr,dwarf_names_print_on_error),
                    get_FORM_name((Dwarf_Half) form,
                        dwarf_names_print_on_error));
            } else {
                printf("       <0x%08lx>              %-28s%s\n",
                    (unsigned long) off,
                    get_AT_name(attr,
                        dwarf_names_print_on_error),
                    get_FORM_name((Dwarf_Half) form,
                        dwarf_names_print_on_error));
            }
        }
        dwarf_dealloc(dbg, ab, DW_DLA_ABBREV);
        offset += length;
        if (dense) {
            printf("\n");
        }
    }
    if (abres == DW_DLV_ERROR) {
        print_error(dbg, "dwarf_get_abbrev", abres, err);
    }
}

/* Number of abbreviations for current CU */
static Dwarf_Unsigned CU_abbrev_count = 0;

/* Abbreviations array info for checking  abbrev tags.
   The [zero] entry is not used.
   We never shrink the array, but it never grows beyond
   the largest abbreviation count of all the CUs.
*/

static Dwarf_Signed *abbrev_array = NULL;
/* Size of the array, the same as the abbrev tag
   count of the CU with the most of them.  */
static Dwarf_Unsigned abbrev_array_size = 0;
#define ABBREV_ARRAY_INITIAL_SIZE 64

/* Calculate the number of abbreviations for the
   current CU and set up a basic abbreviations array info,
   storing the number of attributes per abbreviation.
*/
void
get_abbrev_array_info(Dwarf_Debug dbg, Dwarf_Unsigned offset_in)
{
    Dwarf_Unsigned offset = offset_in;
    if (check_abbreviations) {
        Dwarf_Abbrev ab;
        Dwarf_Unsigned length = 0;
        Dwarf_Unsigned abbrev_entry_count = 0;
        Dwarf_Unsigned abbrev_code;
        int abres = DW_DLV_OK;
        Dwarf_Error err;

        Dwarf_Bool bMore = TRUE;
        CU_abbrev_count = 0;

        if (abbrev_array == NULL) {
            /* Allocate initial abbreviation array info */
            abbrev_array = (Dwarf_Signed *)
                calloc(ABBREV_ARRAY_INITIAL_SIZE+1,sizeof(Dwarf_Signed));
            abbrev_array_size = ABBREV_ARRAY_INITIAL_SIZE;
        } else {
            /* Clear out values from previous CU */
            memset((void *)abbrev_array,0,
                (abbrev_array_size+1) * sizeof(Dwarf_Signed));
        }

        while (bMore && (abres = dwarf_get_abbrev(dbg, offset, &ab,
            &length, &abbrev_entry_count,
            &err)) == DW_DLV_OK) {
            dwarf_get_abbrev_code(ab,&abbrev_code,&err);
            if (abbrev_code == 0) {
                /* End of abbreviation table for this CU */
                ++offset; /* Skip abbreviation code */
                bMore = FALSE;
            } else {
                /* Valid abbreviation code */
                if (abbrev_code > 0) {
                    if (abbrev_code > abbrev_array_size) {
                        /* Resize abbreviation array */
                        abbrev_array_size *= 2;
                        abbrev_array = (Dwarf_Signed *)
                            realloc(abbrev_array,
                            (abbrev_array_size+1) * sizeof(Dwarf_Signed));
                    }
                    abbrev_array[abbrev_code] = abbrev_entry_count;
                    ++CU_abbrev_count;
                    offset += length;
                } else {
                    /* Invalid abbreviation code */
                    print_error(dbg, "get_abbrev_array_info", abres, err);
                }
            }
            dwarf_dealloc(dbg, ab, DW_DLA_ABBREV);
        }
    }
}

/* Validate an abbreviation for the current CU. */
void
validate_abbrev_code(Dwarf_Debug dbg,Dwarf_Unsigned abbrev_code)
{
    char buf[128];

    DWARF_CHECK_COUNT(abbreviations_result,1);
    if (abbrev_code < 0 || (abbrev_code && abbrev_code > CU_abbrev_count)) {
        snprintf(buf, sizeof(buf),
            "Abbrev code %" DW_PR_DUu
            " outside valid range of [0-%" DW_PR_DUu "]",
            abbrev_code,CU_abbrev_count);
        DWARF_CHECK_ERROR2(abbreviations_result,buf,
            "Invalid abbreviation code.");
    } else {
        Dwarf_Signed abbrev_entry_count =
            abbrev_array[abbrev_code];
        if (abbrev_entry_count < 0 ||
            abbrev_entry_count > SNLINKER_MAX_ATTRIB_COUNT) {
            snprintf(buf, sizeof(buf),
                "Abbrev code %" DW_PR_DUu
                ", with %" DW_PR_DUu " attributes: "
                "outside valid range.",
                abbrev_code,
                abbrev_entry_count);
            DWARF_CHECK_ERROR2(abbreviations_result,buf,
                "Invalid number of attributes.");
        }
    }
}
