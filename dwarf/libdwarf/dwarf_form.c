/*

  Copyright (C) 2000,2002,2004,2005 Silicon Graphics, Inc. All Rights Reserved.
  Portions Copyright 2007-2010 Sun Microsystems, Inc. All rights reserved.
  Portions Copyright 2008-2012 David Anderson. All rights reserved.
  Portions Copyright 2010-2012 SN Systems Ltd. All rights reserved.

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
#include "dwarf_die_deliv.h"

/* This code was repeated many times, now it
   is all in one place. */
static int
get_attr_dbg(Dwarf_Debug *dbg,
    Dwarf_CU_Context * cu_context,
    Dwarf_Attribute attr,
    Dwarf_Error *error)
{
    Dwarf_CU_Context cup;
    if (attr == NULL) {
        _dwarf_error(NULL, error, DW_DLE_ATTR_NULL);
        return (DW_DLV_ERROR);
    }

    cup = attr->ar_cu_context;
    if (cup == NULL) {
        _dwarf_error(NULL, error, DW_DLE_ATTR_NO_CU_CONTEXT);
        return (DW_DLV_ERROR);
    }

    if (cup->cc_dbg == NULL) {
        _dwarf_error(NULL, error, DW_DLE_ATTR_DBG_NULL);
        return (DW_DLV_ERROR);
    }
    *cu_context = cup;
    *dbg = cup->cc_dbg;
    return DW_DLV_OK;

}

int
dwarf_hasform(Dwarf_Attribute attr,
    Dwarf_Half form,
    Dwarf_Bool * return_bool, Dwarf_Error * error)
{
    Dwarf_Debug dbg = 0;
    Dwarf_CU_Context cu_context = 0;

    int res  =get_attr_dbg(&dbg,&cu_context, attr,error);
    if (res != DW_DLV_OK) {
        return res;
    }
    *return_bool = (attr->ar_attribute_form == form);
    return DW_DLV_OK;
}

/* Not often called, we do not worry about efficiency here.
   The dwarf_whatform() call does the sanity checks for us.
*/
int
dwarf_whatform_direct(Dwarf_Attribute attr,
    Dwarf_Half * return_form, Dwarf_Error * error)
{
    int res = dwarf_whatform(attr, return_form, error);

    if (res != DW_DLV_OK) {
        return res;
    }

    *return_form = attr->ar_attribute_form_direct;
    return (DW_DLV_OK);
}


/*  This code was contributed some time ago
    and the return
    value is in the wrong form,
    but we are not fixing it.
*/
void *
dwarf_uncompress_integer_block(
    Dwarf_Debug      dbg,
    Dwarf_Bool       unit_is_signed,
    Dwarf_Small      unit_length_in_bits,
    void*            input_block,
    Dwarf_Unsigned   input_length_in_bytes,
    Dwarf_Unsigned*  output_length_in_units_ptr,
    Dwarf_Error*     error
)
{
    Dwarf_Unsigned output_length_in_units = 0;
    void * output_block = 0;
    unsigned i = 0;
    char * ptr = 0;
    int remain = 0;
    Dwarf_sfixed * array = 0;

    if (dbg == NULL) {
        _dwarf_error(NULL, error, DW_DLE_DBG_NULL);
        return((void *)DW_DLV_BADADDR);
    }

    if (unit_is_signed == false ||
        unit_length_in_bits != 32 ||
        input_block == NULL ||
        input_length_in_bytes == 0 ||
        output_length_in_units_ptr == NULL) {

        _dwarf_error(NULL, error, DW_DLE_BADBITC);
        return ((void *) DW_DLV_BADADDR);
    }

    /* At this point we assume the format is: signed 32 bit */

    /* first uncompress everything to find the total size. */

    output_length_in_units = 0;
    remain = input_length_in_bytes;
    ptr = input_block;
    while (remain > 0) {
        Dwarf_Word len = 0;
        _dwarf_decode_s_leb128((unsigned char *)ptr, &len);
        ptr += len;
        remain -= len;
        output_length_in_units++;
    }

    if (remain != 0) {
        _dwarf_error(NULL, error, DW_DLE_ALLOC_FAIL);
        return((void *)DW_DLV_BADADDR);
    }

    /* then alloc */

    output_block = (void *)
        _dwarf_get_alloc(dbg,
            DW_DLA_STRING,
            output_length_in_units * (unit_length_in_bits / 8));
    if (output_block == NULL) {
        _dwarf_error(dbg, error, DW_DLE_ALLOC_FAIL);
        return((void*)DW_DLV_BADADDR);
    }

    /* then uncompress again and copy into new buffer */

    array = (Dwarf_sfixed *) output_block;
    remain = input_length_in_bytes;
    ptr = input_block;
    for (i=0; i<output_length_in_units && remain>0; i++) {
        Dwarf_Signed num;
        Dwarf_Word len;
        num = _dwarf_decode_s_leb128((unsigned char *)ptr, &len);
        ptr += len;
        remain -= len;
        array[i] = num;
    }

    if (remain != 0) {
        dwarf_dealloc(dbg, (unsigned char *)output_block, DW_DLA_STRING);
        _dwarf_error(dbg, error, DW_DLE_ALLOC_FAIL);
        return((Dwarf_P_Attribute)DW_DLV_BADADDR);
    }

    *output_length_in_units_ptr = output_length_in_units;
    return output_block;
}

void
dwarf_dealloc_uncompressed_block(Dwarf_Debug dbg, void * space)
{
    dwarf_dealloc(dbg, space, DW_DLA_STRING);
}


int
dwarf_whatform(Dwarf_Attribute attr,
    Dwarf_Half * return_form, Dwarf_Error * error)
{
    Dwarf_CU_Context cu_context = 0;
    Dwarf_Debug dbg = 0;

    int res  =get_attr_dbg(&dbg,&cu_context, attr,error);
    if (res != DW_DLV_OK) {
        return res;
    }
    *return_form = attr->ar_attribute_form;
    return (DW_DLV_OK);
}


/*
    This function is analogous to dwarf_whatform.
    It returns the attribute in attr instead of
    the form.
*/
int
dwarf_whatattr(Dwarf_Attribute attr,
    Dwarf_Half * return_attr, Dwarf_Error * error)
{
    Dwarf_CU_Context cu_context = 0;
    Dwarf_Debug dbg = 0;

    int res  =get_attr_dbg(&dbg,&cu_context, attr,error);
    if (res != DW_DLV_OK) {
        return res;
    }
    *return_attr = (attr->ar_attribute);
    return DW_DLV_OK;
}


/*  Convert an offset within the local CU into a section-relative
    debug_info (or debug_types) offset.
    See dwarf_global_formref() and dwarf_formref()
    for additional information on conversion rules.
*/
int
dwarf_convert_to_global_offset(Dwarf_Attribute attr,
    Dwarf_Off offset, Dwarf_Off * ret_offset, Dwarf_Error * error)
{
    Dwarf_Debug dbg = 0;
    Dwarf_CU_Context cu_context = 0;
    int res = 0;

    res  = get_attr_dbg(&dbg,&cu_context,attr,error);
    if (res != DW_DLV_OK) {
        return res;
    }
    switch (attr->ar_attribute_form) {
    case DW_FORM_ref1:
    case DW_FORM_ref2:
    case DW_FORM_ref4:
    case DW_FORM_ref8:
    case DW_FORM_ref_udata:
        /*  It is a cu-local offset. Convert to section-global. */
        /*  It would be nice to put some code to check
            legality of the offset */
        /*  cc_debug_offset always has any DWP Package File
            offset included (when the cu_context created)
            so there is no extra work for DWP.

        /*  globalize the offset */
        offset += cu_context->cc_debug_offset;

        break;

    case DW_FORM_ref_addr:
        /*  This offset is defined to be debug_info global already, so
            use this value unaltered.

            Since a DWP package file is not relocated there
            is no way that this reference offset to an address in
            any other CU can be correct for a DWP Package File offset
            */
        break;

    default:
        _dwarf_error(dbg, error, DW_DLE_BAD_REF_FORM);
        return (DW_DLV_ERROR);
    }

    *ret_offset = (offset);
    return DW_DLV_OK;
}


/*  A global offset cannot be returned by this interface:
    see dwarf_global_formref().

    DW_FORM_ref_addr is considered an incorrect form
    for this call because DW_FORM_ref_addr is a global-offset into
    the debug_info section.

    For the same reason DW_FORM_data4/data8 are not returned
    from this function.

    For the same reason DW_FORM_sec_offset is not returned
    from this function, DW_FORM_sec_offset is a global offset
    (to various sections, not a CU relative offset.

    DW_FORM_ref_addr has a value which was documented in
    DWARF2 as address-size but which was always an offset
    so should have always been offset size (wording
    corrected in DWARF3).
    The dwarfstd.org FAQ "How big is a DW_FORM_ref_addr?"
    suggested all should use offset-size, but that suggestion
    seems to have been ignored in favor of doing what the
    DWARF2 and 3 standards actually say.

    November, 2010: *ret_offset is always set now.
    Even in case of error.
    Set to zero for most errors, but for
        DW_DLE_ATTR_FORM_OFFSET_BAD
    *ret_offset is set to the bad offset.

    DW_FORM_addrx
    DW_FORM_strx
    DW_FORM_GNU_addr_index
    DW_FORM_GNU_str_index
    are not references to .debug_info/.debug_types,
    so they are not allowed here. */


int
dwarf_formref(Dwarf_Attribute attr,
   Dwarf_Off * ret_offset,
   Dwarf_Error * error)
{
    Dwarf_Debug dbg = 0;
    Dwarf_Unsigned offset = 0;
    Dwarf_CU_Context cu_context = 0;
    Dwarf_Unsigned maximumoffset = 0;
    int res = DW_DLV_ERROR;

    *ret_offset = 0;
    res  = get_attr_dbg(&dbg,&cu_context,attr,error);
    if (res != DW_DLV_OK) {
        return res;
    }
    switch (attr->ar_attribute_form) {

    case DW_FORM_ref1:
        offset = *(Dwarf_Small *) attr->ar_debug_ptr;
        break;

    case DW_FORM_ref2:
        READ_UNALIGNED(dbg, offset, Dwarf_Unsigned,
            attr->ar_debug_ptr, sizeof(Dwarf_Half));
        break;

    case DW_FORM_ref4:
        READ_UNALIGNED(dbg, offset, Dwarf_Unsigned,
            attr->ar_debug_ptr, sizeof(Dwarf_ufixed));
        break;

    case DW_FORM_ref8:
        READ_UNALIGNED(dbg, offset, Dwarf_Unsigned,
            attr->ar_debug_ptr, sizeof(Dwarf_Unsigned));
        break;

    case DW_FORM_ref_udata:
        offset = _dwarf_decode_u_leb128(attr->ar_debug_ptr, NULL);
        break;
    case DW_FORM_ref_sig8:
        /*  We cannot handle this here.
            The reference is to .debug_types
            not a .debug_info CU local offset. */
        _dwarf_error(dbg, error, DW_DLE_REF_SIG8_NOT_HANDLED);
        return (DW_DLV_ERROR);
    default:
        _dwarf_error(dbg, error, DW_DLE_BAD_REF_FORM);
        return (DW_DLV_ERROR);
    }

    /* Check that offset is within current cu portion of .debug_info. */

    maximumoffset = cu_context->cc_length +
        cu_context->cc_length_size +
        cu_context->cc_extension_size;
    if (offset >= maximumoffset) {
        /*  For the DW_TAG_compile_unit is legal to have the
            DW_AT_sibling attribute outside the current cu portion of
            .debug_info.
            In other words, sibling points to the end of the CU.
            It is used for precompiled headers.
            The valid condition will be: 'offset == maximumoffset'. */
        Dwarf_Half tag = 0;
        if (DW_DLV_OK != dwarf_tag(attr->ar_die,&tag,error)) {
            _dwarf_error(dbg, error, DW_DLE_DIE_BAD);
            return (DW_DLV_ERROR);
        }

        if (DW_TAG_compile_unit != tag &&
            DW_AT_sibling != attr->ar_attribute &&
            offset > maximumoffset) {
            _dwarf_error(dbg, error, DW_DLE_ATTR_FORM_OFFSET_BAD);
            /* Return the incorrect offset for better error reporting */
            *ret_offset = (offset);
            return (DW_DLV_ERROR);
        }
    }
    *ret_offset = (offset);
    return DW_DLV_OK;
}

static int
_dwarf_formsig8_internal(Dwarf_Attribute attr,
    int formexpected,
    int formerrnum,
    Dwarf_Sig8 * returned_sig_bytes,
    Dwarf_Error*     error)
{
    Dwarf_Debug dbg = 0;
    Dwarf_Unsigned field_end_offset = 0;
    Dwarf_CU_Context cu_context = 0;
    Dwarf_Small *dataptr = 0;

    int res  = get_attr_dbg(&dbg,&cu_context,attr,error);
    if (res != DW_DLV_OK) {
        return res;
    }

    if (attr->ar_attribute_form != formexpected ) {
        _dwarf_error(dbg, error, formerrnum);
        return (DW_DLV_ERROR);
    }

    dataptr = cu_context->cc_is_info? dbg->de_debug_info.dss_data:
        dbg->de_debug_types.dss_data;

    field_end_offset = attr->ar_debug_ptr + sizeof(Dwarf_Sig8) -
        (dataptr + cu_context->cc_debug_offset);
    /* Check that offset is within current cu portion of .debug_info. */
    if (field_end_offset > cu_context->cc_length +
        cu_context->cc_length_size + cu_context->cc_extension_size) {
        _dwarf_error(dbg, error, DW_DLE_ATTR_FORM_OFFSET_BAD);
        return (DW_DLV_ERROR);
    }

    memcpy(returned_sig_bytes, attr->ar_debug_ptr,
        sizeof(Dwarf_Sig8));
    return DW_DLV_OK;
}

int
dwarf_formsig8_const(Dwarf_Attribute attr,
    Dwarf_Sig8 * returned_sig_bytes,
    Dwarf_Error* error)
{
    _dwarf_formsig8_internal(attr, DW_FORM_data8,
        DW_DLE_ATTR_FORM_NOT_DATA8,
        returned_sig_bytes,error);
}

/*  dwarf_formsig8 returns in the caller-provided 8 byte area
    the 8 bytes of a DW_FORM_ref_sig8 (copying the bytes
    directly to the caller).  Not a string, an 8 byte
    MD5 hash.  This function is new in DWARF4 libdwarf.
*/
int
dwarf_formsig8(Dwarf_Attribute attr,
    Dwarf_Sig8 * returned_sig_bytes,
    Dwarf_Error* error)
{
    _dwarf_formsig8_internal(attr, DW_FORM_ref_sig8,
        DW_DLE_BAD_REF_SIG8_FORM,
        returned_sig_bytes,error);
}




/*  Since this returns section-relative debug_info offsets,
    this can represent all REFERENCE forms correctly
    and allows all applicable forms.

    DW_FORM_ref_addr has a value which was documented in
    DWARF2 as address-size but which was always an offset
    so should have always been offset size (wording
    corrected in DWARF3).
    gcc and Go and libdwarf producer code
    define the length of the value of DW_FORM_ref_addr
    per the version. So for V2 it is address-size and V3 and later
    it is offset-size.

    See the DWARF4 document for the 3 cases fitting
    reference forms.  The caller must determine which section the
    reference 'points' to.  The function added in November 2009,
    dwarf_get_form_class(), helps in this regard.  */

int
dwarf_global_formref(Dwarf_Attribute attr,
    Dwarf_Off * ret_offset, Dwarf_Error * error)
{
    Dwarf_Debug dbg = 0;
    Dwarf_Unsigned offset = 0;
    Dwarf_CU_Context cu_context = 0;
    Dwarf_Half context_version = 0;

    int res  = get_attr_dbg(&dbg,&cu_context,attr,error);
    if (res != DW_DLV_OK) {
        return res;
    }
    context_version = cu_context->cc_version_stamp;
    switch (attr->ar_attribute_form) {

    case DW_FORM_ref1:
        offset = *(Dwarf_Small *) attr->ar_debug_ptr;
        goto fixoffset;

    case DW_FORM_ref2:
        READ_UNALIGNED(dbg, offset, Dwarf_Unsigned,
            attr->ar_debug_ptr, sizeof(Dwarf_Half));
        goto fixoffset;

    case DW_FORM_ref4:
        READ_UNALIGNED(dbg, offset, Dwarf_Unsigned,
            attr->ar_debug_ptr, sizeof(Dwarf_ufixed));
        goto fixoffset;

    case DW_FORM_ref8:
        READ_UNALIGNED(dbg, offset, Dwarf_Unsigned,
            attr->ar_debug_ptr, sizeof(Dwarf_Unsigned));
        goto fixoffset;

    case DW_FORM_ref_udata:
        {
        offset = _dwarf_decode_u_leb128(attr->ar_debug_ptr, NULL);

        fixoffset: /* we have a local offset, make it global */

        /* check legality of offset */
        if (offset >= cu_context->cc_length +
            cu_context->cc_length_size +
            cu_context->cc_extension_size) {
            _dwarf_error(dbg, error, DW_DLE_ATTR_FORM_OFFSET_BAD);
            return (DW_DLV_ERROR);
        }

        /* globalize the offset */
        offset += cu_context->cc_debug_offset;
        }
        break;

    /*  The DWARF2 document did not make clear that
        DW_FORM_data4( and 8) were references with
        global offsets to some section.
        That was first clearly documented in DWARF3.
        In DWARF4 these two forms are no longer references. */
    case DW_FORM_data4:
        if (context_version == DW_CU_VERSION4) {
            _dwarf_error(dbg, error, DW_DLE_NOT_REF_FORM);
            return (DW_DLV_ERROR);
        }
        READ_UNALIGNED(dbg, offset, Dwarf_Unsigned,
            attr->ar_debug_ptr, sizeof(Dwarf_ufixed));
        /* The offset is global. */
        break;
    case DW_FORM_data8:
        if (context_version == DW_CU_VERSION4) {
            _dwarf_error(dbg, error, DW_DLE_NOT_REF_FORM);
            return (DW_DLV_ERROR);
        }
        READ_UNALIGNED(dbg, offset, Dwarf_Unsigned,
            attr->ar_debug_ptr, sizeof(Dwarf_Unsigned));
        /* The offset is global. */
        break;
    case DW_FORM_ref_addr:
        {
            /*  In Dwarf V2 DW_FORM_ref_addr was defined
                as address-size even though it is a .debug_info
                offset.  Fixed in Dwarf V3 to be offset-size.
                */
            unsigned length_size = 0;
            if (context_version == 2) {
                length_size = cu_context->cc_address_size;
            } else {
                length_size = cu_context->cc_length_size;
            }
            if (length_size == 4) {
                READ_UNALIGNED(dbg, offset, Dwarf_Unsigned,
                    attr->ar_debug_ptr, sizeof(Dwarf_ufixed));
            } else if (length_size == 8) {
                READ_UNALIGNED(dbg, offset, Dwarf_Unsigned,
                    attr->ar_debug_ptr, sizeof(Dwarf_Unsigned));
            } else {
                _dwarf_error(dbg, error, DW_DLE_FORM_SEC_OFFSET_LENGTH_BAD);
                return (DW_DLV_ERROR);
            }
        }
        break;
    case DW_FORM_sec_offset:
    case DW_FORM_GNU_ref_alt:  /* 2013 GNU extension */
    case DW_FORM_GNU_strp_alt:  /* 2013 GNU extension */
        {
            /*  DW_FORM_sec_offset first exists in DWARF4.*/
            /*  It is up to the caller to know what the offset
                of DW_FORM_sec_offset refers to,
                the offset is not going to refer to .debug_info! */
            unsigned length_size = cu_context->cc_length_size;
            if (length_size == 4) {
                READ_UNALIGNED(dbg, offset, Dwarf_Unsigned,
                    attr->ar_debug_ptr, sizeof(Dwarf_ufixed));
            } else if (length_size == 8) {
                READ_UNALIGNED(dbg, offset, Dwarf_Unsigned,
                    attr->ar_debug_ptr, sizeof(Dwarf_Unsigned));
            } else {
                _dwarf_error(dbg, error, DW_DLE_FORM_SEC_OFFSET_LENGTH_BAD);
                return (DW_DLV_ERROR);
            }
        }
        break;
    case DW_FORM_ref_sig8: /* FIXME */
        /*  We cannot handle this yet.
            The reference is to .debug_types, and
            this function only returns an offset in
            .debug_info at this point. */
        _dwarf_error(dbg, error, DW_DLE_REF_SIG8_NOT_HANDLED);
        return (DW_DLV_ERROR);
    default:
        _dwarf_error(dbg, error, DW_DLE_BAD_REF_FORM);
        return (DW_DLV_ERROR);
    }

    /*  We do not know what section the offset refers to, so
        we have no way to check it for correctness. */
    *ret_offset = offset;
    return DW_DLV_OK;
}

/*  Part of DebugFission.  So a consumer can get the index when
    the object with the actual debug_addr  is
    elsewhere.  New May 2014*/
int
dwarf_get_debug_addr_index(Dwarf_Attribute attr,
    Dwarf_Unsigned * return_index,
    Dwarf_Error * error)
{
    int theform = attr->ar_attribute_form;
    if (theform == DW_FORM_GNU_addr_index ||
        theform == DW_FORM_addrx) {
        Dwarf_Unsigned index = 0;
        Dwarf_Word uleblen = 0;
        Dwarf_Small *info_ptr = attr->ar_debug_ptr;
        index = _dwarf_decode_u_leb128(info_ptr,&uleblen);
        *return_index = index;
        return DW_DLV_OK;
    }

    {
        Dwarf_CU_Context cu_context = 0;
        Dwarf_Debug dbg = 0;
        int res  = get_attr_dbg(&dbg,&cu_context,attr,error);
        if (res != DW_DLV_OK) {
            return res;
        }
        _dwarf_error(dbg, error, DW_DLE_ATTR_FORM_NOT_ADDR_INDEX);
    }
    return (DW_DLV_ERROR);
}

/*  Part of DebugFission.  So a dwarf dumper application
    can get the index and print it for the user.
    A convenience function.  New May 2014*/
int
dwarf_get_debug_str_index(Dwarf_Attribute attr,
    Dwarf_Unsigned *return_index,
    Dwarf_Error *error)
{
    int theform = attr->ar_attribute_form;
    if (theform == DW_FORM_strx ||
        theform == DW_FORM_GNU_str_index) {
        Dwarf_Unsigned index = 0;
        Dwarf_Word uleblen = 0;
        Dwarf_Small *info_ptr = attr->ar_debug_ptr;
        index = _dwarf_decode_u_leb128(info_ptr,&uleblen);
        *return_index = index;
        return DW_DLV_OK;
    }
    {
        Dwarf_CU_Context cu_context = 0;
        Dwarf_Debug dbg = 0;
        int res  = get_attr_dbg(&dbg,&cu_context,attr,error);
        if (res != DW_DLV_OK) {
            return res;
        }
        _dwarf_error(dbg, error, DW_DLE_ATTR_FORM_NOT_ADDR_INDEX);
    }
    return (DW_DLV_ERROR);
}



int
dwarf_formaddr(Dwarf_Attribute attr,
    Dwarf_Addr * return_addr, Dwarf_Error * error)
{
    Dwarf_Debug dbg = 0;
    Dwarf_Addr ret_addr = 0;
    Dwarf_CU_Context cu_context = 0;

    int res  = get_attr_dbg(&dbg,&cu_context,attr,error);
    if (res != DW_DLV_OK) {
        return res;
    }
    if (attr->ar_attribute_form == DW_FORM_GNU_addr_index ||
        attr->ar_attribute_form == DW_FORM_addrx) {
        Dwarf_Addr addr_out = 0;
        res = _dwarf_extract_address_from_debug_addr(dbg,
            cu_context,
            attr->ar_debug_ptr,
            &addr_out,
            error);

        if (res != DW_DLV_OK) {
            return res;
        }
        *return_addr = addr_out;
        return (DW_DLV_OK);
    }
    if (attr->ar_attribute_form == DW_FORM_addr
        /*  || attr->ar_attribute_form == DW_FORM_ref_addr Allowance of
            DW_FORM_ref_addr was a mistake. The value returned in that
            case is NOT an address it is a global debug_info offset (ie,
            not CU-relative offset within the CU in debug_info). The
            Dwarf document refers to it as an address (misleadingly) in
            sec 6.5.4 where it describes the reference form. It is
            address-sized so that the linker can easily update it, but
            it is a reference inside the debug_info section. No longer
            allowed. */
        ) {

        READ_UNALIGNED(dbg, ret_addr, Dwarf_Addr,
            attr->ar_debug_ptr,
            cu_context->cc_address_size);
        *return_addr = ret_addr;
        return (DW_DLV_OK);
    }
    _dwarf_error(dbg, error, DW_DLE_ATTR_FORM_BAD);
    return (DW_DLV_ERROR);
}


int
dwarf_formflag(Dwarf_Attribute attr,
    Dwarf_Bool * ret_bool, Dwarf_Error * error)
{
    Dwarf_CU_Context cu_context = 0;

    if (attr == NULL) {
        _dwarf_error(NULL, error, DW_DLE_ATTR_NULL);
        return (DW_DLV_ERROR);
    }

    cu_context = attr->ar_cu_context;
    if (cu_context == NULL) {
        _dwarf_error(NULL, error, DW_DLE_ATTR_NO_CU_CONTEXT);
        return (DW_DLV_ERROR);
    }

    if (cu_context->cc_dbg == NULL) {
        _dwarf_error(NULL, error, DW_DLE_ATTR_DBG_NULL);
        return (DW_DLV_ERROR);
    }
    if (attr->ar_attribute_form == DW_FORM_flag_present) {
        /*  Implicit means we don't read any data at all. Just
            the existence of the Form does it. DWARF4. */
        *ret_bool = 1;
        return (DW_DLV_OK);
    }

    if (attr->ar_attribute_form == DW_FORM_flag) {
        *ret_bool = *(Dwarf_Small *)(attr->ar_debug_ptr);
        return (DW_DLV_OK);
    }
    _dwarf_error(cu_context->cc_dbg, error, DW_DLE_ATTR_FORM_BAD);
    return (DW_DLV_ERROR);
}

/*  If the form is DW_FORM_constx and the .debug_addr section
    is missing, this returns DW_DLV_ERROR and the error number
    in the Dwarf_Error is  DW_DLE_MISSING_NEEDED_DEBUG_ADDR_SECTION.
    When that arises, a consumer should call
    dwarf_get_debug_addr_index() and use that on the appropriate
    .debug_addr section in the executable or another object. */
int
dwarf_formudata(Dwarf_Attribute attr,
    Dwarf_Unsigned * return_uval, Dwarf_Error * error)
{
    Dwarf_Unsigned ret_value = 0;
    Dwarf_Debug dbg = 0;
    Dwarf_CU_Context cu_context = 0;

    int res  = get_attr_dbg(&dbg,&cu_context,attr,error);
    if (res != DW_DLV_OK) {
        return res;
    }
    switch (attr->ar_attribute_form) {

    case DW_FORM_data1:
        READ_UNALIGNED(dbg, ret_value, Dwarf_Unsigned,
            attr->ar_debug_ptr, sizeof(Dwarf_Small));
        *return_uval = ret_value;
        return DW_DLV_OK;

    /*  READ_UNALIGNED does the right thing as it reads
        the right number bits and generates host order.
        So we can just assign to *return_uval. */
    case DW_FORM_data2:{
        READ_UNALIGNED(dbg, ret_value, Dwarf_Unsigned,
            attr->ar_debug_ptr, sizeof(Dwarf_Half));
        *return_uval = ret_value;
        return DW_DLV_OK;
        }

    case DW_FORM_data4:{
        READ_UNALIGNED(dbg, ret_value, Dwarf_Unsigned,
            attr->ar_debug_ptr,
            sizeof(Dwarf_ufixed));
        *return_uval = ret_value;
        return DW_DLV_OK;
        }

    case DW_FORM_data8:{
        READ_UNALIGNED(dbg, ret_value, Dwarf_Unsigned,
            attr->ar_debug_ptr,
            sizeof(Dwarf_Unsigned));
        *return_uval = ret_value;
        return DW_DLV_OK;
        }
        break;
    /* real udata */
    case DW_FORM_udata:
        ret_value =
            (_dwarf_decode_u_leb128(attr->ar_debug_ptr, NULL));
        *return_uval = ret_value;
        return DW_DLV_OK;


        /*  IRIX bug 583450. We do not allow reading sdata from a udata
            value. Caller can retry, calling sdata */


    default:
        break;
    }
    _dwarf_error(dbg, error, DW_DLE_ATTR_FORM_BAD);
    return (DW_DLV_ERROR);
}


int
dwarf_formsdata(Dwarf_Attribute attr,
    Dwarf_Signed * return_sval, Dwarf_Error * error)
{
    Dwarf_Signed ret_value = 0;
    Dwarf_Debug dbg = 0;
    Dwarf_CU_Context cu_context = 0;

    int res  = get_attr_dbg(&dbg,&cu_context,attr,error);
    if (res != DW_DLV_OK) {
        return res;
    }
    switch (attr->ar_attribute_form) {

    case DW_FORM_data1:
        *return_sval = (*(Dwarf_Sbyte *) attr->ar_debug_ptr);
        return DW_DLV_OK;

    /*  READ_UNALIGNED does not sign extend.
        So we have to use a cast to get the
        value sign extended in the right way for each case. */
    case DW_FORM_data2:{
        READ_UNALIGNED(dbg, ret_value, Dwarf_Signed,
            attr->ar_debug_ptr,
            sizeof(Dwarf_Shalf));
        *return_sval = (Dwarf_Shalf) ret_value;
        return DW_DLV_OK;

        }

    case DW_FORM_data4:{
        READ_UNALIGNED(dbg, ret_value, Dwarf_Signed,
            attr->ar_debug_ptr,
            sizeof(Dwarf_sfixed));
        *return_sval = (Dwarf_sfixed) ret_value;
        return DW_DLV_OK;
        }

    case DW_FORM_data8:{
        READ_UNALIGNED(dbg, ret_value, Dwarf_Signed,
            attr->ar_debug_ptr,
            sizeof(Dwarf_Signed));
        *return_sval = (Dwarf_Signed) ret_value;
        return DW_DLV_OK;
        }

    case DW_FORM_sdata:
        ret_value =
            (_dwarf_decode_s_leb128(attr->ar_debug_ptr, NULL));
        *return_sval = ret_value;
        return DW_DLV_OK;

        /* IRIX bug 583450. We do not allow reading sdata from a udata
            value. Caller can retry, calling udata */

    default:
        break;
    }
    _dwarf_error(dbg, error, DW_DLE_ATTR_FORM_BAD);
    return DW_DLV_ERROR;
}


int
dwarf_formblock(Dwarf_Attribute attr,
    Dwarf_Block ** return_block, Dwarf_Error * error)
{
    Dwarf_CU_Context cu_context = 0;
    Dwarf_Debug dbg = 0;
    Dwarf_Unsigned length = 0;
    Dwarf_Small *data = 0;
    Dwarf_Word leb128_length = 0;
    Dwarf_Block *ret_block = 0;
    Dwarf_Small *dataptr = 0;

    int res  = get_attr_dbg(&dbg,&cu_context,attr,error);
    if (res != DW_DLV_OK) {
        return res;
    }
    switch (attr->ar_attribute_form) {

    case DW_FORM_block1:
        length = *(Dwarf_Small *) attr->ar_debug_ptr;
        data = attr->ar_debug_ptr + sizeof(Dwarf_Small);
        break;

    case DW_FORM_block2:
        READ_UNALIGNED(dbg, length, Dwarf_Unsigned,
            attr->ar_debug_ptr, sizeof(Dwarf_Half));
        data = attr->ar_debug_ptr + sizeof(Dwarf_Half);
        break;

    case DW_FORM_block4:
        READ_UNALIGNED(dbg, length, Dwarf_Unsigned,
            attr->ar_debug_ptr, sizeof(Dwarf_ufixed));
        data = attr->ar_debug_ptr + sizeof(Dwarf_ufixed);
        break;

    case DW_FORM_block:
        length = _dwarf_decode_u_leb128(attr->ar_debug_ptr,
            &leb128_length);
        data = attr->ar_debug_ptr + leb128_length;
        break;

    default:
        _dwarf_error(cu_context->cc_dbg, error, DW_DLE_ATTR_FORM_BAD);
        return (DW_DLV_ERROR);
    }

    /* Check that block lies within current cu in .debug_info. */
    dataptr = cu_context->cc_is_info? dbg->de_debug_info.dss_data:
        dbg->de_debug_types.dss_data;

    if (attr->ar_debug_ptr + length >=
        dataptr + cu_context->cc_debug_offset +
        cu_context->cc_length + cu_context->cc_length_size +
        cu_context->cc_extension_size) {
        _dwarf_error(dbg, error, DW_DLE_ATTR_FORM_SIZE_BAD);
        return (DW_DLV_ERROR);
    }

    ret_block = (Dwarf_Block *) _dwarf_get_alloc(dbg, DW_DLA_BLOCK, 1);
    if (ret_block == NULL) {
        _dwarf_error(dbg, error, DW_DLE_ALLOC_FAIL);
        return (DW_DLV_ERROR);
    }

    ret_block->bl_len = length;
    ret_block->bl_data = (Dwarf_Ptr) data;
    ret_block->bl_from_loclist = 0;
    ret_block->bl_section_offset = data - dataptr;


    *return_block = ret_block;
    return (DW_DLV_OK);
}

int
_dwarf_extract_string_offset_via_str_offsets(Dwarf_Debug dbg,
    Dwarf_Small *info_data_ptr,
    Dwarf_Half   attrnum,
    Dwarf_Half   attrform,
    Dwarf_CU_Context cu_context,
    Dwarf_Unsigned *str_sect_offset_out,
    Dwarf_Error *error)
{
    Dwarf_Unsigned offsettostr= 0;
    Dwarf_Unsigned offset_base = 0;
    Dwarf_Word leb_len = 0;
    Dwarf_Unsigned index_to_offset_entry = 0;
    Dwarf_Unsigned offsetintable = 0;
    Dwarf_Unsigned end_offsetintable = 0;
    Dwarf_Unsigned strofflen = 0;
    int res = 0;

    res = _dwarf_load_section(dbg, &dbg->de_debug_str_offsets,error);
    if (res != DW_DLV_OK) {
        return res;
    }
    index_to_offset_entry = (_dwarf_decode_u_leb128(info_data_ptr, &leb_len));
    /*  DW_FORM_GNU_str_index has no 'base' value.
        DW_FORM_strx has a base value
        for the offset table */
    if( attrform == DW_FORM_strx) {
        res = _dwarf_get_string_base_attr_value(dbg,cu_context,
            &offset_base,error);
        if (res != DW_DLV_OK) {
            return res;
        }
    }

    offsetintable = (index_to_offset_entry*cu_context->cc_length_size )
        + offset_base;
    {
        Dwarf_Unsigned fissoff = 0;
        Dwarf_Unsigned size = 0;
        fissoff = _dwarf_get_dwp_extra_offset(&cu_context->cc_dwp_offsets,
            DW_SECT_STR_OFFSETS, &size);
        offsetintable += fissoff;
    }
    end_offsetintable = offsetintable + cu_context->cc_length_size;
    /* The offsets table is a series of offset-size entries. */
    if ((end_offsetintable) >= dbg->de_debug_str_offsets.dss_size ) {
        _dwarf_error(dbg, error, DW_DLE_ATTR_FORM_SIZE_BAD);
        return (DW_DLV_ERROR);
    }

    /* Now read the string offset from the offset table. */
    READ_UNALIGNED(dbg,offsettostr,Dwarf_Unsigned,
        dbg->de_debug_str_offsets.dss_data + offsetintable,
        cu_context->cc_length_size);
    *str_sect_offset_out = offsettostr;
    return DW_DLV_OK;
}


/* Contrary to pre-2005 documentation,
   The string pointer returned thru return_str must
   never have dwarf_dealloc() applied to it.
   Documentation fixed July 2005.
*/
int
dwarf_formstring(Dwarf_Attribute attr,
    char **return_str, Dwarf_Error * error)
{
    Dwarf_CU_Context cu_context = 0;
    Dwarf_Debug dbg = 0;
    Dwarf_Unsigned offset = 0;
    int res = DW_DLV_ERROR;
    Dwarf_Small *dataptr = 0;
    Dwarf_Small *infoptr = attr->ar_debug_ptr;
    res  = get_attr_dbg(&dbg,&cu_context,attr,error);
    if (res != DW_DLV_OK) {
        return res;
    }
    dataptr = cu_context->cc_is_info?
        dbg->de_debug_info.dss_data:
        dbg->de_debug_types.dss_data;
    if (attr->ar_attribute_form == DW_FORM_string) {

        void *begin = attr->ar_debug_ptr;

        if (0 == dbg->de_assume_string_in_bounds) {
            /* Check that string lies within current cu in .debug_info.
            */

            void *end = dataptr +
                cu_context->cc_debug_offset +
                cu_context->cc_length + cu_context->cc_length_size +
                cu_context->cc_extension_size;
            if (0 == _dwarf_string_valid(begin, end)) {
                _dwarf_error(dbg, error, DW_DLE_ATTR_FORM_SIZE_BAD);
                return (DW_DLV_ERROR);
            }
        }
        *return_str = (char *) (begin);
        return DW_DLV_OK;
    }
    if (attr->ar_attribute_form == DW_FORM_GNU_strp_alt) {
        /* Unsure what this is really. FIXME */
        *return_str = (char *)"<DW_FORM_GNU_strp_alt not handled>";
        return DW_DLV_OK;
    }
    if (attr->ar_attribute_form == DW_FORM_GNU_str_index ||
        attr->ar_attribute_form == DW_FORM_strx) {
        Dwarf_Unsigned offsettostr= 0;
        res = _dwarf_extract_string_offset_via_str_offsets(dbg,
            infoptr,
            attr->ar_attribute,
            attr->ar_attribute_form,
            cu_context,
            &offsettostr,
            error);
        offset = offsettostr;
        /* FALL THRU */
    } else {
        if (attr->ar_attribute_form == DW_FORM_strp) {
            READ_UNALIGNED(dbg, offset, Dwarf_Unsigned,
                infoptr,
                cu_context->cc_length_size);
        }
    }
    if (attr->ar_attribute_form == DW_FORM_strp ||
        attr->ar_attribute_form == DW_FORM_GNU_str_index ||
        attr->ar_attribute_form == DW_FORM_strx) {
        /* The 'offset' into .debug_str is set. */
        res = _dwarf_load_section(dbg, &dbg->de_debug_str,error);
        if (res != DW_DLV_OK) {
            return res;
        }
        if (offset >= dbg->de_debug_str.dss_size) {
            /*  Badly damaged DWARF here. */
            _dwarf_error(dbg, error, DW_DLE_STRP_OFFSET_BAD);
            return (DW_DLV_ERROR);
        }
        if (0 == dbg->de_assume_string_in_bounds) {
            /* Check that string lies within its   .debug_str.  */
            void *end = dbg->de_debug_str.dss_data +
                dbg->de_debug_str.dss_size;
            void*begin = dbg->de_debug_str.dss_data + offset;
            if (0 == _dwarf_string_valid(begin, end)) {
                _dwarf_error(dbg, error, DW_DLE_STRP_OFFSET_BAD);
                return (DW_DLV_ERROR);
            }
        }

        /*  Ensure the offset lies within the .debug_str */
        if (offset >= dbg->de_debug_str.dss_size) {
            _dwarf_error(dbg, error, DW_DLE_DEBUG_STR_OFFSET_BAD);
            return (DW_DLV_ERROR);
        }

        *return_str = (char *) (dbg->de_debug_str.dss_data + offset);
        return DW_DLV_OK;
    }

    _dwarf_error(dbg, error, DW_DLE_ATTR_FORM_BAD);
    return (DW_DLV_ERROR);
}


int
dwarf_formexprloc(Dwarf_Attribute attr,
    Dwarf_Unsigned * return_exprlen,
    Dwarf_Ptr  * block_ptr,
    Dwarf_Error * error)
{
    Dwarf_Debug dbg = 0;
    Dwarf_CU_Context cu_context = 0;

    int res  = get_attr_dbg(&dbg,&cu_context,attr,error);
    if (res != DW_DLV_OK) {
        return res;
    }
    if (dbg == NULL) {
        _dwarf_error(NULL, error, DW_DLE_ATTR_DBG_NULL);
        return (DW_DLV_ERROR);
    }
    if (attr->ar_attribute_form == DW_FORM_exprloc ) {
        Dwarf_Die die = 0;
        Dwarf_Word leb_len = 0;
        Dwarf_Unsigned exprlen =
            (_dwarf_decode_u_leb128(attr->ar_debug_ptr, &leb_len));
        Dwarf_Small * addr = attr->ar_debug_ptr;
        /*  Is the block entirely in the section, or is
            there bug somewhere? */
        die = attr->ar_die;
        if (_dwarf_reference_outside_section(die,
            (Dwarf_Small *)addr, ((Dwarf_Small *)addr)+exprlen +leb_len)) {
            _dwarf_error(dbg, error,DW_DLE_ATTR_OUTSIDE_SECTION);
            return DW_DLV_ERROR;
        }
        *return_exprlen = exprlen;
        *block_ptr = addr + leb_len;
        return DW_DLV_OK;

    }
    _dwarf_error(dbg, error, DW_DLE_ATTR_EXPRLOC_FORM_BAD);
    return (DW_DLV_ERROR);
}
