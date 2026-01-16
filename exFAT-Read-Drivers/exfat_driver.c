#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "exfat_io.h"
#include "exfat_types.h"

/**
 * Convert a Unicode-formatted string containing only ASCII characters
 * into a regular ASCII-formatted string (16 bit chars to 8 bit
 * chars).
 *
 * NOTE: this function does a heap allocation for the string it
 *       returns (like strdup), caller is responsible for `free`-ing the
 *       allocation when necessary.
 *
 * uint16_t *unicode_string: the Unicode-formatted string to be
 *                           converted.
 * uint8_t   length: the length of the Unicode-formatted string (in
 *                   characters).
 *
 * returns: a heap allocated ASCII-formatted string.
 */
char *unicode2ascii(uint16_t *unicode_string, uint8_t length) {
  assert(unicode_string != NULL);
  assert(length > 0);

  char *ascii_string = NULL;

  if (unicode_string != NULL && length > 0) {
    // +1 for a NULL terminator
    ascii_string = calloc(sizeof(char), length + 1);

    if (ascii_string) {
      // strip the top 8 bits from every character in the
      // unicode string
      for (uint8_t i = 0; i < length; i++) {
        ascii_string[i] = (char)unicode_string[i];
      }
      // stick a null terminator at the end of the string.
      ascii_string[length] = '\0';
    }
  }

  return ascii_string;
}

/* Return: EXFAT_UNSUPPORTED_FS if the current implementation does not support
 *         the file system specified, EXFAT_FSCK_FAIL if the super block does
 * not pass the basic file system check, EXFAT_INVAL if an invalid argument has
 * been passed (e.g., NULL),or EXFAT_OK on success.
 */
exfat_error exfat_mount(const char *source, exfat_fs_type fs_type) {
  // boolean isValidSuperBlock = true;
  (void)source;
  (void)fs_type;

  if (NULL == source) {
    return EXFAT_INVAL;
  }
  if (EXFAT_FS_EXFAT != fs_type) {
    return EXFAT_UNSUPPORTED_FS;
  }

  // opening the file for reading in binary for validating
  FILE *file_system_bin = fopen(source, "rb");
  if (NULL == file_system_bin) {
    return EXFAT_INVAL;
  }

  // reading the opened binary file into the main_boot_record
  main_boot_record mbr; // struct to store the main boot record data from the fs
  // reading 1 mbr worth of data from the file
  if (1 != fread(&mbr, sizeof(main_boot_record), 1, file_system_bin)) {
    // failed to read the main boot record
    fclose(file_system_bin);
    return EXFAT_FSCK_FAIL;
  }

  // checking the FileSystemName Field
  if (0 != strcmp(mbr.fs_name, "EXFAT   ")) {
    // invalid file system FileSystemName
    fclose(file_system_bin);
    return EXFAT_FSCK_FAIL;
  }

  // checking the must_be_zero field, there should be 53 bytes of 0 init
  if (0 != memcmp(mbr.must_be_zero, 0, 53)) {
    // invalid must_be_zero field in our file system
    fclose(file_system_bin);
    return EXFAT_FSCK_FAIL;
  }

  // checking boot signature
  if (0xAA55 != mbr.boot_signature) {
    fclose(file_system_bin);
    return EXFAT_FSCK_FAIL;
  }

  // checking the FirstClusterOfRootDirectory field should be in range
  // [2,ClusterCount+1];
  assert(mbr.cluster_count > 0);
  if (mbr.first_cluster_of_root_directory < 2 ||
      mbr.first_cluster_of_root_directory > mbr.cluster_count + 1) {
    assert(mbr.first_cluster_of_root_directory >= 2);
    assert(mbr.first_cluster_of_root_directory <= mbr.cluster_count + 1);
    fclose(file_system_bin);
    return EXFAT_FSCK_FAIL;
  }

  // all the checks passed, this is a valid exFAT file system
  fclose(file_system_bin);
  return EXFAT_OK;
}

exfat_error exfat_unmount(void) { return EXFAT_INVAL; }

int exfat_open(const char *pathname) {
  (void)pathname;

  return EXFAT_INVAL;
}

int exfat_close(int fd) {
  (void)fd;

  return EXFAT_INVAL;
}

ssize_t exfat_read(int fd, void *buffer, size_t count) {
  (void)fd;
  (void)buffer;
  (void)count;

  return EXFAT_INVAL;
}
