/*
  Copyright (C) 2006 Silicon Graphics, Inc.  All Rights Reserved.
  Portions Copyright (C) 2011-2012 SN Systems Ltd. All Rights Reserved.
  Portions Copyright (C) 2007-2012 David Anderson. All Rights Reserved.

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

/*  From 199x through 2010 print_frames relied on
    the order of the fdes matching the order of the functions
    in the CUs when it came to printing a function name with
    an FDE.   This sometimes worked for SGI/IRIX because of
    the way the compiler often emitted things.  It always worked poorly
    for gcc and other compilers.

    As of 2010 the addrmap.h addrmap.h code provides help
    in doing a better job when the tsearch functions (part of
    POSIX) are available.  */

#include "globals.h"
#include "print_frames.h"
#include "dwconf.h"
#include "esb.h"
#include "addrmap.h"

static void
print_one_frame_reg_col(Dwarf_Debug dbg,
    Dwarf_Unsigned rule_id,
    Dwarf_Small value_type,
    Dwarf_Unsigned reg_used,
    Dwarf_Half addr_size,
    struct dwconf_s *config_data,
    Dwarf_Signed offset_relevant,
    Dwarf_Signed offset, Dwarf_Ptr block_ptr);

/*  A strcpy which ensures NUL terminated string
    and never overruns the output.
*/
void
safe_strcpy(char *out, long outlen, const char *in, long inlen)
{
    if (inlen >= (outlen - 1)) {
        strncpy(out, in, outlen - 1);
        out[outlen - 1] = 0;
    } else {
        strcpy(out, in);
    }
}

/* For inlined functions, try to find name */
static int
get_abstract_origin_funcname(Dwarf_Debug dbg,Dwarf_Attribute attr,
    char *name_out, unsigned maxlen)
{
    Dwarf_Off off = 0;
    Dwarf_Die origin_die = 0;
    Dwarf_Attribute *atlist = NULL;
    Dwarf_Signed atcnt = 0;
    Dwarf_Signed i = 0;
    int dres = 0;
    int atres;
    int name_found = 0;
    int res = dwarf_global_formref(attr,&off,&err);
    if (res != DW_DLV_OK) {
        return DW_DLV_NO_ENTRY;
    }
    dres = dwarf_offdie(dbg,off,&origin_die,&err);
    if (dres != DW_DLV_OK) {
        return DW_DLV_NO_ENTRY;
    }
    atres = dwarf_attrlist(origin_die, &atlist, &atcnt, &err);
    if (atres != DW_DLV_OK) {
        dwarf_dealloc(dbg,origin_die,DW_DLA_DIE);
        return DW_DLV_NO_ENTRY;
    }
    for (i = 0; i < atcnt; i++) {
        Dwarf_Half lattr;
        int ares;
        ares = dwarf_whatattr(atlist[i], &lattr, &err);
        if (ares == DW_DLV_ERROR) {
            break;
        } else if (ares == DW_DLV_OK) {
            if (lattr == DW_AT_name) {
                int sres = 0;
                char* temps = 0;
                sres = dwarf_formstring(atlist[i], &temps, &err);
                if (sres == DW_DLV_OK) {
                    long len = (long) strlen(temps);
                    safe_strcpy(name_out, maxlen, temps, len);
                    name_found = 1;
                    break;
                }
            }
        }
    }
    for (i = 0; i < atcnt; i++) {
        dwarf_dealloc(dbg, atlist[i], DW_DLA_ATTR);
    }
    dwarf_dealloc(dbg, atlist, DW_DLA_LIST);
    dwarf_dealloc(dbg,origin_die,DW_DLA_DIE);
    if (!name_found) {
        return DW_DLV_NO_ENTRY;
    }
    return DW_DLV_OK;
}
/*
    Returns 1 if a proc with this low_pc found.
    Else returns 0.

    From print_die.c this has no pcMap passed in,
    we do not really have a sensible context, so this
    really just looks at the current attributes for a name.
*/
int
get_proc_name(Dwarf_Debug dbg, Dwarf_Die die, Dwarf_Addr low_pc,
    char *proc_name_buf, int proc_name_buf_len, void **pcMap)
{
    Dwarf_Signed atcnt = 0;
    Dwarf_Signed i = 0;
    Dwarf_Attribute *atlist = NULL;
    Dwarf_Addr low_pc_die = 0;
    int atres = 0;
    int funcres = 1;
    int funcpcfound = 0;
    int funcnamefound = 0;

    proc_name_buf[0] = 0;       /* always set to something */
    if (pcMap) {
        struct Addr_Map_Entry *ame = 0;
        ame = addr_map_find(low_pc,pcMap);
        if (ame && ame->mp_name) {
            /* mp_name is NULL only if we ran out of heap space. */
            safe_strcpy(proc_name_buf, proc_name_buf_len,
                ame->mp_name,(long) strlen(ame->mp_name));
            return 1;
        }
    }

    atres = dwarf_attrlist(die, &atlist, &atcnt, &err);
    if (atres == DW_DLV_ERROR) {
        print_error(dbg, "dwarf_attrlist", atres, err);
        return 0;
    }
    if (atres == DW_DLV_NO_ENTRY) {
        return 0;
    }
    for (i = 0; i < atcnt; i++) {
        Dwarf_Half attr = 0;
        int ares = 0;
        string temps = 0;
        int sres = 0;
        int dres = 0;

        if (funcnamefound == 1 && funcpcfound == 1) {
            /* stop as soon as both found */
            break;
        }
        ares = dwarf_whatattr(atlist[i], &attr, &err);
        if (ares == DW_DLV_ERROR) {
            print_error(dbg, "get_proc_name whatattr error", ares, err);
        } else if (ares == DW_DLV_OK) {
            switch (attr) {
            case DW_AT_specification:
            case DW_AT_abstract_origin:
                {
                    if (!funcnamefound) {
                        /*  Only use this if we have not seen DW_AT_name
                            yet .*/
                        int aores = get_abstract_origin_funcname(dbg,
                            atlist[i], proc_name_buf,proc_name_buf_len);
                        if (aores == DW_DLV_OK) {
                            /* FOUND THE NAME */
                            funcnamefound = 1;
                        }
                    }
                }
                break;
            case DW_AT_name:
                /*  Even if we saw DW_AT_abstract_origin, go ahead
                    and take DW_AT_name. */
                sres = dwarf_formstring(atlist[i], &temps, &err);
                if (sres == DW_DLV_ERROR) {
                    print_error(dbg,
                        "formstring in get_proc_name failed",
                        sres, err);
                    /*  50 is safe wrong length since is bigger than the
                        actual string */
                    safe_strcpy(proc_name_buf, proc_name_buf_len,
                        "ERROR in dwarf_formstring!", 50);
                } else if (sres == DW_DLV_NO_ENTRY) {
                    /*  50 is safe wrong length since is bigger than the
                        actual string */
                    safe_strcpy(proc_name_buf, proc_name_buf_len,
                        "NO ENTRY on dwarf_formstring?!", 50);
                } else {
                    long len = (long) strlen(temps);

                    safe_strcpy(proc_name_buf, proc_name_buf_len, temps,
                        len);
                }
                funcnamefound = 1;      /* FOUND THE NAME */
                break;
            case DW_AT_low_pc:
                dres = dwarf_formaddr(atlist[i], &low_pc_die, &err);
                if (dres == DW_DLV_ERROR) {
                    if (DW_DLE_MISSING_NEEDED_DEBUG_ADDR_SECTION ==
                        dwarf_errno(err)) {
                        print_error_and_continue(dbg,
                            "The .debug_addr section is missing, "
                            "low_pc unavailable",
                            dres,err);
                    } else {
                        print_error(dbg, "formaddr in get_proc_name failed",
                            dres, err);
                    }
                    low_pc_die = ~low_pc;
                    /* ensure no match */
                }
                funcpcfound = 1;

                break;
            default:
                break;
            }
        }
    }
    for (i = 0; i < atcnt; i++) {
        dwarf_dealloc(dbg, atlist[i], DW_DLA_ATTR);
    }
    dwarf_dealloc(dbg, atlist, DW_DLA_LIST);
    if (funcnamefound && funcpcfound && pcMap ) {
        /*  Insert every name to map, not just the one
            we are looking for.
            This version does extra work in that
            early symbols in a CU will be inserted
            multiple times (the extra times have no
            effect), the dwarfdump2
            version of this does less work.  */
        addr_map_insert(low_pc_die,proc_name_buf,pcMap);
    }
    if (funcnamefound == 0 || funcpcfound == 0 || low_pc != low_pc_die) {
        funcres = 0;
    }
    return (funcres);
}

/*  Modified Depth First Search looking for the procedure:
    a)  only looks for children of subprogram.
    b)  With subprogram looks at current die *before* looking
        for a child.

    Needed since some languages, including SGI MP Fortran,
    have nested functions.
    Return 0 on failure, 1 on success.
*/
static int
load_nested_proc_name(Dwarf_Debug dbg, Dwarf_Die die, Dwarf_Addr low_pc,
    char *ret_name_buf, int ret_name_buf_len,
    void **pcMap)
{
    char name_buf[BUFSIZ];
    Dwarf_Die curdie = die;
    int die_locally_gotten = 0;
    Dwarf_Die prev_child = 0;
    Dwarf_Die newchild = 0;
    Dwarf_Die newsibling = 0;
    Dwarf_Half tag;
    Dwarf_Error err = 0;
    int chres = DW_DLV_OK;

    ret_name_buf[0] = 0;
    name_buf[0] = 0;
    while (chres == DW_DLV_OK) {
        int tres;

        tres = dwarf_tag(curdie, &tag, &err);
        newchild = 0;
        err = 0;
        if (tres == DW_DLV_OK) {
            int lchres;

            if (tag == DW_TAG_subprogram) {
                int proc_name_v = get_proc_name(dbg, curdie, low_pc,
                    name_buf, BUFSIZ,pcMap);
                if (proc_name_v) {
                    safe_strcpy(ret_name_buf, ret_name_buf_len,
                        name_buf, (long) strlen(name_buf));
                    if (die_locally_gotten) {
                        /*  If we got this die from the parent, we do
                            not want to dealloc here! */
                        dwarf_dealloc(dbg, curdie, DW_DLA_DIE);
                    }
                    return 1;
                }
                /* Check children of subprograms recursively should
                    this really be check children of anything,
                    or just children of subprograms? */

                lchres = dwarf_child(curdie, &newchild, &err);
                if (lchres == DW_DLV_OK) {
                    /* look for inner subprogram */
                    int newprog =
                        load_nested_proc_name(dbg, newchild, low_pc,
                            name_buf, BUFSIZ,
                            pcMap);

                    dwarf_dealloc(dbg, newchild, DW_DLA_DIE);
                    if (newprog) {
                        /*  Found it.  We could just take this name or
                            we could concatenate names together For now,
                            just take name */
                        if (die_locally_gotten) {
                            /*  If we got this die from the parent, we
                                do not want to dealloc here! */
                            dwarf_dealloc(dbg, curdie, DW_DLA_DIE);
                        }
                        safe_strcpy(ret_name_buf, ret_name_buf_len,
                            name_buf, (long) strlen(name_buf));
                        return 1;
                    }
                } else if (lchres == DW_DLV_NO_ENTRY) {
                    /* nothing to do */
                } else {
                    print_error(dbg,
                        "load_nested_proc_name dwarf_child() failed ",
                        chres, err);
                    if (die_locally_gotten) {
                        /*  If we got this die from the parent, we do
                            not want to dealloc here! */
                        dwarf_dealloc(dbg, curdie, DW_DLA_DIE);
                    }
                    return 0;
                }
            }                   /* end if TAG_subprogram */
        } else {
            print_error(dbg, "no tag on child read ", tres, err);
            if (die_locally_gotten) {
                /*  If we got this die from the parent, we do not want
                    to dealloc here! */
                dwarf_dealloc(dbg, curdie, DW_DLA_DIE);
            }
            return 0;
        }
        /* try next sibling */
        prev_child = curdie;
        chres = dwarf_siblingof(dbg, curdie, &newsibling, &err);
        if (chres == DW_DLV_ERROR) {
            print_error(dbg, "dwarf_cu_header On Child read ", chres,
                err);
            if (die_locally_gotten) {
                /*  If we got this die from the parent, we do not want
                    to dealloc here! */
                dwarf_dealloc(dbg, curdie, DW_DLA_DIE);
            }
            return 0;
        } else if (chres == DW_DLV_NO_ENTRY) {
            if (die_locally_gotten) {
                /*  If we got this die from the parent, we do not want
                    to dealloc here! */
                dwarf_dealloc(dbg, prev_child, DW_DLA_DIE);
            }
            return 0;/* proc name not at this level */
        } else {
            /* DW_DLV_OK */
            curdie = newsibling;
            if (die_locally_gotten) {
                /*  If we got this die from the parent, we do not want
                    to dealloc here! */
                dwarf_dealloc(dbg, prev_child, DW_DLA_DIE);
            }
            prev_child = 0;
            die_locally_gotten = 1;
        }

    }
    if (die_locally_gotten) {
        /*  If we got this die from the parent, we do not want to
            dealloc here! */
        dwarf_dealloc(dbg, curdie, DW_DLA_DIE);
    }
    return 0;
}

/*  For SGI MP Fortran and other languages, functions
    nest!  As a result, we must dig thru all functions,
    not just the top level.
    This remembers the CU die and restarts each search at the start
    of  the current cu.
*/
static string
get_fde_proc_name(Dwarf_Debug dbg, Dwarf_Addr low_pc,
    void **pcMap,
    int *all_cus_seen)
{
    static char proc_name[BUFSIZ];
    Dwarf_Unsigned cu_header_length = 0;
    Dwarf_Unsigned abbrev_offset = 0;
    Dwarf_Half version_stamp = 0;
    Dwarf_Half address_size = 0;
    Dwarf_Unsigned next_cu_offset = 0;
    int cures = DW_DLV_OK;
    int dres = DW_DLV_OK;
    int chres = DW_DLV_OK;
    int looping = 0;

    proc_name[0] = 0;
    {
        struct Addr_Map_Entry *ame = 0;
        ame = addr_map_find(low_pc,pcMap);
        if (ame && ame->mp_name) {
            /* mp_name is only NULL here if we just ran out of heap memory! */
            safe_strcpy(proc_name, sizeof(proc_name),
                ame->mp_name,(long) strlen(ame->mp_name));
            return proc_name;
        }
        if (*all_cus_seen) {
            return "";
        }
    }
    if (current_cu_die_for_print_frames == NULL) {
        /* Call depends on dbg->cu_context to know what to do. */
        cures = dwarf_next_cu_header(dbg, &cu_header_length,
            &version_stamp, &abbrev_offset,
            &address_size, &next_cu_offset,
            &err);
        if (cures == DW_DLV_ERROR) {
            return NULL;
        } else if (cures == DW_DLV_NO_ENTRY) {
            /* loop thru the list again */
            current_cu_die_for_print_frames = 0;
            ++looping;
        } else {                /* DW_DLV_OK */
            dres = dwarf_siblingof(dbg, NULL,
                &current_cu_die_for_print_frames,
                &err);
            if (dres == DW_DLV_ERROR) {
                return NULL;
            }
        }
    }
    if (dres == DW_DLV_OK) {
        Dwarf_Die child = 0;

        if (current_cu_die_for_print_frames == 0) {
            /*  no information. Possibly a stripped file */
            return NULL;
        }
        chres =
            dwarf_child(current_cu_die_for_print_frames, &child, &err);
        if (chres == DW_DLV_ERROR) {
            print_error(dbg, "dwarf_cu_header on child read ", chres,
                err);
        } else if (chres == DW_DLV_NO_ENTRY) {
        } else {                /* DW_DLV_OK */
            int gotname =
                load_nested_proc_name(dbg, child, low_pc, proc_name,
                    BUFSIZ,pcMap);

            dwarf_dealloc(dbg, child, DW_DLA_DIE);
            if (gotname) {
                return (proc_name);
            }
            child = 0;
        }
    }
    for (;;) {
        Dwarf_Die ldie = 0;

        cures = dwarf_next_cu_header(dbg, &cu_header_length,
            &version_stamp, &abbrev_offset,
            &address_size, &next_cu_offset,
            &err);
        if (cures != DW_DLV_OK) {
            *all_cus_seen = 1;
            break;
        }

        dres = dwarf_siblingof(dbg, NULL, &ldie, &err);
        if (current_cu_die_for_print_frames) {
            dwarf_dealloc(dbg, current_cu_die_for_print_frames,
                DW_DLA_DIE);
        }
        current_cu_die_for_print_frames = 0;
        if (dres == DW_DLV_ERROR) {
            print_error(dbg,
                "dwarf_cu_header Child Read finding proc name for .debug_frame",
                chres, err);
            continue;
        } else if (dres == DW_DLV_NO_ENTRY) {
            ++looping;
            if (looping > 1) {
                print_error(dbg, "looping  on cu headers!", dres, err);
                return NULL;
            }
            continue;
        }
        /* DW_DLV_OK */
        current_cu_die_for_print_frames = ldie;
        {
            int chres = 0;
            Dwarf_Die child = 0;

            chres =
                dwarf_child(current_cu_die_for_print_frames, &child,
                    &err);
            if (chres == DW_DLV_ERROR) {
                print_error(dbg, "dwarf Child Read ", chres, err);
            } else if (chres == DW_DLV_NO_ENTRY) {

                ;/* do nothing, loop on cu */
            } else {
                /* DW_DLV_OK) */

                int gotname =
                    load_nested_proc_name(dbg, child, low_pc, proc_name,
                        BUFSIZ,pcMap);

                dwarf_dealloc(dbg, child, DW_DLA_DIE);
                if (gotname) {
                    return (proc_name);
                }
            }
        }
    }
    return (NULL);
}

/*  Gather the fde print logic here so the control logic
    determining what FDE to print is clearer.  */
static int
print_one_fde(Dwarf_Debug dbg, Dwarf_Fde fde,
    Dwarf_Unsigned fde_index,
    Dwarf_Cie * cie_data,
    Dwarf_Signed cie_element_count,
    Dwarf_Half address_size, int is_eh,
    struct dwconf_s *config_data,
    void **pcMap,
    void **lowpcSet,
    int * all_cus_seen)
{
    Dwarf_Addr j = 0;
    Dwarf_Addr low_pc = 0;
    Dwarf_Unsigned func_length = 0;
    Dwarf_Ptr fde_bytes = NULL;
    Dwarf_Unsigned fde_bytes_length = 0;
    Dwarf_Off cie_offset = 0;
    Dwarf_Signed cie_index = 0;
    Dwarf_Off fde_offset = 0;
    Dwarf_Signed eh_table_offset = 0;
    int fres = 0;
    int offres = 0;
    string temps = 0;
    Dwarf_Error err = 0;
    int printed_intro_addr = 0;

    fres = dwarf_get_fde_range(fde,
        &low_pc, &func_length,
        &fde_bytes,
        &fde_bytes_length,
        &cie_offset, &cie_index,
        &fde_offset, &err);
    if (fres == DW_DLV_ERROR) {
        print_error(dbg, "dwarf_get_fde_range", fres, err);
    }
    if (fres == DW_DLV_NO_ENTRY) {
        return DW_DLV_NO_ENTRY;
    }
    if (cu_name_flag &&
        fde_offset_for_cu_low != DW_DLV_BADOFFSET &&
        (fde_offset < fde_offset_for_cu_low ||
        fde_offset > fde_offset_for_cu_high)) {
        return DW_DLV_NO_ENTRY;
    }
    /* eh_table_offset is IRIX ONLY. */
    fres = dwarf_get_fde_exception_info(fde, &eh_table_offset, &err);
    if (fres == DW_DLV_ERROR) {
        print_error(dbg, "dwarf_get_fde_exception_info", fres, err);
    }
    if (suppress_nested_name_search) {
        temps = 0;
    } else {
        struct Addr_Map_Entry *mp = 0;
        temps = get_fde_proc_name(dbg, low_pc,pcMap,all_cus_seen);
        mp = addr_map_find(low_pc,lowpcSet);
        if (check_frames || check_frames_extended) {
            DWARF_CHECK_COUNT(fde_duplication,1);
        }
        if (mp) {
            if (check_frames || check_frames_extended) {
                char msg[400];
                if (temps && (strlen(temps) > 0)) {
                    snprintf(msg,sizeof(msg),"An fde low pc of 0x%"
                        DW_PR_DUx
                        " is not the first fde with that pc. "
                        "The first is named \"%s\"",
                        (Dwarf_Unsigned)low_pc,
                        temps);
                } else {
                    snprintf(msg,sizeof(msg),"An fde low pc of 0x%"
                        DW_PR_DUx
                        " is not the first fde with that pc. "
                        "The first is not named.",
                        (Dwarf_Unsigned)low_pc);

                }
                DWARF_CHECK_ERROR(fde_duplication,msg);
            }
        } else {
            addr_map_insert(low_pc,0,lowpcSet);
        }
    }

    /* Do not print if in check mode */
    if (!check_frames_extended) {
        printf("<%5" DW_PR_DSd "><0x%" DW_PR_XZEROS  DW_PR_DUx
            ":0x%" DW_PR_XZEROS DW_PR_DUx
            "><%s><fde offset 0x%" DW_PR_XZEROS DW_PR_DUx
            " length: 0x%" DW_PR_XZEROS  DW_PR_DUx ">",
            cie_index, (Dwarf_Unsigned)low_pc,
            (Dwarf_Unsigned)(low_pc + func_length),
            temps ? temps : "", (Dwarf_Unsigned)fde_offset, fde_bytes_length);
    }



    if (!is_eh) {
        /* IRIX uses eh_table_offset. */
        /* Do not print if in check mode */
        if (!check_frames_extended) {
            if (eh_table_offset == DW_DLX_NO_EH_OFFSET) {
                printf("<eh offset %s>\n", "none");
            } else if (eh_table_offset == DW_DLX_EH_OFFSET_UNAVAILABLE) {
                printf("<eh offset %s>\n", "unknown");
            } else {
                printf("<eh offset 0x%" DW_PR_XZEROS DW_PR_DUx
                    ">\n", eh_table_offset);
            }
        }
    } else {
        int ares = 0;
        Dwarf_Small *data = 0;
        Dwarf_Unsigned len = 0;

        ares = dwarf_get_fde_augmentation_data(fde, &data, &len, &err);
        if (ares == DW_DLV_NO_ENTRY) {
            /* do nothing. */
        } else if (ares == DW_DLV_OK) {
            /* Do not print if in check mode */
            if (!check_frames_extended) {
                unsigned k2 = 0;

                printf("<eh aug data len 0x%" DW_PR_DUx , len);
                for (k2 = 0; k2 < len; ++k2) {
                    if (k2 == 0) {
                        printf(" bytes 0x");
                    }
                    printf("%02x ", (unsigned char) data[k2]);
                }
                printf(">");
            }
        }                       /* else DW_DLV_ERROR, do nothing */

        /* Do not print if in check mode */
        if (!check_frames_extended) {
            printf("\n");

        }
    }

    for (j = low_pc; j < low_pc + func_length; j++) {
        Dwarf_Half k = 0;

        if (config_data->cf_interface_number == 3) {
            Dwarf_Signed reg = 0;
            Dwarf_Signed offset_relevant = 0;
            Dwarf_Small value_type = 0;
            Dwarf_Signed offset_or_block_len = 0;
            Dwarf_Signed offset = 0;
            Dwarf_Ptr block_ptr = 0;
            Dwarf_Addr row_pc = 0;

            int fires = dwarf_get_fde_info_for_cfa_reg3(fde,
                j,
                &value_type,
                &offset_relevant,
                &reg,
                &offset_or_block_len,
                &block_ptr,
                &row_pc,
                &err);
            offset = offset_or_block_len;
            if (fires == DW_DLV_ERROR) {
                print_error(dbg,
                    "dwarf_get_fde_info_for_reg", fires, err);
            }
            if (fires == DW_DLV_NO_ENTRY) {
                continue;
            }
            if (row_pc != j) {
                /* duplicate row */
                continue;
            }

            /* Do not print if in check mode */
            if (!printed_intro_addr && !check_frames_extended) {
                printf("        0x%" DW_PR_XZEROS DW_PR_DUx
                    ": ", (Dwarf_Unsigned)j);
                printed_intro_addr = 1;
            }
            print_one_frame_reg_col(dbg, config_data->cf_cfa_reg,
                value_type,
                reg,
                address_size,
                config_data,
                offset_relevant, offset, block_ptr);
        }
        for (k = 0; k < config_data->cf_table_entry_count; k++) {
            Dwarf_Signed reg = 0;
            Dwarf_Signed offset_relevant = 0;
            int fires = 0;
            Dwarf_Small value_type = 0;
            Dwarf_Ptr block_ptr = 0;
            Dwarf_Signed offset_or_block_len = 0;
            Dwarf_Signed offset = 0;
            Dwarf_Addr row_pc = 0;

            if (config_data->cf_interface_number == 3) {
                fires = dwarf_get_fde_info_for_reg3(fde,
                    k,
                    j,
                    &value_type,
                    &offset_relevant,
                    &reg,
                    &offset_or_block_len,
                    &block_ptr,
                    &row_pc, &err);
                offset = offset_or_block_len;
            } else {
                /*  This interface is deprecated. Is the old
                    MIPS/DWARF2 interface. */
                /*  ASSERT: config_data->cf_interface_number == 2 */
                value_type = DW_EXPR_OFFSET;
                fires = dwarf_get_fde_info_for_reg(fde,
                    k,
                    j,
                    &offset_relevant,
                    &reg,
                    &offset, &row_pc,
                    &err);
            }
            if (fires == DW_DLV_ERROR) {
                printf("\n");
                print_error(dbg,
                    "dwarf_get_fde_info_for_reg", fires, err);
            }
            if (fires == DW_DLV_NO_ENTRY) {
                continue;
            }
            if (row_pc != j) {
                /* duplicate row */
                break;
            }

            /* Do not print if in check mode */
            if (!printed_intro_addr && !check_frames_extended) {
                printf("        0x%" DW_PR_XZEROS DW_PR_DUx ": ",
                    (Dwarf_Unsigned)j);
                printed_intro_addr = 1;
            }
            print_one_frame_reg_col(dbg,k,
                value_type,
                reg,
                address_size,
                config_data,
                offset_relevant, offset, block_ptr);
        }
        if (printed_intro_addr) {
            printf("\n");
            printed_intro_addr = 0;
        }
    }
    if (verbose > 1) {
        Dwarf_Off fde_off = 0;
        Dwarf_Off cie_off = 0;

        /*  Get the fde instructions and print them in raw form, just
            like cie instructions */
        Dwarf_Ptr instrs = 0;
        Dwarf_Unsigned ilen = 0;
        int res = 0;

        res = dwarf_get_fde_instr_bytes(fde, &instrs, &ilen, &err);
        offres =
            dwarf_fde_section_offset(dbg, fde, &fde_off, &cie_off,
                &err);
        if (offres == DW_DLV_OK) {
            /* Do not print if in check mode */
            if (!check_frames_extended) {
                printf(" fde section offset %" DW_PR_DUu
                    " 0x%" DW_PR_XZEROS DW_PR_DUx
                    " cie offset for fde: %" DW_PR_DUu
                    " 0x%" DW_PR_XZEROS DW_PR_DUx "\n",
                    (Dwarf_Unsigned) fde_off,
                    (Dwarf_Unsigned) fde_off,
                    (Dwarf_Unsigned) cie_off,
                    (Dwarf_Unsigned) cie_off);
            }
        }


        if (res == DW_DLV_OK) {
            int cires = 0;
            Dwarf_Unsigned cie_length = 0;
            Dwarf_Small version = 0;
            string augmenter = 0;
            Dwarf_Unsigned code_alignment_factor = 0;
            Dwarf_Signed data_alignment_factor = 0;
            Dwarf_Half return_address_register_rule = 0;
            Dwarf_Ptr initial_instructions = 0;
            Dwarf_Unsigned initial_instructions_length = 0;

            if (cie_index >= cie_element_count) {
                printf("Bad cie index %" DW_PR_DSd
                    " with fde index %" DW_PR_DUu "! "
                    "(table entry max %" DW_PR_DSd ")\n",
                    cie_index, fde_index,
                    cie_element_count);
                exit(1);
            }
            cires = dwarf_get_cie_info(cie_data[cie_index],
                &cie_length,
                &version,
                &augmenter,
                &code_alignment_factor,
                &data_alignment_factor,
                &return_address_register_rule,
                &initial_instructions,
                &initial_instructions_length,
                &err);
            if (cires == DW_DLV_ERROR) {
                printf
                    ("Bad cie index %" DW_PR_DSd
                    " with fde index %" DW_PR_DUu "!\n",
                    cie_index,  fde_index);
                print_error(dbg, "dwarf_get_cie_info", cires, err);
            }
            if (cires == DW_DLV_NO_ENTRY) {
                ; /* ? */
            } else {
                /* Do not print if in check mode */
                if (!check_frames_extended) {
                    print_frame_inst_bytes(dbg, instrs,
                        (Dwarf_Signed) ilen,
                        data_alignment_factor,
                        (int) code_alignment_factor,
                        address_size, config_data);
                }
            }
        } else if (res == DW_DLV_NO_ENTRY) {
            printf("Impossible: no instr bytes for fde index %"
                DW_PR_DUu "?\n",
                fde_index);
        } else {
            /* DW_DLV_ERROR */
            printf("Error: on gettinginstr bytes for fde index %"
                DW_PR_DUu "?\n",
                fde_index);
            print_error(dbg, "dwarf_get_fde_instr_bytes", res, err);
        }

    }
    return DW_DLV_OK;
}


/*  Print a cie.  Gather the print logic here so the
    control logic deciding what to print
    is clearer.
*/
int
print_one_cie(Dwarf_Debug dbg, Dwarf_Cie cie,
    Dwarf_Unsigned cie_index, Dwarf_Half address_size,
    struct dwconf_s *config_data)
{

    int cires = 0;
    Dwarf_Unsigned cie_length = 0;
    Dwarf_Small version = 0;
    string augmenter = "";
    Dwarf_Unsigned code_alignment_factor = 0;
    Dwarf_Signed data_alignment_factor = 0;
    Dwarf_Half return_address_register_rule = 0;
    Dwarf_Ptr initial_instructions = 0;
    Dwarf_Unsigned initial_instructions_length = 0;
    Dwarf_Off cie_off = 0;
    Dwarf_Error err = 0;

    cires = dwarf_get_cie_info(cie,
        &cie_length,
        &version,
        &augmenter,
        &code_alignment_factor,
        &data_alignment_factor,
        &return_address_register_rule,
        &initial_instructions,
        &initial_instructions_length, &err);
    if (cires == DW_DLV_ERROR) {
        print_error(dbg, "dwarf_get_cie_info", cires, err);
    }
    if (cires == DW_DLV_NO_ENTRY) {
        printf("Impossible DW_DLV_NO_ENTRY on cie %" DW_PR_DUu "\n",
            cie_index);
        return DW_DLV_NO_ENTRY;
    }
    {
        /* Do not print if in check mode */
        if (!check_frames_extended) {
            printf("<%5" DW_PR_DUu ">\tversion\t\t\t\t%d\n",
                cie_index, version);
            cires = dwarf_cie_section_offset(dbg, cie, &cie_off, &err);
            if (cires == DW_DLV_OK) {
                printf("\tcie section offset\t\t%" DW_PR_DUu
                    " 0x%" DW_PR_XZEROS DW_PR_DUx "\n",
                    (Dwarf_Unsigned) cie_off,
                    (Dwarf_Unsigned) cie_off);
            }
            printf("\taugmentation\t\t\t%s\n", augmenter);
            printf("\tcode_alignment_factor\t\t%" DW_PR_DUu "\n",
                code_alignment_factor);
            printf("\tdata_alignment_factor\t\t%" DW_PR_DSd "\n",
                data_alignment_factor);
            printf("\treturn_address_register\t\t%d\n",
                return_address_register_rule);
        }

        {
            int ares = 0;
            Dwarf_Small *data = 0;
            Dwarf_Unsigned len = 0;

            ares =
                dwarf_get_cie_augmentation_data(cie, &data, &len, &err);
            if (ares == DW_DLV_NO_ENTRY) {
                /* do nothing. */
            } else if (ares == DW_DLV_OK && len > 0) {
                /* Do not print if in check mode */
                if (!check_frames_extended) {
                    unsigned k2 = 0;

                    printf(" eh aug data len 0x%" DW_PR_DUx , len);
                    for (k2 = 0; data && k2 < len; ++k2) {
                        if (k2 == 0) {
                            printf(" bytes 0x");
                        }
                        printf("%02x ", (unsigned char) data[k2]);
                    }
                    printf("\n");
                }
            }  /* else DW_DLV_ERROR or no data, do nothing */
        }

        /* Do not print if in check mode */
        if (!check_frames_extended) {
            printf("\tbytes of initial instructions\t%" DW_PR_DUu "\n",
                initial_instructions_length);
            printf("\tcie length\t\t\t%" DW_PR_DUu "\n", cie_length);
            /*  For better layout */
            printf("\tinitial instructions\n");
            print_frame_inst_bytes(dbg, initial_instructions,
                (Dwarf_Signed) initial_instructions_length,
                data_alignment_factor,
                (int) code_alignment_factor,
                address_size, config_data);
        }
    }
    return DW_DLV_OK;
}

void
get_string_from_locs(Dwarf_Debug dbg,
    Dwarf_Ptr bytes_in,
    Dwarf_Unsigned block_len,
    Dwarf_Half addr_size,
    struct esb_s *out_string)
{

    Dwarf_Locdesc *locdescarray = 0;
    Dwarf_Signed listlen = 0;
    Dwarf_Error err2 =0;
    int skip_locdesc_header=1;
    int res = 0;
    int res2 = dwarf_loclist_from_expr_a(dbg,
        bytes_in,block_len,
        addr_size,
        &locdescarray,
        &listlen,&err2);
    if (res2 == DW_DLV_ERROR) {
        print_error(dbg, "dwarf_get_loclist_from_expr_a",
            res2, err2);
    }
    if (res2==DW_DLV_NO_ENTRY) {
        return;
    }
    /* lcnt is always 1 */

    /* Use locdescarray  here.*/
    res = dwarfdump_print_one_locdesc(dbg,
        locdescarray,
        skip_locdesc_header,
        out_string);
    if (res != DW_DLV_OK) {
        printf("Bad status from _dwarf_print_one_locdesc %d\n",res);
        exit(1);
    }

    dwarf_dealloc(dbg, locdescarray->ld_s, DW_DLA_LOC_BLOCK);
    dwarf_dealloc(dbg, locdescarray, DW_DLA_LOCDESC);
    return ;
}

/*  Print the frame instructions in detail for a glob of instructions.
*/

/*ARGSUSED*/ void
print_frame_inst_bytes(Dwarf_Debug dbg,
    Dwarf_Ptr cie_init_inst, Dwarf_Signed len,
    Dwarf_Signed data_alignment_factor,
    int code_alignment_factor, Dwarf_Half addr_size,
    struct dwconf_s *config_data)
{
    unsigned char *instp = (unsigned char *) cie_init_inst;
    Dwarf_Unsigned uval = 0;
    Dwarf_Unsigned uval2 = 0;
    unsigned int uleblen = 0;
    unsigned int off = 0;
    unsigned int loff = 0;
    unsigned short u16 = 0;
    unsigned int u32 = 0;
    unsigned long long u64;

    for (; len > 0;) {
        unsigned char ibyte = *instp;
        int top = ibyte & 0xc0;
        int bottom = ibyte & 0x3f;
        int delta = 0;
        int reg = 0;

        switch (top) {
        case DW_CFA_advance_loc:
            delta = ibyte & 0x3f;
            printf("\t%2u DW_CFA_advance_loc %d", off,
                (int) (delta * code_alignment_factor));
            if (verbose) {
                printf("  (%d * %d)", (int) delta,
                    (int) code_alignment_factor);
            }
            printf("\n");
            break;
        case DW_CFA_offset:
            loff = off;
            reg = ibyte & 0x3f;
            uval = local_dwarf_decode_u_leb128(instp + 1, &uleblen);
            instp += uleblen;
            len -= uleblen;
            off += uleblen;
            printf("\t%2u DW_CFA_offset ", loff);
            printreg((Dwarf_Signed) reg, config_data);
            printf(" %" DW_PR_DSd , (Dwarf_Signed)
                (((Dwarf_Signed) uval) * data_alignment_factor));
            if (verbose) {
                printf("  (%" DW_PR_DUu " * %" DW_PR_DSd ")", uval,
                    data_alignment_factor);
            }
            printf("\n");
            break;

        case DW_CFA_restore:
            reg = ibyte & 0x3f;
            printf("\t%2u DW_CFA_restore ", off);
            printreg((Dwarf_Signed) reg, config_data);
            printf("\n");
            break;

        default:
            loff = off;
            switch (bottom) {
            case DW_CFA_set_loc:
                /* operand is address, so need address size */
                /* which will be 4 or 8. */
                switch (addr_size) {
                case 4:
                    {
                        __uint32_t v32 = 0;
                        memcpy(&v32, instp + 1, addr_size);
                        uval = v32;
                    }
                    break;
                case 8:
                    {
                        __uint64_t v64 = 0;
                        memcpy(&v64, instp + 1, addr_size);
                        uval = v64;
                    }
                    break;
                default:
                    printf
                        ("Error: Unexpected address size %d in DW_CFA_set_loc!\n",
                        addr_size);
                    uval = 0;
                }

                instp += addr_size;
                len -= (Dwarf_Signed) addr_size;
                off += addr_size;
                printf("\t%2u DW_CFA_set_loc %" DW_PR_DUu "\n",
                    loff,  uval);
                break;
            case DW_CFA_advance_loc1:
                delta = (unsigned char) *(instp + 1);
                uval2 = delta;
                instp += 1;
                len -= 1;
                off += 1;
                printf("\t%2u DW_CFA_advance_loc1 %" DW_PR_DUu "\n",
                    loff, uval2);
                break;
            case DW_CFA_advance_loc2:
                memcpy(&u16, instp + 1, 2);
                uval2 = u16;
                instp += 2;
                len -= 2;
                off += 2;
                printf("\t%2u DW_CFA_advance_loc2 %" DW_PR_DUu "\n",
                    loff,  uval2);
                break;
            case DW_CFA_advance_loc4:
                memcpy(&u32, instp + 1, 4);
                uval2 = u32;
                instp += 4;
                len -= 4;
                off += 4;
                printf("\t%2u DW_CFA_advance_loc4 %" DW_PR_DUu "\n",
                    loff, uval2);
                break;
            case DW_CFA_MIPS_advance_loc8:
                memcpy(&u64, instp + 1, 8);
                uval2 = u64;
                instp += 8;
                len -= 8;
                off += 8;
                printf("\t%2u DW_CFA_MIPS_advance_loc8 %" DW_PR_DUu "\n",
                    loff,  uval2);
                break;
            case DW_CFA_offset_extended:
                uval = local_dwarf_decode_u_leb128(instp + 1, &uleblen);
                instp += uleblen;
                len -= uleblen;
                off += uleblen;
                uval2 =
                    local_dwarf_decode_u_leb128(instp + 1, &uleblen);
                instp += uleblen;
                len -= uleblen;
                off += uleblen;
                printf("\t%2u DW_CFA_offset_extended ", loff);
                printreg((Dwarf_Signed) uval, config_data);
                printf(" %" DW_PR_DSd ,
                    (Dwarf_Signed) (((Dwarf_Signed) uval2) *
                        data_alignment_factor));
                if (verbose) {
                    printf("  (%" DW_PR_DUu " * %d)",  uval2,
                        (int) data_alignment_factor);
                }
                printf("\n");
                break;

            case DW_CFA_restore_extended:
                uval = local_dwarf_decode_u_leb128(instp + 1, &uleblen);
                instp += uleblen;
                len -= uleblen;
                off += uleblen;
                printf("\t%2u DW_CFA_restore_extended ", loff);
                printreg((Dwarf_Signed) uval, config_data);
                printf("\n");
                break;
            case DW_CFA_undefined:
                uval = local_dwarf_decode_u_leb128(instp + 1, &uleblen);
                instp += uleblen;
                len -= uleblen;
                off += uleblen;
                printf("\t%2u DW_CFA_undefined ", loff);
                printreg((Dwarf_Signed) uval, config_data);
                printf("\n");
                break;
            case DW_CFA_same_value:
                uval = local_dwarf_decode_u_leb128(instp + 1, &uleblen);
                instp += uleblen;
                len -= uleblen;
                off += uleblen;
                printf("\t%2u DW_CFA_same_value ", loff);
                printreg((Dwarf_Signed) uval, config_data);
                printf("\n");
                break;
            case DW_CFA_register:
                uval = local_dwarf_decode_u_leb128(instp + 1, &uleblen);
                instp += uleblen;
                len -= uleblen;
                off += uleblen;
                uval2 =
                    local_dwarf_decode_u_leb128(instp + 1, &uleblen);
                instp += uleblen;
                len -= uleblen;
                off += uleblen;
                printf("\t%2u DW_CFA_register ", loff);
                printreg((Dwarf_Signed) uval, config_data);
                printf(" = ");
                printreg((Dwarf_Signed) uval2, config_data);
                printf("\n");
                break;
            case DW_CFA_remember_state:
                printf("\t%2u DW_CFA_remember_state\n", loff);
                break;
            case DW_CFA_restore_state:
                printf("\t%2u DW_CFA_restore_state\n", loff);
                break;
            case DW_CFA_def_cfa:
                uval = local_dwarf_decode_u_leb128(instp + 1, &uleblen);
                instp += uleblen;
                len -= uleblen;
                off += uleblen;
                uval2 =
                    local_dwarf_decode_u_leb128(instp + 1, &uleblen);
                instp += uleblen;
                len -= uleblen;
                off += uleblen;
                printf("\t%2u DW_CFA_def_cfa ", loff);
                printreg((Dwarf_Signed) uval, config_data);
                printf(" %" DW_PR_DUu , (unsigned long long) uval2);
                printf("\n");
                break;
            case DW_CFA_def_cfa_register:
                uval = local_dwarf_decode_u_leb128(instp + 1, &uleblen);
                instp += uleblen;
                len -= uleblen;
                off += uleblen;
                printf("\t%2u DW_CFA_def_cfa_register ", loff);
                printreg((Dwarf_Signed) uval, config_data);
                printf("\n");
                break;
            case DW_CFA_def_cfa_offset:
                uval = local_dwarf_decode_u_leb128(instp + 1, &uleblen);
                instp += uleblen;
                len -= uleblen;
                off += uleblen;
                printf("\t%2u DW_CFA_def_cfa_offset %" DW_PR_DUu "\n",
                    loff, uval);
                break;

            case DW_CFA_nop:
                printf("\t%2u DW_CFA_nop\n", loff);
                break;

            case DW_CFA_def_cfa_expression:     /* DWARF3 */
                {
                    Dwarf_Unsigned block_len =
                        local_dwarf_decode_u_leb128(instp + 1,
                            &uleblen);

                    instp += uleblen;
                    len -= uleblen;
                    off += uleblen;
                    printf
                        ("\t%2u DW_CFA_def_cfa_expression expr block len %"
                        DW_PR_DUu "\n",
                        loff,
                        block_len);
                    dump_block("\t\t", (char *) instp+1,
                        (Dwarf_Signed) block_len);
                    printf("\n");
                    if (verbose) {
                        struct esb_s exprstring;
                        esb_constructor(&exprstring);
                        get_string_from_locs(dbg,
                            instp+1,block_len,addr_size,&exprstring);
                        printf("\t\t%s\n",esb_get_string(&exprstring));
                        esb_destructor(&exprstring);
                    }
                    instp += block_len;
                    len -= block_len;
                    off += block_len;
                }
                break;
            case DW_CFA_expression:     /* DWARF3 */
                uval = local_dwarf_decode_u_leb128(instp + 1, &uleblen);
                instp += uleblen;
                len -= uleblen;
                off += uleblen;
                {
                    /*  instp is always 1 byte back, so we need +1
                        when we use it. See the final increment
                        of this for loop. */
                    Dwarf_Unsigned block_len =
                        local_dwarf_decode_u_leb128(instp + 1,
                            &uleblen);

                    instp += uleblen;
                    len -= uleblen;
                    off += uleblen;
                    printf
                        ("\t%2u DW_CFA_expression %" DW_PR_DUu
                        " expr block len %" DW_PR_DUu "\n",
                        loff,  uval,
                        block_len);
                    dump_block("\t\t", (char *) instp+1,
                        (Dwarf_Signed) block_len);
                    printf("\n");
                    if (verbose) {
                        struct esb_s exprstring;
                        esb_constructor(&exprstring);
                        get_string_from_locs(dbg,
                            instp+1,block_len,addr_size,&exprstring);
                        printf("\t\t%s\n",esb_get_string(&exprstring));
                        esb_destructor(&exprstring);
                    }
                    instp += block_len;
                    len -= block_len;
                    off += block_len;
                }
                break;
            case DW_CFA_offset_extended_sf: /* DWARF3 */
                uval = local_dwarf_decode_u_leb128(instp + 1, &uleblen);
                instp += uleblen;
                len -= uleblen;
                off += uleblen;
                {
                    /* instp is always 1 byte back, so we need +1
                        when we use it. See the final increment
                        of this for loop. */
                    Dwarf_Signed sval2 =
                        local_dwarf_decode_s_leb128(instp + 1,
                            &uleblen);

                    instp += uleblen;
                    len -= uleblen;
                    off += uleblen;
                    printf("\t%2u DW_CFA_offset_extended_sf ", loff);
                    printreg((Dwarf_Signed) uval, config_data);
                    printf(" %" DW_PR_DSd , (Dwarf_Signed)
                        ((sval2) * data_alignment_factor));
                    if (verbose) {
                        printf("  (%" DW_PR_DSd " * %d)", sval2,
                            (int) data_alignment_factor);
                    }
                }
                printf("\n");
                break;
            case DW_CFA_def_cfa_sf:     /* DWARF3 */
                /*  instp is always 1 byte back, so we need +1
                    when we use it. See the final increment
                    of this for loop. */
                uval = local_dwarf_decode_u_leb128(instp + 1, &uleblen);
                instp += uleblen;
                len -= uleblen;
                off += uleblen;
                {
                    Dwarf_Signed sval2 =
                        local_dwarf_decode_s_leb128(instp + 1,
                            &uleblen);

                    instp += uleblen;
                    len -= uleblen;
                    off += uleblen;
                    printf("\t%2u DW_CFA_def_cfa_sf ", loff);
                    printreg((Dwarf_Signed) uval, config_data);
                    printf(" %" DW_PR_DSd , (long long) sval2);
                    printf(" (*data alignment factor=>%" DW_PR_DSd ")",
                        (Dwarf_Signed)(sval2*data_alignment_factor));
                }
                printf("\n");
                break;
            case DW_CFA_def_cfa_offset_sf:      /* DWARF3 */
                {
                    /*  instp is always 1 byte back, so we need +1
                        when we use it. See the final increment
                        of this for loop. */
                    Dwarf_Signed sval =
                        local_dwarf_decode_s_leb128(instp + 1,
                            &uleblen);

                    instp += uleblen;
                    len -= uleblen;
                    off += uleblen;
                    printf("\t%2u DW_CFA_def_cfa_offset_sf %"
                        DW_PR_DSd " (*data alignment factor=> %"
                        DW_PR_DSd ")\n",
                        loff, sval,
                        (Dwarf_Signed)(data_alignment_factor*sval));

                }
                break;
            case DW_CFA_val_offset:     /* DWARF3 */
                /*  instp is always 1 byte back, so we need +1
                    when we use it. See the final increment
                    of this for loop. */
                uval = local_dwarf_decode_u_leb128(instp + 1, &uleblen);
                instp += uleblen;
                len -= uleblen;
                off += uleblen;
                {
                    Dwarf_Signed sval2 =
                        local_dwarf_decode_s_leb128(instp + 1,
                            &uleblen);
                    instp += uleblen;
                    len -= uleblen;
                    off += uleblen;
                    printf("\t%2u DW_CFA_val_offset ", loff);
                    printreg((Dwarf_Signed)uval, config_data);
                    printf(" %" DW_PR_DSd ,
                        (Dwarf_Signed) (sval2 *
                            data_alignment_factor));
                    if (verbose) {
                        printf("  (%" DW_PR_DSd " * %d)",
                            (Dwarf_Signed) sval2,
                            (int) data_alignment_factor);
                    }
                }
                printf("\n");

                break;
            case DW_CFA_val_offset_sf:  /* DWARF3 */
                /*  instp is always 1 byte back, so we need +1
                    when we use it. See the final increment
                    of this for loop. */
                uval = local_dwarf_decode_u_leb128(instp + 1, &uleblen);
                instp += uleblen;
                len -= uleblen;
                off += uleblen;
                {
                    Dwarf_Signed sval2 =
                        local_dwarf_decode_s_leb128(instp + 1,
                            &uleblen);

                    instp += uleblen;
                    len -= uleblen;
                    off += uleblen;
                    printf("\t%2u DW_CFA_val_offset_sf ", loff);
                    printreg((Dwarf_Signed) uval, config_data);
                    printf(" %" DW_PR_DSd , (signed long long)
                        ((sval2) * data_alignment_factor));
                    if (verbose) {
                        printf("  (%" DW_PR_DSd " * %d)", sval2,
                            (int) data_alignment_factor);
                    }
                }
                printf("\n");

                break;
            case DW_CFA_val_expression: /* DWARF3 */
                /*  instp is always 1 byte back, so we need +1
                    when we use it. See the final increment
                    of this for loop. */
                uval = local_dwarf_decode_u_leb128(instp + 1, &uleblen);
                instp += uleblen;
                len -= uleblen;
                off += uleblen;
                {
                    Dwarf_Unsigned block_len =
                        local_dwarf_decode_u_leb128(instp + 1,
                            &uleblen);

                    instp += uleblen;
                    len -= uleblen;
                    off += uleblen;
                    printf
                        ("\t%2u DW_CFA_val_expression %" DW_PR_DUu
                        " expr block len %" DW_PR_DUu "\n",
                        loff,  uval,
                        block_len);
                    dump_block("\t\t", (char *) instp+1,
                        (Dwarf_Signed) block_len);
                    printf("\n");
                    if (verbose) {
                        struct esb_s exprstring;
                        esb_constructor(&exprstring);
                        get_string_from_locs(dbg,
                            instp+1,block_len,addr_size,&exprstring);
                        printf("\t\t%s\n",esb_get_string(&exprstring));
                        esb_destructor(&exprstring);
                    }
                    instp += block_len;
                    len -= block_len;
                    off += block_len;
                }


                break;


#ifdef DW_CFA_GNU_window_save
            case DW_CFA_GNU_window_save:{
                /*  no information: this just tells unwinder to
                    restore the window registers from the previous
                    frame's window save area */
                printf("\t%2u DW_CFA_GNU_window_save \n", loff);
                }
                break;
#endif
#ifdef DW_CFA_GNU_negative_offset_extended
            case DW_CFA_GNU_negative_offset_extended:{
                printf("\t%2u DW_CFA_GNU_negative_offset_extended \n",
                    loff);
                }
                break;
#endif
#ifdef  DW_CFA_GNU_args_size
                /*  single uleb128 is the current arg area size in
                    bytes. no register exists yet to save this in */
            case DW_CFA_GNU_args_size:{
                Dwarf_Unsigned lreg = 0;

                /*  instp is always 1 byte back, so we need +1
                    when we use it. See the final increment
                    of this for loop. */
                lreg = local_dwarf_decode_u_leb128(instp + 1,
                    &uleblen);
                printf("\t%2u DW_CFA_GNU_args_size arg size: %"
                    DW_PR_DUu "\n",
                    loff, lreg);
                instp += uleblen;
                len -= uleblen;
                off += uleblen;
                }
                break;
#endif

            default:
                printf(" %u Unexpected op 0x%x: \n",
                    loff, (unsigned int) bottom);
                len = 0;
                break;
            }
        }
        instp++;
        len--;
        off++;
    }
}

/* Print our register names for the cases we have a name.
   Delegate to the configure code to actually do the print.
*/
void
printreg(Dwarf_Signed reg, struct dwconf_s *config_data)
{
    print_reg_from_config_data(reg, config_data);
}

/*  Actually does the printing of a rule in the table.
    This may print something or may print nothing!  */
static void
print_one_frame_reg_col(Dwarf_Debug dbg,
    Dwarf_Unsigned rule_id,
    Dwarf_Small value_type,
    Dwarf_Unsigned reg_used,
    Dwarf_Half addr_size,
    struct dwconf_s *config_data,
    Dwarf_Signed offset_relevant,
    Dwarf_Signed offset,
    Dwarf_Ptr block_ptr)
{
    char *type_title = "";
    int print_type_title = 1;

    if (check_frames_extended) {
        return;
    }

    if (config_data->cf_interface_number == 2)
        print_type_title = 0;

    switch (value_type) {
    case DW_EXPR_OFFSET:
        type_title = "off";
        goto preg2;
    case DW_EXPR_VAL_OFFSET:
        type_title = "valoff";

        preg2:
        if (reg_used == config_data->cf_initial_rule_value) {
            break;
        }
        if (print_type_title)
            printf("<%s ", type_title);
        printreg((Dwarf_Signed) rule_id, config_data);
        printf("=");
        if (offset_relevant == 0) {
            printreg((Dwarf_Signed) reg_used, config_data);
            printf(" ");
        } else {
            printf("%02" DW_PR_DSd , offset);
            printf("(");
            printreg((Dwarf_Signed) reg_used, config_data);
            printf(") ");
        }
        if (print_type_title)
            printf("%s", "> ");
        break;
    case DW_EXPR_EXPRESSION:
        type_title = "expr";
        goto pexp2;
    case DW_EXPR_VAL_EXPRESSION:
        type_title = "valexpr";

        pexp2:
        if (print_type_title)
            printf("<%s ", type_title);
        printreg((Dwarf_Signed) rule_id, config_data);
        printf("=");
        printf("expr-block-len=%" DW_PR_DSd , offset);
        if (print_type_title)
            printf("%s", "> ");
        if (verbose) {
            char pref[40];

            strcpy(pref, "<");
            strcat(pref, type_title);
            strcat(pref, "bytes:");
            dump_block(pref, block_ptr, offset);
            printf("%s", "> ");
            if (verbose) {
                struct esb_s exprstring;
                esb_constructor(&exprstring);
                get_string_from_locs(dbg,
                    block_ptr,offset,addr_size,&exprstring);
                printf("<expr:%s>",esb_get_string(&exprstring));
                esb_destructor(&exprstring);
            }
        }
        break;
    default:
        printf("Internal error in libdwarf, value type %d\n",
            value_type);
        exit(1);
    }
    return;
}


/*  get all the data in .debug_frame (or .eh_frame).
    The '3' versions mean print using the dwarf3 new interfaces.
    The non-3 mean use the old interfaces.
    All combinations of requests are possible.  */
extern void
print_frames(Dwarf_Debug dbg, int print_debug_frame, int print_eh_frame,
    struct dwconf_s *config_data)
{
    Dwarf_Signed i;
    int fres = 0;
    Dwarf_Half address_size = 0;
    int framed = 0;
    void * map_lowpc_to_name = 0;

    current_section_id = DEBUG_FRAME;

    /*  The address size here will not be right for all frames.
        Only in DWARF4 is there a real address size known
        in the frame data itself.  If any DIE
        is known then a real address size can be gotten from
        dwarf_get_die_address_size(). */
    fres = dwarf_get_address_size(dbg, &address_size, &err);
    if (fres != DW_DLV_OK) {
        print_error(dbg, "dwarf_get_address_size", fres, err);
    }
    for (framed = 0; framed < 2; ++framed) {
        Dwarf_Cie *cie_data = NULL;
        Dwarf_Signed cie_element_count = 0;
        Dwarf_Fde *fde_data = NULL;
        Dwarf_Signed fde_element_count = 0;
        int frame_count = 0;
        int cie_count = 0;
        int all_cus_seen = 0;
        void * lowpcSet = 0;
        char *framename = 0;
        int silent_if_missing = 0;
        int is_eh = 0;

        if (framed == 0) {
            if (!print_debug_frame) {
                continue;
            }
            framename = ".debug_frame";
            /*  Big question here is how to print all the info?
                Can print the logical matrix, but that is huge,
                though could skip lines that don't change.
                Either that, or print the instruction statement program
                that describes the changes.  */
            fres = dwarf_get_fde_list(dbg, &cie_data, &cie_element_count,
                &fde_data, &fde_element_count, &err);
            if (check_harmless) {
                print_any_harmless_errors(dbg);
            }
        } else {
            if (!print_eh_frame) {
                continue;
            }
            is_eh = 1;
            /*  This is gnu g++ exceptions in a .eh_frame section. Which
                is just like .debug_frame except that the empty, or
                'special' CIE_id is 0, not -1 (to distinguish fde from
                cie). And the augmentation is "eh". As of egcs-1.1.2
                anyway. A non-zero cie_id is in a fde and is the
                difference between the fde address and the beginning of
                the cie it belongs to. This makes sense as this is
                intended to be referenced at run time, and is part of
                the running image. For more on augmentation strings, see
                libdwarf/dwarf_frame.c.  */

            /*  Big question here is how to print all the info?
                Can print the logical matrix, but that is huge,
                though could skip lines that don't change.
                Either that, or print the instruction statement program
                that describes the changes.  */
            silent_if_missing = 1;
            framename = ".eh_frame";
            fres = dwarf_get_fde_list_eh(dbg, &cie_data,
                &cie_element_count, &fde_data,
                &fde_element_count, &err);
            if (check_harmless) {
                print_any_harmless_errors(dbg);
            }
        }

        /* Do not print any frame info if in check mode */
        if (check_frames) {
            addr_map_destroy(lowpcSet);
            lowpcSet = 0;
            continue;
        }

        if (fres == DW_DLV_ERROR) {
            printf("\n%s\n", framename);
            print_error(dbg, "dwarf_get_fde_list", fres, err);
        } else if (fres == DW_DLV_NO_ENTRY) {
            if (!silent_if_missing) {
                printf("\n%s\n", framename);
            }
            /* no frame information */
        } else {                /* DW_DLV_OK */
            /* Do not print if in check mode */
            if (!check_frames_extended) {
                printf("\n%s\n", framename);
                printf("\nfde:\n");
            }

            for (i = 0; i < fde_element_count; i++) {
                print_one_fde(dbg, fde_data[i],
                    i, cie_data, cie_element_count,
                    address_size, is_eh, config_data,
                    &map_lowpc_to_name,
                    &lowpcSet,
                    &all_cus_seen);
                ++frame_count;
                if (frame_count >= break_after_n_units) {
                    break;
                }
            }
            /* Print the cie set. */
            if (verbose) {
                /* Do not print if in check mode */
                if (!check_frames_extended) {
                    printf("\ncie:\n");
                }
                for (i = 0; i < cie_element_count; i++) {
                    print_one_cie(dbg, cie_data[i], i, address_size,
                        config_data);
                    ++cie_count;
                    if (cie_count >= break_after_n_units) {
                        break;
                    }
                }
            }
            dwarf_fde_cie_list_dealloc(dbg, cie_data, cie_element_count,
                fde_data, fde_element_count);
        }
        addr_map_destroy(lowpcSet);
        lowpcSet = 0;
    }
    if (current_cu_die_for_print_frames) {
        dwarf_dealloc(dbg, current_cu_die_for_print_frames, DW_DLA_DIE);
        current_cu_die_for_print_frames = 0;
    }
    addr_map_destroy(map_lowpc_to_name);
    map_lowpc_to_name = 0;
}

