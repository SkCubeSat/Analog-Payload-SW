
#include <stdint.h>
#include <stdio.h>
#include <time.h>

//1 routine = 10 samples run 5 times + 6 blocks  for timestamp = 56 uint16_t blocks
//we will keep last 3 routines in memory => 56 x 3 = 168 uint16_t blocks
#define DATA_LOG_CAPACITY  168
#define ROUTINES  3
#define PER_ROUTINE_DATA_COUNT  56

extern uint16_t data_log[ROUTINES][PER_ROUTINE_DATA_COUNT];
extern uint16_t data_count;
extern uint16_t routine_num;

void data_log_push(uint16_t value);
void data_log_clear(void);
uint16_t data_log_count(void);
void data_log_new_routine(void);
