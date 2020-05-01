/* socket.c: Simple Socket Functions */

#include "spidey.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

/**
 * Allocate socket, bind it, and listen to specified port.
 *
 * @param   port        Port number to bind to and listen on.
 * @return  Allocated server socket file descriptor.
 **/
int socket_listen(const char *port) {
    /* Lookup server address information */
    
    // Set Up Hints
    struct addrinfo hints = {
        .ai_family = AF_UNSPEC,
        .ai_socktype = SOCK_STREAM,
        .ai_flags = AI_PASSIVE
    };

    struct addrinfo *results;  // To Store Address Info
    // Populate Linked List of Potential ID Addresses to Use in Connection
    int status;
    if ( (status = getaddrinfo(NULL, port, &hints, &results)) != 0 ){
        fprintf(stderr, "getaddrinfo failure: %s\n", gai_strerror(status));
        return -1;  
    }

    // Get a Socket FD
    int server_fd = -1;   
    for(struct addrinfo *ptr = results; ptr && (server_fd < 0); ptr = ptr->ai_next){

        // Attempt to Allocate Socket
        if( (server_fd = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol)) < 0 ){
            fprintf(stderr, "socket failed: %s\n", strerror(errno));
            continue;
        }

        // Attempt to Bind to Socket FD 
        if( (bind(server_fd, ptr->ai_addr, ptr->ai_addrlen)) < 0 ){
            fprintf(stderr, "bind failed: %s\n", strerror(errno));
            close(server_fd);
            server_fd = -1;
            continue;
        }

        // Listen on the Socket
        if (listen(server_fd, SOMAXCONN) < 0 ){
            fprintf(stderr, "listen failed: %s\n", strerror(errno));
            close(server_fd);
            server_fd = -1;
            continue;
        }
    }

    


    freeaddrinfo(results);
    return server_fd;
}

/* vim: set expandtab sts=4 sw=4 ts=8 ft=c: */
