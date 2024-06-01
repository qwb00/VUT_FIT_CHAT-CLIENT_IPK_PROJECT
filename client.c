#include <signal.h>

#include "client.h"

void internal_error(char *str)
{
    fprintf(stderr, "ERR: %s\n", str);
    exit(EXIT_FAILURE);
}

UserInformation *user;
MessageContent *mc;
int prog_mode = 0;

void parse_input_command_AUTH(void) {
    int valid_command = 0;
    int auth_first = 0;
    while (!valid_command) {
        char* message_content = read_input();

        char* tokens[4];
        int token_count = 0;

        char* token = strtok(message_content, " ");
        while (token != NULL && token_count < 4) {
            tokens[token_count++] = token;
            token = strtok(NULL, " ");
        }

        if (token_count == 4 && strcmp(tokens[0], "/auth") == 0) {
            valid_command = 1; // Command is valid
            auth_first = 1;
            user->username = strdup(tokens[1]);
            user->secret = strdup(tokens[2]);
            user->display_name = strdup(tokens[3]);
        } else if (strcmp(tokens[0], "/rename") == 0 && token_count == 2 && auth_first == 1) {
            user->display_name = strdup(tokens[1]);
        } else if (strcmp(tokens[0], "/help") == 0 && token_count == 1) {
            print_help();
        }
        else {
            printf("Invalid command format. Please try again.\n");
        }
        free(message_content);
    }
}

void clear_message_content(void)
{
	mc->msg_type=UNK;
	mc->display_name[0]=0;
	mc->message_content[0]=0;
	mc->channel_id[0]=0;
}

void print_help(void) {
    printf("Available commands:\n");
    printf("  /auth <username> <secret> <display_name> - Authenticate with the server\n");
    printf("  /rename <new_display_name> - Change the display name\n");
    printf("  /join <channel_id> - Join a channel\n");
    printf("  /help - prints this message\n");
}

// Read input from the user
char *read_input(void) {
    size_t buffer_size = 128; // Initial buffer size
    size_t input_length = 0;  // Current input length
    char *input_buffer = malloc(buffer_size);

    if (!input_buffer) {
        fprintf(stderr, "ERR: Allocation error\n");
        exit(EXIT_FAILURE);
    }

    int c;

    // Read input character by character
    while ((c = getchar()) != '\n' && c != EOF) {
        input_buffer[input_length++] = (char)c;

        // If the buffer is full, reallocate it
        if (input_length == buffer_size) {
            buffer_size *= 2;
            char *new_buffer = realloc(input_buffer, buffer_size);

            if (!new_buffer) {
                free(input_buffer);
                fprintf(stderr, "ERR: Reallocation error\n");
                exit(EXIT_FAILURE);
            }

            input_buffer = new_buffer;
        }
    }

    input_buffer[input_length] = '\0';

    char *final_buffer = realloc(input_buffer, input_length + 1);
    if (!final_buffer) {
        free(input_buffer); // Free the buffer if reallocation failed
        fprintf(stderr, "ERR: Final reallocation error\n");
        exit(EXIT_FAILURE);
    }

    return final_buffer;
}

void run_fsm(int socket, int mode) {
    State state = START;
  
    while (state != END) {
        switch (state) {
            case START:
			    if(mode == UDP) {
                    fsm_auth_state_UDP(socket);
                }
			    else fsm_auth_state_TCP(socket);
                state = AUTH;
                break;
            case AUTH:
                if (mc->msg_type==NREPLY) {
				    if(mode == UDP) fsm_auth_state_UDP(socket);
				    else fsm_auth_state_TCP(socket);
	                state = AUTH;
	                break;
	            }
                if (mc->msg_type==REPLY) {
                	state = OPEN;
                	break;
                }
                if (mc->msg_type==ERR) {
				    if(mode == UDP) fsm_bye_state_UDP(socket);
				    else fsm_bye_state_TCP(socket);
                	state = END;
                	break;
                }
                if (mc->msg_type==UNK) {
                    if(mode == UDP) fsm_error_state_UDP(socket);
                    else fsm_error_state_TCP(socket);
                    state=ERROR;
                    break;
                }
                break;
            case OPEN:
            	//JOIN, MSG already in fsm_open_state_XXX
                if(mode == UDP) fsm_open_state_UDP(socket);
                else fsm_open_state_TCP(socket);
                if (mc->msg_type==MSG || mc->msg_type==REPLY || mc->msg_type==NREPLY) {
                	state=OPEN;
                	break;
                }
                if (mc->msg_type==ERR) {
    	            if(mode == UDP) fsm_bye_state_UDP(socket);
	                else fsm_bye_state_TCP(socket);
                	state=END;
                	break;
                }
                if (mc->msg_type==BYE) {
                	state=END;
                	break;
                }
                if (mc->msg_type==UNK) {
                    if(mode == UDP) fsm_error_state_UDP(socket);
                    else fsm_error_state_TCP(socket);
                    state=ERROR;
                    break;
                }
                //*
   	            if(mode == UDP) fsm_error_state_UDP(socket);
                else fsm_error_state_TCP(socket);
               	state=ERROR;
               	break;
            case ERROR:
			    if(mode == UDP) fsm_bye_state_UDP(socket);
			    else fsm_bye_state_TCP(socket);
               	state = END;
                break;
            case END:
                if(mode == UDP) fsm_end_state_UDP(socket);
                else fsm_end_state_TCP(socket);
                state = END;
                break;
        }
    }
}

// Print usage information
void print_usage(void) {
    printf("Usage: program [options]\n");
    printf("Options:\n");
    printf("  -t <tcp|udp>   Transport protocol (required)\n");
    printf("  -s <address>   Server IP address or hostname (required)\n");
    printf("  -p <port>      Server port (default 4567)\n");
    printf("  -d <ms>        Confirmation timeout for UDP (default 250 ms)\n");
    printf("  -r <count>     Maximum number of UDP retransmissions (default 3)\n");
    printf("  -h             Display this help and exit\n");
}

// Handler function for the interrupt signal (Ctrl+C)
void handle_sigint(int sig) {
    //printf("\nInterrupt signal %d received, exiting...\n", sig);
    if(prog_mode == TCP) {
        fsm_bye_state_TCP(user->client_socket);
    } else {
        fsm_bye_state_UDP(user->client_socket);
    }
    if (user!=NULL)
    {
        if(user->server_addr) {
            free(user->server_addr);
            user->server_addr = NULL;
        }
	    if (user->client_socket!=0)
	    {
	    	close(user->client_socket);
	    	user->client_socket=0;
	    	//printf("client socket closed\n");
	    }
	    free(user);
	    user=NULL;
	}
    if (mc!=NULL)
    {
    	free(mc);
    	mc=NULL;
    }

    exit(sig);
}

int main(int argc, char *argv[]) {
    int opt;
    char *protocol = NULL;
    char *server_hostname = NULL;
    int port_number = 4567; // Default value
    int udp_timeout = 250; // Default value
    int udp_retry = 3; // Default value

    signal(SIGINT, handle_sigint);

    // Allocate memory for the user information
    user = (UserInformation*)malloc(sizeof(UserInformation));
    if (!user) {
        fprintf(stderr, "ERR: Failed to allocate memory for user\n");
        exit(EXIT_FAILURE);
    }
    memset(user, 0, sizeof(UserInformation));

    // Allocate memory for the message content
    mc = (MessageContent*)malloc(sizeof(MessageContent));
    if (!mc) {
        fprintf(stderr, "ERR: Failed to allocate memory for message content\n");
        exit(EXIT_FAILURE);
    }
    memset(mc, 0, sizeof(MessageContent));

    // Parse command line options
    while ((opt = getopt(argc, argv, "t:s:p:d:r:h")) != -1) {
        switch (opt) {
            case 't':
                protocol = optarg;
                break;
            case 's':
                server_hostname = optarg;
                break;
            case 'p':
                port_number = atoi(optarg);
                break;
            case 'd':
                udp_timeout = atoi(optarg);
                break;
            case 'r':
                udp_retry = atoi(optarg);
                break;
            case 'h':
                print_usage();
                return 0;
            default: /* '?' */
                print_usage();
                return -1;
        }
    }

    if (!protocol || !server_hostname) {
        fprintf(stderr, "ERR: Transport protocol and server IP address are required to specify.\n");
        print_usage();
        return -1;
    }

    // Validate transport protocol
    if (strcmp(protocol, "udp") == 0) {
        prog_mode = UDP;
        udp_client(server_hostname, port_number, udp_timeout, udp_retry);
    } else if (strcmp(protocol, "tcp") == 0) {
        prog_mode = TCP;
        tcp_client(server_hostname, port_number);
    }
    free(user);
    return 0;
}
