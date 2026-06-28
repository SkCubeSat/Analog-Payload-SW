/*
 * i2c_queue.c
 *
 * Purpose:
 * This module implements a small fixed-size ring buffer used to pass decoded
 * I2C commands from interrupt context to the main loop.
 *
 * Broader system role:
 * - Producer: I2C ISR path (via HAL callbacks in i2c_slave.c) enqueues commands.
 * - Consumer: Main loop (main.c) dequeues and executes commands when safe.
 * - Benefit: keeps ISR short and defers command execution to non-interrupt code.
 *
 * Design notes:
 * - Queue storage is uint8_t command IDs.
 * - Head advances on enqueue; tail advances on dequeue.
 * - Full queue policy is drop-new-command (no overwrite, no blocking).
 * - Empty queue returns command value 0 from dequeue_i2c_cmd().
 * - Variables are volatile because they are accessed across ISR and main context.
 */

#include "i2c_queue.h"

/**
 * @brief Ring buffer storage for pending I2C command bytes.
 */
volatile uint8_t i2c_cmd_queue[I2C_CMD_QUEUE_SIZE];

/**
 * @brief Write index for the ring buffer.
 *
 * Owned by producer path (typically ISR callback flow).
 */
volatile uint8_t i2c_cmd_head = 0;

/**
 * @brief Read index for the ring buffer.
 *
 * Owned by consumer path (main loop).
 */
volatile uint8_t i2c_cmd_tail = 0;

/**
 * @brief Check whether at least one command is queued.
 *
 * @retval true  Queue contains one or more pending commands.
 * @retval false Queue is empty.
 */
bool is_i2c_cmd_pending(void) {
    return i2c_cmd_head != i2c_cmd_tail;
}

/**
 * @brief Enqueue one I2C command byte.
 *
 * If the queue is full, the command is dropped (non-blocking behavior).
 * This is intentional to keep ISR-side handling short and deterministic.
 *
 * @param cmd Command byte to queue.
 */
void enqueue_i2c_cmd(uint8_t cmd) {
    uint8_t next = (uint8_t)((i2c_cmd_head + 1) % I2C_CMD_QUEUE_SIZE);
    if (next != i2c_cmd_tail) {
        i2c_cmd_queue[i2c_cmd_head] = cmd;
        i2c_cmd_head = next;
    }
}

/**
 * @brief Dequeue one I2C command byte.
 *
 * @retval uint8_t Next command byte if available.
 * @retval 0       If queue is empty.
 */
uint8_t dequeue_i2c_cmd(void) {
    uint8_t cmd = 0;
    if (i2c_cmd_head != i2c_cmd_tail) {
        cmd = i2c_cmd_queue[i2c_cmd_tail];
        i2c_cmd_tail = (uint8_t)((i2c_cmd_tail + 1) % I2C_CMD_QUEUE_SIZE);
    }
    return cmd;
}
