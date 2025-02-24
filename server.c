#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <arpa/inet.h>
#include <sys/socket.h>

/**
 * Project 1 starter code
 * All parts needed to be changed/added are marked with TODO
 */

#define BUFFER_SIZE 1024
#define DEFAULT_SERVER_PORT 8081
#define DEFAULT_REMOTE_HOST "131.179.176.34"
#define DEFAULT_REMOTE_PORT 5001

struct server_app {
    // Parameters of the server
    // Local port of HTTP server
    uint16_t server_port;

    // Remote host and port of remote proxy
    char *remote_host;
    uint16_t remote_port;
};

// The following function is implemented for you and doesn't need
// to be change
void parse_args(int argc, char *argv[], struct server_app *app);

// The following functions need to be updated
void handle_request(struct server_app *app, int client_socket);
void serve_local_file(int client_socket, const char *path);
void proxy_remote_file(struct server_app *app, int client_socket, const char *path);

// The main function is provided and no change is needed
int main(int argc, char *argv[])
{
    struct server_app app;
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len;
    int ret;

    parse_args(argc, argv, &app);

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(app.server_port);

    // The following allows the program to immediately bind to the port in case
    // previous run exits recently
    int optval = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_socket, 10) == -1) {
        perror("listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d\n", app.server_port);

    while (1) {
        client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_len);
        if (client_socket == -1) {
            perror("accept failed");
            continue;
        }
        
        printf("Accepted connection from %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        handle_request(&app, client_socket);
        close(client_socket);
    }

    close(server_socket);
    return 0;
}

void parse_args(int argc, char *argv[], struct server_app *app)
{
    int opt;

    app->server_port = DEFAULT_SERVER_PORT;
    app->remote_host = NULL;
    app->remote_port = DEFAULT_REMOTE_PORT;

    while ((opt = getopt(argc, argv, "b:r:p:")) != -1) {
        switch (opt) {
        case 'b':
            app->server_port = atoi(optarg);
            break;
        case 'r':
            app->remote_host = strdup(optarg);
            break;
        case 'p':
            app->remote_port = atoi(optarg);
            break;
        default: /* Unrecognized parameter or "-?" */
            fprintf(stderr, "Usage: server [-b local_port] [-r remote_host] [-p remote_port]\n");
            exit(-1);
            break;
        }
    }

    if (app->remote_host == NULL) {
        app->remote_host = strdup(DEFAULT_REMOTE_HOST);
    }
}

void handle_request(struct server_app *app, int client_socket) {
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;

    // Read the request from HTTP client
    // Note: This code is not ideal in the real world because it
    // assumes that the request header is small enough and can be read
    // once as a whole.
    // However, the current version suffices for our testing.
    bytes_read = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
    if (bytes_read <= 0) {
        return;  // Connection closed or error
    }

    buffer[bytes_read] = '\0';

    // copy buffer to new strings
    char *request = malloc(strlen(buffer) + 1);
    char *request_strtok = malloc(strlen(buffer) + 1);

    strcpy(request, buffer);
    strcpy(request_strtok, buffer);

    // TODO: Parse the header and extract essential fields, e.g. file name
    // Hint: if the requested path is "/" (root), default to index.html
    // char file_name[] = "index.html";
    char* file_name;

    // request is of the form "GET /path HTTP/1.1"
    // we want to extract the path from the request
    char *path = strtok(request_strtok, " ");
    path = strtok(NULL, " ");

    if (path == NULL || strcmp(path, "/") == 0) {
        // default to index.html if path is empty or root
        file_name = "index.html";
    } else {
        // remove the leading / from the path
        file_name = path + 1;
    }

    // check the file extension. only txt, html and jpg files are supported
    char *extension = strrchr(path, '.');
    char *response;

    // no period in the file name, so the file has no extension we return an
    // octet-stream response in this case
    if (extension == NULL) {
        extension = "";
    }

    // if the file extension isn't supported, return a 501 response only txt,
    // html, jpg, ts, m3u8 and no extension files are supported
    if (strcmp(extension, ".txt") != 0 && 
        strcmp(extension, ".html") != 0 &&
        strcmp(extension, ".jpg") != 0 &&
        strcmp(extension, ".ts") != 0 &&
        strcmp(extension, ".m3u8") != 0 &&
        strcmp(extension, "") != 0) {
        response = "HTTP/1.0 501 Not Implemented\r\n\r\n"
                    "File extension not supported";
        send(client_socket, response, strlen(response), 0);
        return;
    }

    // parse the file name to replace %20 with spaces
    char *temp = malloc(strlen(file_name) + 1);
    int i = 0;
    int j = 0;
    while (file_name[i] != '\0') {
        if (file_name[i] == '%' && file_name[i + 1] == '2' && file_name[i + 2] == '0') {
            temp[j] = ' ';
            i += 3;
        } else if (file_name[i] == '%' && file_name[i + 1] == '2' && file_name[i + 2] == '5') {
            temp[j] = '%';
            i += 3;
        } else {
            temp[j] = file_name[i];
            i++;
        }
        j++;
    }
    
    file_name = temp;

    // printf("file_name: %s\n", file_name);

    // TODO: Implement proxy and call the function under condition
    // specified in the spec

    if (strcmp(extension, ".ts") == 0) {
        // forward original request
        proxy_remote_file(app, client_socket, request);
    } else {
        serve_local_file(client_socket, file_name);
    }
}

void serve_local_file(int client_socket, const char *path) {
    // TODO: Properly implement serving of local files
    // The following code returns a dummy response for all requests
    // but it should give you a rough idea about what a proper response looks like
    // What you need to do 
    // (when the requested file exists):
    // * Open the requested file
    // * Build proper response headers (see details in the spec), and send them
    // * Also send file content
    // (When the requested file does not exist):
    // * Generate a correct response

    // char* response = "HTTP/1.0 200 OK\r\n"
    //                   "Content-Type: text/plain; charset=UTF-8\r\n"
    //                   "Content-Length: 15\r\n"
    //                   "\r\n"
    //                   "Sample response";

    char* response;

    // our server does not need to handle cases where the client requests a 
    // file that is in a subdirectory, so we can expect the file to be in the
    // same directory as the server. 
    // additionally, the file name only contains alphabets, periods, spaces or
    // % characters.

    // open the file in read mode
    // we attempt opening first to check if the file exists
    FILE *file = fopen(path, "r");

    if (file == NULL) {
        // extra credit: if the file does not exist, return a 404 response
        response = "HTTP/1.0 404 Not Found\r\n\r\n"
                   "File not found";
        send(client_socket, response, strlen(response), 0);
        return;
    }

    // printf("file exists\n");

    // get the file extension (it was verified in handle_request that the file
    // has an extension of txt, html, jpg, or no extension. ts was handled
    // separately in proxy_remote_file)
    char *extension = strrchr(path, '.');

    // no period in the file name, so the file has no extension
    if (extension == NULL) {
        extension = "";
    }

    // get the file size
    fseek(file, 0, SEEK_END);
    int file_size = ftell(file);
    rewind(file);

    // read the file into a buffer
    char *file_buffer = malloc(file_size);
    fread(file_buffer, 1, file_size, file);

    // build the response
    char* sprintfed_response = malloc(100);
    sprintf(sprintfed_response, "HTTP/1.0 200 OK\r\n"
                        "Content-Type: %s\r\n"
                        "Content-Length: %d\r\n"
                        "\r\n", 
                        (strcmp(extension, ".txt") == 0 ? "text/plain" : 
                        (strcmp(extension, ".html") == 0 ? "text/html" : 
                        (strcmp(extension, ".jpg") == 0 ? "image/jpeg" : 
                        (strcmp(extension, ".m3u8") == 0 ? "text/plain" : "application/octet-stream")))), 
                        file_size);

    // send the response
    send(client_socket, sprintfed_response, strlen(sprintfed_response), 0);

    // send the file
    send(client_socket, file_buffer, file_size, 0);

    return;
}

void proxy_remote_file(struct server_app *app, int client_socket, const char *request) {
    // TODO: Implement proxy request and replace the following code
    // What's needed:
    // * Connect to remote server (app->remote_server/app->remote_port)
    // * Forward the original request to the remote server
    // * Pass the response from remote server back
    // Bonus:
    // * When connection to the remote server fail, properly generate
    // HTTP 502 "Bad Gateway" response

    // printf("Proxying request to remote server\n");
    // printf("request: %s\n", request);

    // create socket
    int remote_socket = socket(AF_INET, SOCK_STREAM, 0);

    // printf("socket created\n");

    if (remote_socket == -1) {
        // if the socket creation fails, return a 502 response
        char response[] = "HTTP/1.0 502 Bad Gateway\r\n\r\n"
                            "Bad Gateway";
        send(client_socket, response, strlen(response), 0);
        return;
    }

    // set up connection to the remote server on localhost:5001
    struct sockaddr_in remote_addr;
    remote_addr.sin_family = AF_INET;
    remote_addr.sin_port = htons(app->remote_port);
    remote_addr.sin_addr.s_addr = inet_addr(app->remote_host);

    // printf("remote host: %s\n", app->remote_host);
    // printf("remote port: %d\n", app->remote_port);

    // connect to the remote server
    if (connect(remote_socket, (struct sockaddr *)&remote_addr, sizeof(remote_addr)) == -1) {
        // if the connection fails, return a 502 response
        char response[] = "HTTP/1.0 502 Bad Gateway\r\n\r\n"
                            "Bad Gateway";
        send(client_socket, response, strlen(response), 0);
        return;
    }

    // printf("connected to remote server\n");

    // send the request to the remote server
    send(remote_socket, request, strlen(request), 0);

    // printf("sent request to remote server\n");
    // printf("request: %s\n", request);

    // receive the response from the remote server
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;

    while ((bytes_read = recv(remote_socket, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[bytes_read] = '\0';
        send(client_socket, buffer, bytes_read, 0);
    }

    // close the remote socket
    close(remote_socket);

    return;

    // char response[] = "HTTP/1.0 501 Not Implemented\r\n\r\n";
    // send(client_socket, response, strlen(response), 0);
}