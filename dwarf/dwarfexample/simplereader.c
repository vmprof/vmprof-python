/*
  Copyright (c) 2009-2013 David Anderson.  All rights reserved.

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
    This is an example of code reading dwarf .debug_info.
    It is kept simple to expose essential features.
    It does not do all possible error reporting or error handling.
    It does to a bit of error checking as a help in ensuring
    that some code works properly... for error checks.

    The --names
    option adds some extra printing.

    The --check
    option does some interface and error checking.

    Options new 03 May 2015:
    These options do something different.
    They use specific DWARF5 package file libdwarf operations
    as a way to ensure libdwarf works properly (these
    specials used by the libdwarf regresson test suite).
    Examples given assuming dwp object fissionb-ld-new.dwp
    from the regressiontests
        --tuhash=hashvalue
        example: --tuhash=b0dd19898e8aa823
        It prints a DIE.

        --cuhash=hashvalue
        example: --cuhash=1e9983f631642b1a
        It prints a DIE.

        --cufissionhash=hashvalue
        example: --tufissionhash=1e9983f631642b1a
        It prints the fission data for this hash.

        --tufissionhash=hashvalue
        example: --tufissionhash=b0dd19898e8aa823
        It prints the fission data for this hash.

        --fissionfordie=cunumber
        example: --fissionfordie=5
        For CU number 5 (0 is the initial CU/TU)
        it accesses the CU/TU DIE and then
        uses that DIE to get the fission data.

    To use, try
        make
        ./simplereader simplereader
*/
#include <sys/types.h> /* For open() */
#include <sys/stat.h>  /* For open() */
#include <fcntl.h>     /* For open() */
#include <stdlib.h>     /* For exit() */
#include <unistd.h>     /* For close() */
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include "dwarf.h"
#include "libdwarf.h"

struct srcfilesdata {
    char ** srcfiles;
    Dwarf_Signed srcfilescount;
    int srcfilesres;
};

#define TRUE 1
#define FALSE 0

static void read_cu_list(Dwarf_Debug dbg);
static void print_die_data(Dwarf_Debug dbg, Dwarf_Die print_me,
    int is_info,int level,
    struct srcfilesdata *sf);
static void get_die_and_siblings(Dwarf_Debug dbg, Dwarf_Die in_die,
    int is_info, int in_level,
    struct srcfilesdata *sf);
static void resetsrcfiles(Dwarf_Debug dbg,struct srcfilesdata *sf);

static int namesoptionon = 0;
static int checkoptionon = 0;
#if 0
DW_UT_compile                   0x01  /* DWARF5 */
DW_UT_type                      0x02  /* DWARF5 */
DW_UT_partial                   0x03  /* DWARF5 */
#endif

static int stdrun = TRUE;

static int unittype      = DW_UT_compile;
static Dwarf_Bool g_is_info = TRUE;

/*  dienumberr is used to count DIEs.
    The point is to match fissionfordie. */
static int dienumber = 0;
static int fissionfordie = -1;
/*  These hash representations have to be converted to Dwarf_Sig8
    before use. */
static const  char * cuhash = 0;
static const  char * tuhash = 0;
static const  char * cufissionhash = 0;
static const  char * tufissionhash = 0;

static unsigned  char_to_uns4bit(unsigned char c)
{
    unsigned v;
    if( c >= '0' && c <= '9') {
        v =  c - '0';
    }
    else if( c >= 'a' && c <= 'f') {
        v =  c - 'a' + 10;
    }
    else if( c >= 'A' && c <= 'F') {
        v =  c - 'A' + 10;
    } else {
        printf("Garbage hex char in %c 0x%x\n",c,c);
        exit(1);
    }
    return v;
}

static void
xfrm_to_sig8(const char *cuhash, Dwarf_Sig8 *hash_out)
{
    char localhash[16];
    unsigned hashin_len = strlen(cuhash);
    unsigned fixed_size = sizeof(localhash);
    unsigned init_byte = 0;
    unsigned i;

    memset(localhash,0,fixed_size);
    if (hashin_len > fixed_size) {
        printf("FAIL: argument hash too long, len %u val:\"%s\"\n",hashin_len,
            cuhash);
        exit(1);
    }
    if (hashin_len  < fixed_size) {
        unsigned add_zeros = fixed_size - hashin_len;
        for ( ; add_zeros > 0; add_zeros--) {
            localhash[init_byte] = '0';
            init_byte++;
        }
    }
    for (i = 0; i < hashin_len; ++i,++init_byte) {
        localhash[init_byte] = cuhash[i];
    }

    /*  So now local hash as a full 16 bytes of hex characters with
        any needed leading zeros.
        transform it to 8 byte hex signature */

    for (i = 0; i < sizeof(Dwarf_Sig8) ; ++i) {
        unsigned char hichar = localhash[2*i];
        unsigned char lochar = localhash[2*i+1];
        hash_out->signature[i] =
            (char_to_uns4bit(hichar) << 4)  |
            char_to_uns4bit(lochar);
    }
    printf("Hex key = 0x");
    for (i = 0; i < sizeof(Dwarf_Sig8) ; ++i) {
        unsigned char c = hash_out->signature[i];
        printf("%02x",c);
    }
    printf("\n");
}

static int
startswithextractnum(const char *arg,const char *lookfor, int *numout)
{
    const char *s = 0;
    unsigned prefixlen = strlen(lookfor);
    int v = 0;
    if(strncmp(arg,lookfor,prefixlen)) {
        return FALSE;
    }
    s = arg+prefixlen;
    /*  We are not making any attempt to deal with garbage.
        Pass in good data... */
    v = atoi(s);
    *numout = v;
    return TRUE;
}

static int
startswithextractstring(const char *arg,const char *lookfor,
    const char ** ptrout)
{
    const char *s = 0;
    unsigned prefixlen = strlen(lookfor);
    if(strncmp(arg,lookfor,prefixlen)) {
        return FALSE;
    }
    s = arg+prefixlen;
    *ptrout = strdup(s);
    return TRUE;
}

void
format_sig8_string(Dwarf_Sig8*data, char* str_buf,unsigned
  buf_size)
{
    unsigned i = 0;
    char *cp = str_buf;
    if (buf_size <  19) {
        printf("FAIL: internal coding error in test.\n");
        exit(1);
    }
    strcpy(cp,"0x");
    cp += 2;
    buf_size -= 2;
    for (; i < sizeof(data->signature); ++i,cp+=2,buf_size--) {
        snprintf(cp, buf_size, "%02x",
            (unsigned char)(data->signature[i]));
    }
    return;
}

static void
print_debug_fission_header(struct Dwarf_Debug_Fission_Per_CU_s *fsd)
{
    const char * fissionsec = ".debug_cu_index";
    unsigned i  = 0;
    char str_buf[30];

    if (!fsd || !fsd->pcu_type) {
        /* No fission data. */
        return;
    }
    printf("\n");
    if (!strcmp(fsd->pcu_type,"tu")) {
        fissionsec = ".debug_tu_index";
    }
    printf("  %-19s = %s\n","Fission section",fissionsec);
    printf("  %-19s = 0x%"  DW_PR_XZEROS DW_PR_DUx "\n","Fission index ",
        fsd->pcu_index);
    format_sig8_string(&fsd->pcu_hash,str_buf,sizeof(str_buf));
    printf("  %-19s = %s\n","Fission hash",str_buf);
    /* 0 is always unused. Skip it. */
    printf("  %-19s = %s\n","Fission entries","offset     size        DW_SECTn");
    for( i = 1; i < DW_FISSION_SECT_COUNT; ++i)  {
        const char *nstring = 0;
        Dwarf_Unsigned off = 0;
        Dwarf_Unsigned size = fsd->pcu_size[i];
        int res = 0;
        if (size == 0) {
            continue;
        }
        res = dwarf_get_SECT_name(i,&nstring);
        if (res != DW_DLV_OK) {
            nstring = "Unknown SECT";
        }
        off = fsd->pcu_offset[i];
        printf("  %-19s = 0x%08llx 0x%08llx %2u\n",
            nstring,
            off,size,i);
    }
}


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
    Dwarf_Sig8 hash8;

    if(argc < 2) {
        fd = 0; /* stdin */
    } else {
        int i = 0;
        for(i = 1; i < (argc-1) ; ++i) {
            if(strcmp(argv[i],"--names") == 0) {
                namesoptionon=1;
            } else if(strcmp(argv[i],"--check") == 0) {
                checkoptionon=1;
            } else if(startswithextractstring(argv[i],"--tuhash=",&tuhash)) {
                /* done */
            } else if(startswithextractstring(argv[i],"--cuhash=",&cuhash)) {
                /* done */
            } else if(startswithextractstring(argv[i],"--tufissionhash=",
                &tufissionhash)) {
                /* done */
            } else if(startswithextractstring(argv[i],"--cufissionhash=",
                &cufissionhash)) {
                /* done */
            } else if(startswithextractnum(argv[i],"--isinfo=",&g_is_info)) {
                /* done */
            } else if(startswithextractnum(argv[i],"--type=",&unittype)) {
                /* done */
            } else if(startswithextractnum(argv[i],"--fissionfordie=",
                &fissionfordie)) {
                /* done */
            } else {
                printf("Unknown argument \"%s\" ignored\n",argv[i]);
            }
        }
        filepath = argv[i];
        fd = open(filepath,O_RDONLY);
    }
    if(argc > 2) {
    }
    if(fd < 0) {
        printf("Failure attempting to open \"%s\"\n",filepath);
    }
    res = dwarf_init(fd,DW_DLC_READ,errhand,errarg, &dbg,&error);
    if(res != DW_DLV_OK) {
        printf("Giving up, cannot do DWARF processing\n");
        exit(1);
    }

    if(cuhash) {
        Dwarf_Die die;
        stdrun = FALSE;
        xfrm_to_sig8(cuhash,&hash8);
        printf("\n");
        printf("Getting die for CU key %s\n",cuhash);
        res = dwarf_die_from_hash_signature(dbg,
            &hash8,"cu",
            &die,&error);
        if (res == DW_DLV_OK) {
            printf("Hash found.\n");
            struct srcfilesdata sf;
            sf.srcfilesres = DW_DLV_ERROR;
            sf.srcfiles = 0;
            sf.srcfilescount = 0;
            print_die_data(dbg,die,g_is_info,0,&sf);
            dwarf_dealloc(dbg,die, DW_DLA_DIE);
        } else if (res == DW_DLV_NO_ENTRY) {
            printf("cuhash DW_DLV_NO_ENTRY.\n");
        } else { /* DW_DLV_ERROR */
            printf("cuhash DW_DLV_ERROR %s\n",
                dwarf_errmsg(error));
        }
    }
    if(tuhash) {
        Dwarf_Die die;
        stdrun = FALSE;
        xfrm_to_sig8(tuhash,&hash8);
        printf("\n");
        printf("Getting die for TU key %s\n",tuhash);
        res = dwarf_die_from_hash_signature(dbg,
            &hash8,"tu",
            &die,&error);
        if (res == DW_DLV_OK) {
            printf("Hash found.\n");
            struct srcfilesdata sf;
            sf.srcfilesres = DW_DLV_ERROR;
            sf.srcfiles = 0;
            sf.srcfilescount = 0;
            print_die_data(dbg,die,g_is_info,0,&sf);
            dwarf_dealloc(dbg,die, DW_DLA_DIE);
        } else if (res == DW_DLV_NO_ENTRY) {
            printf("tuhash DW_DLV_NO_ENTRY.\n");
        } else { /* DW_DLV_ERROR */
            printf("tuhash DW_DLV_ERROR %s\n",
                dwarf_errmsg(error));
        }

    }
    if(cufissionhash) {
        Dwarf_Debug_Fission_Per_CU  fisdata;
        stdrun = FALSE;
        memset(&fisdata,0,sizeof(fisdata));
        xfrm_to_sig8(cufissionhash,&hash8);
        printf("\n");
        printf("Getting fission data for CU key %s\n",cufissionhash);
        res = dwarf_get_debugfission_for_key(dbg,
            &hash8,"cu",
            &fisdata,&error);
        if (res == DW_DLV_OK) {
            printf("Hash found.\n");
            print_debug_fission_header(&fisdata);
        } else if (res == DW_DLV_NO_ENTRY) {
            printf("cufissionhash DW_DLV_NO_ENTRY.\n");
        } else { /* DW_DLV_ERROR */
            printf("cufissionhash DW_DLV_ERROR %s\n",
                dwarf_errmsg(error));
        }
    }
    if(tufissionhash) {
        Dwarf_Debug_Fission_Per_CU  fisdata;
        stdrun = FALSE;
        memset(&fisdata,0,sizeof(fisdata));
        xfrm_to_sig8(tufissionhash,&hash8);
        printf("\n");
        printf("Getting fission data for TU key %s\n",tufissionhash);
        res = dwarf_get_debugfission_for_key(dbg,
            &hash8,"tu",
            &fisdata,&error);
        if (res == DW_DLV_OK) {
            printf("Hash found.\n");
            print_debug_fission_header(&fisdata);
        } else if (res == DW_DLV_NO_ENTRY) {
            printf("tufissionhash DW_DLV_NO_ENTRY.\n");
        } else { /* DW_DLV_ERROR */
            printf("tufissionhash DW_DLV_ERROR %s\n",
                dwarf_errmsg(error));
        }
    }
    if (stdrun) {
        read_cu_list(dbg);
    }
    res = dwarf_finish(dbg,&error);
    if(res != DW_DLV_OK) {
        printf("dwarf_finish failed!\n");
    }
    close(fd);
    return 0;
}

static void
read_cu_list(Dwarf_Debug dbg)
{
    Dwarf_Unsigned cu_header_length = 0;
    Dwarf_Half     version_stamp = 0;
    Dwarf_Unsigned abbrev_offset = 0;
    Dwarf_Half     address_size = 0;
    Dwarf_Half     offset_size = 0;
    Dwarf_Half     extension_size = 0;
    Dwarf_Sig8     signature;
    Dwarf_Unsigned typeoffset = 0;
    Dwarf_Unsigned next_cu_header = 0;
    Dwarf_Half     header_cu_type = unittype;
    Dwarf_Bool     is_info = g_is_info;
    Dwarf_Error error;
    int cu_number = 0;


    for(;;++cu_number) {
        struct srcfilesdata sf;
        sf.srcfilesres = DW_DLV_ERROR;
        sf.srcfiles = 0;
        sf.srcfilescount = 0;
        Dwarf_Die no_die = 0;
        Dwarf_Die cu_die = 0;
        int res = DW_DLV_ERROR;
        memset(&signature,0, sizeof(signature));
        res = dwarf_next_cu_header_d(dbg,is_info,&cu_header_length,
            &version_stamp, &abbrev_offset,
            &address_size, &offset_size,
            &extension_size,&signature,
            &typeoffset, &next_cu_header,
            &header_cu_type,&error);
        if(res == DW_DLV_ERROR) {
            char *em = dwarf_errmsg(error);
            printf("Error in dwarf_next_cu_header: %s\n",em);
            exit(1);
        }
        if(res == DW_DLV_NO_ENTRY) {
            /* Done. */
            return;
        }
        /* The CU will have a single sibling, a cu_die. */
        res = dwarf_siblingof_b(dbg,no_die,is_info,
            &cu_die,&error);
        if(res == DW_DLV_ERROR) {
            char *em = dwarf_errmsg(error);
            printf("Error in dwarf_siblingof on CU die: %s\n",em);
            exit(1);
        }
        if(res == DW_DLV_NO_ENTRY) {
            /* Impossible case. */
            printf("no entry! in dwarf_siblingof on CU die \n");
            exit(1);
        }
        get_die_and_siblings(dbg,cu_die,is_info,0,&sf);
        dwarf_dealloc(dbg,cu_die,DW_DLA_DIE);
        resetsrcfiles(dbg,&sf);
    }
}

static void
get_die_and_siblings(Dwarf_Debug dbg, Dwarf_Die in_die,
    int is_info,int in_level,
   struct srcfilesdata *sf)
{
    int res = DW_DLV_ERROR;
    Dwarf_Die cur_die=in_die;
    Dwarf_Die child = 0;
    Dwarf_Error error;

    print_die_data(dbg,in_die,is_info,in_level,sf);

    for(;;) {
        Dwarf_Die sib_die = 0;
        res = dwarf_child(cur_die,&child,&error);
        if(res == DW_DLV_ERROR) {
            printf("Error in dwarf_child , level %d \n",in_level);
            exit(1);
        }
        if(res == DW_DLV_OK) {
            get_die_and_siblings(dbg,child,is_info,in_level+1,sf);
        }
        /* res == DW_DLV_NO_ENTRY */
        res = dwarf_siblingof_b(dbg,cur_die,is_info,&sib_die,&error);
        if(res == DW_DLV_ERROR) {
            char *em = dwarf_errmsg(error);
            printf("Error in dwarf_siblingof , level %d :%s \n",in_level,em);
            exit(1);
        }
        if(res == DW_DLV_NO_ENTRY) {
            /* Done at this level. */
            break;
        }
        /* res == DW_DLV_OK */
        if(cur_die != in_die) {
            dwarf_dealloc(dbg,cur_die,DW_DLA_DIE);
        }
        cur_die = sib_die;
        print_die_data(dbg,cur_die,is_info,in_level,sf);
    }
    return;
}
static void
get_addr(Dwarf_Attribute attr,Dwarf_Addr *val)
{
    Dwarf_Error error = 0;
    int res;
    Dwarf_Addr uval = 0;
    res = dwarf_formaddr(attr,&uval,&error);
    if(res == DW_DLV_OK) {
        *val = uval;
        return;
    }
    return;
}
static void
get_number(Dwarf_Attribute attr,Dwarf_Unsigned *val)
{
    Dwarf_Error error = 0;
    int res;
    Dwarf_Signed sval = 0;
    Dwarf_Unsigned uval = 0;
    res = dwarf_formudata(attr,&uval,&error);
    if(res == DW_DLV_OK) {
        *val = uval;
        return;
    }
    res = dwarf_formsdata(attr,&sval,&error);
    if(res == DW_DLV_OK) {
        *val = sval;
        return;
    }
    return;
}
static void
print_subprog(Dwarf_Debug dbg,Dwarf_Die die, 
    int is_info, int level,
    struct srcfilesdata *sf,
    const char *name)
{
    int res;
    Dwarf_Error error = 0;
    Dwarf_Attribute *attrbuf = 0;
    Dwarf_Addr lowpc = 0;
    Dwarf_Addr highpc = 0;
    Dwarf_Signed attrcount = 0;
    Dwarf_Unsigned i;
    Dwarf_Unsigned filenum = 0;
    Dwarf_Unsigned linenum = 0;
    char *filename = 0;
    res = dwarf_attrlist(die,&attrbuf,&attrcount,&error);
    if(res != DW_DLV_OK) {
        return;
    }
    for(i = 0; i < attrcount ; ++i) {
        Dwarf_Half aform;
        res = dwarf_whatattr(attrbuf[i],&aform,&error);
        if(res == DW_DLV_OK) {
            if(aform == DW_AT_decl_file) {
                get_number(attrbuf[i],&filenum);
                if((filenum > 0) && (sf->srcfilescount > (filenum-1))) {
                    filename = sf->srcfiles[filenum-1];
                }
            }
            if(aform == DW_AT_decl_line) {
                get_number(attrbuf[i],&linenum);
            }
            if(aform == DW_AT_low_pc) {
                get_addr(attrbuf[i],&lowpc);
            }
            if(aform == DW_AT_high_pc) {
                /*  This will FAIL with DWARF4 highpc form
                    of 'class constant'.  */
                get_addr(attrbuf[i],&highpc);
            }
        }
        dwarf_dealloc(dbg,attrbuf[i],DW_DLA_ATTR);
    }
    /*  Here let's test some alternative interfaces for high and low pc.
        We only do both dwarf_highpc and dwarf_highpcb_b as
        an error check. Do not do both yourself. */
    if(checkoptionon){
        int hres = 0;
        int hresb = 0;
        int lres = 0;
        Dwarf_Addr althipc = 0;
        Dwarf_Addr hipcoffset = 0;
        Dwarf_Addr althipcb = 0;
        Dwarf_Addr altlopc = 0;
        Dwarf_Half highform = 0;
        enum Dwarf_Form_Class highclass = 0;

        /*  Should work for DWARF 2/3 DW_AT_high_pc, and
            all high_pc where the FORM is DW_FORM_addr
            Avoid using this interface as of 2013. */
        hres  = dwarf_highpc(die,&althipc,&error);

        /* Should work for all DWARF DW_AT_high_pc.  */
        hresb = dwarf_highpc_b(die,&althipcb,&highform,&highclass,&error);

        lres = dwarf_lowpc(die,&altlopc,&error);
        printf("high_pc checking %s ",name);

        if (hres == DW_DLV_OK) {
            /* present, FORM addr */
            printf("highpc   0x%" DW_PR_XZEROS DW_PR_DUx " ",
                althipc);
        } else if (hres == DW_DLV_ERROR) {
            printf("dwarf_highpc() error not class address ");
        } else {
            /* absent */
        }
        if(hresb == DW_DLV_OK) {
            /* present, FORM addr or const. */
            if(highform == DW_FORM_addr) {
                printf("highpcb  0x%" DW_PR_XZEROS DW_PR_DUx " ",
                    althipcb);
            } else {
                if(lres == DW_DLV_OK) {
                    hipcoffset = althipcb;
                    althipcb = altlopc + hipcoffset;
                    printf("highpcb  0x%" DW_PR_XZEROS DW_PR_DUx " "
                        "highoff  0x%" DW_PR_XZEROS DW_PR_DUx " ",
                        althipcb,hipcoffset);
                } else {
                    printf("highoff  0x%" DW_PR_XZEROS DW_PR_DUx " ",
                        althipcb);
                }
            }
        } else if (hresb == DW_DLV_ERROR) {
            printf("dwarf_highpc_b() error!");
        } else {
            /* absent */
        }

        /* Should work for all DWARF DW_AT_low_pc */
        if (lres == DW_DLV_OK) {
            /* present, FORM addr. */
            printf("lowpc    0x%" DW_PR_XZEROS DW_PR_DUx " ",
                altlopc);
        } else if (lres == DW_DLV_ERROR) {
            printf("dwarf_lowpc() error!");
        } else {
            /* absent. */
        }
        printf("\n");



    }
    if(namesoptionon && (filenum || linenum)) {
        printf("<%3d> file: %" DW_PR_DUu " %s  line %"
            DW_PR_DUu "\n",level,filenum,filename?filename:"",linenum);
    }
    if(namesoptionon && lowpc) {
        printf("<%3d> low_pc : 0x%" DW_PR_DUx  "\n",
            level, (Dwarf_Unsigned)lowpc);
    }
    if(namesoptionon && highpc) {
        printf("<%3d> high_pc: 0x%" DW_PR_DUx  "\n",
            level, (Dwarf_Unsigned)highpc);
    }
    dwarf_dealloc(dbg,attrbuf,DW_DLA_LIST);
}

static void
print_comp_dir(Dwarf_Debug dbg,Dwarf_Die die,
    int is_info, int level, struct srcfilesdata *sf)
{
    int res;
    Dwarf_Error error = 0;
    Dwarf_Attribute *attrbuf = 0;
    Dwarf_Signed attrcount = 0;
    Dwarf_Unsigned i;
    res = dwarf_attrlist(die,&attrbuf,&attrcount,&error);
    if(res != DW_DLV_OK) {
        return;
    }
    sf->srcfilesres = dwarf_srcfiles(die,&sf->srcfiles,&sf->srcfilescount,
        &error);
    for(i = 0; i < attrcount ; ++i) {
        Dwarf_Half aform;
        res = dwarf_whatattr(attrbuf[i],&aform,&error);
        if(res == DW_DLV_OK) {
            if(aform == DW_AT_comp_dir) {
                char *name = 0;
                res = dwarf_formstring(attrbuf[i],&name,&error);
                if(res == DW_DLV_OK) {
                    printf(    "<%3d> compilation directory : \"%s\"\n",
                        level,name);
                }
            }
            if(aform == DW_AT_stmt_list) {
                /* Offset of stmt list for this CU in .debug_line */
            }
        }
        dwarf_dealloc(dbg,attrbuf[i],DW_DLA_ATTR);
    }
    dwarf_dealloc(dbg,attrbuf,DW_DLA_LIST);
}

static void
resetsrcfiles(Dwarf_Debug dbg,struct srcfilesdata *sf)
{
    Dwarf_Signed sri = 0;
    for (sri = 0; sri < sf->srcfilescount; ++sri) {
        dwarf_dealloc(dbg, sf->srcfiles[sri], DW_DLA_STRING);
    }
    dwarf_dealloc(dbg, sf->srcfiles, DW_DLA_LIST);
    sf->srcfilesres = DW_DLV_ERROR;
    sf->srcfiles = 0;
    sf->srcfilescount = 0;
}



static void
print_die_data_i(Dwarf_Debug dbg, Dwarf_Die print_me,
    int is_info,int level,
    struct srcfilesdata *sf)
{
    char *name = 0;
    Dwarf_Error error = 0;
    Dwarf_Half tag = 0;
    const char *tagname = 0;
    int localname = 0;
    int res = 0;

    res = dwarf_diename(print_me,&name,&error);
    if(res == DW_DLV_ERROR) {
        printf("Error in dwarf_diename , level %d \n",level);
        exit(1);
    }
    if(res == DW_DLV_NO_ENTRY) {
        name = "<no DW_AT_name attr>";
        localname = 1;
    }
    res = dwarf_tag(print_me,&tag,&error);
    if(res != DW_DLV_OK) {
        printf("Error in dwarf_tag , level %d \n",level);
        exit(1);
    }
    res = dwarf_get_TAG_name(tag,&tagname);
    if(res != DW_DLV_OK) {
        printf("Error in dwarf_get_TAG_name , level %d \n",level);
        exit(1);
    }
    if(namesoptionon ||checkoptionon) {
        if( tag == DW_TAG_subprogram) {
            if(namesoptionon) {
                printf(    "<%3d> subprogram            : \"%s\"\n",level,name);
            }
            print_subprog(dbg,print_me,is_info,level,sf,name);
        }
        if( (namesoptionon) && (tag == DW_TAG_compile_unit ||
            tag == DW_TAG_partial_unit ||
            tag == DW_TAG_type_unit)) {

            resetsrcfiles(dbg,sf);
            printf(    "<%3d> source file           : \"%s\"\n",level,name);
            print_comp_dir(dbg,print_me,is_info,level,sf);
        }
    } else {
        printf("<%d> tag: %d %s  name: \"%s\"\n",level,tag,tagname,name);
    }
    if(!localname) {
        dwarf_dealloc(dbg,name,DW_DLA_STRING);
    }
}

static void
print_die_data(Dwarf_Debug dbg, Dwarf_Die print_me,
    int is_info,int level,
    struct srcfilesdata *sf)
{

    if (fissionfordie != -1) {
        Dwarf_Debug_Fission_Per_CU percu;
        memset(&percu,0,sizeof(percu));
        if (fissionfordie == dienumber) {
            Dwarf_Error error = 0;
            int res = 0;
            res = dwarf_get_debugfission_for_die(print_me,
                &percu,&error);
            if(res == DW_DLV_ERROR) {
                printf("FAIL: Error in dwarf_diename  on fissionfordie %d\n",
                    fissionfordie);
                exit(1);
            }
            if(res == DW_DLV_NO_ENTRY) {
                printf("FAIL: no-entry in dwarf_diename  on fissionfordie %d\n",
                    fissionfordie);
                exit(1);
            }
            print_die_data_i(dbg,print_me,is_info,level,sf);
            print_debug_fission_header(&percu);
            exit(0);
        }
        dienumber++;
        return;
    }
    print_die_data_i(dbg,print_me,is_info,level,sf);
    dienumber++;
}

