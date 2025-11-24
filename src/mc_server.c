#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>

#define PORT 11235
#define MAX_CLIENTS 100
#define BUFFER_SIZE 1024
#define MAX_USERNAME_LEN 20
#define MAX_PASSWORD_LEN 20


typedef enum {
    STATE_DEFAULT,
    STATE_IN_LOBBY,
    STATE_IN_GROUP_CHAT,
    STATE_IN_PRIVATE_CHAT
} ClientState;

typedef struct {
    int fd;
    char username[MAX_USERNAME_LEN];
    ClientState state;
    int peer_fd;
    int authenticated;
} ClientSession;

ClientSession clients[MAX_CLIENTS] = {0};  // Initialize all fields to 0
fd_set master_fds, read_fds;
int max_fd;

// Store users and passwords (for simplicity, in memory)
typedef struct {
    char username[MAX_USERNAME_LEN];
    char password[MAX_PASSWORD_LEN];
} UserCredential;

UserCredential user_db[MAX_CLIENTS];
int user_count = 0;

void add_user(const char *username, const char *password) {
    strncpy(user_db[user_count].username, username, MAX_USERNAME_LEN);
    strncpy(user_db[user_count].password, password, MAX_PASSWORD_LEN);
    user_count++;
}

int authenticate_user(const char *username, const char *password) {
    for (int i = 0; i < user_count; i++) {
        if (strcmp(user_db[i].username, username) == 0 &&
            strcmp(user_db[i].password, password) == 0) {
            return 1;
        }
    }
    return 0;
}

ClientSession* get_client_by_fd(int fd) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].fd == fd)
            return &clients[i];
    }
    return NULL;
}

void broadcast_group_chat(int sender_fd, const char *message) {
    ClientSession *sender = get_client_by_fd(sender_fd);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].fd > 0 && clients[i].state == STATE_IN_GROUP_CHAT && clients[i].fd != sender_fd) {
            char buffer[BUFFER_SIZE];
            snprintf(buffer, sizeof(buffer), "PUBLIC:%s:%s\n", sender->username, message);
            send(clients[i].fd, buffer, strlen(buffer), 0);
        }
    }
}

void send_lobby_clients(int fd) {
    char buffer[BUFFER_SIZE] = "";
    int found = 0;
    for (int i = 0; i < MAX_CLIENTS; i++) {
       // printf("%s, %d, %d, %d\n",clients[i].username,clients[i].fd,fd,clients[i].state);
        if ((clients[i].fd > 0) && (clients[i].state == STATE_IN_LOBBY) && (clients[i].fd != fd)) {
          //  printf("\n MATCH FAOUND \n");
            strcat(buffer, clients[i].username);
            strcat(buffer, ",");
            found = 1;
        }
    }

    if (!found) {
        send(fd, "EMPTY", 5, 0);
    } else {
        buffer[strlen(buffer) - 1] = '\n'; // Remove trailing comma
        send(fd, buffer, strlen(buffer), 0);
    }
}


void handle_new_connection(int server_fd) {
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    int new_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);

    if (new_fd < 0) {
        perror("accept");
        return;
    }

    // Add new client to session list
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].fd == 0) {
            clients[i].fd = new_fd;
            clients[i].state = STATE_DEFAULT;
            clients[i].authenticated = 0;
            clients[i].peer_fd = -1;
            printf("\nNEW CLIENT FD :- %d\n",new_fd);
            FD_SET(new_fd, &master_fds);
            if (new_fd > max_fd) max_fd = new_fd;
            send(new_fd, "Welcome to Pandaemonium. Login or Register.\n", 44, 0);
            return;
        }
    }

    // Server full
    send(new_fd, "Server full.\n", 13, 0);
    close(new_fd);
}

void handle_client_message(int fd) {
    char buffer[BUFFER_SIZE] = {0};
    int bytes_read = read(fd, buffer, sizeof(buffer) - 1);

    if (bytes_read <= 0) {
        printf("Client disconnected.\n");
        close(fd);
        FD_CLR(fd, &master_fds);
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].fd == fd) {
                clients[i] = (ClientSession){0};  // Reset client
                break;
            }
        }
        return;
    }

    buffer[bytes_read] = '\0';
    printf(" CLIENT MESSAGE :- %s\n",buffer);
    ClientSession *client = get_client_by_fd(fd);

    if (!client->authenticated) {
        if (strncmp(buffer, "LOGIN:", 6) == 0) {
            char *username = strtok(buffer + 6, ":");
            char *password = strtok(NULL, "\n");
            if (authenticate_user(username, password)) {
                strncpy(client->username, username, MAX_USERNAME_LEN);
                client->authenticated = 1;
                send(fd, "success", 7, 0);
            } else {
                send(fd, "fail", 4, 0);
            }
        } else if (strncmp(buffer, "REGISTER:", 9) == 0) {
            char *username = strtok(buffer + 9, ":");
            char *password = strtok(NULL, "\n");

            int isPresent =0;
            for (int i = 0; i < user_count; i++) {
                if (strcmp(user_db[i].username, username) == 0) {
                    isPresent=1;
                    break;
                    
                }
            }            
            if(isPresent==1) {
                send(fd, "fail", 4, 0);
            }
            else {
                add_user(username, password);
                strncpy(client->username, username, MAX_USERNAME_LEN);
                client->authenticated = 1;
                send(fd, "Registration successful.\n", 25, 0);
            }
            
        } else {
            send(fd, "fail",4, 0);
        }
        return;
    }

    // If authenticated, check command
    if (strcmp(buffer, "JOIN_GROUP_CHAT") == 0) {
        client->state = STATE_IN_GROUP_CHAT;
        send(fd, "Joined group chat.\n", 19, 0);
        char announce[100] ={0};
        snprintf(announce,sizeof(announce),"%s joined the group chat",client->username);
        broadcast_group_chat(fd, announce);
    } else if (strncmp(buffer, "MSG:", 4) == 0) {
        if (client->state == STATE_IN_GROUP_CHAT) {
            broadcast_group_chat(fd, buffer + 4);
        } else if (client->state == STATE_IN_PRIVATE_CHAT) {
            ClientSession *peer = get_client_by_fd(client->peer_fd);
            if (peer && peer->state == STATE_IN_PRIVATE_CHAT) {
                char relay[BUFFER_SIZE];
                snprintf(relay, sizeof(relay), "PRIVATE:%s > %s", client->username, buffer + 4);
                send(peer->fd, relay, strlen(relay), 0);
            }
        }
    }
    else if(strcmp(buffer, "WAIT_PRIVATE_CHAT") == 0) {
        client->state = STATE_IN_LOBBY;
    }
    else if(strcmp(buffer, "INIT_PRIVATE_CHAT") == 0) {
        client->state = STATE_IN_LOBBY;
        send_lobby_clients(fd);
    
    }
    else if (strcmp(buffer, "PRIVATE_ACCEPT") == 0) {
        if (client->peer_fd > 0) {
            ClientSession *initiator = get_client_by_fd(client->peer_fd);
            if (initiator) {
                client->state = STATE_IN_PRIVATE_CHAT;
                initiator->state = STATE_IN_PRIVATE_CHAT;
    
                //send(client->fd, "PRIVATE_ACCEPT", 14, 0);
                send(initiator->fd, "PRIVATE_ACCEPT", 14, 0);
            }
        }
    } else if (strcmp(buffer, "PRIVATE_REJECT") == 0) {
        if (client->peer_fd > 0) {
            ClientSession *initiator = get_client_by_fd(client->peer_fd);
            if (initiator) {
                send(initiator->fd, "PRIVATE_REJECT", 14, 0);
                initiator->peer_fd = -1;
                client->peer_fd = -1;
            }
        }
        client->peer_fd = -1;
        client->state = STATE_IN_LOBBY;
    } else if (strncmp(buffer, "PRIVATE_REQUEST:", 16) == 0) {
        char *target_username = strtok(buffer + 16, "\n");
        ClientSession *target = NULL;
        printf(" PRIVATE_REQUEST FOR %s\n",target_username);
        for (int i = 0; i < MAX_CLIENTS; i++) {
            //printf("%d, %s, %d\n",clients[i].fd,clients[i].username,clients[i].state);
            if ((clients[i].fd > 0) && (strcmp(clients[i].username, target_username) == 0) && (clients[i].state == STATE_IN_LOBBY)) {
                target = &clients[i];
                break;
            }
        }
    
        if (target) {
            target->peer_fd = client->fd;  // who initiated
            client->peer_fd = target->fd;
            char notify[BUFFER_SIZE] = {0};
            snprintf(notify, sizeof(notify), "PRIVATE_INVITE:%s\n", client->username);
            send(target->fd, notify, strlen(notify), 0);
        } else {
            send(fd, "EMPTY\n", 40, 0);
        }
    }
}

int main() {
    int server_fd;
    struct sockaddr_in server_addr;

    FD_ZERO(&master_fds);
    FD_ZERO(&read_fds);

    // Create socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, MAX_CLIENTS) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    FD_SET(server_fd, &master_fds);
    max_fd = server_fd;

    printf("Server listening on port %d...\n", PORT);

    while (1) {
        read_fds = master_fds;

        if (select(max_fd + 1, &read_fds, NULL, NULL, NULL) < 0) {
            perror("select");
            exit(EXIT_FAILURE);
        }

        for (int fd = 0; fd <= max_fd; fd++) {
            if (FD_ISSET(fd, &read_fds)) {
                if (fd == server_fd) {
                    printf("\n NEW CONNECTION \n");
                    handle_new_connection(server_fd);
                } else {
                    printf("\n NEW MESSAGE \n");
                    handle_client_message(fd);
                }
            }
        }
    }

    return 0;
}
