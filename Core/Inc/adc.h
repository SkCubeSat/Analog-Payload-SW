/*
 * adc.h
 *
 *  Created on: Mar 25, 2025
 *      Author: jiang35j
 */

#ifndef INC_ADC_H_
#define INC_ADC_H_

#include "main.h"


/*
 * Function: delay for x us
 * input: # of us
 * output: none, delay for x us
 * */
void delay_us (uint16_t us);

/*
 * Function: Find the maximum element in the array
 * input: array, length of the array
 * output: maximum element
 * */
uint16_t max(uint16_t arr[], int len);

/*
 * Function: select the channel of ADC to read
 * input: the channel number
 * output: none, switch the adc to the selected channel
 * */
void select_adc_channel(int channel);

/*
 * Function: read the op amp inputs and outputs, store onto the SD card
 * TODO: Maybe split into 2 functions: 1 store onto some buffers and the other write to the SD card
 * input: none
 * output: none, write the numbers to the SD card
 * */
void preform_opamp_measurement_log_to_sd(uint8_t);

//same as above but for voltage references
void preform_vref_measurement_log_to_sd(uint8_t);

//same but for the lm35
//I don't think the temp is getting written to the sd card but I could be wrong
//TODO: add sd card related stuffs
void read_lm35(uint8_t);

//same but for the opto couplers
//suspecting there are some problems with the algorithm but could be the current hardware issues
void preform_opto_measurement_log_to_sd(uint8_t);


#endif /* INC_ADC_H_ */
