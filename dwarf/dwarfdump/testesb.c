/* testesb.c
   test code for esb.h esb.c

   Not part of a compiled dwarfdump.

*/

#include <stdio.h>
#include <stdarg.h>   /* For va_start va_arg va_list */
#include <string.h>
typedef char *string;

#include "esb.h"

void
check(string msg, struct esb_s *data, string v)
{
    string b = esb_get_string(data);
    size_t l = 0;
    size_t alloc = 0;

    if (strcmp(b, v)) {
        fprintf(stderr, "ERROR: %s  content error  %s != %s\n", msg, b,
            v);
    }

    l = esb_string_len(data);

    if (l != strlen(v)) {
        fprintf(stderr, "ERROR: %s length error  %lu != %lu\n", msg,
            (unsigned long) l, (unsigned long) strlen(v));
    }
    alloc = esb_get_allocated_size(data);
    if (l > alloc) {
        fprintf(stderr, "ERROR: %s allocation error  %lu > %lu\n", msg,
            (unsigned long) l, (unsigned long) alloc);

    }

    return;
}

int
main(void)
{
    struct esb_s data;


    esb_alloc_size(2);          /* small to get all code paths tested. */
    esb_constructor(&data);

    esb_append(&data, "a");
    esb_appendn(&data, "bc", 1);
    esb_append(&data, "d");
    esb_append(&data, "e");
    check("test 1", &data, "abde");

    esb_destructor(&data);
    esb_constructor(&data);

    esb_append(&data, "abcdefghij" "0123456789");
    check("test 2", &data, "abcdefghij" "0123456789");

    esb_destructor(&data);
    esb_constructor(&data);
    esb_append(&data, "abcdefghij" "0123456789");

    esb_append(&data, "abcdefghij" "0123456789");

    esb_append(&data, "abcdefghij" "0123456789");

    esb_append(&data, "abcdefghij" "0123456789");
    check("test 3", &data, "abcdefghij"
        "0123456789"
        "abcdefghij"
        "0123456789"
        "abcdefghij" "0123456789" "abcdefghij" "0123456789");
    esb_destructor(&data);
    return 0;
}
