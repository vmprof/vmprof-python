#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <assert.h>
#include <zlib.h>
#include <fcntl.h>
#include <stdio.h>

#define READ_BUFFER_SIZE 64*1024

static int tinygz(int in_fd, int out_fd);


int main(int argc, char *argv[]) {
    int out_fd = atoi(argv[1]);
    return tinygz(STDIN_FILENO, out_fd);
}


static int tinygz(int in_fd, int out_fd) {
    gzFile gz = gzdopen(out_fd, "ab4");
    if (gz == NULL)
        goto err;

    char read_buf[READ_BUFFER_SIZE];

    for (;;) {
        ssize_t bytes_read = read(in_fd, read_buf, READ_BUFFER_SIZE);
        if (bytes_read == 0) {
            goto done;
        } else if (bytes_read < 0) {
            if (errno == EAGAIN) {
                continue;
            } else {
                perror("read()");
                goto err;
            }
        } else {
            size_t bytes_written;
            while (bytes_read > 0) {
                bytes_written = gzwrite(gz, read_buf, bytes_read);
                if (bytes_written == 0) {
                    perror("gzwrite()");
                    goto err;
                } else {
                    bytes_read -= bytes_written;
                }
            }
        }
    }

    assert(0 && "unreachable");

done:
    return gzclose_w(gz) == Z_OK ? 0 : 1;

err:
    if (gz != NULL)
        gzclose_w(gz);
    return 1;
}
