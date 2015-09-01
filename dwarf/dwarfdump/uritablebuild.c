/*
    Copyright 2011 David Anderson. All rights reserved.

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

#include <stdio.h>
#include <ctype.h>

/* Generates a table which identifies a few dangerous characters.
   Ones one does not want to appear in output.

   It's a bit arbitrary in that we allow lots of shell-interpreted
   characters through, and most characters generally.

   But not control characters or single or double quotes.
   The quotes would be particularly problematic for post-processing
   dwarfdump output sensibly.

*/
static void
print_entry(int c)
{
    char v[2];
    v[0] = c;
    v[1] = 0;
    if(c == 0) {
        printf("0, /* NUL 0x%02x */\n",c);
        return;
    }
    if(isalnum(c) || c == ' ' ) {
        /*  We let the space character print as space since
            lots of files are named that way in Mac and Windows.
        */
        printf("1, /* \'%s\' 0x%02x */\n",v,c);
        return;
    }
    if(c == 0x21 || c == 0x23 || c == 0x26) {
        /*  We let the space character print as space since
            lots of files are named that way in Mac and Windows.
        */
        printf("1, /* \'%s\' 0x%02x */\n",v,c);
        return;
    }
    if(isspace(c) ) {
        /* Other white space translated. */
        printf("0, /* whitespace 0x%02x */\n",c);
        return;
    }
    if(c == 0x7f) {
        printf("0, /* DEL 0x%02x */\n",c);
        return;
    }
    if(c >= 0x01 && c <=  0x20 ) {
        /* ASCII control characters. */
        printf("0, /* control 0x%02x */\n",c);
        return;
    }
    if(c == '\'' || c == '\"' || c == '%' || c == ';' ) {
        printf("0, /* \'%s\' 0x%02x */\n",v,c);
        return;
    }
    if(c >= 0x3a && c <=  0x40 ) {
        /* ASCII */
        printf("1, /* \'%s\' 0x%02x */\n",v,c);
        return;
    }
    if(c == 0xa0 || c == 0xff ) {
        printf("0, /* other: 0x%02x */\n",c);
        return;
    }
    if(c >= 0x27 && c <=  0x2f ) {
        /* ASCII */
        printf("1, /* \'%s\' 0x%02x */\n",v,c);
        return;
    }
    if(c >= 0x5b && c <=  0x5f ) {
        /* ASCII */
        printf("1, /* \'%s\' 0x%02x */\n",v,c);
        return;
    }
    if(c >= 0x60 && c <=  0x60 ) {
        /* ASCII */
        printf("0, /* \'%s\' 0x%02x */\n",v,c);
        return;
    }
    if(c >= 0x7b && c <=  0x7e ) {
        /* ASCII */
        printf("1, /* \'%s\' 0x%02x */\n",v,c);
        return;
    }
    if (c < 0x7f) {
        /* ASCII */
        printf("1, /* \'%s\' 0x%02x */\n",v,c);
        return;
    }
    /* We are allowing other iso 8859 characters through unchanged. */
    printf("1, /* 0x%02x */\n",c);
}


int
main()
{
    int i = 0;
    printf("/* dwarfdump_ctype table */\n");
    printf("char dwarfdump_ctype_table[256] = { \n");
    for ( i = 0 ; i <= 255; ++i) {
        print_entry(i);
    }
    printf("};\n");
}

