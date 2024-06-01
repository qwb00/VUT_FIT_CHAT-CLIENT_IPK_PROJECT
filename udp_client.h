//
// Created by Aleksander on 15.03.2024.
//

#ifndef PROJECT1_C_UDP_CLIENT_H
#define PROJECT1_C_UDP_CLIENT_H

#include "client.h"

void fsm_auth_state_UDP(int socket);
void fsm_open_state_UDP(int socket);
void fsm_error_state_UDP(int socket);
void fsm_bye_state_UDP(int socket);
void fsm_end_state_UDP(int socket);
void udp_client(const char *server_hostname, int port_number, uint16_t timeout, int retry);

#endif //PROJECT1_C_UDP_CLIENT_H
