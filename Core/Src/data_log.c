#include "data_log.h"


uint16_t data_count = 0;          // how many valid entries (0..DATA_LOG_CAPACITY-1)
uint16_t routine_num = 0;
uint16_t data_log[ROUTINES][PER_ROUTINE_DATA_COUNT];

void data_log_push(uint16_t value)
{
	if (data_count >= PER_ROUTINE_DATA_COUNT) {
	    printf("Out of bound error for routine data\n");
	    return;
	}

	data_log[routine_num][data_count] = value;
	data_count++;

}

void data_log_new_routine(void) {
	routine_num =(routine_num+1)%ROUTINES;
	data_count = 0;
}

uint16_t data_log_count(void) {
	return data_count;
}

void data_log_clear(void)
{
    data_count = 0;
}


typedef struct {
    uint16_t year_offset;  // years since 1932
    uint16_t month;        // 1–12
    uint16_t day;          // 1–31
    uint16_t hour;         // 0–23
    uint16_t minute;       // 0–59
    uint16_t second;       // 0–59
} DateTimeStamp;
/**
 * Appends the current date and time components to a uint16_t array.
 * Components stored: Year (offset from 1932), Month, Day, Hour, Minute, Second.
 *
 * @param array The destination array pointer.
 * @param current_size_ptr A pointer to the current count of elements in the array.
 * @param max_size The total capacity of the array.
 */
void append_current_datetime_to_array(uint16_t array[], uint16_t *current_size_ptr, uint16_t max_size) {
    // Get current time using standard C library functions
    time_t now = time(NULL);
    struct tm *ptm = localtime(&now);

    // Calculate the year offset using the 1932 epoch
    uint16_t year_offset = (uint16_t)(ptm->tm_year + 1900 - 1932);

    // Extract remaining components and cast to uint16_t
    uint16_t month  = (uint16_t)(ptm->tm_mon + 1); // tm_mon is 0-11
    uint16_t day    = (uint16_t)ptm->tm_mday;
    uint16_t hour   = (uint16_t)ptm->tm_hour;
    uint16_t minute = (uint16_t)ptm->tm_min;
    uint16_t second = (uint16_t)ptm->tm_sec;

    // Check if we have enough space for 6 new entries
    if (*current_size_ptr + 6 <= max_size) {
        array[(*current_size_ptr)++] = year_offset;
        array[(*current_size_ptr)++] = month;
        array[(*current_size_ptr)++] = day;
        array[(*current_size_ptr)++] = hour;
        array[(*current_size_ptr)++] = minute;
        array[(*current_size_ptr)++] = second;

        printf("Appended Date/Time: Y:%u(offset) M:%u D:%u H:%u M:%u S:%u\n",
               year_offset, month, day, hour, minute, second);
    } else {
        printf("Error: Not enough space in the array to append full timestamp.\n");
    }
}
