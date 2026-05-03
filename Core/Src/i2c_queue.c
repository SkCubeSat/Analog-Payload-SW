#include "i2c_queue.h"

volatile uint8_t i2c_cmd_queue[I2C_CMD_QUEUE_SIZE];
volatile uint8_t i2c_cmd_head = 0;  // write index (ISR)
volatile uint8_t i2c_cmd_tail = 0;  // read index (main)

bool is_i2c_cmd_pending(void) {
    return i2c_cmd_head != i2c_cmd_tail;
}

void enqueue_i2c_cmd(uint8_t cmd) {
    uint8_t next = (uint8_t)((i2c_cmd_head + 1) % I2C_CMD_QUEUE_SIZE);
    if (next != i2c_cmd_tail) {          // drop if full
        i2c_cmd_queue[i2c_cmd_head] = cmd;
        i2c_cmd_head = next;
    }
}

uint8_t dequeue_i2c_cmd(void) {
    uint8_t cmd = 0;
    if (i2c_cmd_head != i2c_cmd_tail) {
        cmd = i2c_cmd_queue[i2c_cmd_tail];
        i2c_cmd_tail = (uint8_t)((i2c_cmd_tail + 1) % I2C_CMD_QUEUE_SIZE);
    }
    return cmd;
}
