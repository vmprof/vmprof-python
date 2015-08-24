/*
  Copyright 2014-2014 David Anderson. All rights reserved.

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
#include "esb.h"

#include "print_sections.h"

static int
print_culist_array(Dwarf_Debug dbg,
    Dwarf_Gdbindex  gdbindex,
    Dwarf_Unsigned *cu_list_len,
    Dwarf_Error * err)
{
    Dwarf_Unsigned list_len = 0;
    Dwarf_Unsigned i;
    int res = dwarf_gdbindex_culist_array(gdbindex,
        &list_len,err);
    if (res != DW_DLV_OK) {
        print_error_and_continue(dbg,
            "dwarf_gdbindex_culist_array failed",res,*err);
        return res;
    }
    printf("  CU list. array length: %" DW_PR_DUu
        " format: [entry#] cuoffset culength\n",
        list_len);

    for( i  = 0; i < list_len; i++) {
        Dwarf_Unsigned cuoffset = 0;
        Dwarf_Unsigned culength = 0;
        res = dwarf_gdbindex_culist_entry(gdbindex,i,
            &cuoffset,&culength,err);
        if (res != DW_DLV_OK) {
            print_error_and_continue(dbg,
                "dwarf_gdbindex_culist_entry failed",res,*err);
            return res;
        }
        printf("    [%4" DW_PR_DUu "] 0x%"
            DW_PR_XZEROS DW_PR_DUx
            " 0x%" DW_PR_XZEROS DW_PR_DUx "\n",
            i,
            cuoffset,
            culength);
    }
    printf("\n");
    *cu_list_len = list_len;
    return DW_DLV_OK;
}

static int
print_types_culist_array(Dwarf_Debug dbg,
    Dwarf_Gdbindex  gdbindex,
    Dwarf_Error * err)
{
    Dwarf_Unsigned list_len = 0;
    Dwarf_Unsigned i;
    int res = dwarf_gdbindex_types_culist_array(gdbindex,
        &list_len,err);
    if (res != DW_DLV_OK) {
        print_error_and_continue(dbg,
            "dwarf_gdbindex_types_culist_array failed",res,*err);
        return res;
    }
    printf("  TU list. array length: %" DW_PR_DUu
        " format: [entry#] cuoffset culength signature\n",
        list_len);

    for( i  = 0; i < list_len; i++) {
        Dwarf_Unsigned cuoffset = 0;
        Dwarf_Unsigned culength = 0;
        Dwarf_Unsigned signature,
        res = dwarf_gdbindex_types_culist_entry(gdbindex,i,
            &cuoffset,&culength,
            &signature,
            err);
        if (res != DW_DLV_OK) {
            print_error_and_continue(dbg,
                "dwarf_gdbindex_culist_entry failed",res,*err);
            return res;
        }
        printf("    [%4" DW_PR_DUu "] 0x%"
            DW_PR_XZEROS DW_PR_DUx
            " 0x%" DW_PR_XZEROS DW_PR_DUx
            " 0x%" DW_PR_XZEROS DW_PR_DUx "\n",
            i,
            cuoffset,
            culength,
            signature);
    }
    printf("\n");
    return DW_DLV_OK;
}

static int
print_addressarea(Dwarf_Debug dbg,
    Dwarf_Gdbindex  gdbindex,
    Dwarf_Error * err)
{
    Dwarf_Unsigned list_len = 0;
    Dwarf_Unsigned i;
    int res = dwarf_gdbindex_addressarea(gdbindex,
        &list_len,err);
    if (res != DW_DLV_OK) {
        print_error_and_continue(dbg,
            "dwarf_gdbindex_addressarea failed",res,*err);
        return res;
    }
    printf("  Address table array length: %" DW_PR_DUu
        " format: [entry#] lowpc highpc cu-index\n",
        list_len);

    for( i  = 0; i < list_len; i++) {
        Dwarf_Unsigned lowpc = 0;
        Dwarf_Unsigned highpc = 0;
        Dwarf_Unsigned cu_index,
        res = dwarf_gdbindex_addressarea_entry(gdbindex,i,
            &lowpc,&highpc,
            &cu_index,
            err);
        if (res != DW_DLV_OK) {
            print_error_and_continue(dbg,
                "dwarf_gdbindex_addressarea_entry failed",res,*err);
            return res;
        }
        printf("    [%4" DW_PR_DUu "] 0x%"
            DW_PR_XZEROS DW_PR_DUx
            " 0x%" DW_PR_XZEROS DW_PR_DUx
            " %4" DW_PR_DUu "\n",
            i,
            lowpc,
            highpc,
            cu_index);
    }
    printf("\n");
    return DW_DLV_OK;
}


const char *kind_list[] = {
  "unknown(0)  ",
  "type(1)     ",
  "var-enum(2) ",
  "function(3) ",
  "other-sym(4)",
  "reserved(5) ",
  "function(6) ",
  "reserved(7) ",
};
const char *
get_kind(unsigned k)
{
    if (k <= 7) {
        return kind_list[k];
    }
    return "kind-erroneous";
}

/*  NOTE: Returns pointer to static local string.
    Use the returned pointer immediately or
    things will not work properly.  */
static char *
cu_index_string(Dwarf_Unsigned index,
    Dwarf_Unsigned culist_len)
{
    static char temp_space[40];
    Dwarf_Unsigned type_index = 0;
    if (index < culist_len) {
        snprintf(temp_space,sizeof(temp_space), "%4" DW_PR_DUu,index);
        return temp_space;
    }
    type_index = index-culist_len;
    snprintf(temp_space,sizeof(temp_space),
        "%4" DW_PR_DUu "(T%4" DW_PR_DUu ")",
        index,type_index);
    return temp_space;
}




static int
print_symtab_entry(Dwarf_Debug dbg,
    Dwarf_Gdbindex gdbindex,
    Dwarf_Unsigned index,
    Dwarf_Unsigned symnameoffset,
    Dwarf_Unsigned cuvecoffset,
    Dwarf_Unsigned culist_len,
    Dwarf_Error *err)
{
    int res = 0;
    const char *name = 0;
    Dwarf_Unsigned cuvec_len = 0;
    Dwarf_Unsigned ii = 0;

    if (symnameoffset == 0 && cuvecoffset == 0) {
        if (verbose > 1) {
            printf("        [%4" DW_PR_DUu "] \"empty-hash-entry\"\n", index);
        }
        return DW_DLV_OK;
    }
    res = dwarf_gdbindex_string_by_offset(gdbindex,
        symnameoffset,&name,err);
    if(res != DW_DLV_OK) {
        print_error_and_continue(dbg,
            "dwarf_gdbindex_string_by_offset failed",res,*err);
        return res;
    }
    res = dwarf_gdbindex_cuvector_length(gdbindex,
        cuvecoffset,&cuvec_len,err);
    if( res != DW_DLV_OK) {
        print_error_and_continue(dbg,
            "dwarf_gdbindex_cuvector_length failed",res,*err);
        return res;
    }
    if (verbose > 1) {
        printf("     [%4" DW_PR_DUu "]"
            "stroff 0x%"    DW_PR_XZEROS DW_PR_DUx
            " cuvecoff 0x%"    DW_PR_XZEROS DW_PR_DUx
            " cuveclen 0x%"    DW_PR_XZEROS DW_PR_DUx "\n",
            index,symnameoffset,cuvecoffset,cuvec_len);
    }
    for(ii = 0; ii < cuvec_len; ++ii ) {
        Dwarf_Unsigned attributes = 0;
        Dwarf_Unsigned cu_index = 0;
        Dwarf_Unsigned reserved1 = 0;
        Dwarf_Unsigned symbol_kind = 0;
        Dwarf_Unsigned is_static = 0;

        res = dwarf_gdbindex_cuvector_inner_attributes(
            gdbindex,cuvecoffset,ii,
            &attributes,err);
        if( res != DW_DLV_OK) {
            print_error_and_continue(dbg,
                "dwarf_gdbindex_cuvector_inner_attributes failed",res,*err);
            return res;
        }
        res = dwarf_gdbindex_cuvector_instance_expand_value(gdbindex,
            attributes, &cu_index,&reserved1,&symbol_kind, &is_static,
            err);
        if( res != DW_DLV_OK) {
            print_error_and_continue(dbg,
                "dwarf_gdbindex_cuvector_instance_expand_value failed",res,*err);
            return res;
        }
        /*  if cu_index is > the cu-count, then it  refers
            to a tu_index of  'cu_index - cu-count' */
        if (cuvec_len == 1) {
            printf("  [%4" DW_PR_DUu "]"
                "%s"
                " [%s %s] \"%s\"\n",
                index,
                cu_index_string(cu_index,culist_len),
                is_static?
                    "static ":
                    "global ",
                get_kind(symbol_kind),
                name);
        } else if (ii == 0) {
            printf("  [%4" DW_PR_DUu "] \"%s\"\n" ,
                index,
                name);
            printf("         %s [%s %s]\n",
                cu_index_string(cu_index,culist_len),
                is_static?
                    "static ":
                    "global ",
                get_kind(symbol_kind));
        }else{
            printf("         %s [%s %s]\n",
                cu_index_string(cu_index,culist_len),
                is_static?
                    "static ":
                    "global ",
                get_kind(symbol_kind));
        }
        if (verbose > 1) {
            printf("        [%4" DW_PR_DUu "]"
                "attr 0x%"    DW_PR_XZEROS DW_PR_DUx
                " cuindx 0x%"    DW_PR_XZEROS DW_PR_DUx
                " kind 0x%"    DW_PR_XZEROS DW_PR_DUx
                " static 0x%"    DW_PR_XZEROS DW_PR_DUx "\n",
                ii,attributes,cu_index,symbol_kind,is_static);
        }

    }
    return DW_DLV_OK;
}

static int
print_symboltable(Dwarf_Debug dbg,
    Dwarf_Gdbindex  gdbindex,
    Dwarf_Unsigned culist_len,
    Dwarf_Error * err)
{
    Dwarf_Unsigned list_len = 0;
    Dwarf_Unsigned i;
    int res = dwarf_gdbindex_symboltable_array(gdbindex,
        &list_len,err);
    if (res != DW_DLV_OK) {
        print_error_and_continue(dbg,
            "dwarf_gdbindex_symboltable failed",res,*err);
        return res;
    }
    printf("\n  Symbol table: length %" DW_PR_DUu
        " format: [entry#] symindex cuindex [type] \"name\" or \n",
        list_len);
    printf("                          "
        " format: [entry#]  \"name\" , list of  cuindex [type]\n");

    for( i  = 0; i < list_len; i++) {
        Dwarf_Unsigned symnameoffset = 0;
        Dwarf_Unsigned cuvecoffset = 0;
        res = dwarf_gdbindex_symboltable_entry(gdbindex,i,
            &symnameoffset,&cuvecoffset,
            err);
        if (res != DW_DLV_OK) {
            print_error_and_continue(dbg,
                "dwarf_gdbindex_symboltable_entry failed",res,*err);
            return res;
        }
        res = print_symtab_entry(dbg,gdbindex,i,symnameoffset,cuvecoffset,
            culist_len,err);
        if (res != DW_DLV_OK) {
            return res;
        }
    }
    printf("\n");
    return DW_DLV_OK;
}




extern void
print_gdb_index(Dwarf_Debug dbg)
{
    Dwarf_Gdbindex  gdbindex = 0;
    Dwarf_Unsigned version = 0;
    Dwarf_Unsigned cu_list_offset = 0;
    Dwarf_Unsigned types_cu_list_offset = 0;
    Dwarf_Unsigned address_area_offset = 0;
    Dwarf_Unsigned symbol_table_offset = 0;
    Dwarf_Unsigned constant_pool_offset = 0;
    Dwarf_Unsigned section_size = 0;
    Dwarf_Unsigned unused = 0;
    Dwarf_Error error = 0;
    const char *section_name = 0;
    Dwarf_Unsigned culist_len = 0;

    int res = 0;
    current_section_id = DEBUG_GDB_INDEX;
    res = dwarf_gdbindex_header(dbg, &gdbindex,
        &version,
        &cu_list_offset,
        &types_cu_list_offset,
        &address_area_offset,
        &symbol_table_offset,
        &constant_pool_offset,
        &section_size,
        &unused,
        &section_name,
        &error);

    if (!do_print_dwarf) {
        return;
    }
    if(res == DW_DLV_NO_ENTRY) {
        /*  Silently! The section is rare so lets
            say nothing. */
        return;
    }
    if (!section_name || !*section_name) {
        section_name = ".gdb_index";
    }
    printf("\n%s\n",section_name);
    if( res == DW_DLV_ERROR) {
        print_error(dbg,"dwarf_gdbindex_header",res,error);
        return;
    }

    printf("  Version             : "
        "0x%" DW_PR_XZEROS DW_PR_DUx  "\n",
        version);
    printf("  CU list offset      : "
        "0x%" DW_PR_XZEROS DW_PR_DUx "\n",
        cu_list_offset);
    printf("  Address area offset : "
        "0x%" DW_PR_XZEROS DW_PR_DUx "\n",
        types_cu_list_offset);
    printf("  Symbol table offset : "
        "0x%" DW_PR_XZEROS DW_PR_DUx "\n",
        address_area_offset);
    printf("  Constant pool offset: "
        "0x%" DW_PR_XZEROS DW_PR_DUx "\n",
        constant_pool_offset);
    printf("  section size        : "
        "0x%" DW_PR_XZEROS DW_PR_DUx "\n",
        section_size);


    res = print_culist_array(dbg,gdbindex,&culist_len,&error);
    if (res != DW_DLV_OK) {
        return;
    }
    res = print_types_culist_array(dbg,gdbindex,&error);
    if (res != DW_DLV_OK) {
        return;
    }
    res = print_addressarea(dbg,gdbindex,&error);
    if (res != DW_DLV_OK) {
        return;
    }
    res = print_symboltable(dbg,gdbindex,culist_len,&error);
    if (res != DW_DLV_OK) {
        return;
    }
}

