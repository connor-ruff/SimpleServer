/* single.c: Single User HTTP Server */

#include "spidey.h"

#include <errno.h>
#include <string.h>

#include <unistd.h>

/**
 * Handle one HTTP request at a time.
 *
 * @param   sfd         Server socket file descriptor.
 * @return  Exit status of server (EXIT_SUCCESS).
 **/
int single_server(int sfd) {
    /* Accept and handle HTTP request */
    while (true) {
    	/* Accept request */
        Request * r = accept_request(sfd);
        if (!r){
            fprintf(stderr, "Could Not Accept Request\n");
            return EXIT_FAILURE;
        }

            
        handle_request(r);
        
	/* Free request */
        free_request(r);
    }

    close(sfd); 
    
    /* Close server socket */
    return EXIT_SUCCESS;
}

/* vim: set expandtab sts=4 sw=4 ts=8 ft=c: */
