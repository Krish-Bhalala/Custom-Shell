#include "exfat_io.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
  exfat_dirent entry = {0};
  exfat_error err;
  int fd;
  ssize_t dirents_read;

  // This ls takes two arguments: The file system image and the directory to
  // list the contents of.
  if (argc == 3) {
    err = exfat_mount(argv[1], EXFAT_FS_EXFAT);
    if (err == EXFAT_OK) {
      fd = exfat_open(argv[2]);

      if (fd == EXFAT_FILE_NOT_FOUND) {
        fprintf(stderr, "%s not found\n", argv[2]);
      } else {
        while ((dirents_read = exfat_getdents(fd, &entry, 1)) > 0) {
          printf("%" PRIu64 " %s", entry.inode_number, entry.name);

          if (entry.type == DT_DIR) {
            putchar('/');
          }

          putchar('\n');

          free(entry.name);
        }

        if (dirents_read == -1) {
          fprintf(stderr, "%s is not a directory\n", argv[2]);
        }

        exfat_close(fd);
      }
    }

    exfat_unmount();
  }

  return EXIT_SUCCESS;
}
