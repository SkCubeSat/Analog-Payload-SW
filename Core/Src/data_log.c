/*
 * data_log.c
 *
 * Purpose:
 * This module provides a RAM-based fallback logging buffer for payload telemetry
 * when SD card storage is unavailable or when recent routine data must be
 * retained in memory for I2C readback.
 *
 * 
 * - The log is a circular buffer of routines (rows).
 * - Each routine row contains fixed-width uint16_t values.
 * - A routine row stores measurement fields followed by a 6-field timestamp
 *   (year, month, day, hour, minute, second).
 *
 * The file provides functions to:
 * - Append values to the current routine.
 * - Advance to a new routine slot.
 * - Report and clear write position metadata.
 * - Append RTC timestamp fields to a destination array in a consistent format.
 *
 * Notes:
 * - Bounds checks prevent writing past a routine row.
 * - RTC reads follow STM32 requirements: read time before date.
 * - This file intentionally contains no transport logic (I2C framing happens in
 *   i2c_slave.c).
 */

#include "data_log.h"
#include "main.h"

/**
 * @brief RTC handle defined in main.c and shared here for timestamp reads.
 */
extern RTC_HandleTypeDef hrtc;

/**
 * @brief Number of valid uint16_t entries currently written in the active routine.
 *
 * Range: 0 to PER_ROUTINE_DATA_COUNT.
 */
uint16_t data_count = 0;          

/**
 * @brief Index of the active routine slot in the circular routine buffer.
 *
 * Range: 0 to (ROUTINES - 1).
 */
uint16_t routine_num = 0;

/**
 * @brief RAM fallback log storage.
 *
 * Layout:
 * - First index selects the routine slot.
 * - Second index selects a field within that routine.
 *
 * Each row is expected to hold one routine's measurements plus timestamp fields.
 */
uint16_t data_log[ROUTINES][PER_ROUTINE_DATA_COUNT];

/**
 * @brief Append one measurement value to the current routine.
 *
 * If the current routine row is full, the value is rejected and an error message
 * is printed.
 *
 * @param value One uint16_t measurement field to append.
 */
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

/**
 * @brief Advance to the next routine slot and reset the per-routine write index.
 *
 * The routine slot wraps around at ROUTINES, implementing circular buffering.
 */
void data_log_new_routine(void) {
	// Move to next routine slot in a circular manner and reset write index.
	routine_num =(routine_num+1)%ROUTINES;
	data_count = 0;
}

/**
 * @brief Get the number of currently stored fields in the active routine.
 *
 * @retval uint16_t Current per-routine field count.
 */
uint16_t data_log_count(void) {
	return data_count;
}

/**
 * @brief Clear the active routine write count.
 *
 * This does not erase array contents; it only resets metadata so subsequent
 * writes begin at index 0 for the current routine.
 */
void data_log_clear(void)
{
    data_count = 0;
}


/**
 * @brief Timestamp field container used for documentation/reference of layout.
 *
 * This struct mirrors the 6-field timestamp order appended by
 * append_current_datetime_to_array().
 */
typedef struct {
    uint16_t year_offset;  // years since 2000 (if used as RTC offset)
    uint16_t month;        // 1-12
    uint16_t day;          // 1-31
    uint16_t hour;         // 0-23
    uint16_t minute;       // 0-59
    uint16_t second;       // 0-59
} DateTimeStamp;

/**
 * @brief Append current RTC date/time as six uint16_t fields.
 *
 * Stored order is:
 * [year_full, month, day, hour, minute, second]
 *
 * Notes:
 * - STM32 stores RTC year as offset from 2000; this function stores full year
 *   (e.g. 2026) for consistency with other telemetry paths.
 * - The caller owns buffer sizing; this function checks for space before writing.
 *
 * @param array Destination uint16_t array.
 * @param current_size_ptr Pointer to caller-maintained used element count.
 * @param max_size Total capacity (in uint16_t elements) of array.
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
