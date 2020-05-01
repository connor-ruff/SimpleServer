/* handler.c: HTTP Request Handlers */

#include "spidey.h"

#include <errno.h>
#include <limits.h>
#include <string.h>

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

/* Internal Declarations */
Status handle_browse_request(Request *request);
Status handle_file_request(Request *request);
Status handle_cgi_request(Request *request);
Status handle_error(Request *request, Status status);

/**
 * Handle HTTP Request.
 *
 * @param   r           HTTP Request structure
 * @return  Status of the HTTP request.
 *
 * This parses a request, determines the request path, determines the request
 * type, and then dispatches to the appropriate handler type.
 *
 * On error, handle_error should be used with an appropriate HTTP status code.
 **/
Status  handle_request(Request *r) {
    Status result;
    /* Parse request */
    if(parse_request(r) < 0){
        result = HTTP_STATUS_BAD_REQUEST;       // We should double check these statuses if we're ever having trouble with valgrind later
        handle_error(r, result);
        return result;
    }

    /* Determine request path */
    r->path = determine_request_path(r->uri);
    if(!(r->path)){
        result = HTTP_STATUS_NOT_FOUND;
        handle_error(r, result);
        return result;
    }

    debug("HTTP REQUEST PATH: %s", r->path);
    /* Dispatch to appropriate request handler type based on file type */
    struct stat path_stat;
    if( stat(r->path, &path_stat) < 0 ){
        result = handle_error(r, HTTP_STATUS_NOT_FOUND);
    }
    
    if(S_ISDIR(path_stat.st_mode) == 1){
       result = handle_browse_request(r);
       if(result != HTTP_STATUS_OK)
            handle_error(r, result);
    }
    else if(access(r->path, X_OK) == 0){
       result = handle_cgi_request(r);
       if(result != HTTP_STATUS_OK)
           handle_error(r, result);
    }
    else if(S_ISREG(path_stat.st_mode) == 1){
       result = handle_file_request(r);
       if(result != HTTP_STATUS_OK)
           handle_error(r, result);
    }
    else{                                           // the else condition may be unnecessary here
        result = HTTP_STATUS_BAD_REQUEST;
        handle_error(r, result);
    }

    log("HTTP REQUEST STATUS: %s", http_status_string(result));

    return result;
}

/**
 * Handle browse request.
 *
 * @param   r           HTTP Request structure.
 * @return  Status of the HTTP browse request.
 *
 * This lists the contents of a directory in HTML.
 *
 * If the path cannot be opened or scanned as a directory, then handle error
 * with HTTP_STATUS_NOT_FOUND.
 **/
Status  handle_browse_request(Request *r) {
    struct dirent **entries;
    int n;
    

    /* Open a directory for reading or scanning */
    n = scandir(r->path, &entries, 0, alphasort);
    if(n < 0){
        return HTTP_STATUS_NOT_FOUND;
    }
    /* Write HTTP Header with OK Status and text/html Content-Type */
    fprintf(r->stream, "HTTP/1.0 200 OK\r\n");                      // Looking at bui's code in lecture and the hints for the project, I think these are the only headers we need
    fprintf(r->stream, "Content-Type: text/html\r\n");
    fprintf(r->stream, "\r\n");
    /* For each entry in directory, emit HTML list item */
    fprintf(r->stream, "<ul>\n");
    for(int i = 0; i < n; i++){
        if(!(streq(entries[i]->d_name, ".")) && !(streq(r->uri, "/"))){                              // Used to not print previous directory.  Changed based on bui's output
            fprintf(r->stream, "<li><a href=\"%s/%s\">%s</a></li>\n", r->uri, entries[i]->d_name, entries[i]->d_name);    // using href now
            free(entries[i]);
        }
        else if(streq(r->uri, "/")){
            if(!streq(entries[i]->d_name, ".")){
                fprintf(r->stream, "<li><a href=\"/%s\">%s</a></li>\n", entries[i]->d_name, entries[i]->d_name);
                free(entries[i]);
            }
            else{
                free(entries[i]);
            }
        }
        else{
            free(entries[i]);
        }
    }
    fprintf(r->stream, "</ul>\n");
    
    free(entries);

    /* Return OK */
    return HTTP_STATUS_OK;
}

/**
 * Handle file request.
 *
 * @param   r           HTTP Request structure.
 * @return  Status of the HTTP file request.
 *
 * This opens and streams the contents of the specified file to the socket.
 *
 * If the path cannot be opened for reading, then handle error with
 * HTTP_STATUS_NOT_FOUND.
 **/
Status  handle_file_request(Request *r) {
    FILE *fs;
    char buffer[BUFSIZ];
    char *mimetype = NULL;
    size_t nread;

    /* Open file for reading */
    fs = fopen(r->path, "r");
    if(!fs)
        return HTTP_STATUS_NOT_FOUND;

    /* Determine mimetype */
    mimetype = determine_mimetype(r->path);
    if(!mimetype)
        goto fail;

    /* Write HTTP Headers with OK status and determined Content-Type */
    fprintf(r->stream, "HTTP/1.0 200 OK\r\n"); 
    fprintf(r->stream, "Content-Type: %s\r\n", mimetype);           
    fprintf(r->stream, "\r\n");                                 // Separate file text and headers

    /* Read from file and write to socket in chunks */
    nread = fread(buffer, 1, BUFSIZ, fs);
    while(nread > 0){
        fwrite(buffer, 1, nread, r->stream);
        nread = fread(buffer, 1, BUFSIZ, fs);
    }

    /* Close file, deallocate mimetype, return OK */
    free(mimetype);                     
    fclose(fs);

    return HTTP_STATUS_OK;

fail:
    /* Close file, free mimetype, return INTERNAL_SERVER_ERROR */
    if(fs){
        fclose(fs);
    }
    if(mimetype){
        free(mimetype);
    }
    return HTTP_STATUS_INTERNAL_SERVER_ERROR;
}

/**
 * Handle CGI request
 *
 * @param   r           HTTP Request structure.
 * @return  Status of the HTTP file request.
 *
 * This popens and streams the results of the specified executables to the
 * socket.
 *
 * If the path cannot be popened, then handle error with
 * HTTP_STATUS_INTERNAL_SERVER_ERROR.
 **/
Status  handle_cgi_request(Request *r) {
    FILE *pfs;
    char buffer[BUFSIZ];

    /* Export CGI environment variables from request:
     * http://en.wikipedia.org/wiki/Common_Gateway_Interface */
    if(r->query)
        setenv("QUERY_STRING", r->query, 1);
    /* Export CGI environment variables from request headers */
    setenv("SCRIPT_FILENAME", r->uri, 1);
    setenv("REQUEST_METHOD", r->method, 1);
    setenv("REMOTE_ADDR", r->host, 1);
    setenv("REQUEST_URI", r->uri, 1);
    setenv("REMOTE_PORT", r->port, 1);
    setenv("DOCUMENT_ROOT", r->path, 1);
    setenv("SERVER_PORT", r->port, 1);
    setenv("HTTP_HOST", r->host, 1);
    setenv("HTTP_USER_AGENT", r->uri, 1);

    
    /* POpen CGI Script */
    pfs = popen(r->path, "r");
    if(!pfs)
        return HTTP_STATUS_INTERNAL_SERVER_ERROR;
    /* Copy data from popen to socket */
    while(fgets(buffer, BUFSIZ, pfs)){
        fputs(buffer, r->stream);
    }
    /* Close popen, return OK */
    pclose(pfs);
    return HTTP_STATUS_OK;
}

/**
 * Handle displaying error page
 *
 * @param   r           HTTP Request structure.
 * @return  Status of the HTTP error request.
 *
 * This writes an HTTP status error code and then generates an HTML message to
 * notify the user of the error.
 **/
Status  handle_error(Request *r, Status status) {
    const char *status_string = http_status_string(status);
    /* Write HTTP Header */
    fprintf(r->stream, "HTTP/1.0 %s\r\n", status_string);
    fprintf(r->stream, "Content-Type: text/html\r\n");
    fprintf(r->stream, "\r\n");
    /* Write HTML Description of Error*/
    fprintf(r->stream,"<html><body>");
    fprintf(r->stream, "<h1><strong>%s</strong></h1><h2>Good Try!</h2>", status_string);
    fprintf(r->stream, "<center><img src=\"https://www.dailydot.com/wp-content/uploads/2019/04/winnie-the-pooh-1024x512.jpg\"></center>");
    fprintf(r->stream,"</html></body>");
    
    /* Return specified status */
    return status;
}

/* vim: set expandtab sts=4 sw=4 ts=8 ft=c: */
