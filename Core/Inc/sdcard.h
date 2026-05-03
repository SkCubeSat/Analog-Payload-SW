/*
 * sdcard.h
 *
 *  Created on: Feb 8, 2024
 *      Author: karim
 */

// Need like this as initalizing in freertos leadings to hardfault
// increasing stack did not work, may need to tune so that it uses heap


#ifndef INC_SDCARD_H_
#define INC_SDCARD_H_

#include <string.h>
#include "fatfs.h"
#include <stdio.h>
#include "main.h"

//https://community.st.com/t5/stm32-mcus-products/stm32f103-sdio-fatfs-fault-issue/m-p/411951/highlight/true#M118933
uint8_t mount_sdcard(void);
void print_sdcard_stats(void);
void open_sdcard_file_read(char* filename);
void read_sdcard_file(void);
void close_sdcard_file(void);
void open_sdcard_file_write(char* filename);
void write_sdcard_file(char* to_write);
void unmount_sdcard(void);


#endif /* INC_SDCARD_H_ */
