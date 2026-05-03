/*
 * sdcard.c
 *
 *  Created on: Feb 9, 2024
 *      Author: karim
 */
//#include "cmsis_os.h"
#include "sdcard.h"
#include "i2c_slave.h"


#define MAX_BUFF_BYTES 255

	  FIL fil; 		//File handle
	  FRESULT fres; //Result after operations
	  DWORD free_clusters, free_sectors, total_sectors;

	  FATFS* getFreeFs;

	  //some variables for FatFs
	  FATFS FatFs; 	//Fatfs handle
	  //FIL fil; 		//File handle
	  BYTE working_buff[MAX_BUFF_BYTES];
	  UINT bytesWrote;
	  TCHAR* rres;
	  char* _filename;


	  uint8_t mount_sdcard(void) {
	  //Open the file system
	  fres = f_mount(&FatFs, "", 1); //1=mount now

	  if (fres != FR_OK) {
		printf("f_mount error (%i)\r\n", fres);
		return 0;
//		Error_Handler();
	  }

	  printf("sd card mounted!\r\n");
	  return 1;


}

void print_sdcard_stats(void) {
	  fres = f_getfree("", &free_clusters, &getFreeFs);
	  if (fres != FR_OK) {
		printf("f_getfree error (%i)\r\n", fres);
		Error_Handler();
	  }

	  //Formula comes from ChaN's documentation
	  total_sectors = (getFreeFs->n_fatent - 2) * getFreeFs->csize;
	  free_sectors = free_clusters * getFreeFs->csize;

	  printf("SD card stats:\r\n%10lu KiB total drive space.\r\n%10lu KiB available.\r\n", total_sectors / 2, free_sectors / 2);

}

void open_sdcard_file_read(char* filename) {
	  //Now let's try to open file "test.txt"
	_filename = filename;

	  fres = f_open(&fil, filename, FA_READ);
	  if (fres != FR_OK) {
		printf("f_open error (%i)\r\n", fres);
		Error_Handler();
	  }
	  printf("I was able to open '%s' for reading!\r\n", filename);


}

void read_sdcard_file(void) {

	  //Read 30 bytes from "test.txt" on the SD card

	  //We can either use f_read OR f_gets to get data out of files
	  //f_gets is a wrapper on f_read that does some string formatting for us
	  rres = f_gets((TCHAR*)working_buff, MAX_BUFF_BYTES, &fil);
	  if(rres != 0) {
		printf("Read string from '%s' contents: %s\r\n",_filename, working_buff);
	  } else {
		printf("f_gets error (%i)\r\n", fres);
	  }
}

void close_sdcard_file(void) {


	  f_close(&fil);

}

void open_sdcard_file_write(char* filename) {

	// lots of checks to do here...
	char buf[20];
	// append incremented number to filename
	uint32_t i = 0;
	while(1){
		// append i to filename

		snprintf(buf, 20, "%s_%lu.csv", filename, i);
		_filename = buf;

		printf("try filename: %s\r\n", _filename);


	  fres = f_open(&fil, _filename, FA_WRITE | FA_CREATE_NEW | FA_OPEN_EXISTING);
	  if(fres == FR_OK) {
		printf("'%s' successful open for write\r\n", _filename);
		return;
	  } else if (fres == FR_EXIST){
		printf("f_open error (%i)\r\n", fres);
		i++;
	  } else { // file may be corrupted, skip it
			printf("unexpected file state, skipping (%i)\r\n", fres);
			i++;
	  }
	}
}

void write_sdcard_file(char* to_write) {

	  //Copy in a string
		uint16_t str_size = strlen(to_write);
	if (str_size > MAX_BUFF_BYTES) {
		printf("error, size of str [%u] too large to write [%u]",str_size, MAX_BUFF_BYTES );
		Error_Handler();

	}

	  strncpy((char*)working_buff, to_write, str_size);
	  fres = f_write(&fil, working_buff, str_size, &bytesWrote);
	  if(fres == FR_OK) {
		printf("Wrote %i bytes to the file!\r\n", bytesWrote);
	  } else {
		printf("f_write error (%i)\r\n", fres);
	  }

}

void unmount_sdcard(void) {

	  //We're done, so de-mount the drive
	  f_mount(NULL, "", 0);
}
