/*
  Copyright (C) 2000-2006 Silicon Graphics, Inc.  All Rights Reserved.
  Portions Copyright 2007-2010 Sun Microsystems, Inc. All rights reserved.
  Portions Copyright 2009-2012 SN Systems Ltd. All rights reserved.
  Portions Copyright 2008-2012 David Anderson. All rights reserved.

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
#include "esb.h"

static struct esb_s esb_string;

/* Because we do not know what DIE is involved, if the
   object being printed has different address sizes
   in different compilation units this will not work
   properly: anything could happen. */
extern void
print_ranges(Dwarf_Debug dbg)
{
    Dwarf_Unsigned off = 0;
    int group_number = 0;
    int wasdense = 0;

    current_section_id = DEBUG_RANGES;
    if (!do_print_dwarf) {
        return;
    }
    printf("\n.debug_ranges\n");

    /*  Turn off dense, we do not want  print_ranges_list_to_extra
        to use dense form here. */
    wasdense = dense;
    dense = 0;
    for (;;) {
        Dwarf_Ranges *rangeset = 0;
        Dwarf_Signed rangecount = 0;
        Dwarf_Unsigned bytecount = 0;

        /*  We do not know what DIE is involved, we use
            the older call here. */
        int rres = dwarf_get_ranges(dbg,off,&rangeset,
            &rangecount,&bytecount,&err);
        if (rres == DW_DLV_OK) {
            char *val = 0;
            printf(" Ranges group %d:\n",group_number);
            esb_empty_string(&esb_string);
            print_ranges_list_to_extra(dbg,off,
                rangeset,rangecount,bytecount,
                &esb_string);
            dwarf_ranges_dealloc(dbg,rangeset,rangecount);
            val = esb_get_string(&esb_string);
            printf("%s",val);
            ++group_number;
        } else if (rres == DW_DLV_NO_ENTRY) {
            printf("End of .debug_ranges.\n");
            break;
        } else {
            /*  ERROR, which does not quite mean a real error,
                as we might just be misaligned reading things without
                a DW_AT_ranges offset.*/
            printf("End of .debug_ranges..\n");
            break;
        }
        off += bytecount;
    }
    dense = wasdense;
}

/*  Extracted this from print_range_attribute() to isolate the check of
    the range list.
*/
static void
check_ranges_list(Dwarf_Debug dbg,
    Dwarf_Off die_off,
    Dwarf_Die cu_die,
    Dwarf_Unsigned original_off,
    Dwarf_Ranges *rangeset,
    Dwarf_Signed rangecount,
    Dwarf_Unsigned bytecount)
{
    Dwarf_Unsigned off = original_off;

    Dwarf_Signed index = 0;
    Dwarf_Addr base_address = CU_base_address;
    Dwarf_Addr lopc = 0;
    Dwarf_Addr hipc = 0;
    Dwarf_Bool bError = FALSE;

    static boolean do_print = TRUE;
#if 0
{
/* START -> Just for debugging */
struct esb_s rangesstr;
esb_constructor(&rangesstr);
printf("\n**** START ****\n");
printf("\tGLB_OFF: (0x%" DW_PR_XZEROS DW_PR_DUx ") ",die_off);
printf("\tRGN_OFF: (0x%" DW_PR_XZEROS DW_PR_DUx ")\n",original_off);
print_ranges_list_to_extra(dbg,original_off,
    rangeset,rangecount,bytecount,
    &rangesstr);
printf("%s\n", esb_get_string(&rangesstr));
printf("**** END ****\n");
/* END <- Just for debugging */
}
#endif /* 0 */

    /* Ignore last entry, is the end-of-list */
    for (index = 0; index < rangecount - 1; index++) {
        Dwarf_Ranges *r = rangeset + index;

        if (r->dwr_addr1 == elf_max_address) {
            /* (0xffffffff,addr), use specific address (current PU address) */
            base_address = r->dwr_addr2;
        } else {
            /* (offset,offset), update using CU address */
            lopc = r->dwr_addr1 + base_address;
            hipc = r->dwr_addr2 + base_address;
            DWARF_CHECK_COUNT(ranges_result,1);

            /*  Check the low_pc and high_pc
                are within a valid range in
                the .text section */
            if (IsValidInBucketGroup(pRangesInfo,lopc) &&
                IsValidInBucketGroup(pRangesInfo,hipc)) {
                /* Valid values; do nothing */
            } else {
                /*  At this point may be we
                    are dealing with a
                    linkonce symbol */
                if (IsValidInLinkonce(pLinkonceInfo,
                    PU_name,lopc,hipc)) {
                    /* Valid values; do nothing */
                } else {
                    bError = TRUE;
                    DWARF_CHECK_ERROR(ranges_result,
                        ".debug_ranges: Address outside a "
                        "valid .text range");
                    if (check_verbose_mode && do_print) {
                        /*  Update DIEs offset just for printing */
                        int res = dwarf_die_offsets(cu_die,
                            &DIE_overall_offset,&DIE_offset,&err);
                        if (res != DW_DLV_OK) {
                            print_error(dbg, "dwarf_die_offsets",res,err);
                        }
                        printf(
                            "Offset = 0x%" DW_PR_XZEROS DW_PR_DUx
                            ", Base = 0x%" DW_PR_XZEROS DW_PR_DUx
                            ", "
                            "Low = 0x%" DW_PR_XZEROS DW_PR_DUx
                            " (0x%" DW_PR_XZEROS  DW_PR_DUx
                            "), High = 0x%"
                            DW_PR_XZEROS  DW_PR_DUx
                            " (0x%" DW_PR_XZEROS DW_PR_DUx
                            ")\n",
                            off,base_address,lopc,
                            r->dwr_addr1,hipc,
                            r->dwr_addr2);
                    }
                }
            }
        }
        /*  Each entry holds 2 addresses (offsets) */
        off += elf_address_size * 2;
    }

    /*  In the case of errors, we have to print the range records that
        caused the error. */
    if (bError && check_verbose_mode && do_print) {
        struct esb_s rangesstr;
        esb_constructor(&rangesstr);

        printf("\n");
        print_ranges_list_to_extra(dbg,original_off,
            rangeset,rangecount,bytecount,
            &rangesstr);
        printf("%s\n", esb_get_string(&rangesstr));
    }

    /*  In the case of printing unique errors, stop the printing of any
        subsequent errors, which have the same text. */
    if (bError && check_verbose_mode && print_unique_errors) {
        do_print = FALSE;
    }
}

/*  Records information about compilers (producers) found in the
    debug information, including the check results for several
    categories (see -k option). */
typedef struct {
    Dwarf_Off die_off;
    Dwarf_Off range_off;
} Range_Array_Entry;

/*  Array to record the DW_AT_range attribute DIE, to be used at the end
    of the CU, to check the range values; DWARF4 allows an offset relative
    to the low_pc as the high_pc value. Also, LLVM generates for the CU the
    pair (low_pc, at_ranges) instead of the traditional (low_pc, high_pc).
*/
static Range_Array_Entry *range_array = NULL;
static Dwarf_Unsigned range_array_size = 0;
static Dwarf_Unsigned range_array_count = 0;
#define RANGE_ARRAY_INITIAL_SIZE 64

/*  Allocate space to store information about the ranges; the values are
    extracted from the DW_AT_ranges attribute. The space is reused by all CUs.
*/
void
allocate_range_array_info()
{
    if (range_array == NULL) {
        /* Allocate initial range array info */
        range_array = (Range_Array_Entry *)
            calloc(RANGE_ARRAY_INITIAL_SIZE,sizeof(Range_Array_Entry));
        range_array_size = RANGE_ARRAY_INITIAL_SIZE;
    }
}

void
release_range_array_info()
{
    if (range_array) {
        free(range_array);
    }
}

/*  Clear out values from previous CU */
static void
reset_range_array_info()
{
    if (range_array) {
        memset((void *)range_array,0,
            (range_array_count) * sizeof(Range_Array_Entry));
        range_array_count = 0;
    }
}

void
record_range_array_info_entry(Dwarf_Off die_off,Dwarf_Off range_off)
{
    /* Record a new detected range info. */
    if (range_array_count == range_array_size) {
        /* Resize range array */
        range_array_size *= 2;
        range_array = (Range_Array_Entry *)
            realloc(range_array,
            (range_array_size) * sizeof(Range_Array_Entry));
    }
    /* The 'die_off' is the Global Die Offset */
    range_array[range_array_count].die_off = die_off;
    range_array[range_array_count].range_off = range_off;
    ++range_array_count;
}

/*  Now that we are at the end of the CU, check the range lists */
void
check_range_array_info(Dwarf_Debug dbg)
{
    if (range_array && range_array_count) {
        /*  Traverse the range array and for each entry:
            Load the ranges
            Check for any outside conditions */
        Dwarf_Off original_off = 0;
        Dwarf_Off die_off = 0;
        Dwarf_Unsigned index = 0;
        Dwarf_Die cu_die;
        int res;

        /*  In case of errors, the correct DIE offset should be
            displayed. At this point we are at the end of the PU */
        Dwarf_Off DIE_overall_offset_bak = DIE_overall_offset;

        for (index = 0; index < range_array_count; ++index) {
            Dwarf_Ranges *rangeset = 0;
            Dwarf_Signed rangecount = 0;
            Dwarf_Unsigned bytecount = 0;

            /* Get a range info record */
            die_off = range_array[index].die_off;
            original_off = range_array[index].range_off;

            res = dwarf_offdie(dbg,die_off,&cu_die,&err);
            if (res != DW_DLV_OK) {
                print_error(dbg,"dwarf_offdie",res,err);
            }
            res = dwarf_get_ranges_a(dbg,original_off,cu_die,
                &rangeset,&rangecount,&bytecount,&err);
            if (res == DW_DLV_OK) {
                check_ranges_list(dbg,die_off,cu_die,original_off,
                    rangeset,rangecount,bytecount);
            }
            dwarf_dealloc(dbg,cu_die,DW_DLA_DIE);
        };

        reset_range_array_info();

        /*  Point back to the end of the PU */
        DIE_overall_offset = DIE_overall_offset_bak;
    }
}
