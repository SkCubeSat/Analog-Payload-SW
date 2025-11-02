#include "i2c_queue.h"

volatile uint8_t i2c_cmd_queue[I2C_CMD_QUEUE_SIZE];
volatile uint8_t i2c_cmd_head = 0;  // write index (ISR)
volatile uint8_t i2c_cmd_tail = 0;  // read index (main)
