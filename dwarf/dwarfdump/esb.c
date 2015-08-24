/*
  Copyright (C) 2005 Silicon Graphics, Inc.  All Rights Reserved.
  Portions Copyright (C) 2013 David Anderson. All Rights Reserved.
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

/* esb.c
   extensible string buffer.

   A simple means (vaguely like a C++ class) that
   enables safely saving strings of arbitrary length built up
   in small pieces.

   We really do allow only c strings here. NUL bytes
   in a string result in adding only up to the NUL and
   in a warning to stderr.

*/

#ifndef SELFTEST
#include "globals.h"
#else
#include <stdio.h> /* SELFTEST */
#include <string.h> /* SELFTEST */
#include <stdlib.h> /* SELFTEST */
typedef char * string; /* SELFTEST */
#include <stdarg.h>   /* For va_start va_arg va_list */
#endif
#include "esb.h"

/*  INITIAL_ALLOC value takes no account of space for a trailing NUL,
    the NUL is accounted for in init_esb_string
    and in later tests against esb_allocated_size. */
#ifdef SELFTEST
#define INITIAL_ALLOC 1  /* SELFTEST */
#else
/*  There is nothing magic about this size.
    It is just big enough to avoid most resizing. */
#define INITIAL_ALLOC 240
#endif
/*  Allow for final NUL */
static size_t alloc_size = INITIAL_ALLOC;


static void
init_esb_string(struct esb_s *data, size_t min_len)
{
    string d;

    if (data->esb_allocated_size > 0) {
        return;
    }
    /* Only esb_constructor applied. Allow for string space. */
    if (min_len <= alloc_size) {
        min_len = alloc_size +1;/* Allow for NUL at end */
    } else  {
        min_len++ ; /* Allow for NUL at end */
    }
    d = malloc(min_len);
    if (!d) {
        fprintf(stderr,
            "dwarfdump is out of memory allocating %lu bytes\n",
            (unsigned long) min_len);
        exit(5);
    }
    data->esb_string = d;
    data->esb_allocated_size = min_len;
    data->esb_string[0] = 0;
    data->esb_used_bytes = 0;
}

/*  Make more room. Leaving  contents unchanged, effectively.
    The NUL byte at end has room and this preserves that room.
*/
static void
allocate_more(struct esb_s *data, size_t len)
{
    size_t new_size = data->esb_allocated_size + len;
    string newd = 0;

    if (new_size < alloc_size)
        new_size = alloc_size;
    newd = realloc(data->esb_string, new_size);
    if (!newd) {
        fprintf(stderr, "dwarfdump is out of memory re-allocating "
            "%lu bytes\n", (unsigned long) new_size);
        exit(5);
    }
    data->esb_string = newd;
    data->esb_allocated_size = new_size;
}

void
esb_force_allocation(struct esb_s *data, size_t minlen)
{
    if (data->esb_allocated_size < minlen) {
        size_t increment = minlen - data->esb_allocated_size;
        allocate_more(data,increment);
    }
}

static void
esb_appendn_internal(struct esb_s *data, const char * in_string, size_t len);

void
esb_appendn(struct esb_s *data, const char * in_string, size_t len)
{
    size_t full_len = strlen(in_string);

    if (full_len < len) {
        fprintf(stderr, "dwarfdump esb internal error, bad string length "
            " %lu  < %lu \n",
            (unsigned long) full_len, (unsigned long) len);
        len = full_len;
    }

    esb_appendn_internal(data, in_string, len);
}

/*  The length is gotten from the in_string itself. */
void
esb_append(struct esb_s *data, const char * in_string)
{
    size_t len = strlen(in_string);

    esb_appendn_internal(data, in_string, len);
}

/*  The 'len' is believed. Do not pass in strings < len bytes long. */
static void
esb_appendn_internal(struct esb_s *data, const char * in_string, size_t len)
{
    size_t remaining = 0;
    size_t needed = len;

    if (data->esb_allocated_size == 0) {
        size_t maxlen = (len >= alloc_size)? (len):alloc_size;

        init_esb_string(data, maxlen);
    }
    /*  ASSERT: data->esb_allocated_size > data->esb_used_bytes  */
    remaining = data->esb_allocated_size - data->esb_used_bytes;
    if (remaining <= needed) {
        allocate_more(data,len);
    }
    strncpy(&data->esb_string[data->esb_used_bytes], in_string, len);
    data->esb_used_bytes += len;
    /* Insist on explicit NUL terminator */
    data->esb_string[data->esb_used_bytes] = 0;
}

/*  Always returns an empty string or a non-empty string. Never 0. */
string
esb_get_string(struct esb_s *data)
{
    if (data->esb_allocated_size == 0) {
        init_esb_string(data, alloc_size);
    }
    return data->esb_string;
}


/*  Sets esb_used_bytes to zero. The string is not freed and
    esb_allocated_size is unchanged.  */
void
esb_empty_string(struct esb_s *data)
{
    if (data->esb_allocated_size == 0) {
        init_esb_string(data, alloc_size);
    }
    data->esb_used_bytes = 0;
    data->esb_string[0] = 0;

}


/*  Return esb_used_bytes. */
size_t
esb_string_len(struct esb_s *data)
{
    return data->esb_used_bytes;
}


/*  The following are for testing esb, not use by dwarfdump. */

/*  *data is presumed to contain garbage, not values, and
    is properly initialized here. */
void
esb_constructor(struct esb_s *data)
{
    memset(data, 0, sizeof(*data));
}

/*  The string is freed, contents of *data set to zeroes. */
void
esb_destructor(struct esb_s *data)
{
    if (data->esb_string) {
        free(data->esb_string);
    }
    esb_constructor(data);
}


/*  To get all paths in the code tested, this sets the
    allocation/reallocation to the given value, which can be quite small
    but must not be zero. */
void
esb_alloc_size(size_t size)
{
    alloc_size = size;
}

size_t
esb_get_allocated_size(struct esb_s *data)
{
    return data->esb_allocated_size;
}

/*  Append a formatted string
    It is up to the caller to ensure the
    data esb_allocated_size is big enough */
void
esb_append_printf_ap(struct esb_s *data,const char *in_string,va_list ap)
{
    size_t netlen = data->esb_allocated_size - data->esb_used_bytes;
    int expandedlen =
        vsnprintf(&data->esb_string[data->esb_used_bytes],
        netlen,in_string,ap);
    if (netlen < expandedlen) {
        /*  If data was too small, the max written was one less than
            netlen. */
        data->esb_used_bytes += netlen - 1;
    } else {
        data->esb_used_bytes += expandedlen;
    }
}

/*  Append a formatted string */
void
esb_append_printf(struct esb_s *data,const char *in_string, ...)
{
    va_list ap;
    va_start(ap,in_string);
    esb_append_printf_ap(data,in_string,ap);
    /*  "The object ap may be passed as an argument to another
        function; if that function invokes the va_arg()
        macro with parameter ap, the value of ap in the calling
        function is unspecified and shall be passed to the va_end()
        macro prior to any further reference to ap."
        Single Unix Specification. */
    va_end(ap);
}

/* Get a copy of the internal data buffer */
string
esb_get_copy(struct esb_s *data)
{
    string copy = NULL;
    size_t len = esb_string_len(data);
    if (len) {
        copy = (string)malloc(len);
        strcpy(copy,esb_get_string(data));
    }

    return copy;
}


#ifdef SELFTEST
static int failcount = 0;
void
validate_esb(int instance,
   struct esb_s* d,
   size_t explen,
   size_t expalloc,
   const char *expout)
{
    printf("TEST instance %d\n",instance);
    if (esb_string_len(d) != explen) {
        ++failcount;
        printf("FAIL instance %d  slen %u explen %u\n",
            instance,(unsigned)esb_string_len(d),(unsigned)explen);
    }
    if (d->esb_allocated_size != expalloc) {
        ++failcount;
        printf("FAIL instance %d  alloclen %u expalloc %u\n",
            instance,(unsigned)d->esb_allocated_size,(unsigned)expalloc);
    }
    if(strcmp(esb_get_string(d),expout)) {
        ++failcount;
        printf("FAIL instance %d  str %s expstr %s\n",
            instance,esb_get_string(d),expout);
    }
}
void
trialprint_1(struct esb_s *d, char *format,...)
{
    va_list ap;

    va_start(ap,format);
    esb_append_printf_ap(d,format,ap);
    va_end(ap);
}

void
trialprint(struct esb_s *d)
{
    const char * s = "insert me";
    trialprint_1(d,"aaaa %s bbbb",s);
}


int main()
{
    {
        struct esb_s d;
        esb_constructor(&d);
        esb_append(&d,"a");
        validate_esb(1,&d,1,2,"a");
        esb_append(&d,"b");
        validate_esb(2,&d,2,3,"ab");
        esb_append(&d,"c");
        validate_esb(3,&d,3,4,"abc");
        esb_empty_string(&d);
        validate_esb(4,&d,0,4,"");
        esb_destructor(&d);
    }
    {
        struct esb_s d;
        esb_constructor(&d);
        esb_append(&d,"aa");
        validate_esb(6,&d,2,3,"aa");
        esb_append(&d,"bbb");
        validate_esb(7,&d,5,6,"aabbb");
        esb_append(&d,"c");
        validate_esb(8,&d,6,7,"aabbbc");
        esb_empty_string(&d);
        validate_esb(9,&d,0,7,"");
        esb_destructor(&d);
    }
    {
        struct esb_s d;
        static char oddarray[7] = {'a','b',0,'c','c','d',0};
        esb_constructor(&d);
        /* This provokes a msg on stderr. Bad input. */
        esb_appendn(&d,oddarray,6);
        validate_esb(10,&d,2,3,"ab");
        esb_appendn(&d,"cc",1);
        validate_esb(11,&d,3,4,"abc");
        esb_empty_string(&d);
        validate_esb(12,&d,0,4,"");
        esb_destructor(&d);
    }
    {
        struct esb_s d;
        esb_constructor(&d);

        esb_force_allocation(&d,7);
        trialprint(&d);
        validate_esb(13,&d,6,7,"aaaa i");
    }
    {
        struct esb_s d;
        esb_constructor(&d);

        esb_force_allocation(&d,50);
        trialprint(&d);
        validate_esb(14,&d,19,50,"aaaa insert me bbbb");
    }
    if (failcount) {
        printf("FAIL esb test\n");
        exit(1);
    }
    printf("PASS esb test\n");
    exit(0);
}
#endif /* SELFTEST */
