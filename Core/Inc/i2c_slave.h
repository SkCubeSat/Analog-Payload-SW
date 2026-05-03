/*
 * i2c_slave.h
 *
 *  Created on: Nov 28, 2024
 *      Author: jiang35j
 */

#ifndef INC_I2C_SLAVE_H_
#define INC_I2C_SLAVE_H_

#include "main.h"
#include <stdlib.h>

#define false 0
#define true 1

#define LATEST_NAME_MAX   16   // big enough for "S_12345.CSV\0"
#define RTC_PAYLOAD_LEN   8
#define RxSIZE	1
//maximum transmit size. The actual size is variable
#define TxSIZE  300
#define MAX_LINE_LEN 120

#define FIELDS_PER_ROW   10
#define ROWS_TO_SEND      5
#define MAX_VALUES      (FIELDS_PER_ROW * ROWS_TO_SEND)
#define TXBUF_BYTES     (MAX_VALUES * 2)   // 2 bytes per uint16_t

#define I2C_FLAG_RESET      0
#define I2C_FLAG_SET        1
#define I2C_FLAG_READ_DATA  2
#define I2C_FLAG_READ_ERROR 3

#define PWR_SAV         1
#define PWR_NOR         0

#define BUF_NOT_EMPTY   0
#define BUF_EMPTY       1

#define I2C_CMD_RESET   97 // Reset the payload board 
#define I2C_CMD_STOP    98 // Stop all routines on the payload board 
#define I2C_CMD_START   99 // Forced start of single testing routine
#define I2C_CMD_NORMAL 100 // read sensors, write to sd card
#define I2C_CMD_PWRSAV 101 // Power saving mode, no routines till OBC says normal mode. 
#define I2C_CMD_PWRNOR 102 // Normal power mode
#define I2C_CMD_SET_RTC 103

// TODO: add read commands
#define I2C_CMD_SEND_DATA   197
#define I2C_CMD_SEND_ERROR	198

extern volatile uint8_t is_i2c_reinit_needed;

//this function let the main.c to read the i2c flag
uint8_t i2c_flag_getter();

void i2c_set_busy(uint8_t on);
void i2c_set_ready(uint8_t on);
void i2c_set_error(uint8_t on);
uint8_t i2c_get_status(void);   // optional helper

//this function let the main.c to reset the i2c flag
void i2c_flag_reset();

uint8_t pwr_flag_getter();
void pwr_flag_setter(uint8_t flag);

void load_buf();
uint8_t get_latest_s_file(char *outName, size_t outSize);
static void handle_rtc_payload(void);
static uint32_t parse_csv_rows(uint16_t *values);
static uint32_t pack_values(const uint16_t *values, uint32_t count, uint8_t *buffer);
static uint32_t append_file_timestamp(const char *filename, uint8_t *buffer, uint32_t offset);

//Start listening again after the ISR finished
extern void HAL_I2C_ListenCpltCallback (I2C_HandleTypeDef *hi2c);

//address matching and transmit direction
extern void HAL_I2C_AddrCallback(I2C_HandleTypeDef *hi2c, uint8_t TransferDirection, uint16_t AddrMatchCode);

//Receive call back function, which also parses the commands
void HAL_I2C_SlaveRxCpltCallback(I2C_HandleTypeDef *hi2c);

//TODO: finish Transmit function

//error call back function
void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *hi2c);
void increment();


#endif /* INC_I2C_SLAVE_H_ */
