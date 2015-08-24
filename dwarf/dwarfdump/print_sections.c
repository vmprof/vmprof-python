/*
  Copyright (C) 2000-2006 Silicon Graphics, Inc.  All Rights Reserved.
  Portions Copyright 2007-2010 Sun Microsystems, Inc. All rights reserved.
  Portions Copyright 2009-2010 SN Systems Ltd. All rights reserved.
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

#include "print_sections.h"
#include "print_frames.h"

/*
    Print line number information:
        filename
        new basic-block
        [line] [address] <new statement>
*/

int dwarf_names_print_on_error = 1;

/* referred in dwarfdump.c */
Dwarf_Die current_cu_die_for_print_frames;

void
deal_with_name_offset_err(Dwarf_Debug dbg,
    char *err_loc,
    char *name, Dwarf_Unsigned die_off,
    int nres, Dwarf_Error err)
{
    if (nres == DW_DLV_ERROR) {
        Dwarf_Unsigned myerr = dwarf_errno(err);

        if (myerr == DW_DLE_OFFSET_BAD) {
            printf("Error: bad offset %s, %s %" DW_PR_DUu
                " (0x%08" DW_PR_DUx ")\n",
                err_loc,
                name,
                die_off,
                die_off);
        }
        print_error(dbg, err_loc, nres, err);
    }
}



/* The April 2005 dwarf_get_section_max_offsets()
   in libdwarf returns all max-offsets, but we only
   want one of those offsets. This function returns
   the one we want from that set,
   making functions needing this offset as readable as possible.
   (avoiding code duplication).
*/
Dwarf_Unsigned
get_info_max_offset(Dwarf_Debug dbg)
{
    Dwarf_Unsigned debug_info_size = 0;
    Dwarf_Unsigned debug_abbrev_size = 0;
    Dwarf_Unsigned debug_line_size = 0;
    Dwarf_Unsigned debug_loc_size = 0;
    Dwarf_Unsigned debug_aranges_size = 0;
    Dwarf_Unsigned debug_macinfo_size = 0;
    Dwarf_Unsigned debug_pubnames_size = 0;
    Dwarf_Unsigned debug_str_size = 0;
    Dwarf_Unsigned debug_frame_size = 0;
    Dwarf_Unsigned debug_ranges_size = 0;
    Dwarf_Unsigned debug_pubtypes_size = 0;

    dwarf_get_section_max_offsets(dbg,
        &debug_info_size,
        &debug_abbrev_size,
        &debug_line_size,
        &debug_loc_size,
        &debug_aranges_size,
        &debug_macinfo_size,
        &debug_pubnames_size,
        &debug_str_size,
        &debug_frame_size,
        &debug_ranges_size,
        &debug_pubtypes_size);

    return debug_info_size;
}

/*
    decode ULEB
*/
Dwarf_Unsigned
local_dwarf_decode_u_leb128(unsigned char *leb128,
    unsigned int *leb128_length)
{
    unsigned char byte = 0;
    Dwarf_Unsigned number = 0;
    unsigned int shift = 0;
    unsigned int byte_length = 1;

    byte = *leb128;
    for (;;) {
        number |= (byte & 0x7f) << shift;
        shift += 7;

        if ((byte & 0x80) == 0) {
            if (leb128_length != NULL)
                *leb128_length = byte_length;
            return (number);
        }

        byte_length++;
        byte = *(++leb128);
    }
}

#define BITSINBYTE 8
Dwarf_Signed
local_dwarf_decode_s_leb128(unsigned char *leb128,
    unsigned int *leb128_length)
{
    Dwarf_Signed number = 0;
    Dwarf_Bool sign = 0;
    Dwarf_Signed shift = 0;
    unsigned char byte = *leb128;
    Dwarf_Signed byte_length = 1;

    /*  byte_length being the number of bytes of data absorbed so far in
        turning the leb into a Dwarf_Signed. */

    for (;;) {
        sign = byte & 0x40;
        number |= ((Dwarf_Signed) ((byte & 0x7f))) << shift;
        shift += 7;

        if ((byte & 0x80) == 0) {
            break;
        }
        ++leb128;
        byte = *leb128;
        byte_length++;
    }

    if ((shift < sizeof(Dwarf_Signed) * BITSINBYTE) && sign) {
        number |= -((Dwarf_Signed) 1 << shift);
    }

    if (leb128_length != NULL)
        *leb128_length = byte_length;
    return (number);
}


/* Dumping a dwarf-expression as a byte stream. */
void
dump_block(char *prefix, char *data, Dwarf_Signed len)
{
    char *end_data = data + len;
    char *cur = data;
    int i = 0;

    printf("%s", prefix);
    for (; cur < end_data; ++cur, ++i) {
        if (i > 0 && i % 4 == 0)
            printf(" ");
        printf("%02x", 0xff & *cur);

    }
}

