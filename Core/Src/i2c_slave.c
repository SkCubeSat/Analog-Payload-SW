/*

 *
 *  Created on: Dec 8, 2024
 *      Author: Bailey
 */


#include "main.h"
#include "i2c_slave.h"
#include "sdcard.h"
#include "i2c_queue.h"
#include "data_log.h"

static const uint8_t busyMsg[] = "BUSY";

//i2c command flag
//try remove the static decoration if you find the flag cannot be changed by the setter function
static volatile uint8_t i2c_flag = I2C_FLAG_RESET;
//power saving mode flag
//try remove the static decoration if you find the flag cannot be changed by the setter function
static volatile uint8_t pwr_flag = PWR_NOR;
//Check whether the transmit buffer is empty
static volatile uint8_t tx_buf_empty_flag = BUF_NOT_EMPTY;
static volatile uint8_t awaitingRTC = false;

extern I2C_HandleTypeDef hi2c1;
extern TIM_HandleTypeDef htim2;
extern RTC_HandleTypeDef hrtc;
//extern uint8_t testnumber = 'c';

extern BYTE working_buff[TxSIZE];
extern FIL fil;
extern FRESULT fres; //Result after operations

// Receive buffer
uint8_t RxData[RxSIZE];
//RTC loading buffer
static uint8_t rtcBuf[RTC_PAYLOAD_LEN];
// Transmit buffer. Static because it will be accessed in the main.c
//maximum size is defined by the TxSIZE
static uint8_t TxBuffer[TxSIZE];

//dynamic size of the buffer
static volatile uint16_t buf_size = 0;

// Track # bytes sent/received
volatile uint8_t rxCount = 0;
volatile uint8_t txCount = 0;

int firstByteRecieved = false; 

int countAddr = 0;   // # times AddrCallback is called
int countRxCplt = 0; // # times rxCplt is called
int countError = 0;  // # times error is called

uint8_t count = 0;


volatile uint8_t is_i2c_reinit_needed = 0;

static volatile uint8_t status_byte = 0; // stays private here
static uint8_t status_tx;

void i2c_set_busy(uint8_t on)  { if(on) status_byte |= 0x01; else status_byte &= (uint8_t)~0x01; }
void i2c_set_ready(uint8_t on) { if(on) status_byte |= 0x02; else status_byte &= (uint8_t)~0x02; }
void i2c_set_error(uint8_t on) { if(on) status_byte |= 0x04; else status_byte &= (uint8_t)~0x04; }
uint8_t i2c_get_status(void)   { return status_byte; }


//--------------getter and setter functions-----------------------------------
uint8_t i2c_flag_getter()
{
	return i2c_flag;
}

void i2c_flag_reset()
{
	i2c_flag = I2C_FLAG_RESET;
}


uint8_t pwr_flag_getter()
{
	return pwr_flag;
}

void pwr_flag_setter(uint8_t flag)
{
	pwr_flag = flag;
}


//---------------------tx buffer loading functions---------------------------------------
//TODO this function only reads the data files. To read the error logs we need a different function
//TODO add time stamps to the files
// Assumptions:
// - TxSIZE is the total size of TxBuffer
// - i2c_set_busy(), i2c_set_ready(), i2c_set_error() manipulate status_byte bits
// - pack_values() and append_file_timestamp() work as you posted
// - LATEST_NAME_MAX, MAX_VALUES, etc. are defined
// - TxBuffer, buf_size are module-scope variables (as in your code)

void load_buf(void)
{
	printf("load_buf function: load buffer function");
    char     filename[LATEST_NAME_MAX];
    uint16_t values[MAX_VALUES];
    uint32_t valCount = 0;
    uint32_t offset   = 0;   // payload write index (no header yet)
    uint32_t payload_len;

    // Mark busy while building the buffer
    i2c_set_busy(1);
    i2c_set_ready(0);
    i2c_set_error(0);

    // Safety: require space for 3-byte header later
    if (TxSIZE < 3) {
        i2c_set_error(1);
        i2c_set_busy(0);
        return;
    }

//    uint8_t sdcard_status = mount_sdcard();
    //test remove line below later
    uint8_t sdcard_status = 0;

    if(sdcard_status == 1){

    	 // 1) Find latest S_*.CSV
    	    if (!get_latest_s_file(filename, sizeof(filename))) {
    	        printf("No S_*.CSV files found!\r\n");
    	        unmount_sdcard();
    	        i2c_set_error(1);
    	        i2c_set_busy(0);
    	        return;
    	    }
    	    printf("Loading latest file: %s\r\n", filename);

    	    // 2) Open it (your wrapper sets global 'fres')
    	    open_sdcard_file_read(filename);
    	    if (fres != FR_OK) {
    	        printf("open_sdcard_file_read failed (%d)\r\n", (int)fres);
    	        unmount_sdcard();
    	        i2c_set_error(1);
    	        i2c_set_busy(0);
    	        return;
    	    }

    	    // 3) Parse CSV -> values[]
    	    valCount = parse_csv_rows(values);  // count of uint16s parsed
    	    if (valCount == 0) {
    	        printf("parse_csv_rows returned 0\r\n");
    	        close_sdcard_file();
    	        unmount_sdcard();
    	        i2c_set_error(1);
    	        i2c_set_busy(0);
    	        return;
    	    }
    }

    else {
    	//use buffer instead of SDcard
    	memcpy(values, data_log[routine_num], data_count * sizeof(uint16_t));
    	valCount = data_count;  // Set valCount so pack_values works correctly
    	printf("\nvalCount: %u", data_count);


    }



    // 4) Pack numeric values into TxBuffer (temporarily at [0..))
    //    pack_values returns bytes written (valCount*2)
    offset = pack_values(values, valCount, TxBuffer);

    // 5) Append ASCII timestamp; returns bytes appended
    //    (Write it immediately after packed values)
    if(sdcard_status == 1)
    	offset += append_file_timestamp(filename, TxBuffer, offset);


    // Payload was built at TxBuffer[0..offset)
    payload_len = offset;

    // 6) Final on-wire layout: [STATUS][LEN_L][LEN_H][PAYLOAD...]
    if (payload_len + 3 > TxSIZE) {
        printf("Payload too large: %lu (max %u)\r\n",
               (unsigned long)payload_len, (unsigned)TxSIZE);
        if(sdcard_status == 1) {

            close_sdcard_file();
            unmount_sdcard();
        }
        i2c_set_error(1);
        i2c_set_busy(0);
        return;
    }

    // Shift payload up by 3 bytes (safe with memmove)
    memmove(&TxBuffer[3], &TxBuffer[0], payload_len);

    // Fill LEN (little-endian). STATUS byte (TxBuffer[0]) is set in AddrCallback.
    TxBuffer[1] = (uint8_t)(payload_len & 0xFF);
    TxBuffer[2] = (uint8_t)((payload_len >> 8) & 0xFF);

    // Total bytes available to serve when ready
    buf_size = payload_len + 3;

    // 7) Cleanup storage
    if(sdcard_status == 1) {

        close_sdcard_file();
        unmount_sdcard();
    }


    // 8) Mark READY (not busy)
    i2c_set_busy(0);
    i2c_set_ready(1);
    // leave error as is
}



uint8_t get_latest_s_file(char *outName, size_t outSize) {
    DIR dir;
    FILINFO fno;
    UINT idx;
    UINT maxIdx = 0;
    uint8_t found = false;

    // Open the root directory (or change "/" to your subfolder)
    if (f_opendir(&dir, "/") != FR_OK) {
        return false;
    }

    // Enumerate all entries
    for (;;) {
        if (f_readdir(&dir, &fno) != FR_OK || fno.fname[0] == 0) {
            break;  // error or end of dir
        }
        // Skip directories
        if (fno.fattrib & AM_DIR) {
            continue;
        }
        // Try to parse names like "S_<number>.CSV"
        if (sscanf(fno.fname, "S_%u.CSV", &idx) == 1) {
            if (!found || idx > maxIdx) {
                maxIdx = idx;
                found = true;
            }
        }
    }
    f_closedir(&dir);

    if (!found) {
        return false;
    }
    // Build the filename into outName
    snprintf(outName, outSize, "S_%u.CSV", maxIdx);
    return true;
}

//------------------------------------------------------------------------------
// Read up to ROWS_TO_SEND lines (skipping header) and parse into values[]
//------------------------------------------------------------------------------
static uint32_t parse_csv_rows(uint16_t *values) {
    TCHAR   *line;
    char    *tok;
    uint32_t rowCount = 0, valCount = 0;

    // Skip header
    line = f_gets((TCHAR*)working_buff, MAX_LINE_LEN, &fil);
    if (!line) {
        printf("Error skipping header (%d)\r\n", (int)fres);
        return 0;
    }
    // Read rows
    while (rowCount < ROWS_TO_SEND &&
           (line = f_gets((TCHAR*)working_buff, MAX_LINE_LEN, &fil)) != NULL) {
        tok = strtok((char*)line, ",");
        for (uint32_t fld = 0; tok && fld < FIELDS_PER_ROW; fld++) {
            if (valCount < MAX_VALUES) {
                values[valCount++] = (uint16_t)atoi(tok);
            }
            tok = strtok(NULL, ",");
        }
        rowCount++;
    }
    return valCount;
}

//------------------------------------------------------------------------------
// Pack 'count' uint16 values into 'buffer' (little-endian). Returns byte count.
//------------------------------------------------------------------------------
static uint32_t pack_values(const uint16_t *values, uint32_t count, uint8_t *buffer) {
    uint32_t offset = 0;
    for (uint32_t i = 0; i < count; i++) {
        buffer[offset++] = (uint8_t)(values[i] & 0xFF);
        buffer[offset++] = (uint8_t)(values[i] >> 8);
    }
    return offset;
}

//------------------------------------------------------------------------------
// Append ASCII timestamp from file metadata; returns length of appended text
//------------------------------------------------------------------------------
static uint32_t append_file_timestamp(const char *filename, uint8_t *buffer, uint32_t offset) {
    FILINFO finfo;
    char    tsbuf[32];
    uint32_t len = 0;

    if (f_stat(filename, &finfo) == FR_OK) {
        uint16_t fdate = finfo.fdate;
        uint16_t ftime = finfo.ftime;
        uint16_t year  = ((fdate >> 9) & 0x7F) + 1932;
        uint8_t  month = (fdate >> 5) & 0x0F;
        uint8_t  day   = fdate & 0x1F;
        uint8_t  hour  = (ftime >> 11) & 0x1F;
        uint8_t  minute= (ftime >> 5) & 0x3F;
        uint8_t  second= (ftime & 0x1F) * 2;

        int tslen = snprintf(tsbuf, sizeof(tsbuf),
            "TS:%04u-%02u-%02u %02u:%02u:%02u\r\n",
            year, month, day, hour, minute, second
        );
        if (tslen > 0 && offset + tslen <= TxSIZE) {
            memcpy(&buffer[offset], tsbuf, tslen);
            len = tslen;
        }
    } else {
        printf("f_stat failed (%d)\r\n", (int)fres);
    }
    return len;
}

//-----------------------cmd processing-----------------------------------
void process_data(void)
{
    switch(RxData[0])
    {
        case I2C_CMD_RESET:
            HAL_NVIC_SystemReset();
            break;
        case I2C_CMD_START:
        	i2c_flag = I2C_FLAG_SET;
            break;
        case I2C_CMD_PWRSAV: // turn off the 5V supply for the testing ICs
        	turn_off_5v_plane();
        	pwr_flag_setter(PWR_SAV);
        	HAL_TIM_Base_Stop_IT(&htim2);
            break;
        case I2C_CMD_PWRNOR:
        	turn_on_5v_plane();
        	pwr_flag_setter(PWR_NOR);
        	HAL_TIM_Base_Start_IT(&htim2);
        	break;
        	//TODO add the transmit commands
        case I2C_CMD_SEND_DATA:
        	i2c_flag = I2C_FLAG_READ_DATA;
        	break;
    }
}

//------------------------------------------------------------------------------
// Decode RTC payload and configure hardware RTC, then re-enable listen mode
//------------------------------------------------------------------------------
static void handle_rtc_payload(void)
{
    // rtcBuf[0..1]: year
    uint16_t year = (rtcBuf[0] << 8) | rtcBuf[1];

    // Prepare date structure
    RTC_DateTypeDef sDate;
    sDate.Year    = year - 2000;    // store as offset 00-99
    sDate.Month   = rtcBuf[2];
    sDate.Date    = rtcBuf[3];
    sDate.WeekDay = rtcBuf[4];
    HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BIN);

    // Prepare time structure
    RTC_TimeTypeDef sTime;
    sTime.Hours   = rtcBuf[5];
    sTime.Minutes = rtcBuf[6];
    sTime.Seconds = rtcBuf[7];
    HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BIN);

    // Return to listen mode for next I2C transaction
    //HAL_I2C_EnableListen_IT(hi2c);
}


//---------------------------callback functions---------------------------------------------
void HAL_I2C_ListenCpltCallback(I2C_HandleTypeDef *hi2c)
{
    if (hi2c->Instance == I2C1) {
        HAL_I2C_EnableListen_IT(hi2c);
    }
}

// add a global to cap each transfer
static volatile uint16_t tx_total = 0;

void HAL_I2C_AddrCallback(I2C_HandleTypeDef *hi2c,
                          uint8_t TransferDirection,
                          uint16_t AddrMatchCode)
{
    (void)AddrMatchCode;
    if (hi2c->Instance != I2C1) return;

    if (TransferDirection == I2C_DIRECTION_TRANSMIT) {
        // Master -> Slave (receive command)
        awaitingRTC = false;
        rxCount = 0;
        HAL_I2C_Slave_Seq_Receive_IT(hi2c, &RxData[0], 1, I2C_FIRST_FRAME);
        return;
    }

    // Master READS from us
    txCount = 0;
    TxBuffer[0] = status_byte;  // always serve fresh status first

    // If READY and not BUSY and you have a buffer prepared,
    // serve the entire buffer [STATUS][LEN_L][LEN_H][PAYLOAD...]
    if ((status_byte & 0x02) && !(status_byte & 0x01) && (buf_size >= 1)) {
        tx_total = buf_size;      // e.g. 3 + payload_len
    } else {
        tx_total = 1;             // just STATUS when busy/not-ready
    }

    // kick off with the first byte; TxCplt will feed the rest
    HAL_I2C_Slave_Seq_Transmit_IT(hi2c, &TxBuffer[0], 1, I2C_FIRST_FRAME);
}



//TODO: implement transmitting the information collected in the SD card back to OBC
//2 types of info: 1. data collected, 2. error log
void HAL_I2C_SlaveTxCpltCallback(I2C_HandleTypeDef *hi2c)
{
	if (hi2c->Instance != I2C1) return;

    txCount++;
    if(txCount < buf_size){
    	HAL_I2C_Slave_Seq_Transmit_IT(hi2c, &TxBuffer[txCount], 1, I2C_NEXT_FRAME);
    	//i2c_flag = I2C_FLAG_READ_DATA;
    }
}

// Byte-Receive Complete
void HAL_I2C_SlaveRxCpltCallback(I2C_HandleTypeDef *hi2c)
{

    if (!awaitingRTC) {
        // we just got cmdBuf[0]
        if (RxData[0] == I2C_CMD_SET_RTC) {
            // now receive the 7-byte timestamp
            awaitingRTC = true;
            HAL_I2C_Slave_Seq_Receive_IT(
                hi2c,
                rtcBuf,
                RTC_PAYLOAD_LEN,
                I2C_NEXT_FRAME
            );
        }
        else {
            // … handle other commands …
        	//process_data();
            // enqueue commands instead of calling process_data directly
        	if (busy_flag_getter()) {
        	    enqueue_i2c_cmd(RxData[0]);
        	    HAL_I2C_EnableListen_IT(hi2c);
        	    return;
        	}
            enqueue_i2c_cmd(RxData[0]);
            HAL_I2C_EnableListen_IT(hi2c);
        }
    }
    else {
    	handle_rtc_payload();
    	HAL_I2C_EnableListen_IT(hi2c);
    }
}


void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *hi2c)
{
    countError++; 
    uint32_t errorCode = HAL_I2C_GetError(hi2c);

    if (hi2c->Instance == I2C1) {
        // Mark for reinit outside ISR context
    	is_i2c_reinit_needed = 1;
    }

    if (errorCode == 4) // AF (ack failure): master stopped sending at less than RxSIZE
    {
        process_data();
    }


	HAL_I2C_EnableListen_IT(hi2c);
}
