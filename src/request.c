/* request.c: HTTP Request Functions */

#include "spidey.h"

#include <errno.h>
#include <string.h>

#include <unistd.h>

int parse_request_method(Request *r);
int parse_request_headers(Request *r);

/**
 * Accept request from server socket.
 *
 * @param   sfd         Server socket file descriptor.
 * @return  Newly allocated Request structure.
 *
 * This function does the following:
 *
 *  1. Allocates a request struct initialized to 0.
 *  2. Initializes the headers list in the request struct.
 *  3. Accepts a client connection from the server socket.
 *  4. Looks up the client information and stores it in the request struct.
 *  5. Opens the client socket stream for the request struct.
 *  6. Returns the request struct.
 *
 * The returned request struct must be deallocated using free_request.
 **/
Request * accept_request(int sfd) {
    Request *r;
    struct sockaddr raddr;
    socklen_t rlen = sizeof(struct sockaddr_storage);
    

    /* Allocate request struct (zeroed) */
    r = calloc(1, sizeof(Request));

    // Initialize Headers 
    r->headers = NULL;
    
    
    /* Accept a client */
    if ( (r->fd = accept(sfd, &raddr, &rlen)) == -1){
        fprintf(stderr, "Accept Failure: %s\n", strerror(errno));
        goto fail;
    }



    /* Lookup client information */
    if ( (getnameinfo(&raddr, rlen, r->host, NI_MAXHOST, r->port, NI_MAXSERV,  NI_NUMERICHOST)) != 0){
        fprintf(stderr, "Could Not Lookup Client Info: %s\n", gai_strerror(errno));
        goto fail;
    }
    

    /* Open socket stream */
    if ( (r->stream = fdopen(r->fd, "w+") ) == NULL) {
        fprintf(stderr, "Could Not Open File Stream: %s\n", strerror(errno));
        goto fail;
    }
    

    log("Accepted request from %s:%s", r->host, r->port);
    return r;

fail:
    /* Deallocate request struct */
    free_request(r);
    return NULL;
}

/**
 * Deallocate request struct.
 *
 * @param   r           Request structure.
 *
 * This function does the following:
 *
 *  1. Closes the request socket stream or file descriptor.
 *  2. Frees all allocated strings in request struct.
 *  3. Frees all of the headers (including any allocated fields).
 *  4. Frees request struct.
 **/
void free_request(Request *r) {
    if (!r) {
    	return;
    }

    /* Close socket or fd */
    if(r->stream)
        fclose(r->stream);

    /* Free allocated strings */
    if(r->method)
        free(r->method);
    if(r->uri)
        free(r->uri);
    if(r->path)
        free(r->path);
    if(r->query)
        free(r->query);

    /* Free headers */
    Header * h = r->headers;
    Header *temp;
    while ( h != NULL ){
        temp = h;
        h = h->next;
        free(temp->name);
        free(temp->data);
        free(temp);
        
    }  

    /* Free request */
    free(r);
}

/**
 * Parse HTTP Request.
 *
 * @param   r           Request structure.
 * @return  -1 on error and 0 on success.
 *
 * This function first parses the request method, any query, and then the
 * headers, returning 0 on success, and -1 on error.
 **/
int parse_request(Request *r) {
    /* Parse HTTP Request Method */
    if( parse_request_method(r) < 0 ){
        return -1;
    }
    /* Parse HTTP Requet Headers*/
    if( parse_request_headers(r) < 0 ){
        return -1;
    }

    return 0;
}

/**
 * Parse HTTP Request Method and URI.
 *
 * @param   r           Request structure.
 * @return  -1 on error and 0 on success.
 *
 * HTTP Requests come in the form
 *
 *  <METHOD> <URI>[QUERY] HTTP/<VERSION>
 *
 * Examples:
 *
 *  GET / HTTP/1.1
 *  GET /cgi.script?q=foo HTTP/1.0
 *
 * This function extracts the method, uri, and query (if it exists).
 **/
int parse_request_method(Request *r) {
    char buffer[BUFSIZ];
    char *method;
    char *uri;
    char *query;
    char *realURI;

    /* Read line from socket */
    if(!fgets(buffer, BUFSIZ, r->stream)){
        goto fail;
    }   
    
    /* Parse method and uri */
    method = strtok(buffer, WHITESPACE);
    r->method = strdup(method);
    if(!(r->method)){
        goto fail;
    }
    uri = strtok(NULL, WHITESPACE);
    if(!uri){
        goto fail;
    }

    /* Parse query from uri */
    query = strchr(uri, '?');
    if(!query){
        query = "";
        realURI = uri;
    }
    else{
        *query++ = '\0';
        realURI = strtok(uri, "?"); 
    }

    r->uri = strdup(realURI);           
    if(!(r->uri)){
        goto fail;
    }
    
    r->query = strdup(query);
    if(!(r->query)){
        goto fail;
    }
    /* Record method, uri, and query in request struct */
    debug("HTTP METHOD: %s", r->method);
    debug("HTTP URI:    %s", r->uri);
    debug("HTTP QUERY:  %s", r->query);

    return 0;

fail:
    return -1;
}

/**
 * Parse HTTP Request Headers.
 *
 * @param   r           Request structure.
 * @return  -1 on error and 0 on success.
 *
 * HTTP Headers come in the form:
 *
 *  <NAME>: <DATA>
 *
 * Example:
 *
 *  Host: localhost:8888
 *  User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:29.0) Gecko/20100101 Firefox/29.0
 *  Accept: text/html,application/xhtml+xml
 *  Accept-Language: en-US,en;q=0.5
 *  Accept-Encoding: gzip, deflate
 *  Connection: keep-alive
 *
 * This function parses the stream from the request socket using the following
 * pseudo-code:
 *
 *  while (buffer = read_from_socket() and buffer is not empty):
 *      name, data  = buffer.split(':')
 *      header      = new Header(name, data)
 *      headers.append(header)
 **/
int parse_request_headers(Request *r) {

    Header ** trailer = &(r->headers); 

    char buffer[BUFSIZ];
    char *name;
    char *data;

    /* Parse headers from socket */
    while(fgets(buffer, BUFSIZ, r->stream) && strlen(buffer) > 2){

        name = strtok(buffer, ":");
        if(!name){
            goto fail;
        }
        data = strtok(NULL, ":");
        if(!data){
            goto fail;
        }
        data = skip_whitespace(data); 
        chomp(data);
    
        Header * curr = calloc(1, sizeof(Header));
        if( !curr ){
            fprintf(stderr, "Could Not Allocate Header Struct\n");
            return -1;
        }

        curr->name = strdup(name);
        curr->data = strdup(data);
        *trailer = curr;
        trailer = &(curr->next);
 
    }
    

    if( !r->headers){
        fprintf(stderr, "Header Loading Unsucessful\n");
        goto fail;
    }

#ifndef NDEBUG
    for (Header *header = r->headers; header; header = header->next) {
    	debug("HTTP HEADER %s = %s", header->name, header->data);
    }
#endif
    return 0;

fail:
    return -1;
}

/* vim: set expandtab sts=4 sw=4 ts=8 ft=c: */
