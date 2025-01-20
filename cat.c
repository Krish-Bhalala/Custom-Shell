#include "nqp_io.h"
#include <stdio.h>
#include <stdlib.h>

int main(void)
{

    char *fname = "README.md";
    char buffer[256] = {0};

    nqp_error err = nqp_mount("img", NQP_FS_EXFAT);

    if ( err == NQP_OK )
    {
        int fd = nqp_open(fname);
        ssize_t bytes_read;

        while ( ( bytes_read = nqp_read( fd, buffer, 256 ) ) > 0 )
        {
            for ( ssize_t i = 0 ; i < bytes_read; i++ )
            {
                putchar( buffer[i] );
            }
        }

        nqp_close( fd );

        nqp_unmount( );
    }

    return EXIT_SUCCESS;
}
