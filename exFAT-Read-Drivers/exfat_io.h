#pragma once

#include <stdint.h>
#include <sys/types.h>

#define MAX_OPEN_FILES 8

typedef enum EXFAT_FS_TYPE {
  EXFAT_FS_EXFAT,

  // EXFAT_FS_TYPES should always be last
  EXFAT_FS_TYPES
} exfat_fs_type;

typedef enum EXFAT_DIRECTORY_ENTRY_TYPE {
  DT_DIR, // a directory
  DT_REG, // a regular file
} exfat_dtype;

typedef struct EXFAT_DIRECTORY_ENTRY {
  uint64_t inode_number; // the unique identifier for this entry
  size_t name_len;       // the number of characters in the name
  char *name;            // the actual name
  exfat_dtype type;      // the type of file that this points at
} exfat_dirent;

typedef enum EXFAT_ERROR {
  EXFAT_OK = 0, // no error.

  EXFAT_UNSUPPORTED_FS = -1, // this file system is not supported by the
                             // implementation.

  EXFAT_FSCK_FAIL = -2, // the file system's super block did not pass the
                        // basic file system check.

  EXFAT_INVAL = -3, // an invalid argment was passed.

  EXFAT_FILE_NOT_FOUND = -4, // no file with the given name was found.

} exfat_error;

/**
 * "Mount" a file system.
 *
 * This function must be called before interacting with any other exfat_*
 * functions (they will all use the "mounted" file system).
 *
 * This function does a basic file system check on the super block of the file
 * system being mounted.
 *
 * Parameters:
 *  * source: The file containing the file system to mount. Must not be NULL.
 *  * fs_type: The type of the file system. Must be a value from exfat_fs_type.
 * Return: EXFAT_UNSUPPORTED_FS if the current implementation does not support
 *         the file system specified, EXFAT_FSCK_FAIL if the super block does
 * not pass the basic file system check, EXFAT_INVAL if an invalid argument has
 * been passed (e.g., NULL),or EXFAT_OK on success.
 */
exfat_error exfat_mount(const char *source, exfat_fs_type fs_type);

/**
 * "Unmount" the mounted file system.
 *
 * This function should be called to flush any changes to the file system's
 * volume (there shouldn't be! All operations are read only.)
 *
 * Return: EXFAT_INVAL on error (e.g., there is no fs currently mounted) or
 *         EXFAT_OK on success.
 */
exfat_error exfat_unmount(void);

/**
 * Open the file at pathname in the "mounted" file system.
 *
 * Parameters:
 *  * pathname: The path of the file or directory in the file system that
 *              should be opened.  Must not be NULL.
 * Return: -1 on error, or a nonnegative integer on success. The nonnegative
 *         integer is a file descriptor.
 */
int exfat_open(const char *pathname);

/**
 * Close the file referred to by the descriptor.
 *
 * Parameters:
 *  * fd: The file descriptor to close. Must be a nonnegative integer.
 * Return: -1 on error or 0 on success.
 */
int exfat_close(int fd);

/**
 * Read from a file desriptor.
 *
 * Parameters:
 *  * fd: The file descriptor to read from. Must be a nonnegative integer. The
 *        file descriptor should refer to a file, not a directory.
 *  * buffer: The buffer to read data into. Must not be NULL.
 *  * count: The number of bytes to read into the buffer.
 * Return: The number of bytes read, 0 at the end of the file, or -1 on error.
 */
ssize_t exfat_read(int fd, void *buffer, size_t count);

/**
 * Get the directory entries for a directory. Similar to read()ing a file, you
 * may need to call this function repeatedly to get all directory entries.
 *
 * The name field of struct EXFAT_DIRECTORY_ENTRY may be dynamically allocated,
 * the caller is responsible for free()-ing the name after use.
 *
 * Parameters:
 *  * fd: The file descriptor to read from. Must be a nonnegative integer. The
 *        file descriptor should refer to a directory, not a file.
 *  * dirp: the buffer into which the directory entries will be written. The
 *          buffer must not be NULL.
 *  * count: the number of instances of struct EXFAT_DIRECTORY_ENTRY to read
 *           (e.g., to read EXFAT_DIRECTORY_ENTRY, you would pass 1). Must be
 *           greater than zero.
 * Return: The total number of struct EXFAT_DIRECTORY_ENTRY read into the
 * buffer, 0 at the end of the directory, or -1 on error.
 */
ssize_t exfat_getdents(int fd, void *dirp, size_t count);

#ifdef USE_LIBC_INSTEAD

#include <fcntl.h>
#include <unistd.h>

// these replace our implementation with the standard libc implementations of
// open, read, and close, so that we can get a sense of how the tests are
// supposed to work.
#define exfat_read(fd, buffer, size) read(fd, buffer, size)
#define exfat_open(name) open(name, O_RDONLY)
#define exfat_close(fd) close(fd)

// mount and unmount are not functions we would be able to call, so straight
// up replace these with EXFAT_OK, code expecting EXFAT_OK will just pass
// through.
#define exfat_mount(name, type) EXFAT_OK
#define exfat_unmount() EXFAT_OK

#endif
