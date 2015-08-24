/*
  Copyright (C) 2000-2005 Silicon Graphics, Inc.  All Rights Reserved.
  Portions Copyright (C) 2007-2012 David Anderson. All Rights Reserved.
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
#include <stdarg.h>
#include <stdlib.h> /* For free() */
#include "dwarf_die_deliv.h"
#include "pro_encode_nm.h"


#define MINBUFLEN 1000
#define TRUE  1
#define FALSE 0

Dwarf_Bool
_dwarf_file_has_debug_fission_cu_index(Dwarf_Debug dbg)
{
    if(!dbg) {
        return FALSE;
    }
    if (dbg->de_cu_hashindex_data) {
        return TRUE;
    }
    return FALSE;
}
Dwarf_Bool
_dwarf_file_has_debug_fission_tu_index(Dwarf_Debug dbg)
{
    if(!dbg) {
        return FALSE;
    }
    if (dbg->de_tu_hashindex_data ) {
        return TRUE;
    }
    return FALSE;
}


Dwarf_Bool
_dwarf_file_has_debug_fission_index(Dwarf_Debug dbg)
{
    if(!dbg) {
        return FALSE;
    }
    if (dbg->de_cu_hashindex_data ||
        dbg->de_tu_hashindex_data) {
        return 1;
    }
    return FALSE;
}

/*  Given a form, and a pointer to the bytes encoding
    a value of that form, val_ptr, this function returns
    the length, in bytes, of a value of that form.
    When using this function, check for a return of 0
    a recursive DW_FORM_INDIRECT value.  */
int
_dwarf_get_size_of_val(Dwarf_Debug dbg,
    Dwarf_Unsigned form,
    Dwarf_Half cu_version,
    Dwarf_Half address_size,
    Dwarf_Small * val_ptr,
    int v_length_size,
    Dwarf_Unsigned *size_out,
    Dwarf_Error*error)
{
    Dwarf_Unsigned length = 0;
    Dwarf_Word leb128_length = 0;
    Dwarf_Unsigned form_indirect = 0;
    Dwarf_Unsigned ret_value = 0;

    switch (form) {

    /*  When we encounter a FORM here that
        we know about but forgot to enter here,
        we had better not just continue.
        Usually means we forgot to update this function
        when implementing form handling of a new FORM.
        Disaster results from using a bogus value,
        so generate error. */
    default:
        _dwarf_error(dbg,error,DW_DLE_DEBUG_FORM_HANDLING_INCOMPLETE);
        return DW_DLV_ERROR;


    case 0:  return 0;
    case DW_FORM_GNU_ref_alt:
    case DW_FORM_GNU_strp_alt:
        *size_out = v_length_size;
        return DW_DLV_OK;

    case DW_FORM_addr:
        if (address_size) {
            *size_out = address_size;
        } else {
            /* This should never happen, address_size should be set. */
            *size_out = dbg->de_pointer_size;
        }
        return DW_DLV_OK;
    case DW_FORM_ref_sig8:
        *size_out = 8;
        /* sizeof Dwarf_Sig8 */
        return DW_DLV_OK;

    /*  DWARF2 was wrong on the size of the attribute for
        DW_FORM_ref_addr.  We assume compilers are using the
        corrected DWARF3 text (for 32bit pointer target objects pointer and
        offsets are the same size anyway).
        It is clear (as of 2014) that for 64bit folks used
        the V2 spec in the way V2 was
        written, so the ref_addr has to account for that.*/
    case DW_FORM_ref_addr:
        if (cu_version == DW_CU_VERSION2) {
            *size_out = address_size;
        } else {
            *size_out = v_length_size;
        }
        return DW_DLV_OK;

    case DW_FORM_block1:
        *size_out =  *(Dwarf_Small *) val_ptr + 1;
        return DW_DLV_OK;

    case DW_FORM_block2:
        READ_UNALIGNED(dbg, ret_value, Dwarf_Unsigned,
            val_ptr, sizeof(Dwarf_Half));
        *size_out = ret_value + sizeof(Dwarf_Half);
        return DW_DLV_OK;

    case DW_FORM_block4:
        READ_UNALIGNED(dbg, ret_value, Dwarf_Unsigned,
            val_ptr, sizeof(Dwarf_ufixed));
        *size_out = ret_value + sizeof(Dwarf_ufixed);
        return DW_DLV_OK;

    case DW_FORM_data1:
        *size_out = 1;
        return DW_DLV_OK;

    case DW_FORM_data2:
        *size_out = 2;
        return DW_DLV_OK;

    case DW_FORM_data4:
        *size_out = 4;
        return DW_DLV_OK;

    case DW_FORM_data8:
        *size_out = 8;
        return DW_DLV_OK;

    case DW_FORM_string:
        *size_out = strlen((char *) val_ptr) + 1;
        return DW_DLV_OK;

    case DW_FORM_block:
    case DW_FORM_exprloc:
        length = _dwarf_decode_u_leb128(val_ptr, &leb128_length);
        *size_out = length + leb128_length;
        return DW_DLV_OK;

    case DW_FORM_flag_present:
        *size_out = 0;
        return DW_DLV_OK;

    case DW_FORM_flag:
        *size_out = 1;
        return DW_DLV_OK;

    case DW_FORM_sec_offset:
        /* If 32bit dwarf, is 4. Else is 64bit dwarf and is 8. */
        *size_out = v_length_size;
        return DW_DLV_OK;

    case DW_FORM_ref_udata:
        /*  Discard the decoded value, we just want the length
            of the value. */
        _dwarf_decode_u_leb128(val_ptr, &leb128_length);
        *size_out = leb128_length;
        return DW_DLV_OK;

    case DW_FORM_indirect:
        {
            Dwarf_Word indir_len = 0;
            int res = 0;
            Dwarf_Unsigned real_form_len = 0;

            form_indirect = _dwarf_decode_u_leb128(val_ptr, &indir_len);
            if (form_indirect == DW_FORM_indirect) {
                /* We are in big trouble: The true form
                    of DW_FORM_indirect is
                    DW_FORM_indirect? Nonsense. Should
                    never happen. */
                _dwarf_error(dbg,error,DW_DLE_NESTED_FORM_INDIRECT_ERROR);
                return DW_DLV_ERROR;
            }
            res = _dwarf_get_size_of_val(dbg,
                form_indirect,
                cu_version,
                address_size,
                val_ptr + indir_len,
                v_length_size,
                &real_form_len,
                error);
            if(res != DW_DLV_OK) {
                return res;
            }
            *size_out = indir_len + real_form_len;
            return DW_DLV_OK;
        }

    case DW_FORM_ref1:
        *size_out = 1;
        return DW_DLV_OK;

    case DW_FORM_ref2:
        *size_out = 2;
        return DW_DLV_OK;

    case DW_FORM_ref4:
        *size_out = 4;
        return DW_DLV_OK;

    case DW_FORM_ref8:
        *size_out = 8;
        return DW_DLV_OK;

    case DW_FORM_sdata:
        /*  Discard the decoded value, we just want the length
            of the value. */
        _dwarf_decode_s_leb128(val_ptr, &leb128_length);
        *size_out = (leb128_length);
        return DW_DLV_OK;


    case DW_FORM_addrx:
    case DW_FORM_GNU_addr_index:
    case DW_FORM_strx:
    case DW_FORM_GNU_str_index:
        _dwarf_decode_u_leb128(val_ptr, &leb128_length);
        *size_out = leb128_length;
        return DW_DLV_OK;

    case DW_FORM_strp:
        *size_out = v_length_size;
        return DW_DLV_OK;

    case DW_FORM_udata:
        /*  Discard the decoded value, we just want the length
            of the value. */
        _dwarf_decode_u_leb128(val_ptr, &leb128_length);
        *size_out = leb128_length;
        return DW_DLV_OK;
    }
}

/*  We allow an arbitrary number of HT_MULTIPLE entries
    before resizing.  It seems up to 20 or 30
    would work nearly as well.
    We could have a different resize multiple than 'resize now'
    test multiple, but for now we don't do that.  */
#define HT_MULTIPLE 8

/*  Copy the old entries, updating each to be in
    a new list.  Don't delete anything. Leave the
    htin with stale data. */
static void
copy_abbrev_table_to_new_table(Dwarf_Hash_Table htin,
  Dwarf_Hash_Table htout)
{
    Dwarf_Hash_Table_Entry entry_in = htin->tb_entries;
    unsigned entry_in_count = htin->tb_table_entry_count;
    Dwarf_Hash_Table_Entry entry_out = htout->tb_entries;
    unsigned entry_out_count = htout->tb_table_entry_count;
    unsigned k = 0;
    for (; k < entry_in_count; ++k,++entry_in) {
        Dwarf_Abbrev_List listent = entry_in->at_head;
        Dwarf_Abbrev_List nextlistent = 0;

        for (; listent ; listent = nextlistent) {
            unsigned newtmp = listent->ab_code;
            unsigned newhash = newtmp%entry_out_count;
            Dwarf_Hash_Table_Entry e;
            nextlistent = listent->ab_next;
            e = entry_out+newhash;
            /*  Move_entry_to_new_hash. This reverses the
                order of the entries, effectively, but
                that does not seem significant. */
            listent->ab_next = e->at_head;
            e->at_head = listent;

            htout->tb_total_abbrev_count++;
        }
    }
}

/*  This function returns a pointer to a Dwarf_Abbrev_List_s
    struct for the abbrev with the given code.  It puts the
    struct on the appropriate hash table.  It also adds all
    the abbrev between the last abbrev added and this one to
    the hash table.  In other words, the .debug_abbrev section
    is scanned sequentially from the top for an abbrev with
    the given code.  All intervening abbrevs are also put
    into the hash table.

    This function hashes the given code, and checks the chain
    at that hash table entry to see if a Dwarf_Abbrev_List_s
    with the given code exists.  If yes, it returns a pointer
    to that struct.  Otherwise, it scans the .debug_abbrev
    section from the last byte scanned for that CU till either
    an abbrev with the given code is found, or an abbrev code
    of 0 is read.  It puts Dwarf_Abbrev_List_s entries for all
    abbrev's read till that point into the hash table.  The
    hash table contains both a head pointer and a tail pointer
    for each entry.

    While the lists can move and entries can be moved between
    lists on reallocation, any given Dwarf_Abbrev_list entry
    never moves once allocated, so the pointer is safe to return.

    Returns NULL on error.  */
Dwarf_Abbrev_List
_dwarf_get_abbrev_for_code(Dwarf_CU_Context cu_context, Dwarf_Unsigned code)
{
    Dwarf_Debug dbg = cu_context->cc_dbg;
    Dwarf_Hash_Table hash_table_base = cu_context->cc_abbrev_hash_table;
    Dwarf_Hash_Table_Entry entry_base = 0;
    Dwarf_Hash_Table_Entry entry_cur = 0;
    Dwarf_Word hash_num = 0;
    Dwarf_Unsigned abbrev_code = 0;
    Dwarf_Unsigned abbrev_tag  = 0;
    Dwarf_Unsigned attr_name = 0;
    Dwarf_Unsigned attr_form = 0;

    Dwarf_Abbrev_List hash_abbrev_entry = 0;

    Dwarf_Abbrev_List inner_list_entry = 0;
    Dwarf_Hash_Table_Entry inner_hash_entry = 0;

    Dwarf_Byte_Ptr abbrev_ptr = 0;
    Dwarf_Byte_Ptr end_abbrev_ptr = 0;
    unsigned hashable_val = 0;

    if (!hash_table_base->tb_entries) {
        hash_table_base->tb_table_entry_count =  HT_MULTIPLE;
        hash_table_base->tb_total_abbrev_count= 0;
        hash_table_base->tb_entries =
            (struct  Dwarf_Hash_Table_Entry_s *)_dwarf_get_alloc(dbg,
            DW_DLA_HASH_TABLE_ENTRY,
            hash_table_base->tb_table_entry_count);
        if (!hash_table_base->tb_entries) {
            return NULL;
        }

    } else if (hash_table_base->tb_total_abbrev_count >
        ( hash_table_base->tb_table_entry_count * HT_MULTIPLE) ) {
        struct Dwarf_Hash_Table_s newht;
        /* Effectively multiplies by >= HT_MULTIPLE */
        newht.tb_table_entry_count =  hash_table_base->tb_total_abbrev_count;
        newht.tb_total_abbrev_count = 0;
        newht.tb_entries =
            (struct  Dwarf_Hash_Table_Entry_s *)_dwarf_get_alloc(dbg,
            DW_DLA_HASH_TABLE_ENTRY,
            newht.tb_table_entry_count);

        if (!newht.tb_entries) {
            return NULL;
        }
        /*  Copy the existing entries to the new table,
            rehashing each.  */
        copy_abbrev_table_to_new_table(hash_table_base, &newht);
        /*  Dealloc only the entries hash table array, not the lists
            of things pointed to by a hash table entry array. */
        dwarf_dealloc(dbg, hash_table_base->tb_entries,DW_DLA_HASH_TABLE_ENTRY);
        hash_table_base->tb_entries = 0;
        /*  Now overwrite the existing table descriptor with
            the new, newly valid, contents. */
        *hash_table_base = newht;
    } /* Else is ok as is, add entry */

    hashable_val = code;
    hash_num = hashable_val %
        hash_table_base->tb_table_entry_count;
    entry_base = hash_table_base->tb_entries;
    entry_cur  = entry_base + hash_num;

    /* Determine if the 'code' is the list of synonyms already. */
    for (hash_abbrev_entry = entry_cur->at_head;
        hash_abbrev_entry != NULL && hash_abbrev_entry->ab_code != code;
        hash_abbrev_entry = hash_abbrev_entry->ab_next);
    if (hash_abbrev_entry != NULL) {
        /*  This returns a pointer to an abbrev list entry, not
            the list itself. */
        return (hash_abbrev_entry);
    }

    if (cu_context->cc_last_abbrev_ptr) {
        abbrev_ptr = cu_context->cc_last_abbrev_ptr;
        end_abbrev_ptr = cu_context->cc_last_abbrev_endptr;
    } else {
        /*  This is ok because cc_abbrev_offset includes DWP
            offset if appropriate. */
        abbrev_ptr = dbg->de_debug_abbrev.dss_data +
            cu_context->cc_abbrev_offset;

        if (cu_context->cc_dwp_offsets.pcu_type)  {
            /*  In a DWP the abbrevs
                for this context are known quite precisely. */
            Dwarf_Unsigned size = 0;
            /* Ignore the offset returned. Already in cc_abbrev_offset. */
            _dwarf_get_dwp_extra_offset(&cu_context->cc_dwp_offsets,
                DW_SECT_ABBREV,&size);
            /*  ASSERT: size != 0 */
            end_abbrev_ptr = abbrev_ptr + size;
        } else {
            end_abbrev_ptr = abbrev_ptr +
                dbg->de_debug_abbrev.dss_size;
        }
    }

    /*  End of abbrev's as we are past the end entirely.
        THis can happen */
    if (abbrev_ptr > end_abbrev_ptr) {
        return (NULL);
    }
    /*  End of abbrev's for this cu, since abbrev code is 0. */
    if (*abbrev_ptr == 0) {
        return (NULL);
    }

    do {
        unsigned new_hashable_val = 0;
        DECODE_LEB128_UWORD(abbrev_ptr, abbrev_code);
        DECODE_LEB128_UWORD(abbrev_ptr, abbrev_tag);

        inner_list_entry = (Dwarf_Abbrev_List)
            _dwarf_get_alloc(cu_context->cc_dbg, DW_DLA_ABBREV_LIST, 1);
        if (inner_list_entry == NULL) {
            return (NULL);
        }

        new_hashable_val = abbrev_code;
        hash_num = new_hashable_val %
            hash_table_base->tb_table_entry_count;
        inner_hash_entry = entry_base + hash_num;
        /* Move_entry_to_new_hash */
        inner_list_entry->ab_next = inner_hash_entry->at_head;
        inner_hash_entry->at_head = inner_list_entry;

        hash_table_base->tb_total_abbrev_count++;

        inner_list_entry->ab_code = abbrev_code;
        inner_list_entry->ab_tag = abbrev_tag;
        inner_list_entry->ab_has_child = *(abbrev_ptr++);
        inner_list_entry->ab_abbrev_ptr = abbrev_ptr;

        /*  Cycle thru the abbrev content, ignoring the content except
            to find the end of the content. */
        do {
            DECODE_LEB128_UWORD(abbrev_ptr, attr_name);
            DECODE_LEB128_UWORD(abbrev_ptr, attr_form);
        } while (attr_name != 0 && attr_form != 0);

        /*  We may have fallen off the end of content,  that is not
            a botch in the section, as there is no rule that the last
            abbrev need have abbrev_code of 0. */
    } while ((abbrev_ptr < end_abbrev_ptr) &&
        *abbrev_ptr != 0 && abbrev_code != code);

    cu_context->cc_last_abbrev_ptr = abbrev_ptr;
    cu_context->cc_last_abbrev_endptr = end_abbrev_ptr;
    return (abbrev_code == code ? inner_list_entry : NULL);
}


/* return 1 if string ends before 'endptr' else
** return 0 meaning string is not properly terminated.
** Presumption is the 'endptr' pts to end of some dwarf section data.
*/
int
_dwarf_string_valid(void *startptr, void *endptr)
{

    char *start = startptr;
    char *end = endptr;

    while (start < end) {
        if (*start == 0) {
            return 1;           /* OK! */
        }
        ++start;
        ++end;
    }
    return 0;                   /* FAIL! bad string! */
}


/*  Return non-zero if the start/end are not valid for the
    die's section.
    Return 0 if valid*/
int
_dwarf_reference_outside_section(Dwarf_Die die,
    Dwarf_Small * startaddr,
    Dwarf_Small * pastend)
{
    Dwarf_Debug dbg = 0;
    Dwarf_CU_Context contxt = 0;
    struct Dwarf_Section_s *sec = 0;

    contxt = die->di_cu_context;
    dbg = contxt->cc_dbg;
    if (die->di_is_info) {
        sec = &dbg->de_debug_info;
    } else {
        sec = &dbg->de_debug_types;
    }
    if (startaddr < sec->dss_data) {
        return 1;
    }
    if (pastend > (sec->dss_data + sec->dss_size)) {
        return 1;
    }
    return 0;
}


/*
  A byte-swapping version of memcpy
  for cross-endian use.
  Only 2,4,8 should be lengths passed in.
*/
void *
_dwarf_memcpy_swap_bytes(void *s1, const void *s2, size_t len)
{
    void *orig_s1 = s1;
    unsigned char *targ = (unsigned char *) s1;
    const unsigned char *src = (const unsigned char *) s2;

    if (len == 4) {
        targ[3] = src[0];
        targ[2] = src[1];
        targ[1] = src[2];
        targ[0] = src[3];
    } else if (len == 8) {
        targ[7] = src[0];
        targ[6] = src[1];
        targ[5] = src[2];
        targ[4] = src[3];
        targ[3] = src[4];
        targ[2] = src[5];
        targ[1] = src[6];
        targ[0] = src[7];
    } else if (len == 2) {
        targ[1] = src[0];
        targ[0] = src[1];
    }
/* should NOT get below here: is not the intended use */
    else if (len == 1) {
        targ[0] = src[0];
    } else {
        memcpy(s1, s2, len);
    }

    return orig_s1;
}


/*  This calculation used to be sprinkled all over.
    Now brought to one place.

    We try to accurately compute the size of a cu header
    given a known cu header location ( an offset in .debug_info
    or debug_types).  */
/* ARGSUSED */
Dwarf_Unsigned
_dwarf_length_of_cu_header(Dwarf_Debug dbg, Dwarf_Unsigned offset,
    Dwarf_Bool is_info)
{
    int local_length_size = 0;
    int local_extension_size = 0;
    Dwarf_Unsigned length = 0;
    Dwarf_Unsigned final_size = 0;
    Dwarf_Small *cuptr =
        is_info? dbg->de_debug_info.dss_data + offset:
            dbg->de_debug_types.dss_data+ offset;

    READ_AREA_LENGTH(dbg, length, Dwarf_Unsigned,
        cuptr, local_length_size, local_extension_size);

    final_size = local_extension_size +  /* initial extension, if present */
        local_length_size +     /* Size of cu length field. */
        sizeof(Dwarf_Half) +    /* Size of version stamp field. */
        local_length_size +     /* Size of abbrev offset field. */
        sizeof(Dwarf_Small);    /* Size of address size field. */

    if (!is_info) {
        final_size +=
            /* type signature size */
            sizeof (Dwarf_Sig8) +
            /* type offset size */
            local_length_size;
    }
    return final_size;
}

/*  Pretend we know nothing about the CU
    and just roughly compute the result.  */
Dwarf_Unsigned
_dwarf_length_of_cu_header_simple(Dwarf_Debug dbg,
    Dwarf_Bool dinfo)
{
    Dwarf_Unsigned finalsize = 0;
    finalsize =  dbg->de_length_size +        /* Size of cu length field. */
        sizeof(Dwarf_Half) +    /* Size of version stamp field. */
        dbg->de_length_size +   /* Size of abbrev offset field. */
        sizeof(Dwarf_Small);    /* Size of address size field. */
    if (!dinfo) {
        finalsize +=
            /* type signature size */
            sizeof (Dwarf_Sig8) +
            /* type offset size */
            dbg->de_length_size;
    }
    return finalsize;
}

/*  Now that we delay loading .debug_info, we need to do the
    load in more places. So putting the load
    code in one place now instead of replicating it in multiple
    places.  */
int
_dwarf_load_debug_info(Dwarf_Debug dbg, Dwarf_Error * error)
{
    int res = DW_DLV_ERROR;
    if (dbg->de_debug_info.dss_data) {
        return DW_DLV_OK;
    }
    res = _dwarf_load_section(dbg, &dbg->de_debug_abbrev,error);
    if (res != DW_DLV_OK) {
        return res;
    }
    res = _dwarf_load_section(dbg, &dbg->de_debug_info, error);
    return res;
}
int
_dwarf_load_debug_types(Dwarf_Debug dbg, Dwarf_Error * error)
{
    int res = DW_DLV_ERROR;
    if (dbg->de_debug_types.dss_data) {
        return DW_DLV_OK;
    }
    res = _dwarf_load_section(dbg, &dbg->de_debug_abbrev,error);
    if (res != DW_DLV_OK) {
        return res;
    }
    res = _dwarf_load_section(dbg, &dbg->de_debug_types, error);
    return res;
}
void
_dwarf_free_abbrev_hash_table_contents(Dwarf_Debug dbg,Dwarf_Hash_Table hash_table)
{
    /*  A Hash Table is an array with tb_table_entry_count struct
        Dwarf_Hash_Table_s entries in the array. */
    unsigned hashnum = 0;
    for (; hashnum < hash_table->tb_table_entry_count; ++hashnum) {
        struct Dwarf_Abbrev_List_s *abbrev = 0;
        struct Dwarf_Abbrev_List_s *nextabbrev = 0;
        struct  Dwarf_Hash_Table_Entry_s *tb =  &hash_table->tb_entries[hashnum];

        abbrev = tb->at_head;
        for (; abbrev; abbrev = nextabbrev) {
            nextabbrev = abbrev->ab_next;
            dwarf_dealloc(dbg, abbrev, DW_DLA_ABBREV_LIST);
        }
    }
    /* Frees all the entries at once: an array. */
    dwarf_dealloc(dbg,hash_table->tb_entries,DW_DLA_HASH_TABLE_ENTRY);
}

/*
    If no die provided the size value returned might be wrong.
    If different compilation units have different address sizes
    this may not give the correct value in all contexts if the die
    pointer is NULL.
    If the Elf offset size != address_size
    (for example if address_size = 4 but recorded in elf64 object)
    this may not give the correct value in all contexts if the die
    pointer is NULL.
    If the die pointer is non-NULL (in which case it must point to
    a valid DIE) this will return the correct size.
*/
int
_dwarf_get_address_size(Dwarf_Debug dbg, Dwarf_Die die)
{
    Dwarf_CU_Context context = 0;
    Dwarf_Half addrsize = 0;
    if (!die) {
        return dbg->de_pointer_size;
    }
    context = die->di_cu_context;
    addrsize = context->cc_address_size;
    return addrsize;
}

/* Encode val as an unsigned LEB128. */
int dwarf_encode_leb128(Dwarf_Unsigned val, int *nbytes,
    char *space, int splen)
{
    /* Encode val as an unsigned LEB128. */
    return _dwarf_pro_encode_leb128_nm(val,nbytes,space,splen);
}

/* Encode val as a signed LEB128. */
int dwarf_encode_signed_leb128(Dwarf_Signed val, int *nbytes,
    char *space, int splen)
{
    /* Encode val as a signed LEB128. */
    return _dwarf_pro_encode_signed_leb128_nm(val,nbytes,space,splen);
}


struct  Dwarf_Printf_Callback_Info_s
dwarf_register_printf_callback( Dwarf_Debug dbg,
    struct  Dwarf_Printf_Callback_Info_s * newvalues)
{
    struct  Dwarf_Printf_Callback_Info_s oldval = dbg->de_printf_callback;
    if (!newvalues) {
        return oldval;
    }
    if( newvalues->dp_buffer_user_provided) {
        if( oldval.dp_buffer_user_provided) {
            /* User continues to control the buffer. */
            dbg->de_printf_callback = *newvalues;
        }else {
            /*  Switch from our control of buffer to user
                control.  */
            free(oldval.dp_buffer);
            oldval.dp_buffer = 0;
            dbg->de_printf_callback = *newvalues;
        }
    } else if (oldval.dp_buffer_user_provided){
        /* Switch from user control to our control */
        dbg->de_printf_callback = *newvalues;
        dbg->de_printf_callback.dp_buffer_len = 0;
        dbg->de_printf_callback.dp_buffer= 0;
    } else {
        /* User does not control the buffer. */
        dbg->de_printf_callback = *newvalues;
        dbg->de_printf_callback.dp_buffer_len =
            oldval.dp_buffer_len;
        dbg->de_printf_callback.dp_buffer =
            oldval.dp_buffer;
    }
    return oldval;
}


/* start is a minimum size, but may be zero. */
static void bufferdoublesize(struct  Dwarf_Printf_Callback_Info_s *bufdata)
{
    char *space = 0;
    unsigned int targlen = 0;
    if (bufdata->dp_buffer_len == 0) {
        targlen = MINBUFLEN;
    } else {
        targlen = bufdata->dp_buffer_len * 2;
        if (targlen < bufdata->dp_buffer_len) {
            /* Overflow, we cannot do this doubling. */
            return;
        }
    }
    /* Make big enough for a trailing NUL char. */
    space = (char *)malloc(targlen+1);
    if (!space) {
        /* Out of space, we cannot double it. */
        return;
    }
    free(bufdata->dp_buffer);
    bufdata->dp_buffer = space;
    bufdata->dp_buffer_len = targlen;
    return;
}

int
dwarf_printf(Dwarf_Debug dbg,
    const char * format,
    ...)
{
    va_list ap;
    int maxtries = 4;
    int tries = 0;
    struct Dwarf_Printf_Callback_Info_s *bufdata =
        &dbg->de_printf_callback;
    dwarf_printf_callback_function_type func = bufdata->dp_fptr;
    if (!func) {
        return 0;
    }
    if (!bufdata->dp_buffer) {
        bufferdoublesize(bufdata);
        if (!bufdata->dp_buffer) {
            /*  Something is wrong. Possibly caller
                set up callback wrong. */
            return 0;
        }
    }

    /*  Here we ensure (or nearly ensure) we expand
        the buffer when necessary, but not excessively
        (but only if we control the buffer size).  */
    while (1) {
        int olen = 0;
        tries++;
        va_start(ap,format);
        olen = vsnprintf(bufdata->dp_buffer,
            bufdata->dp_buffer_len, format,ap);
        /*  "The object ap may be passed as an argument to another
            function; if that function invokes the va_arg()
            macro with parameter ap, the value of ap in the calling
            function is unspecified and shall be passed to the va_end()
            macro prior to any further reference to ap."
            Single Unix Specification. */
        va_end(ap);
        if (olen > -1 && (long)olen < (long)bufdata->dp_buffer_len) {
            /*  The caller had better copy or dispose
                of the contents, as next-call will overwrite them. */
            func(bufdata->dp_user_pointer,bufdata->dp_buffer);
            return 0;
        }
        if (bufdata->dp_buffer_user_provided) {
            func(bufdata->dp_user_pointer,bufdata->dp_buffer);
            return 0;
        }
        if (tries > maxtries) {
            /* we did all we could, print what we have space for. */
            func(bufdata->dp_user_pointer,bufdata->dp_buffer);
            return 0;
        }
        bufferdoublesize(bufdata);
    }
    /* Not reached. */
    return 0;
}

