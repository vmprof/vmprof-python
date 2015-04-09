
/* An example consumer of vmprof that uses dtrace
 */

#include "vmprof.h"
#include <stdio.h>

#define BUF_SIZE 8192

int main()
{
    char buf[BUF_SIZE];
    long count;

    while (count = read(0, buf, BUF_SIZE)) {
        if (buf[0] == MARKER_STACKTRACE) {
            fprintf(stderr, "stacktrace\n");
        } else if (buf[0] == MARKER_VIRTUAL_IP) {
            fprintf(stderr, "virtual ip\n");
        } else {
            fprintf(stderr, "unknown marker\n");
        }
    }
}
