#include "exfat_io.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
  char buffer = '\0';
  ssize_t bytes_read = 0;
  int fd1, fd2;

  exfat_error err = exfat_mount(argv[1], EXFAT_FS_EXFAT);

  if (err == EXFAT_OK && argc == 4) {
    fd1 = exfat_open(argv[2]);
    fd2 = exfat_open(argv[3]);

    if (fd1 != EXFAT_FILE_NOT_FOUND && fd2 != EXFAT_FILE_NOT_FOUND) {
      do {
        bytes_read = 0;
        while ((bytes_read += exfat_read(fd1, &buffer, 1)) > 0 &&
               buffer != '\n') {
          putchar(buffer);
        }

        while ((bytes_read += exfat_read(fd2, &buffer, 1)) > 0 &&
               buffer != '\n') {
          putchar(buffer);
        }

        putchar('\n');
      } while (bytes_read > 0);

      exfat_close(fd1);
      exfat_close(fd2);
    }

    exfat_unmount();
  }

  return EXIT_SUCCESS;
}
