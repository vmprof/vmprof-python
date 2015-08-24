/*
  Copyright (c) 2009-2010 David Anderson.
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:
  * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
  * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.
  * Neither the name of the example nor the
    names of its contributors may be used to endorse or promote products
    derived from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY David Anderson ''AS IS'' AND ANY
  EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  DISCLAIMED. IN NO EVENT SHALL David Anderson BE LIABLE FOR ANY
  DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/
/*  simplereader.c
    This is an example of code reading dwarf .debug_frame.
    It is kept as simple as possible to expose essential features.
    It does not do all possible error reporting or error handling.

    It specifically calls dwarf_expand_frame_instructions()
    to verify that works without crashing!

    To use, try
        make
        ./frame1 frame1
*/
#include <sys/types.h> /* For open() */
#include <sys/stat.h>  /* For open() */
#include <fcntl.h>     /* For open() */
#include <stdlib.h>     /* For exit() */
#include <unistd.h>     /* For close() */
#include <string.h>     /* For strcmp* */
#include <stdio.h>
#include <errno.h>
#include "dwarf.h"
#include "libdwarf.h"


static void read_frame_data(Dwarf_Debug dbg);
static void print_fde_instrs(Dwarf_Debug dbg, Dwarf_Fde fde,
   int fdenum, Dwarf_Error *error);
static void print_regtable(Dwarf_Fde fde,Dwarf_Regtable3 *tab3,int oldrulecount,
    Dwarf_Debug dbg,Dwarf_Error *error);
static void
print_cie_instrs(Dwarf_Debug dbg,Dwarf_Cie cie,Dwarf_Error *error);


#define UNDEF_VAL 2000
#define SAME_VAL 2001
#define CFA_VAL 2002


int
main(int argc, char **argv)
{

    Dwarf_Debug dbg = 0;
    int fd = -1;
    const char *filepath = "<stdin>";
    int res = DW_DLV_ERROR;
    Dwarf_Error error;
    Dwarf_Handler errhand = 0;
    Dwarf_Ptr errarg = 0;
    int regtabrulecount = 0;

    if(argc < 2) {
        fd = 0; /* stdin */
    } else {
        filepath = argv[1];
        fd = open(filepath,O_RDONLY);
    }
    if(fd < 0) {
        printf("Failure attempting to open %s\n",filepath);
    }
    res = dwarf_init(fd,DW_DLC_READ,errhand,errarg, &dbg,&error);
    if(res != DW_DLV_OK) {
        printf("Giving up, dwarf_init failed, cannot do DWARF processing\n");
        exit(1);
    }
    /*  Do this setting after init before any real operations.
        These return the old values, but here we do not
        need to know the old values.  The sizes and
        values here are higher than most ABIs and entirely
        arbitrary.

        The setting of initial_value to
        the same as undefined-value (the other possible choice being
        same-value) is arbitrary, different ABIs do differ, and
        you have to know which is right. */
    regtabrulecount=1999;
    dwarf_set_frame_undefined_value(dbg, UNDEF_VAL);
    dwarf_set_frame_rule_initial_value(dbg, UNDEF_VAL);
    dwarf_set_frame_same_value(dbg,SAME_VAL);
    dwarf_set_frame_cfa_value(dbg,CFA_VAL);
    dwarf_set_frame_rule_table_size(dbg,regtabrulecount);

    read_frame_data(dbg);
    res = dwarf_finish(dbg,&error);
    if(res != DW_DLV_OK) {
        printf("dwarf_finish failed!\n");
    }
    close(fd);
    return 0;
}

static void
read_frame_data(Dwarf_Debug dbg)
{
    Dwarf_Error error;
    Dwarf_Signed cie_element_count = 0;
    Dwarf_Signed fde_element_count = 0;
    Dwarf_Cie *cie_data = 0;
    Dwarf_Fde *fde_data = 0;
    int res = DW_DLV_ERROR;
    Dwarf_Signed fdenum = 0;


    res = dwarf_get_fde_list(dbg,&cie_data,&cie_element_count,
        &fde_data,&fde_element_count,&error);
    if(res == DW_DLV_NO_ENTRY) {
        printf("No frame data present ");
        exit(0);
    }
    if( res == DW_DLV_ERROR) {
        printf("Error reading frame data ");
        exit(1);
    }
    printf( "%" DW_PR_DSd " cies present. "
        "%" DW_PR_DSd " fdes present. \n",
        cie_element_count,fde_element_count);
    /*if(fdenum >= fde_element_count) {
        printf("Want fde %d but only %" DW_PR_DSd " present\n",fdenum,
            fde_element_count);
        exit(1);
    }*/

    for(fdenum = 0; fdenum < fde_element_count; ++fdenum) {
        Dwarf_Cie cie = 0;
        printf("Print cie of fde %" DW_PR_DSd  "\n",fdenum);
        res = dwarf_get_cie_of_fde(fde_data[fdenum],&cie,&error);
        if(res != DW_DLV_OK) {
            printf("Error accessing fdenum %" DW_PR_DSd
                " to get its cie\n",fdenum);
            exit(1);
        }
        print_cie_instrs(dbg,cie,&error);
        printf("Print fde %" DW_PR_DSd  "\n",fdenum);
        print_fde_instrs(dbg,fde_data[fdenum],fdenum,&error);
    }

    /* Done with the data. */
    dwarf_fde_cie_list_dealloc(dbg,cie_data,cie_element_count,
        fde_data, fde_element_count);
    return;
}
static void
print_cie_instrs(Dwarf_Debug dbg,Dwarf_Cie cie,Dwarf_Error *error)
{
    int res = DW_DLV_ERROR;
    Dwarf_Unsigned bytes_in_cie = 0;
    Dwarf_Small version = 0;
    char *augmentation = 0;
    Dwarf_Unsigned code_alignment_factor = 0;
    Dwarf_Signed data_alignment_factor = 0;
    Dwarf_Half return_address_register_rule = 0;
    Dwarf_Ptr instrp = 0;
    Dwarf_Unsigned instr_len = 0;

    res = dwarf_get_cie_info(cie,&bytes_in_cie,
        &version, &augmentation, &code_alignment_factor,
        &data_alignment_factor, &return_address_register_rule,
        &instrp,&instr_len,error);
    if(res != DW_DLV_OK) {
        printf("Unable to get cie info!\n");
        exit(1);
    }
}

static void
print_frame_instrs(Dwarf_Debug dbg,Dwarf_Frame_Op *frame_op_list,
  Dwarf_Signed frame_op_count)
{
    Dwarf_Signed i = 0;
    printf("Base op. Ext op. Reg. Offset. Instr-offset.\n");
    for(i = 0; i < frame_op_count; ++i) {
        printf("[%" DW_PR_DSd "]", i);
        printf(" %d. ", frame_op_list[i].fp_base_op);
        printf(" %d. ", frame_op_list[i].fp_extended_op);
        printf(" %" DW_PR_DSd ". ", frame_op_list[i].fp_offset);
        printf(" 0x%" DW_PR_DUx ". ", frame_op_list[i].fp_instr_offset);
        printf("\n");
    }
}

static void
print_fde_instrs(Dwarf_Debug dbg,
    Dwarf_Fde fde,int fdenum, Dwarf_Error *error)
{
    int res;
    Dwarf_Addr lowpc = 0;
    Dwarf_Unsigned func_length = 0;
    Dwarf_Ptr fde_bytes;
    Dwarf_Unsigned fde_byte_length = 0;
    Dwarf_Off cie_offset = 0;
    Dwarf_Signed cie_index = 0;
    Dwarf_Off fde_offset = 0;
    Dwarf_Addr arbitrary_addr = 0;
    Dwarf_Addr actual_pc = 0;
    Dwarf_Regtable3 tab3;
    int oldrulecount = 0;
    Dwarf_Ptr outinstrs = 0;
    Dwarf_Unsigned instrslen = 0;
    Dwarf_Frame_Op * frame_op_list = 0;
    Dwarf_Signed frame_op_count = 0;
    Dwarf_Cie cie = 0;


    res = dwarf_get_fde_range(fde,&lowpc,&func_length,&fde_bytes,
        &fde_byte_length,&cie_offset,&cie_index,&fde_offset,error);
    if(res != DW_DLV_OK) {
        printf("Problem getting fde range \n");
        exit(1);
    }

    arbitrary_addr = lowpc + (func_length/2);
    printf("function low pc 0x%" DW_PR_DUx
        "  and length 0x%" DW_PR_DUx
        "  and addr we choose 0x%" DW_PR_DUx
        "\n",
        lowpc,func_length,arbitrary_addr);

    /*  1 is arbitrary. We are winding up getting the
        rule count here while leaving things unchanged. */
    oldrulecount = dwarf_set_frame_rule_table_size(dbg,1);
    dwarf_set_frame_rule_table_size(dbg,oldrulecount);

    tab3.rt3_reg_table_size = oldrulecount;
    tab3.rt3_rules = (struct Dwarf_Regtable_Entry3_s *) malloc(
        sizeof(struct Dwarf_Regtable_Entry3_s)* oldrulecount);
    if (!tab3.rt3_rules) {
        printf("Unable to malloc for %d rules\n",oldrulecount);
        exit(1);
    }

    res = dwarf_get_fde_info_for_all_regs3(fde,arbitrary_addr ,
        &tab3,&actual_pc,error);

    if(res != DW_DLV_OK) {
        printf("dwarf_get_fde_info_for_all_regs3 failed!\n");
        exit(1);
    }
    print_regtable(fde,&tab3,oldrulecount,dbg,error);

    res = dwarf_get_fde_instr_bytes(fde,&outinstrs,&instrslen,error);
    if(res != DW_DLV_OK) {
        printf("dwarf_get_fde_instr_bytes failed!\n");
        exit(1);
    }
    res = dwarf_get_cie_of_fde(fde,&cie,error);
    if(res != DW_DLV_OK) {
        printf("Error getting cie from fde\n");
        exit(1);
    }

    res = dwarf_expand_frame_instructions(cie,
        outinstrs,instrslen,&frame_op_list,
        &frame_op_count,error);
    if(res != DW_DLV_OK) {
        printf("dwarf_expand_frame_instructions failed!\n");
        exit(1);
    }
    printf("Frame op count: %" DW_PR_DUu "\n",frame_op_count);
    print_frame_instrs(dbg,frame_op_list,frame_op_count);

    dwarf_dealloc(dbg,frame_op_list, DW_DLA_FRAME_BLOCK);
    free(tab3.rt3_rules);
}

static void
print_reg(int r)
{
   switch(r) {
   case SAME_VAL:
        printf(" %d SAME_VAL ",r);
        break;
   case UNDEF_VAL:
        printf(" %d UNDEF_VAL ",r);
        break;
   case CFA_VAL:
        printf(" %d (CFA) ",r);
        break;
   default:
        printf(" r%d ",r);
        break;
   }
}

static void
print_one_regentry(const char *prefix,Dwarf_Fde fde,Dwarf_Debug dbg,
    int oldrulecount,struct Dwarf_Regtable_Entry3_s *entry,
    Dwarf_Error *  error)
{
    int is_cfa = !strcmp("cfa",prefix);
    printf("%s ",prefix);
    printf("type: %d %s ",
        entry->dw_value_type,
        (entry->dw_value_type == DW_EXPR_OFFSET)? "DW_EXPR_OFFSET":
        (entry->dw_value_type == DW_EXPR_VAL_OFFSET)? "DW_EXPR_VAL_OFFSET":
        (entry->dw_value_type == DW_EXPR_EXPRESSION)? "DW_EXPR_EXPRESSION":
        (entry->dw_value_type == DW_EXPR_VAL_EXPRESSION)?
            "DW_EXPR_VAL_EXPRESSION":
            "Unknown");
    switch(entry->dw_value_type) {
    case DW_EXPR_OFFSET:
        print_reg(entry->dw_regnum);
        printf(" offset_rel? %d ",entry->dw_offset_relevant);
        if(entry->dw_offset_relevant) {
            printf(" offset  %" DW_PR_DSd " " ,
                entry->dw_offset_or_block_len);
            if(is_cfa) {
                printf("defines cfa value");
            } else {
                printf("address of value is CFA plus signed offset");
            }
            if(!is_cfa  && entry->dw_regnum != CFA_VAL) {
                printf(" compiler botch, regnum != CFA_VAL");
            }
        } else {
            printf("value in register");
        }
        break;
    case DW_EXPR_VAL_OFFSET:
        print_reg(entry->dw_regnum);
        printf(" offset  %" DW_PR_DSd " " ,
            entry->dw_offset_or_block_len);
        if(is_cfa) {
            printf("does this make sense? No?");
        } else {
            printf("value at CFA plus signed offset");
        }
        if(!is_cfa  && entry->dw_regnum != CFA_VAL) {
            printf(" compiler botch, regnum != CFA_VAL");
        }
        break;
    case DW_EXPR_EXPRESSION:
        print_reg(entry->dw_regnum);
        printf(" offset_rel? %d ",entry->dw_offset_relevant);
        printf(" offset  %" DW_PR_DSd " " ,
            entry->dw_offset_or_block_len);
        printf("Block ptr set? %s ",entry->dw_block_ptr?"yes":"no");
        printf(" Value is at address given by expr val ");
        /* printf(" block-ptr  0x%" DW_PR_DUx " ",
            (Dwarf_Unsigned)entry->dw_block_ptr); */
        break;
    case DW_EXPR_VAL_EXPRESSION:
        printf(" expression byte len  %" DW_PR_DSd " " ,
            entry->dw_offset_or_block_len);
        printf("Block ptr set? %s ",entry->dw_block_ptr?"yes":"no");
        printf(" Value is expr val ");
        if(!entry->dw_block_ptr) {
            printf("Compiler botch. ");
        }
        /* printf(" block-ptr  0x%" DW_PR_DUx " ",
            (Dwarf_Unsigned)entry->dw_block_ptr); */
        break;
    }
    printf("\n");
}

static void
print_regtable(Dwarf_Fde fde,Dwarf_Regtable3 *tab3,int oldrulecount,
    Dwarf_Debug dbg,Dwarf_Error *error)
{
    int r;
    /* We won't print too much. A bit arbitrary. */
    int max = 10;
    if(max > tab3->rt3_reg_table_size) {
        max = tab3->rt3_reg_table_size;
    }
    print_one_regentry("cfa",fde,dbg,oldrulecount,&tab3->rt3_cfa_rule,
        error);

    for(r = 0; r < max; r++) {
        char rn[30];
        snprintf(rn,sizeof(rn),"reg %d",r);
        print_one_regentry(rn, fde,dbg,oldrulecount,tab3->rt3_rules+r,
            error);
    }


}




