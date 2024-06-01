//
// Created by Aleksander on 15.03.2024.
//

#include "tcp_client.h"

//constructs AUTH message to be sent to the server in tcp
char *construct_AUTH_TCP_message(void) {
    size_t username_length = strlen(user->username);
    size_t secret_length = strlen(user->secret);
    size_t display_name_length = strlen(user->display_name);

    size_t message_length = 5 + username_length + 4 + display_name_length + 7 + secret_length + 2 + 1;

    char *message = malloc(message_length);
    if(message == NULL) {
        internal_error("Allocation error");
    }

    snprintf(message, message_length, "AUTH %s AS %s USING %s\r\n", user->username, user->display_name, user->secret);

    return message;
}

int parse_tcp_input(char *msg, char *head, char *field1, int size_field1, char *tail, char *field2, int size_field2)
{
	char *p, *p2;
	p=msg;
	if (memcmp(p, head, strlen(head))!=0) return 1; //incorrect head
	p+=strlen(head); //skip head
	if (*p!=' ') return 1; //incorrect delimiter
	p++; //skip delimiter
	p2 = strchr(p, ' ');
	if(p2==NULL) return 1; //bad field1
	*p2=0;
	if ((int)strlen(p)+1>size_field1) return 1; //too big field1
	strcpy(field1, p); 
	*p2=' ';
	p=p2+1; //point to tail msg

	if (memcmp(p, tail, strlen(tail))!=0) return 1; //incorrect tail
	p+=strlen(tail); //skip tail
	if (*p!=' ') return 1; //incorrect delimiter
	p++; //skip delimiter
	p2 = strchr(p, '\r');
	//if(p2==NULL) return 1; //bad field2
	if (p2) *p2=0;
	if ((int)strlen(p)+1>size_field2) return 1; //too big field2
	strcpy(field2, p); if(p2) *p2='\r';
	return 0;
}

void handle_incoming_message(char *msg, int state)
{
	int result;

	clear_message_content();

	if (memcmp(msg, "ERR FROM", 8)==0)
	{
		result = parse_tcp_input(msg, "ERR FROM", mc->display_name, sizeof(mc->display_name), "IS", mc->message_content, sizeof(mc->message_content));
		if (result) return;
		fprintf(stderr, "ERR FROM %s: %s\n", mc->display_name, mc->message_content);
		mc->msg_type=ERR;
		return;
	}

	if (memcmp(msg, "REPLY", 5)==0)
	{
		char reply_ok[3+1];
		result = parse_tcp_input(msg, "REPLY", reply_ok, sizeof(reply_ok), "IS", mc->message_content, sizeof(mc->message_content));
		if (result) return;
		if (strcmp(reply_ok, "OK")==0)
		{
			fprintf(stderr, "Success: %s\n", mc->message_content);
			mc->msg_type=REPLY;
			return;
		}
		if (strcmp(reply_ok, "NOK")==0)
		{
			fprintf(stderr, "Failure: %s\n", mc->message_content);
			mc->msg_type=NREPLY;
			return;
		}
		mc->msg_type = UNK;
		return;
	}

    if (memcmp(msg, "MSG FROM", 8)==0 && state == OPEN)
	{
		result = parse_tcp_input(msg, "MSG FROM", mc->display_name, sizeof(mc->display_name), "IS", mc->message_content, sizeof(mc->message_content));
		if (result) return;
		fprintf(stdout, "%s: %s\n", mc->display_name, mc->message_content);
		mc->msg_type=MSG;
		return;
	}

	if (memcmp(msg, "BYE", 3)==0 && state == OPEN)
	{
		mc->msg_type = BYE;
		return;
	}

	mc->msg_type = UNK;
	return;
}

void fsm_auth_state_TCP(int socket) {
    parse_input_command_AUTH();
    char *message = construct_AUTH_TCP_message();
    long bytes_tx, bytes_rx;
    char buffer[BUFFER_SIZE] = {0};

    bytes_tx = send(socket, message, strlen(message), 0);
    if (bytes_tx < 0) {
        internal_error("send");
    }
    bytes_rx = recv(socket, buffer, BUFFER_SIZE-1, 0);
    if (bytes_rx < 0) {
        internal_error("recv");
    }

    buffer[bytes_rx]=0;
    handle_incoming_message(buffer, AUTH);
}

void send_join_message(int socket, char *channelID) {
    char message[BUFFER_SIZE];
    snprintf(message, sizeof(message), "JOIN %s AS %s\r\n", channelID, user->display_name);

    // Send the constructed message to the server
    ssize_t bytes_sent = send(socket, message, strlen(message), 0);
    if (bytes_sent < 0) {
        internal_error("Failed to send JOIN message");
    }
}


void send_msg_message(int socket, char *userInput) {
    char message[BUFFER_SIZE];
    snprintf(message, sizeof(message), "MSG FROM %s IS %s\r\n", user->display_name, userInput);

    // Send the constructed message to the server
    ssize_t bytes_sent = send(socket, message, strlen(message), 0);
    if (bytes_sent < 0) {
        internal_error("Failed to send MSG message");
    }
}

void change_displayName(char *new_displayName) {
    user->display_name = strdup(new_displayName);
}

void fsm_open_state_TCP(int socket) {
    fd_set readfds;
    int ret;
    char buffer[BUFFER_SIZE];
    ssize_t bytes_rx;

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
                bytes_rx = recv(socket, buffer, BUFFER_SIZE - 1, 0);
                if (bytes_rx > 0) {
                    buffer[bytes_rx] = '\0'; // Null-terminate the string
                    // Handle incoming message
                    handle_incoming_message(buffer, OPEN);
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
                    send_join_message(socket, tokens[1]);
                } else if(strcmp(tokens[0], "/rename") == 0 && token_count == 2) {
                    change_displayName(tokens[1]);
                } else if (strcmp(tokens[0], "/help") == 0 && token_count == 1) {
                    print_help();
                }
                else {
                    send_msg_message(socket, userInputCopy);
                }
                free(userInput);
                free(userInputCopy);
            }
        }
    }
}

void fsm_error_state_TCP(int socket) {
    size_t message_length = 9 + strlen(user->display_name) + 4 + 60;
    char *message = malloc(message_length);
    if(message == NULL) {
        internal_error("Allocation error");
    }
    snprintf(message, message_length, "ERR FROM %s IS Received a message that is not correspond to his state\r\n", user->display_name);
    ssize_t bytes_tx = send(socket, message, strlen(message), 0);
    if (bytes_tx < 0) {
        internal_error("send");
    }
    free(message);
}

void fsm_bye_state_TCP(int socket) {
    char message[] = "BYE\r\n";
    ssize_t bytes_tx = send(socket, message, strlen(message), 0);
    if (bytes_tx < 0) {
        internal_error("send");
    }
}

void fsm_end_state_TCP(int socket) {
    char buffer[BUFFER_SIZE];
    ssize_t bytes_rx = recv(socket, buffer, BUFFER_SIZE, 0);
    if (bytes_rx < 0) {
        internal_error("recv");
    }
}

void tcp_client(const char *server_hostname, int port_number) {
    struct sockaddr_in servaddr;
    struct hostent *server;

    // socket creation
    user->client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (user->client_socket < 0) {
        perror("ERROR opening socket");
        exit(1);
    }

    // Gets information about the server
    server = gethostbyname(server_hostname);
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host\n");
        exit(0);
    }

    // filling the structure with zeros
    bzero((char *) &servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    // copy address of the server to the structure
    bcopy((char *)server->h_addr,
          (char *)&servaddr.sin_addr.s_addr,
          server->h_length);
    servaddr.sin_port = htons(port_number);

    // connecting to server
    if (connect(user->client_socket, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0) {
        perror("ERROR connecting");
        exit(1);
    }

    run_fsm(user->client_socket, TCP);

    // close the socket for further receptions
    shutdown(user->client_socket, SHUT_RD);
    // close the socket for further transmissions
    shutdown(user->client_socket, SHUT_WR);
    // close the socket for further receptions and transmissions
    shutdown(user->client_socket, SHUT_RDWR);

    close(user->client_socket);
    user->client_socket=0;
}
