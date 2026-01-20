#ifndef SERVERHEADER_H
#define SERVERHEADER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <signal.h>
#include "Project_header.h"

/* Constant Definitions */
#define COUNT_FILE "calls_count.txt"
#define LOG_FILE "testing.log"

/* Function Prototypes */
void handle_sigint(int sig);
int setup_socket();
result_pro_t perform_test(test_command_t *test_cmd, struct sockaddr_in *dest_addr, double *duration, struct timeval *sent_time);
test_command_t test_request_init(int argc, char *argv[]);
int get_id_num();
int file_exists(const char *filename);
void logging(result_pro_t result, struct timeval sent, double duration);
void print_history();

#endif //SERVERHEADER_H

 