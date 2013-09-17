// vim: set et ts=4 sts=4 sw=4:
/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the 
 *     GET method to serve static and dynamic content.
 */
#include "csapp.h"
#include "cache.h"

#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

void *doit(void *fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum, 
                 char *shortmsg, char *longmsg);
void send_error(int fd, int errnum, char *message);

// global variables for the cache
cache tiny_cache;
int readcnt;    /* Initially 0 */
sem_t mutex, w; /* Both initially 1 */

// global variables for POST request
int contentlen;

int main(int argc, char **argv)
{
    int listenfd, connfd, port, clientlen;
    struct sockaddr_in clientaddr;
    pthread_t tid;

    // initialize the readers-writers lock
    readcnt = 0;
    Sem_init(&mutex, 0, 1);
    Sem_init(&w, 0, 1);

    // handle SIGPIPE errors
    signal(SIGPIPE, SIG_IGN);

    // init the cache
    tiny_cache = new_cache();

    /* Check command line args */
    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }
    port = atoi(argv[1]);

    listenfd = Open_listenfd(port);
    while (1) {
        clientlen = sizeof(clientaddr);
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        //*
        //Pthread_create(&tid, NULL, doit, (void *) connfd);
        // */

        doit((void *) connfd);
    }
}
/* $end tinymain */

void get_key(char *header, char *key) {
    int i;
    for (i = 0; header[i] != '\0' && header[i] != ':'
            && header[i] != ' '; i++) {
        key[i] = header[i];
    }
    key[i] = '\0';
}

int get_value_firstchar(char *header) {
    int i = 0;
    while (header[i] != '\0' && header[i] != ':') {
        i++;
    }
    // Now header[i] == ':'. Increment i so that header[i] = 
    // the first character of the value string in header
    i++;

    // Now keep incrementing i so that it is pointing to a character
    // that is not whitespace
    while (header[i] == ' ') {
        i++;
    }

    return i;
}

void get_value(char *header, char *value) {
    int i = get_value_firstchar(header);

    // Now read the header's value into the value string
    int j = 0;
    while (header[i] != '\r' && header[i] != '\0') {
        value[j++] = header[i++];
    }
    value[j] = '\0';
}

void replace_value(char *header, char *new_value) {
    int i = get_value_firstchar(header);
    int j = 0;

    while (new_value[j] != '\0') {
        header[i++] = new_value[j++];
    }

    header[i++] = '\r';
    header[i++] = '\n';
    header[i++] = '\0';
}

void read_header(rio_t *rio, char *buf, int n) {
    char key[MAXLINE];

    rio_readlineb(rio, buf, n);
    get_key(buf, key);

    if (!strcmp(key, "Proxy-Connection")) {
        replace_value(buf, "close");
        return;
    }

    if (!strcmp(key, "Content-Length")) {
        char *value = key;
        get_value(buf, value);
        contentlen = atoi(value);
        printf("INFO: The content length is %d\n", contentlen);
        return;
    }

    if (!strcmp(key, "GET") || !strcmp(key, "POST")) {
        int i = 0;
        char buf1[MAXLINE];
        char buf2[MAXLINE];

        // Only edit the GET request if it begins with http
        sscanf(buf, "%s %s", buf1, buf2);
        if (buf2[0] == 'h' && buf2[1] == 't' && buf2[2] == 't'
                && buf2[3] == 'p') {
            // we need to put ONLY the latter part of the URL into
            // the GET or POST request.
            // Example: http://www.mysite.com/index.php => /index.php
            int numslashes = 0;


            for (i = 0; buf[i] != '\0'; i++) {

                if (buf[i] == '/')
                    numslashes++;

                if (numslashes == 3) {
                    // Now read the rest of the string into buf2
                    int j = 0;
                    while (buf[i] != ' ') {
                        buf2[j++] = buf[i++];
                    }
                    buf2[j++] = '\0';
                    break;
                }
            }

            memset(buf, 0, MAXLINE);
            sprintf(buf, "%s %s HTTP/1.0\r\n", key, buf2);
        }
        else {

            // change the last character to 0
            while (buf[i] != '\r') {
                i++;
            }
            i--;
            buf[i] = '0';
        }

        return;
    }

    if (!strcmp(key, "Connection")) {
        buf[0] = '\0';
        return;
    }

    if (!strcmp(key, "Accept-Encoding")) {
        replace_value(buf, "gzip, deflate");
        return;
    }

    if (!strcmp(key, "Accept")) {
        replace_value(buf, "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8");
        return;
    }

    if (!strcmp(key, "User-Agent")) {
        replace_value(buf, "Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3");
        return;
    }
}

/*
 * doit - handle one HTTP request/response transaction
 */
/* $begin doit */
void *doit(void *fd)
{
//    Pthread_detach(Pthread_self());

    int client_fd = (int) fd;
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char buf2[MAXLINE];
    char hostname[MAXLINE];
    rio_t client_rio, server_rio;

    memset(buf, 0, MAXLINE);

    // initialize client_rio for reading
    Rio_readinitb(&client_rio, client_fd);

    // read the first header
    read_header(&client_rio, buf, MAXLINE);

    // Read in the method, URI, and version
    sscanf(buf, "%s %s %s", method, uri, version);

    //if (strcasecmp(method, "GET") && strcasecmp(method, "POST")) {
//    if (strcasecmp(method, "GET")) {
//        clienterror(client_fd, method, "501", "Not Implemented",
//                "Tiny does not implement this method");
//        return NULL;
//    }

    // read the second header
    rio_readlineb(&client_rio, buf2, MAXLINE);

    // append the second header to our buffer
    strcat(buf, buf2);

    // Debug messages:
    printf("*********************************************\n");
    printf("*********************************************\n");
    printf("Cache size:%d\n", tiny_cache->size);
    printf("Incoming request:\n%s\n", buf);

    // get the hostname from the second header
    get_value(buf2, hostname);

    // attach a default header to the request
    strcat(buf, "Connection: close\r\n");

// DEBUG: very useful
//////////////////////////////////////////////////////////////
////        for (i = 0; i < contentlen; i++) {
//    puts("INFO: the internal buffer of client_rio...");
//    printf("%s", client_rio.rio_bufptr);
//    fflush(stdout);
//        while (1) {
//            int numread = recv(client_fd, buf, 1, 0);
////            Rio_writen(serverfd, buf, numread);
//            putchar(buf[0]);
//            fflush(stdout);
//        }
//  /DEBUG END

    // read the rest of the headers from the client
    while (strcmp(buf2, "\r\n")) {
        read_header(&client_rio, buf2, MAXLINE);
        strcat(buf, buf2);
    }

    // Before opening a connection to the server, check whether we have
    // the web object in our cache
    char URL[MAXLINE];
    char huge_buf[MAX_OBJECT_SIZE];
    int webobj_len;

    // Set the URL that the client is requesting
    memset(URL, 0, MAXLINE);
    strcat(URL, hostname);
    strcat(URL, uri);

    // Need to acquire a readers lock here
    P(&mutex);
    printf("We're reading!\n");
    readcnt++;
    if (readcnt == 1)
        P(&w);
    V(&mutex);
    if ((webobj_len = read_cache(tiny_cache, URL, huge_buf)) > 0) {
        // Now we can release the lock here
        P(&mutex);
        readcnt--;
        if (readcnt == 0)
            V(&w);
        V(&mutex);

        printf("########################################\n");
        printf("webobj_len:%d\n", webobj_len);
        Rio_writen(client_fd, huge_buf, webobj_len);

        printf("Finished handling request\n");

        Close(client_fd);
        return NULL;
    } else {
        // Now we can release the lockhere
        P(&mutex);
        readcnt--;
        if (readcnt == 0)
            V(&w);
        V(&mutex);
    }

    // Open a connection to the server and read in our headers.
    // Then read the server's response back to the client.

    int serverfd = open_clientfd(hostname, 80);

    if (serverfd < 0) {
        printf("Couldn't open client connection\n");
        send_error(client_fd, 404, "Not Found");
        Close(client_fd);
        return NULL;
    }
    else {
        printf("YAYYYY A CONNECTION WAS SUCCESSFUL!\n");
    }

    // initialize server_rio for reading from the server
    rio_readinitb(&server_rio, serverfd);

    // Forward the headers to the server
    Rio_writen(serverfd, buf, strlen(buf));

    puts("INFO: these headers were forwarded");
    printf("%s", buf);

    if (!strcasecmp(method, "POST")) {
        puts("INFO: Forwarding POST content...");

        puts("INFO: the internal buffer of client_rio...");
        printf("%s", client_rio.rio_bufptr);
        fflush(stdout);

        Rio_writen(serverfd, client_rio.rio_bufptr, strlen(client_rio.rio_bufptr));

        //// The following code will work WITHOUT csapp.c....
        //// Because csapp.c buffers it. Really interesting.
        //int i;
        //for (i = 0; i < contentlen; i++) {
        //    int numread = recv(client_fd, buf, 1, 0);
        //    Rio_writen(serverfd, buf, numread);
        //    putchar(buf[0]);
        //    fflush(stdout);
        //}
    }

    // zero the buffer
    memset(buf, 0, MAXLINE);

    // initialize variables for reading from the website's servers
    int numread = 0;
    int count = 0;
    webobj_len = 0;
    char webobj_buf[MAX_OBJECT_SIZE];
    memset(webobj_buf, 0, MAX_OBJECT_SIZE);

    // stream in data from the server
    while ((numread = Rio_readnb(&server_rio, buf, MAXLINE)) > 0) {
        // debug messages:
        count++;
        printf("*************** Iteration %d ********\n", count);
        printf("Numread:%d\n", numread);

        // send off the buffer with each iteration of the while loop
        Rio_writen(client_fd, buf, numread);

        printf("INFO: We are sending this from the website to the client\n");
        printf("%s", buf);

        // Check whether reading in new data will fit in buffer
        // for web object
        if (webobj_len + numread < MAX_OBJECT_SIZE) {
            // read new data into the webobj_buf
            memcpy(webobj_buf + webobj_len, buf, numread);
        }

        // increase webobj_len by numread
        if (webobj_len < MAX_OBJECT_SIZE) {
            webobj_len += numread;
        }

        // reset the buf
        memset(buf, 0, MAXLINE);
    }

    // check whether the final size of the webobj is less than
    // MAX_OBJECT_SIZE
    if (webobj_len < MAX_OBJECT_SIZE) {
        printf("webobj_len:%d\n", webobj_len);

        // Insert the Web Object into our cache.
        P(&w);
        printf("We're writing!\n");
        insert_webobj(tiny_cache, URL, webobj_buf, webobj_len);
        V(&w);
    }

    printf("\nFinished handling request\n");

    // Free resources
    Close(serverfd);
    Close(client_fd);

    return NULL;
}
/* $end doit */

void send_error(int fd, int errnum, char *cause) {
    char body[MAXLINE];
    memset(body, 0, MAXLINE);
    sprintf(body, "HTTP/1.0 %d %s\r\n\r\n", errnum, cause);
    Rio_writen(fd, body, MAXLINE);
}

/*
 * clienterror - returns an error message to the client
 */
/* $begin clienterror */
void clienterror(int fd, char *cause, char *errnum,
                 char *shortmsg, char *longmsg)
{
    char buf[MAXLINE], body[MAXBUF];

    /* Build the HTTP response body */
    sprintf(body, "<html><title>Tiny Error</title>");
    sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
    sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

    /* Print the HTTP response */
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
    Rio_writen(fd, buf, strlen(buf));
    Rio_writen(fd, body, strlen(body));
}
/* $end clienterror */
