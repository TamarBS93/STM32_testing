#ifndef PROJECTHEADER_H
#define PROJECTHEADER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <errno.h>
#include <stdint.h>


// #define SERVER_IP "127.0.0.1"
#define SERVER_PORT 8080
#define CLIENT_IP "192.168.100.2"
#define CLIENT_PORT 5005

#define MAX_BIT_PATTERN_LENGTH 256

typedef uint8_t Peripheral;

#define TIMER  1
#define UART   2
#define SPI    4
#define I2C    8
#define ADC_P  16

#pragma pack(1)  // Disable padding
typedef struct test_command_t {
    uint32_t test_id;                               // 4 bytes: Test-ID
    Peripheral peripheral;                          // 1 byte: Bitfield for peripherals (Timer=1, UART=2, SPI=4, I2C=8, ADC=16)
    uint8_t iterations;                             // 1 byte: Number of test iterations
    uint8_t bit_pattern_length;                     // 1 byte: Length of bit pattern
    uint8_t bit_pattern[MAX_BIT_PATTERN_LENGTH];    // Variable-size, capped array
} test_command_t;
#pragma pack()  // Restore default packing

typedef enum {
	TEST_ERR = -1,
	TEST_PASS = 1,
	TEST_FAIL = 0xff
} Result;

#pragma pack(1)  // Disable padding
typedef struct result_pro_t {
    uint32_t test_id;                // 4 bytes: Test-ID
    Result test_result;             // bitfield: 1 – test succeeded, 0xff –test failed
} result_pro_t;
#pragma pack()  // Restore default packing


#endif