/**
 * @file main.c
 * @brief PC-side Testing Program (Server) for STM32F756ZG hardware verification.
 * * This program sends test commands over UDP using a proprietary protocol and logs results.
 * @author Tamar Ben Shushan
 * @date 2026
 */

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

/* Global variables */
int sockfd;

/**
 * @brief Main entry point for the CLI testing application.
 */
int main(int argc, char *argv[]) {

    // Check for "Print on Demand" request first
    if (argc == 2 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--history") == 0)) {
        print_history();
        return 0;
    }

    // Validate CLI arguments
    if (argc < 3 || argc > 4) {
        printf("Usage: %s <PERIPHERAL> <ITERATIONS> [PATTERN]\n", argv[0]);
        printf("  Log:  %s --history (or -h)\n", argv[0]);
        return 1;
    }

    signal(SIGINT, handle_sigint);

    // Initialize test command based on CLI args
    test_command_t test_cmd = test_request_init(argc, argv);
    if (test_cmd.peripheral == 0){
        printf("Usage: %s <PERIPHERAL> <ITERATIONS> [PATTERN]\n", argv[0]);
        printf("  Log:  %s --history (or -h)\n", argv[0]);
    } return 1;

    // Configure SERVER Address
    sockfd = setup_socket();

    // Configure UUT Address
    struct sockaddr_in uut_addr;
    memset(&uut_addr, 0, sizeof(uut_addr));
    uut_addr.sin_family = AF_INET;
    uut_addr.sin_port = htons(5005); 
    uut_addr.sin_addr.s_addr = inet_addr(CLIENT_IP);

    double test_duration = 0;
    struct timeval sent_time;

    // Execute communication and measure time
    result_pro_t result = perform_test(&test_cmd, &uut_addr, &test_duration, &sent_time);

    // Record results to persistent storage
    logging(result, sent_time, test_duration);

    close(sockfd);
    return 0;
}

/**
 * @brief Signal handler for graceful termination via SIGINT.
 * @param sig Signal number received.
 */
void handle_sigint(int sig) {
    close(sockfd);
    printf("\nShutdown signal (%d) received. Exiting...\n", sig);
    _exit(0); 
}

/**
 * @brief Creates and binds a UDP socket for communication.
 * @return File descriptor of the socket.
 */
int setup_socket() {
    int sockfd;
    struct sockaddr_in server_addr;

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    // server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);
    server_addr.sin_port = htons(SERVER_PORT);

    int opt = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    if (bind(sockfd, (const struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("Socket setup complete. Listening on %s:%d\n", inet_ntoa(server_addr.sin_addr), SERVER_PORT);
    return sockfd;
}

/**
 * @brief Sends the test command and waits for the UUT result.
 * @param sockfd Active socket descriptor.
 * @param test_cmd Pointer to the command structure to send.
 * @param dest_addr Pointer to UUT address structure.
 * @param duration Output pointer for measured test length.
 * @param sent_time Output pointer for the timestamp of sending.
 * @return result_pro_t The result received from UUT.
 */
result_pro_t perform_test(test_command_t *test_cmd, struct sockaddr_in *dest_addr, double *duration, struct timeval *sent_time) {
    result_pro_t result_pack = {0};
    struct timeval recv_time;
    socklen_t addr_len = sizeof(*dest_addr);

    gettimeofday(sent_time, NULL);
    
    // Send Command
    sendto(sockfd, test_cmd, sizeof(test_command_t), 0, (struct sockaddr*)dest_addr, addr_len);

    // Receive Result
    int n = recvfrom(sockfd, &result_pack, sizeof(result_pro_t), 0, (struct sockaddr *)dest_addr, &addr_len);
    
    if (n < 0) {
        perror("Receive failed");
        result_pack.test_result = 0xFF; // Mark as failed
    } else {
        gettimeofday(&recv_time, NULL);
        *duration = (recv_time.tv_sec - sent_time->tv_sec) + 
                    (recv_time.tv_usec - sent_time->tv_usec) / 1000000.0;
    }

    return result_pack;
}

/**
 * @brief Maps CLI arguments to the proprietary bitfield protocol.
 * @param argc Argument count.
 * @param argv Argument vector.
 */
test_command_t test_request_init(int argc, char *argv[]) {
    test_command_t req;
    memset(&req, 0, sizeof(req));

    // Peripheral bitfield mapping 
    if (strcmp(argv[1], "TIMER") == 0) req.peripheral = 1;
    else if (strcmp(argv[1], "UART") == 0) req.peripheral = 2;
    else if (strcmp(argv[1], "SPI") == 0)   req.peripheral = 4;
    else if (strcmp(argv[1], "I2C") == 0)   req.peripheral = 8;
    else if (strcmp(argv[1], "ADC") == 0)   req.peripheral = 16;
    else {
        printf("Error: Invalid Peripheral.\n");
        return req;
    }

    req.iterations = (uint8_t)atoi(argv[2]); 
    req.test_id = get_id_num(); 
    // Bit pattern handling 
    char *pattern = (argc == 4) ? argv[3] : "This is the testing pattern!";
    req.bit_pattern_length = (uint8_t)strlen(pattern);
    memcpy((char*)req.bit_pattern, pattern, req.bit_pattern_length);
    req.bit_pattern[req.bit_pattern_length] = '\0'; // Ensure null termination


    return req;
}

/**
 * @brief Increments and retrieves the persistent Test-ID.
 */
int get_id_num() {
    int id = 0;
    FILE *fp = fopen(COUNT_FILE, "r+");
    if (!fp) fp = fopen(COUNT_FILE, "w+");
    
    if (fp) {
        fscanf(fp, "%d", &id);
        id++;
        rewind(fp);
        fprintf(fp, "%d", id);
        fclose(fp);
    }
    return id;
}

/**
 * @brief Checks if a file exists on the system.
 */
int file_exists(const char *filename) {
    struct stat buffer;
    return (stat(filename, &buffer) == 0);
}

/**
 * @brief Logs test details to a file for persistent records.
 */
void logging(result_pro_t result, struct timeval sent, double duration) {
    FILE *fp = fopen(LOG_FILE, "a");
    if (!fp) return;

    // Write header if new file
    fseek(fp, 0, SEEK_END);
    if (ftell(fp) == 0) {
        fprintf(fp, "%-9s %-25s %-15s %s\n", "Test ID", "Sent At", "Result", "Duration (s)");
    }

    char time_str[64];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", localtime(&sent.tv_sec));

    const char *status = (result.test_result == 1) ? "SUCCESS" : "FAILURE";
    
    fprintf(fp, "%-9d %-25s %-15s %f\n", result.test_id, time_str, status, duration);
    printf("Test %d Result: %s (%.4fs)\n", result.test_id, status, duration);
    
    fclose(fp);
}

/**
 * @brief Reads the persistent log file and prints all test records to the console.
 */
void print_history() {
    FILE *fp = fopen(LOG_FILE, "r");
    if (!fp) {
        printf("No testing records found.\n");
        return;
    }

    char line[256];
    printf("\n--- UUT Hardware Verification History ---\n");
    
    // Read and print each line from the log file
    while (fgets(line, sizeof(line), fp)) {
        printf("%s", line);
    }
    printf("----------------------------------\n\n");

    fclose(fp);
}