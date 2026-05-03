#pragma once
#include <stdint.h>
#include <stdbool.h>

#define I2C_CMD_QUEUE_SIZE 16

extern volatile uint8_t i2c_cmd_queue[I2C_CMD_QUEUE_SIZE];
extern volatile uint8_t i2c_cmd_head;
extern volatile uint8_t i2c_cmd_tail;

bool is_i2c_cmd_pending(void);
void enqueue_i2c_cmd(uint8_t cmd);
uint8_t dequeue_i2c_cmd(void);


