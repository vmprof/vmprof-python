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
#include "naming.h"
#include "dwconf.h"
#include "esb.h"

#include "print_sections.h"
#include "print_frames.h"

/* print data in .debug_loc
   There is no guarantee this will work because we are assuming
   that all bytes are valid loclist data, that there are no
   odd padding or garbage bytes.  In normal use one gets
   into here via an offset from .debug_info, so it could be
   that bytes not referenced from .debug_info are garbage
   or even zero padding.  So this can fail (error off) as such bytes
   can lead dwarf_get_loclist_entry() astray.

   It's also wrong because we don't know what CU or frame each
   loclist is from, so we don't know the address_size for sure.
*/
extern void
print_locs(Dwarf_Debug dbg)
{
    Dwarf_Unsigned offset = 0;
    Dwarf_Addr hipc_offset = 0;
    Dwarf_Addr lopc_offset = 0;
    Dwarf_Ptr data = 0;
    Dwarf_Unsigned entry_len = 0;
    Dwarf_Unsigned next_entry = 0;
    struct esb_s  exprstring;
    int index = 0;
    int lres = 0;
    int fres = 0;

    /* This is sometimes wrong, we need a frame-specific size. */
    Dwarf_Half address_size = 0;

    current_section_id = DEBUG_LOC;

    /* Do nothing if not printing. */
    if (!do_print_dwarf) {
        return;
    }

    fres = dwarf_get_address_size(dbg, &address_size, &err);
    if (fres != DW_DLV_OK) {
        print_error(dbg, "dwarf_get_address_size", fres, err);
    }

    printf("\n.debug_loc");

    printf("\nFormat <i o b e l>: "
        "index section-offset begin-addr end-addr length-of-block-entry\n");
    esb_constructor(&exprstring);
    while ((lres = dwarf_get_loclist_entry(dbg, offset,
        &hipc_offset, &lopc_offset,
        &data, &entry_len,
        &next_entry,
        &err)) == DW_DLV_OK) {
        get_string_from_locs(dbg,data,entry_len,address_size,
            &exprstring);
        /* Display offsets */
        if (display_offsets) {
            ++index;
            printf("  <iobel> [%8d] 0x%" DW_PR_XZEROS DW_PR_DUx,
                index, offset);
            if (verbose) {
                printf(" <expr-off 0x%"  DW_PR_XZEROS  DW_PR_DUx ">",
                    next_entry - entry_len);
            }
        }
        printf(" 0x%"  DW_PR_XZEROS  DW_PR_DUx
            " 0x%" DW_PR_XZEROS DW_PR_DUx
            " %8" DW_PR_DUu " %s\n",
            (Dwarf_Unsigned) lopc_offset,
            (Dwarf_Unsigned) hipc_offset,  entry_len,
            esb_get_string(&exprstring));
        esb_empty_string(&exprstring);
        offset = next_entry;
    }
    esb_destructor(&exprstring);
    if (lres == DW_DLV_ERROR) {
        print_error(dbg, "dwarf_get_loclist_entry", lres, err);
    }
}
