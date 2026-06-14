#include "data_log.h"
#include "main.h"

extern RTC_HandleTypeDef hrtc;


// Current write index inside the active routine slot.
// how many valid entries (0..DATA_LOG_CAPACITY-1)
uint16_t data_count = 0;          
// Index of the active routine ring-buffer slot [0..ROUTINES-1].
uint16_t routine_num = 0;
// RAM fallback log buffer: each row stores one routine (sensor data + timestamp).
uint16_t data_log[ROUTINES][PER_ROUTINE_DATA_COUNT];

void data_log_push(uint16_t value)
{
    // Protect the current routine buffer from overflow.
	if (data_count >= PER_ROUTINE_DATA_COUNT) {
	    printf("Out of bound error for routine data\n");
	    return;
	}

    // Append one uint16 data item to the active routine slot.
	data_log[routine_num][data_count] = value;
	data_count++;

}

void data_log_new_routine(void) {
    // Move to next routine slot in a circular manner and reset write index.
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
 * Appends current RTC date/time as 6 uint16 fields to the provided array.
 * Stored order: full year, month, day, hour, minute, second.
 *
 * Notes:
 * - STM32 stores RTC year as offset from 2000; this function stores full year.
 * - The caller owns buffer sizing; this function checks for space before writing.
 *
 * @param array The destination array pointer.
 * @param current_size_ptr Pointer to current used element count in the array.
 * @param max_size The total capacity of the array.
 */
void append_current_datetime_to_array(uint16_t array[], uint16_t *current_size_ptr, uint16_t max_size) {
    RTC_TimeTypeDef sTime = {0};
    RTC_DateTypeDef sDate = {0};

    // Read time first, then date, as required by STM32 RTC shadow register behavior.
    if (HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN) != HAL_OK ||
        HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN) != HAL_OK) {
        printf("Error: RTC read failed, timestamp not appended.\n");
        return;
    }

    // STM32 RTC year is offset from 2000. Store the FULL year so every
    // timestamp path on the wire uses one format (matches load_latest_ts_buf).
    uint16_t year_full = (uint16_t)(2000u + sDate.Year);

    uint16_t month  = (uint16_t)sDate.Month;
    uint16_t day    = (uint16_t)sDate.Date;
    uint16_t hour   = (uint16_t)sTime.Hours;
    uint16_t minute = (uint16_t)sTime.Minutes;
    uint16_t second = (uint16_t)sTime.Seconds;

    // We append exactly 6 fields (Y-M-D-H-M-S).
    if (*current_size_ptr + 6 <= max_size) {
        array[(*current_size_ptr)++] = year_full;
        array[(*current_size_ptr)++] = month;
        array[(*current_size_ptr)++] = day;
        array[(*current_size_ptr)++] = hour;
        array[(*current_size_ptr)++] = minute;
        array[(*current_size_ptr)++] = second;

         printf("Appended Date/Time: %u-%02u-%02u %02u:%02u:%02u\n",
             year_full, month, day, hour, minute, second);
    } else {
        printf("Error: Not enough space in the array to append full timestamp.\n");
    }
}
