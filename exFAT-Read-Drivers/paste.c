#include "nqp_io.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    char buffer = '\0';
    ssize_t bytes_read = 0;
    int fd1, fd2;

    nqp_error err = nqp_mount(argv[1], NQP_FS_EXFAT);

    if ( err == NQP_OK && argc == 4 )
    {
        fd1 = nqp_open(argv[2]);
        fd2 = nqp_open(argv[3]);

        if ( fd1 != NQP_FILE_NOT_FOUND && fd2 != NQP_FILE_NOT_FOUND )
        {
            do
            {
                bytes_read = 0;
                while ( ( bytes_read += nqp_read( fd1, &buffer, 1 ) ) > 0 && 
                        buffer != '\n' )
                {
                    putchar( buffer );
                }

                while ( ( bytes_read += nqp_read( fd2, &buffer, 1 ) ) > 0 && 
                        buffer != '\n' )
                {
                    putchar( buffer );
                }

                putchar( '\n' );
            } while ( bytes_read > 0 );

            nqp_close( fd1 );
            nqp_close( fd2 );
        }

        nqp_unmount( );
    }

    return EXIT_SUCCESS;
}
