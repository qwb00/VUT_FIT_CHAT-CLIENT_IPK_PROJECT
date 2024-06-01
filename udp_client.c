//
// Created by Aleksander on 15.03.2024.
//

#include "udp_client.h"

// Sends and receives a UDP message
int send_UDP_message(int socket, char *message, int message_len) {
    int attempts = 0;
    char buffer[BUFFER_SIZE]; // Allocate a buffer for the received message

    uint16_t sent_message_id = *(uint16_t *) (message + 1); // Message ID

    while (attempts < user->retry + 1) {
        // Send a message to the server
        long bytes_tx = sendto(socket, message, message_len, 0, user->server_addr, sizeof(struct sockaddr_in));
        if (bytes_tx < 0) {
            internal_error("sendto");
        }
        // Receive a message from the server
        socklen_t len = sizeof(struct sockaddr_in);
        long bytes_rx = recvfrom(socket, buffer, BUFFER_SIZE, 0, user->server_addr, &len);
        if (bytes_rx < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                attempts++;
                continue;
            } else {
                internal_error("recvfrom");
            }
        } else {
            // Check if the received message ID matches the sent message ID
            uint16_t received_message_id = *(uint16_t *) (buffer + 1);
            if (bytes_rx==3 && received_message_id == sent_message_id && buffer[0] == CONFIRM) { //mb change to <=
                return 0;
            } else {
                attempts++;
            }
        }
    }
    return 1;
}

// Сonstructs UDP message
char* construct_message(MessageType msg_type, char *str1, char *str2, char *str3, int *len)
{
    // Calculate the length of the message
	int message_len = 3;
	if (str1) message_len+=strlen(str1)+1;
	if (str2) message_len+=strlen(str2)+1;
	if (str3) message_len+=strlen(str3)+1;

    char *message = (char*)malloc(message_len);
    if(message == NULL) {
        fprintf(stderr, "ERR: Allocation error\n");
        exit(EXIT_FAILURE);
    }

    int pos = 0;
    memcpy(message+pos, &msg_type, 1); pos++;
    uint16_t message_id_net = htons(user->message_id);
    memcpy(message+pos, &message_id_net, 2); pos+=2; user->message_id++;
	if (str1) {memcpy(message+pos, str1, strlen(str1)+1); pos+=strlen(str1)+1;}
	if (str2) {memcpy(message+pos, str2, strlen(str2)+1); pos+=strlen(str2)+1;}
	if (str3) {memcpy(message+pos, str3, strlen(str3)+1); pos+=strlen(str3)+1;}
	*len = pos;
	return message;
}

void handle_incoming_UDP_message(char *response)
{
    clear_message_content();
    if(response[0] == (char) ERR) {
        mc->msg_type=ERR;
        strcpy(mc->message_content, response + 3);
        fprintf(stderr, "ERR FROM: %s\n", mc->message_content);
        return;
    }
    if(response[0] == (char)REPLY) {
        if(response[3] == 1) {
            mc->msg_type=REPLY;
	        strcpy(mc->message_content, response + 6);
            fprintf(stderr, "Success: %s\n", mc->message_content);
        }
        else {
            mc->msg_type=NREPLY;
	        strcpy(mc->message_content, response + 6);
            fprintf(stderr, "Failure: %s\n", mc->message_content);
        }
        return;
    }
    if(response[0] == (char)MSG) {
	    mc->msg_type=MSG;
    	strcpy(mc->display_name, response + 3);
    	strcpy(mc->message_content, response + 3 + strlen(mc->display_name) + 1);
		fprintf(stdout, "%s: %s\n", mc->display_name, mc->message_content);
        return;
    }
    if(response[0] == (char)BYE) {
	    mc->msg_type=BYE;
        return;
    }
}

// AUTH state of the FSM
void fsm_auth_state_UDP(int socket) {
    parse_input_command_AUTH();
    int message_len;
    char *message = construct_message(AUTH_t, user->username, user->display_name, user->secret, &message_len);
    clear_message_content();
    if(send_UDP_message(socket, message, message_len)) return;
    while(1) {
		char response[BUFFER_SIZE];
        struct sockaddr_in from_addr;
        socklen_t len = sizeof(from_addr);
        memset(&from_addr, 0, sizeof(from_addr));
	    int bytes_rx = recvfrom(socket, response, BUFFER_SIZE, 0, (struct sockaddr*)&from_addr, &len);
	    if (bytes_rx < 0) {
        	if (errno == EAGAIN || errno == EWOULDBLOCK) {
                printf("            continue\n");
                continue;
            } else {
                internal_error("recvfrom");
            }
	    }
        else ((struct sockaddr_in*)user->server_addr)->sin_port = from_addr.sin_port;
    char confirm_message[] = {CONFIRM, response[1], response[2]};
        int bytes_tx = sendto(socket, confirm_message, 3, 0, user->server_addr, sizeof(struct sockaddr_in));
        if (bytes_tx < 0) {
            internal_error("sendto");
        }
        handle_incoming_UDP_message(response);
        return;
    }
}

void send_join_UDP_message(int socket, char *channelID)
{
    int message_len;
    char *message = construct_message(JOIN, channelID, user->display_name, NULL, &message_len);
    send_UDP_message(socket, message, message_len);
    free(message);
}

void send_msg_UDP_message(int socket, char *MessageContents)
{
    int message_len;
    char *message = construct_message(MSG, user->display_name, MessageContents, NULL, &message_len);
    send_UDP_message(socket, message, message_len);
    free(message);
}

void fsm_open_state_UDP(int socket) {
    fd_set readfds;
    int ret;
    char buffer[BUFFER_SIZE];
    while (1) {
        FD_ZERO(&readfds);
        FD_SET(socket, &readfds);
        FD_SET(STDIN_FILENO, &readfds);

        ret = select(socket + 1, &readfds, NULL, NULL, NULL); // No timeout for TCP

        if (ret == -1) {
            internal_error("in select function");
        } else {
            if (FD_ISSET(socket, &readfds)) {
                // Data received from the server
			    socklen_t len = sizeof(struct sockaddr_in);
			    int bytes_rx = recvfrom(socket, buffer, BUFFER_SIZE, 0, user->server_addr, &len);
			    if (bytes_rx < 0) {
		        	if (errno == EAGAIN || errno == EWOULDBLOCK) {
        		        continue;
		            } else {
        		        internal_error("recvfrom");
		            }
			    }
                if (bytes_rx > 0) {
                    // Handle incoming message
                    char confirm_message[] = {CONFIRM, buffer[1], buffer[2]};
                    int bytes_tx = sendto(socket, confirm_message, 3, 0, user->server_addr, sizeof(struct sockaddr_in));
                    if (bytes_tx < 0) {
                        internal_error("sendto");
                    }
                    handle_incoming_UDP_message(buffer);
                    return;
                } else if (bytes_rx == 0) {
                    // Connection closed by the server
                    internal_error("Server has closed the connection.\n");
                } else {
                    internal_error("receiving data");
                }
            }

            if (FD_ISSET(STDIN_FILENO, &readfds)) {
                // User input
                char *userInput = read_input();
                char* tokens[4];
                int token_count = 0;
                char *userInputCopy = strdup(userInput);
                char* token = strtok(userInput, " ");
                while (token != NULL && token_count < 4) {
                    tokens[token_count++] = token;
                    token = strtok(NULL, " ");
                }
                // Parse user input and execute command
                if (strcmp(tokens[0], "/join") == 0 && token_count == 2) {
                    send_join_UDP_message(socket, tokens[1]);
                } else if(strcmp(tokens[0], "/rename") == 0 && token_count == 2) {
                    change_displayName(tokens[1]);
                } else if (strcmp(tokens[0], "/help") == 0 && token_count == 1) {
                    print_help();
                }
                else {
                    send_msg_UDP_message(socket, userInputCopy);
                }
                free(userInput);
                free(userInputCopy);
            }
        }
    }
}

void fsm_error_state_UDP(int socket) {
    int message_len;
    char *message = construct_message(ERR, user->display_name, "Received a message that is not correspond to his state or problems with receiving and sending the message.", NULL, &message_len);
    send_UDP_message(socket, message, message_len);
    free(message);
}

void fsm_bye_state_UDP(int socket) {
    int message_len;
    char *message = construct_message(BYE, NULL, NULL, NULL, &message_len);
    send_UDP_message(socket, message, message_len);
    free(message);
}

void fsm_end_state_UDP(int socket) {
    char buffer[BUFFER_SIZE];
    socklen_t len = sizeof(struct sockaddr_in);
    long bytes_rx = recvfrom(socket, buffer, BUFFER_SIZE, 0, user->server_addr, &len);
    if (bytes_rx < 0) {
        internal_error("recv");
    }
}

void udp_client(const char *server_hostname, int port_number, uint16_t timeout, int retry) {
    struct sockaddr_in *server_addr = malloc(sizeof(struct sockaddr_in));
    if (server_addr == NULL) {
        internal_error("malloc failed\n");
    }
    struct timeval tv;

    // Create a UDP socket
    user->client_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (user->client_socket <= 0) {
        internal_error("socket");
    }

    tv.tv_sec = timeout / 1000;
    tv.tv_usec = (timeout % 1000) * 1000;
    if (setsockopt(user->client_socket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        internal_error("setsockopt");
    }
    user->tv=tv;

    struct hostent *server = gethostbyname(server_hostname);
    if (server == NULL) {
        internal_error("no such host\n");
    }
    memset(server_addr, 0, sizeof(struct sockaddr_in));
    server_addr->sin_family = AF_INET;
    server_addr->sin_port = htons(port_number);
    memcpy(&server_addr->sin_addr.s_addr, server->h_addr, server->h_length);
    user->server_addr = (struct sockaddr *)server_addr;
    user->retry=retry;

    run_fsm(user->client_socket, UDP);

    close(user->client_socket);
    user->client_socket=0;
}
