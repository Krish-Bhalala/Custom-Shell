#include "nqp_io.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    nqp_dirent entry = {0};
    nqp_error err;
    int fd;
    ssize_t dirents_read;

    // This ls takes two arguments: The file system image and the directory to
    // list the contents of.
    if ( argc == 3 )
    {
        err = nqp_mount( argv[1], NQP_FS_EXFAT );
        if ( err == NQP_OK )
        {
            fd = nqp_open(argv[2]);

            if ( fd == NQP_FILE_NOT_FOUND )
            {
                fprintf(stderr, "%s not found\n", argv[2] );
            }
            else
            {
                while ( ( dirents_read = nqp_getdents( fd, &entry, 1 ) ) > 0 )
                {
                    printf( "%lu %s", entry.inode_number, entry.name );
                    
                    if ( entry.type == DT_DIR )
                    {
                        putchar('/');
                    }
                    
                    putchar('\n');

                    free( entry.name );
                }

                if ( dirents_read == -1 )
                {
                    fprintf( stderr, "%s is not a directory\n", argv[2] );
                }

                nqp_close( fd );
            }
        }

        nqp_unmount( );
    }
    

    return EXIT_SUCCESS;
}
