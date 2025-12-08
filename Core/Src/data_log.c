#include "data_log.h"

//1 routine = 10 samples run 5 times + 3 blocks for for timestamp = 53 uint16_t blocks
//we will keep last 3 routines in memory => 53 x 3 = 159 uint16_t blocks
#define DATA_LOG_CAPACITY  159

uint16_t data_log[DATA_LOG_CAPACITY];
uint16_t data_count = 0;          // how many valid entries (0..DATA_LOG_CAPACITY-1)

void data_log_push(uint16_t value)
{
    if (data_count < DATA_LOG_CAPACITY) {
        data_log[data_count] = value;
        data_count++;
    } else {
        // simple "shift left" when full (no ring buffer, no pointers)
        for (uint16_t i = 1; i < DATA_LOG_CAPACITY; i++) {
            data_log[i - 1] = data_log[i];
        }
        data_log[DATA_LOG_CAPACITY - 1] = value;
    }
}

void data_log_clear(void)
{
    data_count = 0;
}
