#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include "dir.h"
#include "usage.h"
#include <string.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>
#include <arpa/inet.h>
#include <unistd.h>


#define BACK_LOG 10 // how many pending connections queue will hold

// Here is an example of how to use the above function. It also shows
// one how to get the arguments passed on the command line.
int portNo;



// helper function to see if DIR is past working directory

int startsWith(const char *a, const char *b)
{
    if(strncmp(a, b, strlen(b)) == 0) return 1;
    return 0;
}

int sendString(int conn, const char* f, ...) {
    // using VA for passing messages
    va_list args;
    char msgbuf[1024];
    va_start(args,f);
    vsnprintf(msgbuf, sizeof(msgbuf), f, args);
    va_end(args);
    
    return write(conn, msgbuf, strlen(msgbuf));
}

// when thread has started, this runs:
void *connection_handler (void *server_sock) {
    int sock = * (int *) server_sock;
    // read size
    int size; // to read commands from user
    
    struct sockaddr_in server_addr;
    int server_addr_len = sizeof(server_addr);
    getsockname(sock, (struct sockaddr*) &server_addr, &server_addr_len);
    
    int data_client = -1;
    
    struct sockaddr_in data_client_addr;
    int data_client_len = sizeof(data_client_addr);
    
    char cwd[1024];
    
    char* message;
    message = "220 - Hello, You are successfully connected \n";
    write(sock,message, strlen(message));
    
    message = "220 - Please input username (cs317) \n";
    
    write(sock, message, strlen(message));
    char* command[5];
    char* parameter[1000];
    char* client_message[2000];
    char rootWorkingDirectory[1024];
    getcwd(rootWorkingDirectory, sizeof(rootWorkingDirectory));
    
    int logged_in = 0;
    int stream_mode = 0;
    int fs_type = 0;
    int passive_mode = 0;
    int ascii_type = 0;
    
    int pasv_port;
    int pasv_sock;
    
    struct sockaddr_in pasv_server;
    
    while ((size = recv(sock, client_message, 2000,0)) > 0) {
        
        
        // wanted to use switches but requires integers
        sscanf (client_message, "%s", command);
        
        // USER CASE
        if(!strcasecmp(command, "USER")) {
            if(logged_in == 0) {
                sscanf(client_message, "%s%s", parameter, parameter);
                
                if(!strcasecmp(parameter, "cs317")) {
                    logged_in = 1;
                    sendString(sock, "230 - Login Successful \n");
                }
                else {
                sendString(sock, "331 - This Server is for CS317 only \n");
                }
            }
            else {
                sendString(sock, "331 - Already Logged In \n");
            }
        }
    
        
        
        // QUIT CASE
        else if(!strcasecmp(command, "QUIT")) {
            sendString(sock, "221 - Quitting Connection, Seeya. \n");
            fflush(stdout);
            close(sock);
            close(pasv_sock);
            break;
        }
        
        
        
        
        
        // TYPE CASE
        else if(!strcasecmp (command, "TYPE")) {
            if(logged_in == 1) {
                sscanf(client_message, "%s%s", parameter, parameter);
                
                
                if(!strcasecmp(parameter, "A")) {
                    if(ascii_type == 0) {
                        ascii_type = 1;
                        
                        sendString(sock, "200 - Type is Now ASCII \n");
                    }
                    
                    else {
                        sendString(sock, "200 - Type was already ASCII \n");
                    }
                } else if (!strcasecmp(parameter, "I")) {
                    if(ascii_type == 1) {
                        ascii_type = 0;
                        sendString(sock, "200 - Type is now IMAGE \n");
                    } else {
                        sendString(sock, "200 - Type was already IMAGE \n");
                    }
                } else {
                    sendString(sock, "500 - This server support only A and I Type \n");
                }
            } else {
                // user not logged in yet
                
                sendString(sock, "500 - Please login first \n");
            }
        }
        
        
        // MODE CASE
        
        else if(!strcasecmp (command, "MODE")) {
            if(logged_in == 1) {
                sscanf(client_message, "%s%s", parameter, parameter);
                
                if(!strcasecmp(parameter, "S")) {
                    
                    if(stream_mode == 0) {
                        stream_mode = 1;
                        sendString(sock, "200 - Entering Stream Mode \n");
                    }
                    else {
                        sendString(sock, "200 - Already in Stream Mode \n");
                    }
                }
                
                else {
                    sendString(sock, "Only Supports S Mode \n");
                }
            }
            
            else {
                
                // user not logged in
                sendString(sock, "500 - Please login first \n");
            }
        }
        
        
        // STRU CASE
        
        else if(!strcasecmp(command, "STRU")) {
            if(logged_in == 1) {
                sscanf(client_message, "%s%s", parameter,parameter);
                
                if(!strcasecmp(parameter, "F")) {
                    if(fs_type == 0) {
                        fs_type = 1;
                        
                        sendString(sock, "200 - Structure has been set to File Structure \n");
                    } else {
                        sendString(sock, "200 - Data was already set to File Structure \n");
                    }
                }
                else {
                    sendString(sock, "500 - This Server only supports Type F \n");
                }
            }
            else {
                sendString(sock, "500 - Please Login First \n");
            }
        }
        
        
        // CWD CASE
        else if (!strcasecmp(command, "CWD")) {
            if(logged_in == 1) {
                sscanf(client_message, "%s%s", parameter, parameter);
                
                if(startsWith(parameter, ".") == 1) {
                    sendString(sock, "500 - Cannot use . or .. for a directory change \n");
                
                } else {
                    
                    if(chdir(parameter) == 0) {
                        char directory[1024];
                        getcwd(directory, sizeof(directory));
                        sendString(sock, "200 - Directory is now: %s \n", directory);
                    } else {
                        sendString(sock, "500 - Error in Switching Directories \n");
                    }
                }
            } else {
                sendString(sock, "500 - Please Login First \n");
            }
        }
        
        
        // CWDUP CASE
        else if (!strcasecmp(command, "CDUP")) {
            if(logged_in == 1) {
                char directory[1024];
                char nextDirectory[1024];
                getcwd(directory,sizeof(directory));
                
                if(chdir("..") == 0) {
                    getcwd(nextDirectory,sizeof(nextDirectory));
                    if(startsWith(nextDirectory, rootWorkingDirectory) == 1) {
                        sendString(sock, "200 - In Directory: %s \n", nextDirectory);
                    } else {
                        chdir(directory);
                        sendString(sock, "500 - Access Denied, Back to: %s \n", directory);
                    }
                } else {
                    sendString(sock, "500 - Error in Switching Directories \n");
                }
            } else {
                sendString(sock, "500 - Please Login First \n");
            }
        }
        
        //PASV CASE
        else if (!strcasecmp(command, "PASV")) {
            if (passive_mode == 0) {
                do {
                    pasv_port = (rand() % 64551 + 1024);
                    pasv_sock = socket(AF_INET, SOCK_STREAM, 0);
                    
                    pasv_server.sin_family = AF_INET;
                    pasv_server.sin_addr.s_addr = INADDR_ANY;
                    pasv_server.sin_port = htons( pasv_port );
                    
                } while (bind(pasv_sock, (struct sockaddr *)&pasv_server, sizeof(pasv_server)) < 0);
                
                if (pasv_sock < 0 ) {
                    sendString(sock, "500 - Error entering PASV. \n");
                }
                
                else {
                    listen(pasv_sock, 1);
                    passive_mode = 1;
                    
                    uint32_t _t = server_addr.sin_addr.s_addr;
                    
                    sendString(sock, "227 - Entering passive mode (%d,%d,%d,%d,%d,%d) \r\n",
                             _t&0xff,
                             (_t>>8)&0xff,
                             (_t>>16)&0xff,
                             (_t>>24)&0xff,
                             pasv_port>>8,
                             pasv_port&0xff);
                }
            }
            
            else {
                sendString(sock, "227 - Already in passive mode. Port number: %d\n", pasv_port);
            }
            
        }
        
        else {
            sendString(sock, "500 - Invalid Command \n");
        }
    }
    
}





int main(int argc, char **argv) {
    
    int i;
    // This is some sample code feel free to delete it
    // This is the main program for the thread version of nc
    
    // Check the command line arguments
    if (argc != 2) {
        usage(argv[0]);
        return -1;
    }
    
    portNo = atoi(argv[1]);
    
    int server_sock;
    int client_sock;
    int *new_sock;
    int c;

    struct sockaddr_in server;
    struct sockaddr_in client;
    
    // new socket to be put on the server
    
    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    
    if(server_sock == -1) {
        perror("Failed to create Socket");
        return 1;
    }
    
    puts("Sucessfully created Socket");
    
    // sockaddr_in builtin structure
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(portNo);
    
    if(bind (server_sock, (struct sockaddr *) &server, sizeof(server)) < 0) {
        perror ("Error binding socket");
        return 1;
    }
    
    puts("Bind socket successfull");
    
    //listen
    
    listen (server_sock, BACK_LOG);
    
    // accept connections
    
    puts("Waiting for Connections ..");
    
    c = sizeof(struct sockaddr_in);
    
    while ((client_sock = accept(server_sock, (struct sockaddr *) &client, (socklen_t*) &c))) {
        
        puts("Connection Accepted");
        pthread_t helper_thread;
        
        new_sock = malloc(1);
        
        *new_sock = client_sock;
        
        if(pthread_create (&helper_thread, NULL, connection_handler, (void*) new_sock) < 0) {
            perror ("Failed to create new thread ");
            return 1;
        }
        
        pthread_join(helper_thread, NULL);
        
        puts("Closing Connection");
    }
    
    if(client_sock < 0) {
        perror("Failed to accept");
        return 1;
    }
    
    
    
    
    
        
//         This is how to call the function in dir.c to get a listing of a directory.
//         It requires a file descriptor, so in your code you would pass in the file descriptor
//         returned for the ftp server's data connection
//
//            printf("Printed %d directory entries\n", listFiles(1, "."));
//            return 0;
    
    
        
    return 0;
    
}
