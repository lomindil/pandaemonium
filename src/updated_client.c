#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    #define CLOSESOCKET closesocket
    #define socklen_t int
#else
    #include <unistd.h>
    #include <arpa/inet.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #define CLOSESOCKET close
#endif

#include <string.h>
#include <stdlib.h>
#include <stdio.h>


#define PORT 11235

#define USERNAME_SIZE 50
#define PASSWORD_SIZE 20
#define MESSAGE_SIZE 1024
#define BUFFER_SIZE 1024

#define RED     "\033[1;31m"
#define GREEN   "\033[1;32m"
#define YELLOW  "\033[1;33m"
#define BLUE    "\033[1;34m"
#define MAGENTA "\033[1;35m"
#define CYAN    "\033[1;36m"
#define RESET   "\033[0m"


int UserInfovalidate(char* username, char* buffer, int sock);
int registerUsername(char* username, char* buffer, char* passwd, int sock);
void ChatModeSelect(char* buffer, int *mode);
void StartPrivateChat(char* buffer, char* message, int sock);
void StartGroupChat(char* buffer, char* message, int sock,const char* username);
void ChatSession(char *buffer, char *message, int sock,const char* sessionState);


int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <server_ip>\n", argv[0]);
        return 1;
    }

#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        printf("WSAStartup failed: %d\n", WSAGetLastError());
        return 1;
    }
#endif

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket failed");
        return 1;
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, argv[1], &server_addr.sin_addr) <= 0) {
        printf( "Invalid address / Address not supported\n");
        return 1;
    }

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        return 1;
    }

    //printf("Connected to server at %s:%d\n", argv[1], PORT);

    char message[1024] = {0};
    char buffer[1024] = {0};
    char username[50] = {0};
    int bytes = recv(sock, buffer, BUFFER_SIZE - 1, 0);
    if (bytes <= 0) {
        printf("Server disconnected during username check.\n");
        return 0;
    }

    buffer[bytes] = '\0';
    printf(MAGENTA "%s\n" RESET,buffer); //WELCOME MESSAGE
    memset(buffer,0,BUFFER_SIZE);

    // -------- User Validation --------
    if(UserInfovalidate(username,buffer,sock)==0) {
        printf( "Authentication failed \n" );
        return 1;
    }

    // -------- Chat mode selection --------
    int mode = 0;
    ChatModeSelect(buffer,&mode);

    sprintf(buffer, "MODE:%d", mode);
    send(sock, buffer, strlen(buffer), 0);

    while(1)
    {
        if (mode == 2) {
            StartPrivateChat(buffer, message, sock);
        }
        else
        {
    // -------- Group chat loop --------
            StartGroupChat(buffer, message, sock, username);
        }
        printf("Choose chat mode:\n[1] Group Chat\n[2] Private Chat\n Or Press any other key to quit \n");
        fgets(buffer, sizeof(buffer), stdin);
        mode = atoi(buffer);
        if (mode == 1 || mode == 2) {
            continue;
        }
        else {
            printf(RED"So Long, %s!\n" RESET,username);
            break;
        }
        
   }

    CLOSESOCKET(sock);

#ifdef _WIN32
    WSACleanup();
#endif

    return 0;
}

int UserInfovalidate(char* username,char * buffer, int sock)
{
     static int counter=3;
    while(counter--) {

    char passwd[20]={0};
    printf("Please enter your username or type \'new\' if registering for the first time: \n");  
    fgets(username, USERNAME_SIZE, stdin);
    username[strcspn(username, "\n")] = '\0';  // Remove newline

    if(strcmp(username,"new")==0) {
       return registerUsername(username, buffer, passwd ,sock);
        
    }
    else {
        printf("Please enter your password in the new line : \n");
        fgets(passwd, PASSWORD_SIZE, stdin);
        passwd[strcspn(passwd, "\n")] = '\0';  // Remove newline
        snprintf(buffer,BUFFER_SIZE,"LOGIN:%s:%s",username,passwd);
        send(sock, buffer, strlen(buffer), 0); //send buffer

        memset(buffer,0,BUFFER_SIZE);

        int bytes = recv(sock, buffer, BUFFER_SIZE - 1, 0);
        if (bytes <= 0) {
            printf("Server disconnected during username check.\n");
            return 0;
        }

        buffer[bytes] = '\0';
        if(strcmp(buffer,"fail")==0) {
            printf("Incorrect Username or password, Please try again\n");
        }
        else
        {
            printf(CYAN "Welcome Back , %s!\n" RESET,username);
            return 1;
        }
    }
    
    }
    printf("Please try after some time \n");
    return 0;

}

int registerUsername(char* username,char * buffer,char* passwd, int sock)
{
    while (1) {
        printf("What shall we call you :- \n");
        fgets(username, USERNAME_SIZE, stdin);
        username[strcspn(username, "\n")] = '\0';  // Remove newline
        printf("And how shall we know its you :-\n");
        fgets(passwd, PASSWORD_SIZE, stdin);
        passwd[strcspn(passwd, "\n")] = '\0';  // Remove newline

        memset(buffer,0,BUFFER_SIZE);
        snprintf(buffer,BUFFER_SIZE,"REGISTER:%s:%s",username,passwd);
        send(sock, buffer, strlen(buffer), 0);

        memset(buffer,0,BUFFER_SIZE);

        int bytes = recv(sock, buffer, BUFFER_SIZE - 1, 0);
        if (bytes <= 0) {
            printf("Server disconnected during username check.\n");
            return 0;
        }

        buffer[bytes] = '\0';

        if (strcmp(buffer, "fail") == 0) {
            printf("Username is taken. Please choose another.\n");
        } else {
            printf("%s",buffer);  
            return 1;       
        }
    }
}

void ChatModeSelect(char *buffer,int *mode) {
    while (1) {
        printf("Choose chat mode:\n[1] Group Chat\n[2] Private Chat\n ");
        fgets(buffer, BUFFER_SIZE, stdin);
        buffer[strcspn(buffer, "\n")] = '\0';  // Remove newline
        *mode = atoi(buffer);
        if (*mode == 1 || *mode == 2) break;
        printf("Invalid choice. Please select 1 or 2.\n");
    }
}

void ChatSession(char *buffer, char *message, int sock ,const char *sessionState) {
    printf(GREEN "%s:You > " RESET,sessionState); // print prompt
    fflush(stdout);
    fd_set read_fds;
    int max_fd = (sock > STDIN_FILENO) ? sock : STDIN_FILENO;

    

    while (1) {
        FD_ZERO(&read_fds);
        FD_SET(STDIN_FILENO, &read_fds);  // monitor stdin
        FD_SET(sock, &read_fds);        // monitor socket


        
        int activity = select(max_fd + 1, &read_fds, NULL, NULL, NULL);
        if (activity < 0) {
            perror("select error");
            break;
        }

        // If user input is ready
        if (FD_ISSET(STDIN_FILENO, &read_fds)) {
            memset(message, 0, MESSAGE_SIZE);
            if (fgets(message, MESSAGE_SIZE, stdin) == NULL) {
                printf("Input error\n");
                break;
            }

            // Optionally remove newline
            message[strcspn(message, "\n")] = 0;

            if (strcmp(message, "/quit") == 0) {
                printf("Disconnecting...\n");
                break;
            }
            memset(buffer,0,BUFFER_SIZE);
            snprintf(buffer,BUFFER_SIZE,"MSG:%s",message);
            send(sock, buffer, strlen(buffer), 0);
            memset(buffer,0,BUFFER_SIZE);
            printf(GREEN "%s:You > " RESET,sessionState); // reprint prompt
            fflush(stdout);
        }

        // If data from server is ready
        if (FD_ISSET(sock, &read_fds)) {
            memset(buffer, 0, BUFFER_SIZE);
            int bytes_received = recv(sock, buffer, BUFFER_SIZE - 1, 0);
            if (bytes_received <= 0) {
                printf("Server disconnected.\n");
                break;
            }
            buffer[bytes_received] = '\0';
            printf(YELLOW "\n%s\n" RESET, buffer);
            fflush(stdout);
            printf(GREEN "%s:You > " RESET,sessionState);// print prompt
            fflush(stdout);
        }
    }
}

void StartPrivateChat(char* buffer, char* message, int sock) {
    printf("Private Chat Options:\n[1] Start a private chat\n[2] Wait for someone to message you\nChoose: ");
    fgets(buffer, BUFFER_SIZE, stdin);
    int choice = atoi(buffer);
    memset(buffer,0,BUFFER_SIZE);

    if (choice == 1) {
        // Initiator mode
        
        send(sock,"INIT_PRIVATE_CHAT",17,0);
        int bytes = recv(sock, buffer, BUFFER_SIZE - 1, 0);
        if (bytes <= 0) {
            printf("Failed to receive user list from server.\n");
            return;
        }
        buffer[bytes] = '\0';
        
        if(strcmp(buffer,"EMPTY")==0) {
            printf("\nNo Users Available for private chat\n");
            return;
        }

        printf("\nAvailable users for private chat:\n%s", buffer);
        printf("\nEnter the username to start private chat: \n");

        char partner[50];
        fgets(partner, sizeof(partner), stdin);
        partner[strcspn(partner, "\n")] = '\0';

        memset(buffer,0,BUFFER_SIZE);
        snprintf(buffer,BUFFER_SIZE,"PRIVATE_REQUEST:%s",partner);
        send(sock, buffer, strlen(buffer), 0);

        memset(buffer,0,BUFFER_SIZE);
        printf( "Waiting for %s to accept your request......\n ",partner);
        // Optional confirmation
        bytes = recv(sock, buffer, BUFFER_SIZE - 1, 0);
        if (bytes <= 0 || strcmp(buffer, "PRIVATE_REJECT") == 0) {
            printf("Error: Unable to start private chat.\n");
            return;
        }
        memset(buffer,0,BUFFER_SIZE);
        printf(BLUE "Private chat started with %s\n " RESET, partner);
        fflush(stdout);
        ChatSession(buffer,message,sock,"PRIVATE");

    } else if (choice == 2) {
        // Wait mode: respond to incoming private messages
        printf("Waiting for private messages... Type '/quit' anytime to exit.\n");

        send(sock,"WAIT_PRIVATE_CHAT",17,0);
        fd_set readfds;
        int max_fd = (sock > STDIN_FILENO) ? sock : STDIN_FILENO;
        //struct timeval timeout;
        while (1) {
            FD_ZERO(&readfds);
            FD_SET(STDIN_FILENO, &readfds);  // monitor stdin
            FD_SET(sock, &readfds);        // monitor socket
            
            int activity = select(max_fd + 1, &readfds, NULL, NULL, NULL);
            if (activity < 0) {
                perror("select() failed");
                break;
            }

            if (FD_ISSET(sock, &readfds)) {
               // 
                int bytes = recv(sock, buffer, BUFFER_SIZE - 1, 0);
                if (bytes <= 0) {
                    printf("Disconnected from server.\n");
                    break;
                }

                buffer[bytes] = '\0';
              ////
                if (strncmp(buffer, "PRIVATE_INVITE:", 15) == 0) {
                    char sender[50];
                    char *ptr = strtok(buffer, ":"); // Skip "PRIVATE_INVITE"
                    ptr = strtok(NULL, "\n");        // Get username
                    if (ptr) {
                        strncpy(sender, ptr, sizeof(sender));
                        sender[sizeof(sender) - 1] = '\0'; // Null-terminate
                    }
                    if (sender) {
                        printf("\nPrivate message request recieved from %s\n Do you wish to accept? (y/n)\n", sender);
                        fgets(message, MESSAGE_SIZE, stdin);
                        message[strcspn(message, "\n")] = '\0';

                        if (strcmp(message, "y") == 0) {
                            send(sock, "PRIVATE_ACCEPT", 14, 0);
                            memset(buffer,0,BUFFER_SIZE);

                            printf( BLUE "Private chat started with %s\n" RESET,sender);
                            fflush(stdout);
                            ChatSession(buffer,message,sock,"PRIVATE");
                        } else {
                            send(sock, "PRIVATE_REJECT", 14, 0);

                            if (bytes <= 0) {
                                printf("Oopssss.\n");
                                break;
                            }
                        }
                    }
                }
            }

            if (FD_ISSET(STDIN_FILENO, &readfds)) {
                memset(buffer, 0, BUFFER_SIZE);
                fgets(buffer, BUFFER_SIZE, stdin);
                message[strcspn(buffer, "\n")] = '\0';
                // Optionally remove newline
                buffer[strcspn(buffer, "\n")] = 0;
    
                if (strcmp(buffer, "/quit") == 0) {
                    printf(CYAN "Disconnecting...\n" CYAN);
                    return;
                }
                else {
                    printf("Unknown Command\n");
                }
            }

        }
    } else {
        printf("Invalid option. Returning to mode selection.\n");
    }
}

void StartGroupChat(char* buffer, char* message, int sock, const char* username) {
    send(sock,"JOIN_GROUP_CHAT",15,0);
    ChatSession(buffer,message,sock,"PUBLIC");
}
