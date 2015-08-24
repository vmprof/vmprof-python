/*
  Copyright (C) 2000-2006 Silicon Graphics, Inc.  All Rights Reserved.
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
#ifdef HAVE_ELF_H
#include <elf.h>
#endif
#include <stdio.h>
#include "dwarf_die_deliv.h"

#define FALSE 0
#define TRUE 1
static int
dwarf_next_cu_header_internal(Dwarf_Debug dbg,
    Dwarf_Bool is_info,
    Dwarf_Unsigned * cu_header_length,
    Dwarf_Half * version_stamp,
    Dwarf_Unsigned * abbrev_offset,
    Dwarf_Half * address_size,
    Dwarf_Half * offset_size,
    Dwarf_Half * extension_size,
    Dwarf_Sig8 * signature,
    Dwarf_Unsigned *typeoffset,
    Dwarf_Unsigned * next_cu_offset,
    Dwarf_Half     * header_cu_type,
    Dwarf_Error * error);

/*  New October 2011.  Enables client code to know if
    it is a debug_info or debug_types context. */
Dwarf_Bool
dwarf_get_die_infotypes_flag(Dwarf_Die die)
{
    return die->di_is_info;
}

/*
    For a given Dwarf_Debug dbg, this function checks
    if a CU that includes the given offset has been read
    or not.  If yes, it returns the Dwarf_CU_Context
    for the CU.  Otherwise it returns NULL.  Being an
    internal routine, it is assumed that a valid dbg
    is passed.

    **This is a sequential search.  May be too slow.

    If debug_info and debug_abbrev not loaded, this will
    wind up returning NULL. So no need to load before calling
    this.
*/
static Dwarf_CU_Context
_dwarf_find_CU_Context(Dwarf_Debug dbg, Dwarf_Off offset,Dwarf_Bool is_info)
{
    Dwarf_CU_Context cu_context = 0;
    Dwarf_Debug_InfoTypes dis = is_info? &dbg->de_info_reading:
        &dbg->de_types_reading;

    if (offset >= dis->de_last_offset)
        return (NULL);

    if (dis->de_cu_context != NULL &&
        dis->de_cu_context->cc_next != NULL &&
        dis->de_cu_context->cc_next->cc_debug_offset == offset) {

        return (dis->de_cu_context->cc_next);
    }

    if (dis->de_cu_context != NULL &&
        dis->de_cu_context->cc_debug_offset <= offset) {

        for (cu_context = dis->de_cu_context;
            cu_context != NULL; cu_context = cu_context->cc_next) {

            if (offset >= cu_context->cc_debug_offset &&
                offset < cu_context->cc_debug_offset +
                cu_context->cc_length + cu_context->cc_length_size
                + cu_context->cc_extension_size) {

                return (cu_context);
            }
        }
    }

    for (cu_context = dis->de_cu_context_list;
        cu_context != NULL; cu_context = cu_context->cc_next) {

        if (offset >= cu_context->cc_debug_offset &&
            offset < cu_context->cc_debug_offset +
            cu_context->cc_length + cu_context->cc_length_size
            + cu_context->cc_extension_size) {

            return (cu_context);
        }
    }

    return (NULL);
}


/*  This routine checks the dwarf_offdie() list of
    CU contexts for the right CU context.  */
static Dwarf_CU_Context
_dwarf_find_offdie_CU_Context(Dwarf_Debug dbg, Dwarf_Off offset,
    Dwarf_Bool is_info)
{
    Dwarf_CU_Context cu_context = 0;
    Dwarf_Debug_InfoTypes dis = is_info? &dbg->de_info_reading:
        &dbg->de_types_reading;

    for (cu_context = dis->de_offdie_cu_context;
        cu_context != NULL; cu_context = cu_context->cc_next)

        if (offset >= cu_context->cc_debug_offset &&
            offset < cu_context->cc_debug_offset +
            cu_context->cc_length + cu_context->cc_length_size
            + cu_context->cc_extension_size)

            return (cu_context);

    return (NULL);
}

int
dwarf_get_debugfission_for_die(Dwarf_Die die,
    struct Dwarf_Debug_Fission_Per_CU_s *fission_out,
    Dwarf_Error *error)
{
    Dwarf_CU_Context context = die->di_cu_context;
    Dwarf_Debug dbg = context->cc_dbg;
    struct Dwarf_Debug_Fission_Per_CU_s * percu = 0;

    if (!_dwarf_file_has_debug_fission_index(dbg)) {
        return DW_DLV_NO_ENTRY;
    }

    if (context->cc_unit_type == DW_UT_type ) {
        if (!_dwarf_file_has_debug_fission_tu_index(dbg)) {
            return DW_DLV_NO_ENTRY;
        }
    } else {
        if (!_dwarf_file_has_debug_fission_cu_index(dbg)) {
            return DW_DLV_NO_ENTRY;
        }
    }
    percu = &context->cc_dwp_offsets;
    if (!percu->pcu_type) {
        return DW_DLV_NO_ENTRY;
    }
    *fission_out = *percu;
    return DW_DLV_OK;
}

/* ASSERT: whichone is a DW_SECT* macro value. */
Dwarf_Unsigned
_dwarf_get_dwp_extra_offset(struct Dwarf_Debug_Fission_Per_CU_s* dwp,
    unsigned whichone, Dwarf_Unsigned * size)
{
    Dwarf_Unsigned abbrevoff = 0;
    if (!dwp->pcu_type) {
        return 0;
    }
    abbrevoff = dwp->pcu_offset[whichone];
    *size = dwp->pcu_size[whichone];
    return abbrevoff;
}


/*  _dwarf_get_fission_addition_die return DW_DLV_OK etc instead.
*/
int
_dwarf_get_fission_addition_die(Dwarf_Die die, int dw_sect_index,
   Dwarf_Unsigned *offset,
   Dwarf_Unsigned *size,
   Dwarf_Error *error)
{
    /* We do not yet know the DIE hash, so we cannot use it
        to identify the offset. */
    Dwarf_CU_Context context = die->di_cu_context;
    Dwarf_Unsigned dwpadd = 0;
    Dwarf_Unsigned dwpsize = 0;

    dwpadd =  _dwarf_get_dwp_extra_offset(&context->cc_dwp_offsets,
        DW_SECT_LINE,&dwpsize);
    *offset = dwpadd;
    *size = dwpsize;
    return DW_DLV_OK;
}

#if 0
/*  FIXME: eturn DW_DLV_OK etc instead.
    FIXME: check dw_sect_index.
    FIXME: verify hash
*/

Dwarf_Unsigned
_dwarf_get_fission_addition(Dwarf_Debug dbg,
   int cu_is_info,Dwarf_Off cu_offset,int dw_sect_index)
{
    /* We may not yet know the CU DIE hash, so we cannot use it
        to identify the offset. */
    struct Dwarf_Fission_Offsets_s *fissiondata = 0;
    unsigned info_index = 0;
    struct Dwarf_Fission_Per_CU_s *percu_base = 0;
    unsigned u = 0;
    Dwarf_Unsigned info_offset = 0;

    fissiondata = cu_is_info?
        &dbg->de_cu_fission_data: &dbg->de_tu_fission_data;
    if (fissiondata->dfo_version == 0) {
        /*  Not  a DWP debugfission file.
            Because we have some .debug_info or _types
            without any fission data for existing dbg data
            there is no fission data at all. */
        return 0;
    }
    info_index = cu_is_info?DW_SECT_INFO:DW_SECT_TYPES;
    if (dw_sect_index == DW_SECT_INFO && !cu_is_info ) {
        dw_sect_index = info_index;
    }
    percu_base = fissiondata->dfo_per_cu;
    /*  The following is sequential search.
        We should probably use a faster search
        mechanism as a dwp can have thousands
        of units.   */
    for (u=0; u < fissiondata->dfo_entries; u++) {
        Dwarf_Unsigned fissoff = 0;
        struct Dwarf_Fission_Per_CU_s *percu = percu_base+u;
        Dwarf_Unsigned baseoff = percu->dfp_offsets[info_index].dfs_offset;
        /*  FIXME: check dw_sect_index? */
        if (cu_offset !=  baseoff) {
            continue;
        }
        fissoff = percu->dfp_offsets[dw_sect_index].dfs_offset;
        return fissoff;
    }
    return 0;
}
#endif



/*  This function is used to create a CU Context for
    a compilation-unit that begins at offset in
    .debug_info.  The CU Context is attached to the
    list of CU Contexts for this dbg.  It is assumed
    that the CU at offset has not been read before,
    and so do not call this routine before making
    sure of this with _dwarf_find_CU_Context().
    Returns NULL on error.  As always, being an
    internal routine, assumes a good dbg.

    The offset argument is global offset, the offset
    in the section, irrespective of CUs.
    The offset has the DWP Package File offset built in
    as it comes from the actual section.

    max_cu_local_offset is a local offset in this CU.
    So zero of this field is immediately following the length
    field of the CU header. so max_cu_local_offset is
    identical to the CU length field.
    max_cu_global_offset is the offset one-past the end
    of this entire CU.

    This function must always set a dwarf error code
    before returning NULL. Always.  */
static Dwarf_CU_Context
_dwarf_make_CU_Context(Dwarf_Debug dbg,
    Dwarf_Off offset,Dwarf_Bool is_info,Dwarf_Error * error)
{
    Dwarf_CU_Context cu_context = 0;
    Dwarf_Unsigned length = 0;
    Dwarf_Signed abbrev_offset = 0;
    Dwarf_Unsigned typeoffset = 0;
    Dwarf_Sig8 signaturedata;
    Dwarf_Unsigned types_extra_len = 0;
    Dwarf_Unsigned max_cu_local_offset =  0;
    Dwarf_Unsigned max_cu_global_offset =  0;
    Dwarf_Byte_Ptr cu_ptr = 0;
    int local_extension_size = 0;
    int local_length_size = 0;
    Dwarf_Debug_InfoTypes dis = is_info? &dbg->de_info_reading:
        &dbg->de_types_reading;
    Dwarf_Unsigned section_size = is_info? dbg->de_debug_info.dss_size:
        dbg->de_debug_types.dss_size;
    int unit_type = 0;
    int version = 0;
    int is_type_tu = 0;

    cu_context =
        (Dwarf_CU_Context) _dwarf_get_alloc(dbg, DW_DLA_CU_CONTEXT, 1);
    if (cu_context == NULL) {
        _dwarf_error(dbg, error, DW_DLE_ALLOC_FAIL);
        return (NULL);
    }
    cu_context->cc_dbg = dbg;
    cu_context->cc_is_info = is_info;

    {
        Dwarf_Small *dataptr = is_info? dbg->de_debug_info.dss_data:
            dbg->de_debug_types.dss_data;
        cu_ptr = (Dwarf_Byte_Ptr) (dataptr+offset);
    }

    /* READ_AREA_LENGTH updates cu_ptr for consumed bytes */
    READ_AREA_LENGTH(dbg, length, Dwarf_Unsigned,
        cu_ptr, local_length_size, local_extension_size);
    cu_context->cc_length_size = local_length_size;
    cu_context->cc_extension_size = local_extension_size;


    cu_context->cc_length = length;
    max_cu_local_offset =  length;
    max_cu_global_offset =  offset + length +
        local_extension_size + local_length_size;

    READ_UNALIGNED(dbg, cu_context->cc_version_stamp, Dwarf_Half,
        cu_ptr, sizeof(Dwarf_Half));
    version = cu_context->cc_version_stamp;
    cu_ptr += sizeof(Dwarf_Half);
    if (version == DW_CU_VERSION5) {
        unsigned char ub = 0;
        READ_UNALIGNED(dbg, ub, unsigned char,
            cu_ptr, sizeof(ub));
        cu_ptr += sizeof(ub);
        unit_type = ub;
        if (unit_type != DW_UT_compile && unit_type != DW_UT_partial
            && unit_type != DW_UT_type) {
            _dwarf_error(dbg, error, DW_DLE_CU_UT_TYPE_ERROR);
            return NULL;
        }
    } else {
        /*  We don't know if it is or DW_UT_partial or not. */
        unit_type = is_info?DW_UT_compile:DW_UT_type;
    }

    READ_UNALIGNED(dbg, abbrev_offset, Dwarf_Unsigned,
        cu_ptr, local_length_size);

    cu_ptr += local_length_size;

    /*  In a dwp context, this offset is incomplete.
        We need to add in the base from the .debug_cu_index
        or .debug_tu_index . Done below */
    cu_context->cc_abbrev_offset = abbrev_offset;

    cu_context->cc_address_size = *(Dwarf_Small *) cu_ptr;
    ++cu_ptr;



    if (cu_context->cc_address_size  > sizeof(Dwarf_Addr)) {
        _dwarf_error(dbg, error, DW_DLE_CU_ADDRESS_SIZE_BAD);
        return (NULL);
    }

    is_type_tu = FALSE;
    if (!is_info ||
        (version == DW_CU_VERSION5 && unit_type == DW_UT_type )) {
        is_type_tu = TRUE;
    }
    if (is_type_tu) {
        /* types CU headers have extra header bytes. */
        types_extra_len = sizeof(signaturedata) + local_length_size;
    }
    if ((length < (CU_VERSION_STAMP_SIZE + local_length_size +
        CU_ADDRESS_SIZE_SIZE + types_extra_len)) ||
        (max_cu_global_offset > section_size)) {

        dwarf_dealloc(dbg, cu_context, DW_DLA_CU_CONTEXT);
        _dwarf_error(dbg, error, DW_DLE_CU_LENGTH_ERROR);
        return (NULL);
    }

    if (cu_context->cc_version_stamp != DW_CU_VERSION2
        && cu_context->cc_version_stamp != DW_CU_VERSION3
        && cu_context->cc_version_stamp != DW_CU_VERSION4
        && cu_context->cc_version_stamp != DW_CU_VERSION5) {
        dwarf_dealloc(dbg, cu_context, DW_DLA_CU_CONTEXT);
        _dwarf_error(dbg, error, DW_DLE_VERSION_STAMP_ERROR);
        return (NULL);
    }
    if (is_type_tu) {
        if (version != DW_CU_VERSION4 &&
            version != DW_CU_VERSION5) {
            dwarf_dealloc(dbg, cu_context, DW_DLA_CU_CONTEXT);
            /* Error name  misleading, version 5 has types too. */
            _dwarf_error(dbg, error, DW_DLE_DEBUG_TYPES_ONLY_DWARF4);
            return (NULL);
        }
        /*  Now read the debug_types extra header fields of
            the signature (8 bytes) and the typeoffset. */
        memcpy(&signaturedata,cu_ptr,sizeof(signaturedata));
        cu_context->cc_type_signature_present = TRUE;
        cu_ptr += sizeof(signaturedata);
        READ_UNALIGNED(dbg, typeoffset, Dwarf_Unsigned,
            cu_ptr, local_length_size);
        cu_context->cc_type_signature = signaturedata;
        cu_context->cc_type_signature_offset = typeoffset;
        {
            if (typeoffset >= max_cu_local_offset) {
                dwarf_dealloc(dbg, cu_context, DW_DLA_CU_CONTEXT);
                _dwarf_error(dbg, error, DW_DLE_DEBUG_TYPEOFFSET_BAD);
                return (NULL);
            }
        }
    }
    cu_context->cc_unit_type = unit_type;
    if (is_type_tu) {
        if (_dwarf_file_has_debug_fission_tu_index(dbg) ){
            int resdf = 0;
            resdf = dwarf_get_debugfission_for_key(dbg,
                &signaturedata,
                "tu",
                &cu_context->cc_dwp_offsets,
                error);
            if (resdf != DW_DLV_OK) {
                dwarf_dealloc(dbg, cu_context, DW_DLA_CU_CONTEXT);
                _dwarf_error(dbg, error,
                    DW_DLE_MISSING_REQUIRED_TU_OFFSET_HASH);
                return NULL;
            }
        }
    } else {
        if (_dwarf_file_has_debug_fission_cu_index(dbg) ){
            int resdf = 0;
            resdf = _dwarf_get_debugfission_for_offset(dbg,
                offset,
                &cu_context->cc_dwp_offsets,
                error);
            if (resdf != DW_DLV_OK) {
                dwarf_dealloc(dbg, cu_context, DW_DLA_CU_CONTEXT);
                _dwarf_error(dbg, error,
                    DW_DLE_MISSING_REQUIRED_CU_OFFSET_HASH);
                return NULL;
            }
            /*  Eventually we will see the DW_AT_dwo_id
                or DW_AT_GNU_dwo_id
                and we should check against  this signature
                at that time.  */
            cu_context->cc_type_signature =
                cu_context->cc_dwp_offsets.pcu_hash;
        }
    }
    if (cu_context->cc_dwp_offsets.pcu_type) {
        /*  We need to update certain offsets as this is a package file.
            This is to reflect how DWP files are organized. */
        Dwarf_Unsigned absize = 0;
        Dwarf_Unsigned aboff = 0;
        aboff =  _dwarf_get_dwp_extra_offset(&cu_context->cc_dwp_offsets,
            DW_SECT_ABBREV, &absize);
        cu_context->cc_abbrev_offset +=  aboff;
        abbrev_offset = cu_context->cc_abbrev_offset;
    }

    if ((Dwarf_Unsigned)abbrev_offset >= dbg->de_debug_abbrev.dss_size) {
        dwarf_dealloc(dbg, cu_context, DW_DLA_CU_CONTEXT);
        _dwarf_error(dbg, error, DW_DLE_ABBREV_OFFSET_ERROR);
        return (NULL);
    }

    cu_context->cc_abbrev_hash_table =
        (Dwarf_Hash_Table) _dwarf_get_alloc(dbg, DW_DLA_HASH_TABLE, 1);
    if (cu_context->cc_abbrev_hash_table == NULL) {
        _dwarf_error(dbg, error, DW_DLE_ALLOC_FAIL);
        return (NULL);
    }

    cu_context->cc_debug_offset = offset;

    dis->de_last_offset = max_cu_global_offset;

    if (dis->de_cu_context_list == NULL) {
        dis->de_cu_context_list = cu_context;
        dis->de_cu_context_list_end = cu_context;
    } else {
        dis->de_cu_context_list_end->cc_next = cu_context;
        dis->de_cu_context_list_end = cu_context;
    }
    return (cu_context);
}

static int
reloc_incomplete(Dwarf_Error err)
{
    int e = dwarf_errno(err);
    if (e == DW_DLE_RELOC_MISMATCH_INDEX       ||
        e == DW_DLE_RELOC_MISMATCH_RELOC_INDEX  ||
        e == DW_DLE_RELOC_MISMATCH_STRTAB_INDEX ||
        e == DW_DLE_RELOC_SECTION_MISMATCH      ||
        e == DW_DLE_RELOC_SECTION_MISSING_INDEX  ||
        e == DW_DLE_RELOC_SECTION_LENGTH_ODD     ||
        e == DW_DLE_RELOC_SECTION_PTR_NULL        ||
        e == DW_DLE_RELOC_SECTION_MALLOC_FAIL      ||
        e == DW_DLE_RELOC_SECTION_SYMBOL_INDEX_BAD  ) {
        return 1;
    }
    return 0;
}



/*  Returns offset of next compilation-unit thru next_cu_offset
    pointer.
    It sequentially moves from one
    cu to the next.  The current cu is recorded
    internally by libdwarf.

    The _b form is new for DWARF4 adding new returned fields.  */
int
dwarf_next_cu_header(Dwarf_Debug dbg,
    Dwarf_Unsigned * cu_header_length,
    Dwarf_Half * version_stamp,
    Dwarf_Unsigned * abbrev_offset,
    Dwarf_Half * address_size,
    Dwarf_Unsigned * next_cu_offset,
    Dwarf_Error * error)
{
    Dwarf_Bool is_info = true;
    Dwarf_Half header_type = 0;
    return dwarf_next_cu_header_internal(dbg,
        is_info,
        cu_header_length,
        version_stamp,
        abbrev_offset,
        address_size,
        0,0,0,0,
        next_cu_offset,
        &header_type,
        error);
}
int
dwarf_next_cu_header_b(Dwarf_Debug dbg,
    Dwarf_Unsigned * cu_header_length,
    Dwarf_Half * version_stamp,
    Dwarf_Unsigned * abbrev_offset,
    Dwarf_Half * address_size,
    Dwarf_Half * offset_size,
    Dwarf_Half * extension_size,
    Dwarf_Unsigned * next_cu_offset,
    Dwarf_Error * error)
{
    Dwarf_Bool is_info = true;
    Dwarf_Half header_type = 0;
    return dwarf_next_cu_header_internal(dbg,
        is_info,
        cu_header_length,
        version_stamp,
        abbrev_offset,
        address_size,
        offset_size,extension_size,
        0,0,
        next_cu_offset,
        &header_type,
        error);
}

int
dwarf_next_cu_header_c(Dwarf_Debug dbg,
    Dwarf_Bool is_info,
    Dwarf_Unsigned * cu_header_length,
    Dwarf_Half * version_stamp,
    Dwarf_Unsigned * abbrev_offset,
    Dwarf_Half * address_size,
    Dwarf_Half * offset_size,
    Dwarf_Half * extension_size,
    Dwarf_Sig8 * signature,
    Dwarf_Unsigned * typeoffset,
    Dwarf_Unsigned * next_cu_offset,
    Dwarf_Error * error)
{
    Dwarf_Half header_type = 0;
    return dwarf_next_cu_header_internal(dbg,
        is_info,
        cu_header_length,
        version_stamp,
        abbrev_offset,
        address_size,
        offset_size,
        extension_size,
        signature,
        typeoffset,
        next_cu_offset,
        &header_type,
        error);
}
int
dwarf_next_cu_header_d(Dwarf_Debug dbg,
    Dwarf_Bool is_info,
    Dwarf_Unsigned * cu_header_length,
    Dwarf_Half * version_stamp,
    Dwarf_Unsigned * abbrev_offset,
    Dwarf_Half * address_size,
    Dwarf_Half * offset_size,
    Dwarf_Half * extension_size,
    Dwarf_Sig8 * signature,
    Dwarf_Unsigned * typeoffset,
    Dwarf_Unsigned * next_cu_offset,
    Dwarf_Half * header_cu_type,
    Dwarf_Error * error)
{
    return dwarf_next_cu_header_internal(dbg,
        is_info,
        cu_header_length,
        version_stamp,
        abbrev_offset,
        address_size,
        offset_size,
        extension_size,
        signature,
        typeoffset,
        next_cu_offset,
        header_cu_type,
        error);
}



/* If sig_present_return not set TRUE here
    then something must be wrong. ??
    Compiler bug?
*/
static int
find_context_base_fields_dwo(Dwarf_Debug dbg,
    Dwarf_Die cudie,
    Dwarf_Sig8 *sig_return,
    Dwarf_Bool *sig_present_return,
    Dwarf_Unsigned *str_offsets_base_return,
    Dwarf_Bool *str_offsets_base_present_return,
    Dwarf_Error* error)
{
    Dwarf_Sig8 signature;
    Dwarf_Bool sig_present = FALSE;
    Dwarf_Unsigned str_offsets_base = 0;
    Dwarf_Bool str_offsets_base_present = FALSE;
    Dwarf_Bool hasit = 0;
    int boolres = 0;

    boolres = dwarf_hasattr(cudie,DW_AT_dwo_id,
        &hasit,error);
    if (boolres == DW_DLV_OK && !hasit) {
        boolres = dwarf_hasattr(cudie,DW_AT_GNU_dwo_id,
            &hasit,error);
    }
    if (boolres == DW_DLV_OK) {
        if (hasit) {
            Dwarf_Attribute * alist = 0;
            Dwarf_Signed      atcount = 0;
            int alres = 0;

            alres = dwarf_attrlist(cudie, &alist,
                &atcount,error);
            if(alres == DW_DLV_OK) {
                Dwarf_Signed i = 0;
                for(i = 0;  i < atcount; ++i) {
                    Dwarf_Half attrnum;
                    int ares = 0;
                    Dwarf_Attribute attr = alist[i];
                    ares = dwarf_whatattr(attr,&attrnum,error);
                    if (ares == DW_DLV_OK) {
                        if (attrnum == DW_AT_dwo_id ||
                            attrnum == DW_AT_GNU_dwo_id ) {
                            int sres = 0;
                            sres = dwarf_formsig8_const(attr,
                                &signature,error);
                            if(sres == DW_DLV_OK) {
                                sig_present = TRUE;
                            } else {
                                /* Something is badly wrong. */
                                dwarf_dealloc(dbg,attr,DW_DLA_ATTR);
                                dwarf_dealloc(dbg,alist,DW_DLA_LIST);
                                return sres;
                            }
                        } else if (attrnum == DW_AT_str_offsets_base){
                            int udres = 0;
                            udres = dwarf_formudata(attr,&str_offsets_base,
                                error);
                            if(udres == DW_DLV_OK) {
                                str_offsets_base_present = TRUE;
                            } else {
                                dwarf_dealloc(dbg,attr,DW_DLA_ATTR);
                                dwarf_dealloc(dbg,alist,DW_DLA_LIST);
                                /* Something is badly wrong. */
                                return udres;
                            }
                        }  /* else nothing to do here. */
                    }
                    dwarf_dealloc(dbg,attr,DW_DLA_ATTR);
                }
                dwarf_dealloc(dbg,alist,DW_DLA_LIST);
            } else {
                /* Something is badly wrong. No attrlist. FIXME */
                _dwarf_error(dbg,error, DW_DLE_DWP_MISSING_DWO_ID);
                return DW_DLV_ERROR;
            }
        } else {
            /* There is no DW_AT_dwo_id */
            /* Something is badly wrong. FIXME */
            _dwarf_error(dbg,error, DW_DLE_DWP_MISSING_DWO_ID);
            return DW_DLV_ERROR;
        }
    } else {
        _dwarf_error(dbg,error, DW_DLE_DWP_MISSING_DWO_ID);
    }
    if (!sig_present) {
        /*  Something is badly wrong.  Likely
            a compiler error/omission. */
        _dwarf_error(dbg,error, DW_DLE_DWP_MISSING_DWO_ID);
        return DW_DLV_ERROR;
    }
    *sig_present_return = sig_present;
    if (sig_present) {
        *sig_return = signature;
    }
    *str_offsets_base_present_return = str_offsets_base_present;
    if (str_offsets_base_present) {
        *str_offsets_base_return = str_offsets_base;
    }
    return DW_DLV_OK;
}


static int
dwarf_next_cu_header_internal(Dwarf_Debug dbg,
    Dwarf_Bool is_info,
    Dwarf_Unsigned * cu_header_length,
    Dwarf_Half * version_stamp,
    Dwarf_Unsigned * abbrev_offset,
    Dwarf_Half * address_size,
    Dwarf_Half * offset_size,
    Dwarf_Half * extension_size,
    Dwarf_Sig8 * signature,
    Dwarf_Unsigned *typeoffset,
    Dwarf_Unsigned * next_cu_offset,

    /*  header_type: DW_UT_compile, DW_UT_partial, DW_UT_type
        returned through the pointer.
        A new item in DWARF5, synthesized for earlier DWARF
        CUs (& TUs). */
    Dwarf_Half * header_type,
    Dwarf_Error * error)
{
    /* Offset for current and new CU. */
    Dwarf_Unsigned new_offset = 0;

    /* CU Context for current CU. */
    Dwarf_CU_Context cu_context = 0;
    Dwarf_Debug_InfoTypes dis = 0;
    Dwarf_Unsigned section_size =  0;



    /* ***** BEGIN CODE ***** */

    if (dbg == NULL) {
        _dwarf_error(NULL, error, DW_DLE_DBG_NULL);
        return (DW_DLV_ERROR);
    }
    dis = is_info? &dbg->de_info_reading: &dbg->de_types_reading;
    /*  Get offset into .debug_info of next CU. If dbg has no context,
        this has to be the first one. */
    if (dis->de_cu_context == NULL) {
        Dwarf_Small *dataptr = is_info? dbg->de_debug_info.dss_data:
            dbg->de_debug_types.dss_data;
        new_offset = 0;
        if (!dataptr) {
            Dwarf_Error err2= 0;
            int res = is_info?_dwarf_load_debug_info(dbg, &err2):
                _dwarf_load_debug_types(dbg,&err2);

            if (res != DW_DLV_OK) {
                if (reloc_incomplete(err2)) {
                    /*  We will assume all is ok, though it is not.
                        Relocation errors need not be fatal.  */
                    char msg_buf[200];
                    snprintf(msg_buf,sizeof(msg_buf),
                        "Relocations did not complete successfully, but we are "
                        " ignoring error: %s",dwarf_errmsg(err2));
                    dwarf_insert_harmless_error(dbg,msg_buf);
                    res = DW_DLV_OK;
                } else {
                    if (error) {
                        *error = err2;
                    }
                    return res;
                }

            }
        }

    } else {
        new_offset = dis->de_cu_context->cc_debug_offset +
            dis->de_cu_context->cc_length +
            dis->de_cu_context->cc_length_size +
            dis->de_cu_context->cc_extension_size;
    }

    /*  Check that there is room in .debug_info beyond
        the new offset for at least a new cu header.
        If not, return -1 (DW_DLV_NO_ENTRY) to indicate end
        of debug_info section, and reset
        de_cu_debug_info_offset to
        enable looping back through the cu's. */
    section_size = is_info? dbg->de_debug_info.dss_size:
        dbg->de_debug_types.dss_size;
    if ((new_offset + _dwarf_length_of_cu_header_simple(dbg,is_info)) >=
        section_size) {
        dis->de_cu_context = NULL;
        return (DW_DLV_NO_ENTRY);
    }

    /* Check if this CU has been read before. */
    cu_context = _dwarf_find_CU_Context(dbg, new_offset,is_info);

    /* If not, make CU Context for it. */
    if (cu_context == NULL) {
        cu_context = _dwarf_make_CU_Context(dbg, new_offset,is_info, error);
        if (cu_context == NULL) {
            /*  Error if CU Context could not be made. Since
                _dwarf_make_CU_Context has already registered an error
                we do not do that here: we let the lower error pass
                thru. */
            return (DW_DLV_ERROR);
        }
    }

    dis->de_cu_context = cu_context;

    if (cu_header_length != NULL) {
        *cu_header_length = cu_context->cc_length;
    }

    if (version_stamp != NULL) {
        *version_stamp = cu_context->cc_version_stamp;
    }
    if (abbrev_offset != NULL) {
        *abbrev_offset = cu_context->cc_abbrev_offset;
    }

    if (address_size != NULL) {
        *address_size = cu_context->cc_address_size;
    }
    if (offset_size != NULL) {
        *offset_size = cu_context->cc_length_size;
    }
    if (extension_size != NULL) {
        *extension_size = cu_context->cc_extension_size;
    }
    if (!is_info) {
        if (signature) {
            *signature = cu_context->cc_type_signature;
        }
        if (typeoffset) {
            *typeoffset = cu_context->cc_type_signature_offset;
        }
    }
    if (header_type) {
        *header_type = cu_context->cc_unit_type;
    }

    if (_dwarf_file_has_debug_fission_cu_index(dbg) &&
        (cu_context->cc_unit_type == DW_UT_compile ||
            cu_context->cc_unit_type == DW_UT_partial)) {
        /*  ASSERT: !cu_context->cc_type_signature_present */
        /*  Look for DW_AT_dwo_id and
            if there is one pick up the hash
            and the base array.
            Also pick up cc_str_offset_base. */
        Dwarf_Die cudie = 0;
        int resdwo = dwarf_siblingof_b(dbg,NULL,is_info,
            &cudie, error);
        if (resdwo == DW_DLV_OK) {
            Dwarf_Bool hasit = 0;
            int dwo_idres = 0;
            Dwarf_Sig8 signature;
            Dwarf_Bool sig_present = FALSE;
            Dwarf_Unsigned str_offsets_base = 0;
            Dwarf_Bool str_offsets_base_present = FALSE;
            dwo_idres = find_context_base_fields_dwo(dbg,
                cudie,&signature,&sig_present,
                &str_offsets_base,&str_offsets_base_present,
                error);

            if (dwo_idres == DW_DLV_OK) {
                if(sig_present) {
                    cu_context->cc_type_signature = signature;
                    cu_context->cc_type_signature_present = TRUE;
                } else {
                    dwarf_dealloc(dbg,cudie,DW_DLA_DIE);
                    _dwarf_error(dbg, error, DW_DLE_DWP_MISSING_DWO_ID);
                    return DW_DLV_ERROR;
                }

                if(str_offsets_base_present) {
                    cu_context->cc_str_offsets_base = str_offsets_base;
                    cu_context->cc_str_offsets_base_present = TRUE;
                } else {
                    /* Do...what? */
                }
            } else {
                /* DW_DLE_DWP_MISSING_DWO_ID, really */
                /* something badly wrong. */
                dwarf_dealloc(dbg,cudie,DW_DLA_DIE);
                return dwo_idres;
            }
            dwarf_dealloc(dbg,cudie,DW_DLA_DIE);
        } else if (resdwo == DW_DLV_NO_ENTRY) {
            /* Impossible */
            _dwarf_error(NULL, error, DW_DLE_DWP_SIBLING_ERROR);
            return DW_DLV_ERROR;
        } else {
            /* Something is badly wrong. */
            return resdwo;
        }
    }
    new_offset = new_offset + cu_context->cc_length +
        cu_context->cc_length_size + cu_context->cc_extension_size;
    *next_cu_offset = new_offset;
    return (DW_DLV_OK);
}

/*  Given hash signature, return the CU_die of the applicable CU.
    The hash is assumed to be from 'somewhere',
    such as from a skeleton DIE DW_AT_dwo_id  ("cu" case) or
    from a DW_FORM_ref_sig8 ("tu" case).

    If "tu" request,  the CU_die
    of of the type unit.
    Works on either a dwp package file or a dwo object.

    If "cu" request,  the CU_die
    of the compilation unit.
    Works on either a dwp package file or a dwo object.

    If the hash passed is not present, returns DW_DLV_NO_ENTRY
    (but read the next two paragraphs for more detail).

    If a dwp package file with the hash signature
    is present in the applicable index but no matching
    compilation unit can be found, it returns DW_DLV_ERROR.

    If a .dwo object there is no index and we look at the
    compilation units (possibly all of them). If not present
    then we return DW_DLV_NO_ENTRY.

    The returned_die is a CU DIE if the sig_type is "cu".
    The returned_die is a type DIE if the sig_type is "tu".
    Perhaps both should return CU die. FIXME

    New 27 April, 2015
*/
int
dwarf_die_from_hash_signature(Dwarf_Debug dbg,
    Dwarf_Sig8 *     hash_sig,
    const char *     sig_type  /* "tu" or "cu"*/,
    Dwarf_Die  *     returned_die,
    Dwarf_Error*     error)
{
    Dwarf_Bool is_type_unit = FALSE;
    Dwarf_Bool is_info = !is_type_unit;
    int sres = 0;

    sres = _dwarf_load_debug_info(dbg,error);
    if (sres == DW_DLV_ERROR) {
        return sres;
    }
    sres = _dwarf_load_debug_types(dbg,error);
    if (sres == DW_DLV_ERROR) {
        return sres;
    }

    if (!strcmp(sig_type,"tu")) {
        is_type_unit = TRUE;
    } else if (!strcmp(sig_type,"cu")) {
        is_type_unit = FALSE;
    } else {
        _dwarf_error(dbg,error,DW_DLE_SIG_TYPE_WRONG_STRING);
        return DW_DLV_ERROR;
    }
    is_info = !is_type_unit;

    if (_dwarf_file_has_debug_fission_index(dbg)) {
        /* This is a dwp package file. */
        int fisres = 0;
        Dwarf_Bool is_info2 = 0;
        Dwarf_Off cu_header_off = 0;
        Dwarf_Off cu_size = 0;
        Dwarf_Off cu_die_off = 0;
        Dwarf_Off typeoffset = 0;
        Dwarf_Die cudie = 0;
        Dwarf_Die typedie = 0;
        Dwarf_CU_Context context = 0;
        Dwarf_Debug_Fission_Per_CU fiss;

        memset(&fiss,0,sizeof(fiss));
        fisres = dwarf_get_debugfission_for_key(dbg,hash_sig,
            sig_type,&fiss,error);
        if (fisres != DW_DLV_OK) {
            return fisres;
        }
        /* Found it */
        if(is_type_unit) {
            /*  DW4 has debug_types, so look in .debug_types.
                Else look in .debug_info.  */
            is_info2 = dbg->de_debug_types.dss_size?FALSE:TRUE;
        } else {
            is_info2 = TRUE;
        }

        cu_header_off = _dwarf_get_dwp_extra_offset(&fiss,
            is_info2?DW_SECT_INFO:DW_SECT_TYPES,
            &cu_size);

        fisres = dwarf_get_cu_die_offset_given_cu_header_offset_b(
            dbg,cu_header_off,
            is_info2,
            &cu_die_off,error);
        if (fisres != DW_DLV_OK) {
            return fisres;
        }
        fisres = dwarf_offdie_b(dbg,cu_die_off,is_info2,
            &cudie,error);
        if (fisres != DW_DLV_OK) {
            return fisres;
        }
        if (!is_type_unit) {
            *returned_die = cudie;
            return DW_DLV_OK;
        }
        context = cudie->di_cu_context;
        typeoffset = context->cc_type_signature_offset;
        typeoffset += cu_header_off;
        fisres = dwarf_offdie_b(dbg,typeoffset,is_info2,
            &typedie,error);
        if (fisres != DW_DLV_OK) {
            dwarf_dealloc(dbg,cudie,DW_DLA_DIE);
            return fisres;
        }
        *returned_die = typedie;
        dwarf_dealloc(dbg,cudie,DW_DLA_DIE);
        return DW_DLV_OK;
    }
    /*  Look thru all the CUs, there is no DWP tu/cu index.
        There will be COMDAT sections for  the type TUs
            (DW_UT_type).
        A single non-comdat for the DW_UT_compile. */
    /*  FIXME: DW_DLE_DEBUG_FISSION_INCOMPLETE  */
    _dwarf_error(dbg,error,DW_DLE_DEBUG_FISSION_INCOMPLETE);
    return DW_DLV_ERROR;
}

static int
dwarf_ptr_CU_offset(Dwarf_CU_Context cu_context,
    Dwarf_Byte_Ptr di_ptr,
    Dwarf_Bool is_info,
    Dwarf_Off * cu_off)
{
    Dwarf_Debug dbg = cu_context->cc_dbg;
    Dwarf_Small *dataptr = is_info? dbg->de_debug_info.dss_data:
        dbg->de_debug_types.dss_data;
    *cu_off = (di_ptr - dataptr);
    return DW_DLV_OK;
}
#if 0
/* Just for debug purposes */
void print_sib_offset(Dwarf_Die sibling)
{
    Dwarf_Off sib_off;
    Dwarf_Error error;
    dwarf_dieoffset(sibling,&sib_off,&error);
    fprintf(stderr," SIB OFF = 0x%" DW_PR_XZEROS DW_PR_DUx,sib_off);
}
void print_ptr_offset(Dwarf_CU_Context cu_context,Dwarf_Byte_Ptr di_ptr)
{
    Dwarf_Off ptr_off;
    dwarf_ptr_CU_offset(cu_context,di_ptr,&ptr_off);
    fprintf(stderr," PTR OFF = 0x%" DW_PR_XZEROS DW_PR_DUx,ptr_off);
}
#endif


/*  Validate the sibling DIE. This only makes sense to call
    if the sibling's DIEs have been travsersed and
    dwarf_child() called on each,
    so that the last DIE dwarf_child saw was the last.
    Essentially ensuring that (after such traversal) that we
    are in the same place a sibling attribute would identify.
    In case we return DW_DLV_ERROR, the global offset of the last
    DIE traversed by dwarf_child is returned through *offset

    It is essentially guaranteed that  dbg->de_last_die
    is a stale DIE pointer of a deallocated DIE when we get here.
    It must not be used as a DIE pointer here,
    just as a sort of anonymous pointer that we just check against
    NULL.

    There is a (subtle?) dependence on the fact that when we call this
    the last dwarf_child() call would have been for this sibling.
    Meaning that this works in a depth-first traversal even though there
    is no stack of 'de_last_die' values.

    The check for dbg->de_last_die just ensures sanity.

    If one is switching between normal debug_frame and eh_frame
    (traversing them in tandem, let us say) in a single
    Dwarf_Debug this validator makes no sense.
    It works if one processes a .debug_frame (entirely) and
    then an eh_frame (or vice versa) though.
    Use caution.
*/
int
dwarf_validate_die_sibling(Dwarf_Die sibling,Dwarf_Off *offset)
{
    Dwarf_Debug dbg = 0;
    Dwarf_Error *error = 0;
    Dwarf_Debug_InfoTypes dis = 0;
    CHECK_DIE(sibling, DW_DLV_ERROR);
    dbg = sibling->di_cu_context->cc_dbg;

    dis = sibling->di_is_info? &dbg->de_info_reading: &dbg->de_types_reading;

    *offset = 0;
    if (dis->de_last_die && dis->de_last_di_ptr) {
        if (sibling->di_debug_ptr == dis->de_last_di_ptr) {
            return (DW_DLV_OK);
        }
    }
    /* Calculate global offset used for error reporting */
    dwarf_ptr_CU_offset(sibling->di_cu_context,
        dis->de_last_di_ptr,sibling->di_is_info,offset);
    return (DW_DLV_ERROR);
}

/*  This function does two slightly different things
    depending on the input flag want_AT_sibling.  If
    this flag is true, it checks if the input die has
    a DW_AT_sibling attribute.  If it does it returns
    a pointer to the start of the sibling die in the
    .debug_info section.  Otherwise it behaves the
    same as the want_AT_sibling false case.

    If the want_AT_sibling flag is false, it returns
    a pointer to the immediately adjacent die in the
    .debug_info section.

    Die_info_end points to the end of the .debug_info
    portion for the cu the die belongs to.  It is used
    to check that the search for the next die does not
    cross the end of the current cu.  Cu_info_start points
    to the start of the .debug_info portion for the
    current cu, and is used to add to the offset for
    DW_AT_sibling attributes.  Finally, has_die_child
    is a pointer to a Dwarf_Bool that is set true if
    the present die has children, false otherwise.
    However, in case want_AT_child is true and the die
    has a DW_AT_sibling attribute *has_die_child is set
    false to indicate that the children are being skipped.

    die_info_end  points to the last byte+1 of the cu.  */
static int
_dwarf_next_die_info_ptr(Dwarf_Byte_Ptr die_info_ptr,
    Dwarf_CU_Context cu_context,
    Dwarf_Byte_Ptr die_info_end,
    Dwarf_Byte_Ptr cu_info_start,
    Dwarf_Bool want_AT_sibling,
    Dwarf_Bool * has_die_child,
    Dwarf_Byte_Ptr *next_die_ptr_out,
    Dwarf_Error *error)
{
    Dwarf_Byte_Ptr info_ptr = 0;
    Dwarf_Byte_Ptr abbrev_ptr = 0;
    Dwarf_Word abbrev_code = 0;
    Dwarf_Abbrev_List abbrev_list;
    Dwarf_Half attr = 0;
    Dwarf_Half attr_form = 0;
    Dwarf_Unsigned offset = 0;
    Dwarf_Word leb128_length = 0;
    Dwarf_Unsigned utmp = 0;
    Dwarf_Debug dbg = 0;

    info_ptr = die_info_ptr;
    DECODE_LEB128_UWORD(info_ptr, utmp);
    abbrev_code = (Dwarf_Word) utmp;
    if (abbrev_code == 0) {
        /*  Should never happen. Tested before we got here. */
        _dwarf_error(dbg, error, DW_DLE_NEXT_DIE_PTR_NULL);
        return DW_DLV_ERROR;
    }


    abbrev_list = _dwarf_get_abbrev_for_code(cu_context, abbrev_code);
    if (abbrev_list == NULL) {
        _dwarf_error(dbg, error, DW_DLE_NEXT_DIE_NO_ABBREV_LIST);
        return DW_DLV_ERROR;
    }
    dbg = cu_context->cc_dbg;

    *has_die_child = abbrev_list->ab_has_child;

    abbrev_ptr = abbrev_list->ab_abbrev_ptr;
    do {
        Dwarf_Unsigned utmp2;

        DECODE_LEB128_UWORD(abbrev_ptr, utmp2);
        attr = (Dwarf_Half) utmp2;
        DECODE_LEB128_UWORD(abbrev_ptr, utmp2);
        attr_form = (Dwarf_Half) utmp2;
        if (attr_form == DW_FORM_indirect) {
            Dwarf_Unsigned utmp6;

            /* DECODE_LEB128_UWORD updates info_ptr */
            DECODE_LEB128_UWORD(info_ptr, utmp6);
            attr_form = (Dwarf_Half) utmp6;

        }

        if (want_AT_sibling && attr == DW_AT_sibling) {
            switch (attr_form) {
            case DW_FORM_ref1:
                offset = *(Dwarf_Small *) info_ptr;
                break;
            case DW_FORM_ref2:
                /* READ_UNALIGNED does not update info_ptr */
                READ_UNALIGNED(dbg, offset, Dwarf_Unsigned,
                    info_ptr, sizeof(Dwarf_Half));
                break;
            case DW_FORM_ref4:
                READ_UNALIGNED(dbg, offset, Dwarf_Unsigned,
                    info_ptr, sizeof(Dwarf_ufixed));
                break;
            case DW_FORM_ref8:
                READ_UNALIGNED(dbg, offset, Dwarf_Unsigned,
                    info_ptr, sizeof(Dwarf_Unsigned));
                break;
            case DW_FORM_ref_udata:
                offset =
                    _dwarf_decode_u_leb128(info_ptr, &leb128_length);
                break;
            case DW_FORM_ref_addr:
                /*  Very unusual.  The FORM is intended to refer to
                    a different CU, but a different CU cannot
                    be a sibling, can it?
                    We could ignore this and treat as if no DW_AT_sibling
                    present.   Or derive the offset from it and if
                    it is in the same CU use it directly.
                    The offset here is *supposed* to be a global offset,
                    so adding cu_info_start is wrong  to any offset
                    we find here unless cu_info_start
                    is zero! Lets pretend there is no DW_AT_sibling
                    attribute.  */
                goto no_sibling_attr;
            default:
                _dwarf_error(dbg, error, DW_DLE_NEXT_DIE_WRONG_FORM);
                return DW_DLV_ERROR;
            }

            /*  Reset *has_die_child to indicate children skipped.  */
            *has_die_child = false;

            /*  A value beyond die_info_end indicates an error. Exactly
                at die_info_end means 1-past-cu-end and simply means we
                are at the end, do not return error. Higher level
                will detect that we are at the end. */
            if (cu_info_start + offset > die_info_end) {
                /* Error case, bad DWARF. */
                _dwarf_error(dbg, error, DW_DLE_NEXT_DIE_PAST_END);
                return DW_DLV_ERROR;
            }
            /* At or before end-of-cu */
            *next_die_ptr_out = cu_info_start + offset;
            return DW_DLV_OK;
        }

        no_sibling_attr:
        if (attr_form != 0) {
            int res = 0;
            Dwarf_Unsigned sizeofval = 0;
            res = _dwarf_get_size_of_val(cu_context->cc_dbg,
                attr_form,
                cu_context->cc_version_stamp,
                cu_context->cc_address_size,
                info_ptr,
                cu_context->cc_length_size,
                &sizeofval,
                error);
            if(res != DW_DLV_OK) {
                return res;
            }
            /*  It is ok for info_ptr == die_info_end, as we will test
                later before using a too-large info_ptr */
            info_ptr += sizeofval;
            if (info_ptr > die_info_end) {
                /*  More than one-past-end indicates a bug somewhere,
                    likely bad dwarf generation. */
                _dwarf_error(dbg, error, DW_DLE_NEXT_DIE_PAST_END);
                return DW_DLV_ERROR;
            }
        }
    } while (attr != 0 || attr_form != 0);
    *next_die_ptr_out = info_ptr;
    return DW_DLV_OK;
}

/*  Multiple TAGs are in fact compile units.
    Allow them all.
    Return non-zero if a CU tag.
    Else return 0.
*/
static int
is_cu_tag(int t)
{
    if (t == DW_TAG_compile_unit ||
        t == DW_TAG_partial_unit ||
        t == DW_TAG_imported_unit ||
        t == DW_TAG_type_unit) {
        return 1;
    }
    return 0;
}

/*  Given a Dwarf_Debug dbg, and a Dwarf_Die die, it returns
    a Dwarf_Die for the sibling of die.  In case die is NULL,
    it returns (thru ptr) a Dwarf_Die for the first die in the current
    cu in dbg.  Returns DW_DLV_ERROR on error.

    It is assumed that every sibling chain including those with
    only one element is terminated with a NULL die, except a
    chain with only a NULL die.

    The algorithm moves from one die to the adjacent one.  It
    returns when the depth of children it sees equals the number
    of sibling chain terminations.  A single count, child_depth
    is used to track the depth of children and sibling terminations
    encountered.  Child_depth is incremented when a die has the
    Has-Child flag set unless the child happens to be a NULL die.
    Child_depth is decremented when a die has Has-Child false,
    and the adjacent die is NULL.  Algorithm returns when
    child_depth is 0.

    **NOTE: Do not modify input die, since it is used at the end.  */
int
dwarf_siblingof(Dwarf_Debug dbg,
    Dwarf_Die die,
    Dwarf_Die * caller_ret_die, Dwarf_Error * error)
{
    Dwarf_Bool is_info = true;
    return dwarf_siblingof_b(dbg,die,is_info,caller_ret_die,error);
}
/*  This is the new form, October 2011.  On calling with 'die' NULL,
    we cannot tell if this is debug_info or debug_types, so
    we must be informed!. */
int
dwarf_siblingof_b(Dwarf_Debug dbg,
    Dwarf_Die die,
    Dwarf_Bool is_info,
    Dwarf_Die * caller_ret_die, Dwarf_Error * error)
{
    Dwarf_Die ret_die = 0;
    Dwarf_Byte_Ptr die_info_ptr = 0;
    Dwarf_Byte_Ptr cu_info_start = 0;

    /* die_info_end points 1-past end of die (once set) */
    Dwarf_Byte_Ptr die_info_end = 0;
    Dwarf_Word abbrev_code = 0;
    Dwarf_Unsigned utmp = 0;
    /* Since die may be NULL, we rely on the input argument. */
    Dwarf_Debug_InfoTypes dis = is_info? &dbg->de_info_reading:
        &dbg->de_types_reading;
    Dwarf_Small *dataptr = is_info? dbg->de_debug_info.dss_data:
        dbg->de_debug_types.dss_data;



    if (dbg == NULL) {
        _dwarf_error(NULL, error, DW_DLE_DBG_NULL);
        return (DW_DLV_ERROR);
    }

    if (die == NULL) {
        /*  Find root die of cu */
        /*  die_info_end is untouched here, need not be set in this
            branch. */
        Dwarf_Off off2;
        Dwarf_CU_Context context=0;
        Dwarf_Unsigned headerlen = 0;

        /*  If we've not loaded debug_info
            de_cu_context will be NULL. */

        context = dis->de_cu_context;
        if (context == NULL) {
            _dwarf_error(dbg, error, DW_DLE_DBG_NO_CU_CONTEXT);
            return (DW_DLV_ERROR);
        }

        off2 = context->cc_debug_offset;
        cu_info_start = dataptr + off2;
        headerlen = _dwarf_length_of_cu_header(dbg, off2,is_info);
        die_info_ptr = cu_info_start + headerlen;
        die_info_end = _dwarf_calculate_section_end_ptr(context);
        /*  Recording the CU die pointer so we can later access
            for special FORMs relating to .debug_str_offsets
            and .debug_addr  */
        context->cc_cu_die_offset_present = TRUE;
        context->cc_cu_die_global_sec_offset = off2 + headerlen;
    } else {
        /* Find sibling die. */
        Dwarf_Bool has_child = false;
        Dwarf_Sword child_depth = 0;
        Dwarf_CU_Context context=0;

        /*  We cannot have a legal die unless debug_info was loaded, so
            no need to load debug_info here. */
        CHECK_DIE(die, DW_DLV_ERROR);

        die_info_ptr = die->di_debug_ptr;
        if (*die_info_ptr == 0) {
            return (DW_DLV_NO_ENTRY);
        }
        context = die->di_cu_context;
        cu_info_start = dataptr+ context->cc_debug_offset;
        die_info_end = _dwarf_calculate_section_end_ptr(context);

        if ((*die_info_ptr) == 0) {
            return (DW_DLV_NO_ENTRY);
        }
        child_depth = 0;
        do {
            int res2 = 0;
            Dwarf_Byte_Ptr die_info_ptr2 = 0;
            res2 = _dwarf_next_die_info_ptr(die_info_ptr,
                die->di_cu_context, die_info_end,
                cu_info_start, true, &has_child,
                &die_info_ptr2,
                error);
            if(res2 != DW_DLV_OK) {
                return res2;
            }
            if (die_info_ptr2 < die_info_ptr) {
                /*  There is something very wrong, our die value
                    decreased.  Bad DWARF. */
                _dwarf_error(dbg, error, DW_DLE_NEXT_DIE_LOW_ERROR);
                return (DW_DLV_ERROR);
            }
            if (die_info_ptr2 > die_info_end) {
                _dwarf_error(dbg, error, DW_DLE_NEXT_DIE_PAST_END);
                return (DW_DLV_ERROR);
            }
            die_info_ptr = die_info_ptr2;

            /*  die_info_end is one past end. Do not read it!
                A test for ``!= die_info_end''  would work as well,
                but perhaps < reads more like the meaning. */
            if (die_info_ptr < die_info_end) {
                if ((*die_info_ptr) == 0 && has_child) {
                    die_info_ptr++;
                    has_child = false;
                }
            }

            /* die_info_ptr can be one-past-end. */
            if ((die_info_ptr == die_info_end) ||
                ((*die_info_ptr) == 0)) {
                for (; child_depth > 0 && *die_info_ptr == 0;
                    child_depth--, die_info_ptr++) {
                }
            } else {
                child_depth = has_child ? child_depth + 1 : child_depth;
            }

        } while (child_depth != 0);
    }

    /*  die_info_ptr > die_info_end is really a bug (possibly in dwarf
        generation)(but we are past end, no more DIEs here), whereas
        die_info_ptr == die_info_end means 'one past end, no more DIEs
        here'. */
    if (die_info_ptr >= die_info_end) {
        return (DW_DLV_NO_ENTRY);
    }

    if ((*die_info_ptr) == 0) {
        return (DW_DLV_NO_ENTRY);
    }

    ret_die = (Dwarf_Die) _dwarf_get_alloc(dbg, DW_DLA_DIE, 1);
    if (ret_die == NULL) {
        _dwarf_error(dbg, error, DW_DLE_ALLOC_FAIL);
        return (DW_DLV_ERROR);
    }

    ret_die->di_is_info = is_info;
    ret_die->di_debug_ptr = die_info_ptr;
    ret_die->di_cu_context =
        die == NULL ? dis->de_cu_context : die->di_cu_context;

    DECODE_LEB128_UWORD(die_info_ptr, utmp);
    if (die_info_ptr > die_info_end) {
        /*  We managed to go past the end of the CU!.
            Something is badly wrong. */
        dwarf_dealloc(dbg, ret_die, DW_DLA_DIE);
        _dwarf_error(dbg, error, DW_DLE_ABBREV_DECODE_ERROR);
        return (DW_DLV_ERROR);
    }
    abbrev_code = (Dwarf_Word) utmp;
    if (abbrev_code == 0) {
        /* Zero means a null DIE */
        dwarf_dealloc(dbg, ret_die, DW_DLA_DIE);
        return (DW_DLV_NO_ENTRY);
    }
    ret_die->di_abbrev_code = abbrev_code;
    ret_die->di_abbrev_list =
        _dwarf_get_abbrev_for_code(ret_die->di_cu_context, abbrev_code);
    if (ret_die->di_abbrev_list == NULL ) {
        dwarf_dealloc(dbg, ret_die, DW_DLA_DIE);
        _dwarf_error(dbg, error, DW_DLE_DIE_ABBREV_LIST_NULL);
        return (DW_DLV_ERROR);
    }
    if (die == NULL && !is_cu_tag(ret_die->di_abbrev_list->ab_tag)) {
        dwarf_dealloc(dbg, ret_die, DW_DLA_DIE);
        _dwarf_error(dbg, error, DW_DLE_FIRST_DIE_NOT_CU);
        return (DW_DLV_ERROR);
    }

    *caller_ret_die = ret_die;
    return (DW_DLV_OK);
}


int
dwarf_child(Dwarf_Die die,
    Dwarf_Die * caller_ret_die,
    Dwarf_Error * error)
{
    Dwarf_Byte_Ptr die_info_ptr = 0;
    Dwarf_Byte_Ptr die_info_ptr2 = 0;

    /* die_info_end points one-past-end of die area. */
    Dwarf_Byte_Ptr die_info_end = 0;
    Dwarf_Die ret_die = 0;
    Dwarf_Bool has_die_child = 0;
    Dwarf_Debug dbg;
    Dwarf_Word abbrev_code = 0;
    Dwarf_Unsigned utmp = 0;
    Dwarf_Small *dataptr = 0;
    Dwarf_Debug_InfoTypes dis = 0;
    int res = 0;
    Dwarf_CU_Context context = 0;

    CHECK_DIE(die, DW_DLV_ERROR);
    dbg = die->di_cu_context->cc_dbg;
    dis = die->di_is_info? &dbg->de_info_reading:
        &dbg->de_types_reading;
    die_info_ptr = die->di_debug_ptr;

    /*  We are saving a DIE pointer here, but the pointer
        will not be presumed live later, when it is tested. */
    dis->de_last_die = die;
    dis->de_last_di_ptr = die_info_ptr;

    /* NULL die has no child. */
    if ((*die_info_ptr) == 0)
        return (DW_DLV_NO_ENTRY);

    context = die->di_cu_context;
    die_info_end = _dwarf_calculate_section_end_ptr(context);

    res = _dwarf_next_die_info_ptr(die_info_ptr, die->di_cu_context,
        die_info_end,
        NULL, false,
        &has_die_child,
        &die_info_ptr2,
        error);
    if(res != DW_DLV_OK) {
        return res;
    }
    die_info_ptr = die_info_ptr2;

    dis->de_last_di_ptr = die_info_ptr;

    if (!has_die_child) {
        /* Look for end of sibling chain. */
        while (dis->de_last_di_ptr < die_info_end) {
            if (*dis->de_last_di_ptr) {
                break;
            }
            ++dis->de_last_di_ptr;
        }
        return (DW_DLV_NO_ENTRY);
    }

    ret_die = (Dwarf_Die) _dwarf_get_alloc(dbg, DW_DLA_DIE, 1);
    if (ret_die == NULL) {
        _dwarf_error(dbg, error, DW_DLE_ALLOC_FAIL);
        return (DW_DLV_ERROR);
    }
    ret_die->di_debug_ptr = die_info_ptr;
    ret_die->di_cu_context = die->di_cu_context;
    ret_die->di_is_info = die->di_is_info;

    DECODE_LEB128_UWORD(die_info_ptr, utmp);
    abbrev_code = (Dwarf_Word) utmp;

    dis->de_last_di_ptr = die_info_ptr;

    if (abbrev_code == 0) {
        /* Look for end of sibling chain */
        while (dis->de_last_di_ptr < die_info_end) {
            if (*dis->de_last_di_ptr) {
                break;
            }
            ++dis->de_last_di_ptr;
        }

        /*  We have arrived at a null DIE, at the end of a CU or the end
            of a list of siblings. */
        *caller_ret_die = 0;
        dwarf_dealloc(dbg, ret_die, DW_DLA_DIE);
        return DW_DLV_NO_ENTRY;
    }
    ret_die->di_abbrev_code = abbrev_code;
    ret_die->di_abbrev_list =
        _dwarf_get_abbrev_for_code(die->di_cu_context, abbrev_code);
    if (ret_die->di_abbrev_list == NULL) {
        dwarf_dealloc(dbg, ret_die, DW_DLA_DIE);
        _dwarf_error(dbg, error, DW_DLE_DIE_BAD);
        return (DW_DLV_ERROR);
    }

    *caller_ret_die = ret_die;
    return (DW_DLV_OK);
}

/*  Given a (global, not cu_relative) die offset, this returns
    a pointer to a DIE thru *new_die.
    It is up to the caller to do a
    dwarf_dealloc(dbg,*new_die,DW_DLE_DIE);
    The old form only works with debug_info.
    The new _b form works with debug_info or debug_types.
    */
int
dwarf_offdie(Dwarf_Debug dbg,
    Dwarf_Off offset, Dwarf_Die * new_die, Dwarf_Error * error)
{
    Dwarf_Bool is_info = true;
    return dwarf_offdie_b(dbg,offset,is_info,new_die,error);
}

int
dwarf_offdie_b(Dwarf_Debug dbg,
    Dwarf_Off offset, Dwarf_Bool is_info,
    Dwarf_Die * new_die, Dwarf_Error * error)
{
    Dwarf_CU_Context cu_context = 0;
    Dwarf_Off new_cu_offset = 0;
    Dwarf_Die die = 0;
    Dwarf_Byte_Ptr info_ptr = 0;
    Dwarf_Unsigned abbrev_code = 0;
    Dwarf_Unsigned utmp = 0;
    Dwarf_Debug_InfoTypes dis = 0;


    if (dbg == NULL) {
        _dwarf_error(NULL, error, DW_DLE_DBG_NULL);
        return (DW_DLV_ERROR);
    }
    dis = is_info? &dbg->de_info_reading:
        &dbg->de_types_reading;

    cu_context = _dwarf_find_CU_Context(dbg, offset,is_info);
    if (cu_context == NULL)
        cu_context = _dwarf_find_offdie_CU_Context(dbg, offset,is_info);

    if (cu_context == NULL) {
        Dwarf_Unsigned section_size = is_info? dbg->de_debug_info.dss_size:
            dbg->de_debug_types.dss_size;
        int res = is_info?_dwarf_load_debug_info(dbg, error):
            _dwarf_load_debug_types(dbg,error);

        if (res != DW_DLV_OK) {
            return res;
        }

        if (dis->de_offdie_cu_context_end != NULL) {
            Dwarf_CU_Context lcu_context =
                dis->de_offdie_cu_context_end;
            new_cu_offset =
                lcu_context->cc_debug_offset +
                lcu_context->cc_length +
                lcu_context->cc_length_size +
                lcu_context->cc_extension_size;
        }


        do {
            if ((new_cu_offset +
                _dwarf_length_of_cu_header_simple(dbg,is_info)) >=
                section_size) {
                _dwarf_error(dbg, error, DW_DLE_OFFSET_BAD);
                return (DW_DLV_ERROR);
            }

            cu_context =
                _dwarf_make_CU_Context(dbg, new_cu_offset,is_info, error);
            if (cu_context == NULL) {
                /*  Error if CU Context could not be made. Since
                    _dwarf_make_CU_Context has already registered an
                    error we do not do that here: we let the lower error
                    pass thru. */
                return (DW_DLV_ERROR);
            }

            if (dis->de_offdie_cu_context == NULL) {
                dis->de_offdie_cu_context = cu_context;
                dis->de_offdie_cu_context_end = cu_context;
            } else {
                dis->de_offdie_cu_context_end->cc_next = cu_context;
                dis->de_offdie_cu_context_end = cu_context;
            }

            new_cu_offset = new_cu_offset + cu_context->cc_length +
                cu_context->cc_length_size +
                cu_context->cc_extension_size;

        } while (offset >= new_cu_offset);
    }

    die = (Dwarf_Die) _dwarf_get_alloc(dbg, DW_DLA_DIE, 1);
    if (die == NULL) {
        _dwarf_error(dbg, error, DW_DLE_ALLOC_FAIL);
        return (DW_DLV_ERROR);
    }
    die->di_cu_context = cu_context;
    die->di_is_info = is_info;

    {
        Dwarf_Small *dataptr = is_info? dbg->de_debug_info.dss_data:
            dbg->de_debug_types.dss_data;
        info_ptr = dataptr + offset;
    }
    die->di_debug_ptr = info_ptr;
    DECODE_LEB128_UWORD(info_ptr, utmp);
    abbrev_code = utmp;
    if (abbrev_code == 0) {
        /* we are at a null DIE (or there is a bug). */
        *new_die = 0;
        dwarf_dealloc(dbg, die, DW_DLA_DIE);
        return DW_DLV_NO_ENTRY;
    }
    die->di_abbrev_code = abbrev_code;
    die->di_abbrev_list =
        _dwarf_get_abbrev_for_code(cu_context, abbrev_code);
    if (die->di_abbrev_list == NULL) {
        dwarf_dealloc(dbg, die, DW_DLA_DIE);
        _dwarf_error(dbg, error, DW_DLE_DIE_ABBREV_LIST_NULL);
        return (DW_DLV_ERROR);
    }

    *new_die = die;
    return (DW_DLV_OK);
}

/*  This is useful when printing DIE data.
    The string pointer returned must not be freed.
    With non-elf objects it is possible the
    string returned might be empty or NULL,
    so callers should be prepared for that kind
    of return. */
int
dwarf_get_die_section_name(Dwarf_Debug dbg,
    Dwarf_Bool    is_info,
    const char ** sec_name,
    Dwarf_Error * error)
{
    struct Dwarf_Section_s *sec = 0;
    if (is_info) {
        sec = &dbg->de_debug_info;
    } else {
        sec = &dbg->de_debug_types;
    }
    if (sec->dss_size == 0) {
        /* We don't have such a  section at all. */
        return DW_DLV_NO_ENTRY;
    }
    *sec_name = sec->dss_name;
    return DW_DLV_OK;
}


