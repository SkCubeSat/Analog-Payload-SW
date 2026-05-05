/*
 * adc.c
 *
 *  Created on: Mar 25, 2025
 *      Author: jiang35j
 */

#include "adc.h"
#include "data_log.h"
#include "sdcard.h"

extern ADC_HandleTypeDef hadc1;
extern TIM_HandleTypeDef htim1;

// https://controllerstech.com/create-1-microsecond-delay-stm32/
void delay_us (uint16_t us)
{
	__HAL_TIM_SET_COUNTER(&htim1,0);  // set the counter value a 0
	while (__HAL_TIM_GET_COUNTER(&htim1) < us);  // wait for the counter to reach the us input in the parameter
}

uint16_t max(uint16_t arr[], int len)
{
	uint16_t i;

    // Initialize maximum element
    uint16_t max = arr[0];

    // Traverse array elements
    // from second and compare
    // every element with current max
    for (i = 1; i < len; i++)
        if (arr[i] > max)
            max = arr[i];

    return max;
}

void select_adc_channel(int channel)
{
    ADC_ChannelConfTypeDef sConfig = {0};
    //sConfig.SamplingTime = ADC_SAMPLETIME_15CYCLES;
    sConfig.SamplingTime = ADC_SAMPLETIME_7CYCLES_5;
    switch (channel)
    {
        case 0:
            sConfig.Channel = ADC_CHANNEL_0;
              sConfig.Rank = 1;

              if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
              {
                Error_Handler();
              }
              break;

        case 1:
              /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
              */
              sConfig.Channel = ADC_CHANNEL_1;
              sConfig.Rank = 1;
              if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
              {
                Error_Handler();
              }
              break;
              /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
              */
        case 2:
              sConfig.Channel = ADC_CHANNEL_2;
              sConfig.Rank = 1;
              if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
              {
                Error_Handler();
              }
              break;
              /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
              */
        case 3:
              sConfig.Channel = ADC_CHANNEL_3;
              sConfig.Rank = 1;
              if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
              {
                Error_Handler();
              }
              break;
              /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
              */
        case 4:
              sConfig.Channel = ADC_CHANNEL_4;
              sConfig.Rank = 1;
              if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
              {
                Error_Handler();
              }
              break;
              /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
              */
        case 5:
              sConfig.Channel = ADC_CHANNEL_5;
              sConfig.Rank = 1;
              if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
              {
                Error_Handler();
              }
              break;
              /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
              */
        case 6:
              sConfig.Channel = ADC_CHANNEL_6;
              sConfig.Rank = 1;
              if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
              {
                Error_Handler();
              }
              break;
              /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
              */
        case 7:
              sConfig.Channel = ADC_CHANNEL_7;
              sConfig.Rank = 1;
              if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
              {
                Error_Handler();
              }
              break;
              /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
              */
        case 8:
              sConfig.Channel = ADC_CHANNEL_8;
              sConfig.Rank = 9;
              if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
              {
                Error_Handler();
              }
              break;
              /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
              */
        case 9:
              sConfig.Channel = ADC_CHANNEL_9;
              sConfig.Rank = 1;
              if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
              {
                Error_Handler();
              }
              break;
              /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
              */
        case 10:
              sConfig.Channel = ADC_CHANNEL_10;
              sConfig.Rank = 1;
              if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
              {
                Error_Handler();
              }
              break;
              /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
              */
        case 11:
              sConfig.Channel = ADC_CHANNEL_11;
              sConfig.Rank = 1;
              if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
              {
                Error_Handler();
              }
              break;
              /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
              */
        case 12:
              sConfig.Channel = ADC_CHANNEL_12;
              sConfig.Rank = 1;
              if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
              {
                Error_Handler();
              }
              break;
              /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
              */
        case 13:
              sConfig.Channel = ADC_CHANNEL_13;
              sConfig.Rank = 1;
              if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
              {
                Error_Handler();
              }
              break;
              /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
              */
        case 14:
              sConfig.Channel = ADC_CHANNEL_14;
              sConfig.Rank = 1;
              if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
              {
                Error_Handler();
              }
              break;
        case 15:
              sConfig.Channel = ADC_CHANNEL_15;
              sConfig.Rank = 1;
              if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
              {
                Error_Handler();
              }
              break;
        default:
            break;
    }
}

void preform_opamp_measurement_log_to_sd(uint8_t sd_status) {
	char adc_buf[15];

	// dont need to convert to voltages here, since we divide at the end for gain

	const uint16_t NUM_OF_SAMPLES = 80;

	uint16_t source_sine_samples[NUM_OF_SAMPLES];
	uint16_t peak_source_sine;

	uint16_t test_sine_samples[NUM_OF_SAMPLES];
	uint16_t peak_test_sine;

	uint16_t noshd_gain; // percentage
	uint16_t mel_gain;
	uint16_t al_gain;

	for (uint16_t i = 0; i <= 3; i++) {

		 for (uint8_t j = 0; j < NUM_OF_SAMPLES; j++) {

			  select_adc_channel(i);
			  // Get each ADC value from the group (2 channels in this case)
			  HAL_ADC_Start(&hadc1);
			  // Wait for regular group conversion to be completed
			  HAL_ADC_PollForConversion(&hadc1, HAL_MAX_DELAY);
			  if (i == 0) {
				  source_sine_samples[j] = HAL_ADC_GetValue(&hadc1);
			  } else {
				  test_sine_samples[j] = HAL_ADC_GetValue(&hadc1);
			  }

			  delay_us(80); // ~160 Hz sine wave, try to sample it evenly with 80 samples as 1/(160 Hz * 80) ~ 80us

		 }

		 switch (i) {
		 	 case 0:
		 		 peak_source_sine = max(source_sine_samples, NUM_OF_SAMPLES);
		 		 printf("SINE source peak quantized: %u\r\n", peak_source_sine);
		 		 break;
		 	 case 1: // noshd
		 		 peak_test_sine = max(test_sine_samples, NUM_OF_SAMPLES);

		 		noshd_gain = (uint16_t) (( ((float) peak_test_sine) / ((float) peak_source_sine) ) * 100);
		 		 printf("SINE noshd gain %%: %u\r\n", noshd_gain);


		 		break;
		 	 case 2:
		 		 peak_test_sine = max(test_sine_samples, NUM_OF_SAMPLES);

			 		mel_gain = (uint16_t) (( ((float) peak_test_sine) / ((float) peak_source_sine) ) * 100);
			 		 printf("SINE mel gain %%: %u\r\n", mel_gain);
		 		break;

		 	 case 3:
		 		 peak_test_sine = max(test_sine_samples, NUM_OF_SAMPLES);

			 		al_gain = (uint16_t) (( ((float) peak_test_sine) / ((float) peak_source_sine) ) * 100);
			 		 printf("SINE al gain %%: %u\r\n", al_gain);
		 		break;

		 }

	}
  if(sd_status == 1) {
	  snprintf(adc_buf, 15, "%u,%u,%u,", noshd_gain, mel_gain, al_gain);
      write_sdcard_file(adc_buf);
  }
  else {
	  data_log_push(noshd_gain);
	  data_log_push(mel_gain);
	  data_log_push(al_gain);
  }
}

void preform_vref_measurement_log_to_sd(uint8_t sd_status) {
	char adc_buf[40];
	uint16_t quantized_vref_val;

	for (uint16_t i = 4; i <= 6; i++) {

		  select_adc_channel(i);
		  // Get each ADC value from the group (2 channels in this case)
		  HAL_ADC_Start(&hadc1);
		  // Wait for regular group conversion to be completed
		  HAL_ADC_PollForConversion(&hadc1, HAL_MAX_DELAY);
		  quantized_vref_val = HAL_ADC_GetValue(&hadc1);

		  if (i == 4) { printf("VREF NOSHD quantized val: %u\r\n", quantized_vref_val);}
		  if (i == 5) { printf("VREF MEL quantized val: %u\r\n", quantized_vref_val);}
		  if (i == 6) { printf("VREF AL quantized val: %u\r\n", quantized_vref_val);}

		  if(sd_status == 1) {
			  snprintf(adc_buf, 40, "%u,", quantized_vref_val);
			  write_sdcard_file(adc_buf);
		  }
		  else {
			  data_log_push(quantized_vref_val);
		  }

	}

}

void read_lm35(uint8_t sd_status) {
		char adc_buf[40];

		uint16_t quantized_lm35_v;

	  select_adc_channel(9);
	  // Get each ADC value from the group (2 channels in this case)
	  HAL_ADC_Start(&hadc1);
	  // Wait for regular group conversion to be completed
	  HAL_ADC_PollForConversion(&hadc1, HAL_MAX_DELAY);
	  quantized_lm35_v = HAL_ADC_GetValue(&hadc1);

	  printf("LM35 quantized volt: %u\r\n", quantized_lm35_v);


	  if(sd_status == 1) {
		  snprintf(adc_buf, 40, "%u,", quantized_lm35_v);
		  write_sdcard_file(adc_buf);
		  }
	  else {
			  data_log_push(quantized_lm35_v);
		  }
}

void preform_opto_measurement_log_to_sd(uint8_t sd_status) {
	char adc_buf[15];

	// dont need to convert to voltages here, since we divide at the end for gain

	const uint16_t NUM_OF_SAMPLES = 80;

	uint16_t forward_opto_current[NUM_OF_SAMPLES];
	uint16_t peak_forward_opto_current;

	uint16_t emitter_opto_current[NUM_OF_SAMPLES];
	uint16_t peak_emitter_opto_current;

	uint16_t noshd_ctr; // percentage
	uint16_t mel_ctr;
	uint16_t al_ctr;

	for (uint16_t i = 10; i <= 15; i++) {

		 for (uint8_t j = 0; j < NUM_OF_SAMPLES; j++) {

			  select_adc_channel(i);
			  // Get each ADC value from the group (2 channels in this case)
			  HAL_ADC_Start(&hadc1);
			  // Wait for regular group conversion to be completed
			  HAL_ADC_PollForConversion(&hadc1, HAL_MAX_DELAY);

			  // if adc channel number even
			  // if moving channels around have to change that here
			  if (i % 2 == 0) {
				  // its forward
				  forward_opto_current[j] = HAL_ADC_GetValue(&hadc1);
			  } else {
				  // else its emitter
				  emitter_opto_current[j] = HAL_ADC_GetValue(&hadc1);
			  }

			  delay_us(80); // ~160 Hz sine wave, try to sample it evenly with 80 samples as 1/(160 Hz * 80) ~ 80us

		 }

		 switch (i) {
		 	 case 10:
		 		peak_forward_opto_current = max(forward_opto_current, NUM_OF_SAMPLES);
		 		 printf("OPTO noshd forward current peak: %u\r\n", peak_forward_opto_current);
		 		 break;
		 	 case 11:
		 		peak_emitter_opto_current = max(emitter_opto_current, NUM_OF_SAMPLES);
		 		 printf("OPTO noshd emitter current peak: %u\r\n", peak_emitter_opto_current);

		 		// now we have all noshd data, find ctr = emitter/forward * 50 for our resistor selection

		 		noshd_ctr = (uint16_t) (( ((float) peak_emitter_opto_current) / ((float) peak_forward_opto_current) ) * 50);
		 		 printf("OPTO noshd ctr %%: %u\r\n", noshd_ctr);

		 		break;
		 	 case 12:
		 		peak_forward_opto_current = max(forward_opto_current, NUM_OF_SAMPLES);
		 		 printf("OPTO mel forward current peak: %u\r\n", peak_forward_opto_current);
		 		 break;
		 	 case 13:
		 		peak_emitter_opto_current = max(emitter_opto_current, NUM_OF_SAMPLES);
		 		 printf("OPTO mel emitter current peak: %u\r\n", peak_emitter_opto_current);

		 		// now we have all noshd data, find ctr = emitter/forward * 50 for our resistor selection

		 		mel_ctr = (uint16_t) (( ((float) peak_emitter_opto_current) / ((float) peak_forward_opto_current) ) * 50);
		 		 printf("OPTO mel ctr %%: %u\r\n", mel_ctr);

		 		break;
		 	 case 14:
		 		peak_forward_opto_current = max(forward_opto_current, NUM_OF_SAMPLES);
		 		 printf("OPTO al forward current peak: %u\r\n", peak_forward_opto_current);
		 		 break;
		 	 case 15:
		 		peak_emitter_opto_current = max(emitter_opto_current, NUM_OF_SAMPLES);
		 		 printf("OPTO al emitter current peak: %u\r\n", peak_emitter_opto_current);

		 		// now we have all noshd data, find ctr = emitter/forward * 50 for our resistor selection

		 		al_ctr = (uint16_t) (( ((float) peak_emitter_opto_current) / ((float) peak_forward_opto_current) ) * 50);
		 		 printf("OPTO al ctr %%: %u\r\n", al_ctr);

		 		break;
		 }

	}

  if(sd_status == 1) {
	  snprintf(adc_buf, 15, "%u,%u,%u,", noshd_ctr, mel_ctr, al_ctr);
	  write_sdcard_file(adc_buf);
	  }
  else {
		  data_log_push(noshd_ctr);
		  data_log_push(mel_ctr);
		  data_log_push(al_ctr);
	  }

}
