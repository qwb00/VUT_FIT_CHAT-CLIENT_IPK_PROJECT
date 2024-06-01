//
// Created by Aleksander on 15.03.2024.
//

#ifndef PROJECT1_C_CLIENT_H
#define PROJECT1_C_CLIENT_H

#include <sys/errno.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdbool.h>

#include "udp_client.h"
#include "tcp_client.h"

#define BUFFER_SIZE 1024 // Buffer size for received messages
#define EXIT_FAILURE 1 // Exit failure code
#define UDP 1 // UDP protocol
#define TCP 2 // TCP protocol

typedef enum {
    START,
    AUTH,
    OPEN,
    ERROR,
    END
}State;

typedef enum uint8_t {
    UNK = -1,
    CONFIRM = 0x00,
    REPLY = 0x01,
    NREPLY = 0xF1,
    AUTH_t = 0x02,
    JOIN = 0x03,
    MSG = 0x04,
    ERR = 0xFE,
    BYE = 0xFF
}MessageType;

typedef struct {
    char *username;
    char *secret;
    char *display_name;
    int client_socket;
    uint16_t message_id;
    struct sockaddr *server_addr;
    int retry;
    struct timeval tv;
}UserInformation;

extern UserInformation *user;


#define MAX_DISPLAY 20
#define MAX_MESSAGE 1400

typedef struct {
	MessageType msg_type;
	char display_name[MAX_DISPLAY+1];
	char message_content[MAX_MESSAGE+1];
	char channel_id[MAX_DISPLAY+1];
} MessageContent;

extern MessageContent *mc;
void clear_message_content(void);
void change_displayName(char *new_displayName);
void internal_error(char *str);
void parse_input_command_AUTH(void);
void print_help(void);
char *read_input(void);
void run_fsm(int socket, int mode);
void print_usage(void);
void handle_sigint(int sig);
void internal_error(char *str);

#endif //PROJECT1_C_CLIENT_H
