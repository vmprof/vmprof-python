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

#include "print_sections.h"
#include "print_frames.h"


struct macro_counts_s {
    long mc_start_file;
    long mc_end_file;
    long mc_define;
    long mc_undef;
    long mc_extension;
    long mc_code_zero;
    long mc_unknown;
};

static void
print_one_macro_entry_detail(long i,
    char *type,
    struct Dwarf_Macro_Details_s *mdp)
{
    /* "DW_MACINFO_*: section-offset file-index [line] string\n" */
    if (mdp->dmd_macro) {
        printf("%3ld %s: %6" DW_PR_DUu " %2" DW_PR_DSd " [%4"
            DW_PR_DSd "] \"%s\" \n",
            i,
            type,
            (Dwarf_Unsigned)mdp->dmd_offset,
            mdp->dmd_fileindex, mdp->dmd_lineno, mdp->dmd_macro);
    } else {
        printf("%3ld %s: %6" DW_PR_DUu " %2" DW_PR_DSd " [%4"
            DW_PR_DSd "] 0\n",
            i,
            type,
            (Dwarf_Unsigned)mdp->dmd_offset,
            mdp->dmd_fileindex, mdp->dmd_lineno);
    }

}

static void
print_one_macro_entry(long i,
    struct Dwarf_Macro_Details_s *mdp,
    struct macro_counts_s *counts)
{

    switch (mdp->dmd_type) {
    case 0:
        counts->mc_code_zero++;
        print_one_macro_entry_detail(i, "DW_MACINFO_type-code-0", mdp);
        break;

    case DW_MACINFO_start_file:
        counts->mc_start_file++;
        print_one_macro_entry_detail(i, "DW_MACINFO_start_file", mdp);
        break;

    case DW_MACINFO_end_file:
        counts->mc_end_file++;
        print_one_macro_entry_detail(i, "DW_MACINFO_end_file  ", mdp);
        break;

    case DW_MACINFO_vendor_ext:
        counts->mc_extension++;
        print_one_macro_entry_detail(i, "DW_MACINFO_vendor_ext", mdp);
        break;

    case DW_MACINFO_define:
        counts->mc_define++;
        print_one_macro_entry_detail(i, "DW_MACINFO_define    ", mdp);
        break;

    case DW_MACINFO_undef:
        counts->mc_undef++;
        print_one_macro_entry_detail(i, "DW_MACINFO_undef     ", mdp);
        break;

    default:
        {
            char create_type[50];       /* More than large enough. */

            counts->mc_unknown++;
            snprintf(create_type, sizeof(create_type),
                "DW_MACINFO_0x%x", mdp->dmd_type);
            print_one_macro_entry_detail(i, create_type, mdp);
        }
        break;
    }
}

/*  print data in .debug_macinfo */
/*  FIXME: should print name of file whose index is in macro data
    here  --  somewhere.  */
/*ARGSUSED*/ extern void
print_macinfo(Dwarf_Debug dbg)
{
    Dwarf_Off offset = 0;
    Dwarf_Unsigned max = 0;
    Dwarf_Signed count = 0;
    long group = 0;
    Dwarf_Macro_Details *maclist = NULL;
    int lres = 0;

    current_section_id = DEBUG_MACINFO;
    if (!do_print_dwarf) {
        return;
    }

    printf("\n.debug_macinfo\n");

    while ((lres = dwarf_get_macro_details(dbg, offset,
        max, &count, &maclist,
        &err)) == DW_DLV_OK) {
        long i = 0;
        struct macro_counts_s counts;


        memset(&counts, 0, sizeof(counts));

        printf("\n");
        printf("compilation-unit .debug_macinfo # %ld\n", group);
        printf
            ("num name section-offset file-index [line] \"string\"\n");
        for (i = 0; i < count; i++) {
            struct Dwarf_Macro_Details_s *mdp = &maclist[i];

            print_one_macro_entry(i, mdp, &counts);
        }

        if (counts.mc_start_file == 0) {
            printf
                ("DW_MACINFO file count of zero is invalid DWARF2/3\n");
        }
        if (counts.mc_start_file != counts.mc_end_file) {
            printf("Counts of DW_MACINFO file (%ld) end_file (%ld) "
                "do not match!.\n",
                counts.mc_start_file, counts.mc_end_file);
        }
        if (counts.mc_code_zero < 1) {
            printf("Count of zeros in macro group should be non-zero "
                "(1 preferred), count is %ld\n",
                counts.mc_code_zero);
        }
        printf("Macro counts: start file %ld, "
            "end file %ld, "
            "define %ld, "
            "undef %ld, "
            "ext %ld, "
            "code-zero %ld, "
            "unknown %ld\n",
            counts.mc_start_file,
            counts.mc_end_file,
            counts.mc_define,
            counts.mc_undef,
            counts.mc_extension,
            counts.mc_code_zero, counts.mc_unknown);


        /* int type= maclist[count - 1].dmd_type; */
        /* ASSERT: type is zero */

        offset = maclist[count - 1].dmd_offset + 1;
        dwarf_dealloc(dbg, maclist, DW_DLA_STRING);
        ++group;
    }
    if (lres == DW_DLV_ERROR) {
        print_error(dbg, "dwarf_get_macro_details", lres, err);
    }
}

