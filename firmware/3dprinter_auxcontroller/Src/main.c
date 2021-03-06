/**
  ******************************************************************************
  * File Name          : main.c
  * Description        : Main program body
  ******************************************************************************
  *
  * Copyright (c) 2017 STMicroelectronics International N.V. 
  * All rights reserved.
  *
  * Redistribution and use in source and binary forms, with or without 
  * modification, are permitted, provided that the following conditions are met:
  *
  * 1. Redistribution of source code must retain the above copyright notice, 
  *    this list of conditions and the following disclaimer.
  * 2. Redistributions in binary form must reproduce the above copyright notice,
  *    this list of conditions and the following disclaimer in the documentation
  *    and/or other materials provided with the distribution.
  * 3. Neither the name of STMicroelectronics nor the names of other 
  *    contributors to this software may be used to endorse or promote products 
  *    derived from this software without specific written permission.
  * 4. This software, including modifications and/or derivative works of this 
  *    software, must execute solely and exclusively on microcontroller or
  *    microprocessor devices manufactured by or for STMicroelectronics.
  * 5. Redistribution and use of this software other than as permitted under 
  *    this license is void and will automatically terminate your rights under 
  *    this license. 
  *
  * THIS SOFTWARE IS PROVIDED BY STMICROELECTRONICS AND CONTRIBUTORS "AS IS" 
  * AND ANY EXPRESS, IMPLIED OR STATUTORY WARRANTIES, INCLUDING, BUT NOT 
  * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A 
  * PARTICULAR PURPOSE AND NON-INFRINGEMENT OF THIRD PARTY INTELLECTUAL PROPERTY
  * RIGHTS ARE DISCLAIMED TO THE FULLEST EXTENT PERMITTED BY LAW. IN NO EVENT 
  * SHALL STMICROELECTRONICS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
  * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, 
  * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF 
  * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING 
  * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
  * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  *
  ******************************************************************************
  */
/* Includes ------------------------------------------------------------------*/
#include "stm32f1xx_hal.h"
#include "usb_device.h"
#include "usbd_custom_hid_if.h"
#include "commpackets.h"
#include "housekeeping.h"
#include <math.h>

/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;
DMA_HandleTypeDef hdma_adc1;

RTC_HandleTypeDef hrtc;

TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim4;

/* USER CODE BEGIN PV */
/* Private variables ---------------------------------------------------------*/
extern USBD_HandleTypeDef hUsbDeviceFS;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void Error_Handler(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_ADC1_Init(void);
static void MX_RTC_Init(void);
static void MX_TIM4_Init(void);
static void MX_TIM2_Init(void);

void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim);
                                

/* USER CODE BEGIN PFP */
/* Private function prototypes -----------------------------------------------*/

/* USER CODE END PFP */
volatile uint16_t ADCresults[NR_AVG_POINTS][NR_ADC_CHANNELS], avgctr=0;	/* ADC result buffer, filled in with DMA */
volatile bool bGotUSBData = false, bGotADCResults = false, IsRunning=false;
volatile int32_t CommTimeoutCtr;		/* Communications timeout downcounter */
volatile bool IsInitialised = false;
uint8_t USBTxBuf[65];

// One modulator and one PID per output channel
OutModulator modulator[NROUTCHANNELS];
OutPID PID[NROUTCHANNELS];
// Thermistor housekeeping structures
Thermistor thermistor[NRTHERMISTORS];

// Device to host packet, type 1
// Note that it cannot be larger than 64 bytes.
#pragma pack(push,1)
typedef struct {
	sPkt_ThermistorValue tv[NRTHERMISTORS];
	sPkt_Endofcommand eoc;
} _d2h_pkt1;
#pragma pack(pop)
/* USER CODE END PFP */

/* USER CODE BEGIN 0 */

/* ADC conversion complete. Set flag, and let main() handle the results and start a new conversion */
void HAL_ADC_ConvCpltCallback ( ADC_HandleTypeDef *hadc) {
	static uint8_t ctr=0;
	if (avgctr==(NR_AVG_POINTS-1))
		bGotADCResults = true;
	if (++ctr == 0)
		HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
}

/* Systick callback. Called at a 1000Hz rate */
void SysTick_callback(void)
{
	float output = 0.0f, err, adcavg;
	static uint8_t pidctr=0, adcconv_ctr=0;
	int j;
	bool bEvalPID;
	uint32_t incr;
	// PID sample time. This routine is called at a 1000Hz rate
	#define PIDSAMPLETIME (0.001f*(float)NROUTCHANNELS)
	#define INVPIDSAMPLETIME (1.0f/(0.001f*(float)NROUTCHANNELS))
	if (!IsInitialised)
		return;
	bEvalPID = false;
	PID[pidctr].feedback = 0.0f;
	// Fetch appropriate feedback value
	// Is it a thermistor?
	if (PID[pidctr].inID >= 0 && PID[pidctr].inID < NRTHERMISTORS) {
		if (thermistor[PID[pidctr].inID].bIsValid) {
			PID[pidctr].feedback = thermistor[PID[pidctr].inID].Temperature;
			bEvalPID = true;
		} else {
			PID[pidctr].feedback = 0.0f;
		}
	}
	// Is it an analog input?
	else if (PID[pidctr].inID >= NRTHERMISTORS && PID[pidctr].inID < (NRTHERMISTORS+NRANALOG)) {
		adcavg = 0.0f;
		for (j=0;j<NR_AVG_POINTS;j++) {
			adcavg += (float)ADCresults[j][PID[pidctr].inID];
		}
		adcavg *= (1.0f/(float)NR_AVG_POINTS);
		PID[pidctr].feedback = adcavg;
		bEvalPID = true;
	}
	// Run with feedback = 0.0f? (testing, controlling the modulator output directly using P=1.0f)
	else if (PID[pidctr].inID < 0) {
		PID[pidctr].feedback = 0.0f;
		bEvalPID = true;
	}
	// Is it enabled?
	if (!PID[pidctr].bEnabled)
		bEvalPID = false;
	// Evaluate one of the PID controllers
	if (bEvalPID) {
		err = PID[pidctr].command - PID[pidctr].feedback;
		output = err * PID[pidctr].Kp + \
				 PID[pidctr].integrator + \
				 (err - PID[pidctr].preverror)*INVPIDSAMPLETIME*PID[pidctr].Kd + \
				 PID[pidctr].command * PID[pidctr].FF0;
		PID[pidctr].preverror = err;
		if (output >= PID[pidctr].maxlim)
			output = PID[pidctr].maxlim;
		else if (output <= PID[pidctr].minlim)
			output = PID[pidctr].minlim;
		else {
			// Only update integrator when output is not at it's limits.
			PID[pidctr].integrator += PID[pidctr].Ki*err*PIDSAMPLETIME;
		}
	}
	// Reverse action?
	if (PID[pidctr].bReverseAction)
		output = PID[pidctr].maxlim - output;
	// Update modulator
	PID[pidctr].output = output;
	incr = (uint32_t)(output * 8388608.0f);
	__disable_irq();
	modulator[pidctr].increment = incr;
	__enable_irq();
	// Update PID instance counter
	if (++pidctr >= NROUTCHANNELS)
		pidctr=0;
	// Start a ADC/DMA cycle
	if ((++adcconv_ctr & 0x07) == 0x07) {
		if (++avgctr >= NR_AVG_POINTS)
			avgctr=0;
		HAL_ADC_Start_DMA(&hadc1, (uint32_t *)&ADCresults[avgctr][0], NR_ADC_CHANNELS);
	}
}

/* Timer callback. This is where we run our pulse engine.
 * Note that for efficiency we might unroll the loops later */
void HAL_TIM_PeriodElapsedCallback (TIM_HandleTypeDef *htim)
{
	static uint16_t lowspeedctr = LOWSPEED_DIV;
	int i;
	if (!IsInitialised)
		return;
	/* Handle running state and comm timeout */
	if (!IsRunning) {
		// Not running -> outputs disabled
		for (i=0;i<NROUTCHANNELS;i++) {
			HAL_GPIO_WritePin(modulator[i].GPIOport, modulator[i].GPIOpin, GPIO_PIN_RESET);
		}
		if (CommTimeoutCtr > 0) {
			IsRunning = true;
		}
		return;
	} else {
		/* We are running. Decrement timeout counter. If this reaches 0, stop running */
		if (CommTimeoutCtr > 0) {
			CommTimeoutCtr--;
		} else {
			IsRunning = false;
			return;
		}
		/* Decrement lowspeed divisor counter. Check for 0 later */
		lowspeedctr--;
HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
		/* Do modulators. */
#if 1
		for (i=0;i<NROUTCHANNELS;i++) {
			/* modulator needs evaluation? */
			if (modulator[i].Speed == DC_FAST || (modulator[i].Speed == AC_SLOW && !lowspeedctr)) {
				modulator[i].accumulator += modulator[i].increment;
				if (modulator[i].accumulator >= 0x00800000) {
					modulator[i].accumulator -= 0x00800000;
					HAL_GPIO_WritePin(modulator[i].GPIOport, modulator[i].GPIOpin, GPIO_PIN_SET);
					//if (i==3) HAL_GPIO_WritePin (LED_GPIO_Port, LED_Pin, GPIO_PIN_SET);

				} else {
					HAL_GPIO_WritePin(modulator[i].GPIOport, modulator[i].GPIOpin, GPIO_PIN_RESET);
					//if (i==3) HAL_GPIO_WritePin (LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);
				}
			}
		}
#endif
		/* Wrap lowspeed counter if it reached 0 */
		if (lowspeedctr==0)
			lowspeedctr = LOWSPEED_DIV;
	}

}
/* USER CODE END 0 */

int main(void)
{

  /* USER CODE BEGIN 1 */
	GPIO_InitTypeDef GPIO_InitStruct;
	int i,j;
	float adcavg, logntcres, invT;
//	HAL_StatusTypeDef res;
	// Packet used to return thermistor temps to host
	_d2h_pkt1 d2h_pkt1;
	CommTimeoutCtr = 0;

	// We cannot use the preprocessor for this :(
	if (sizeof (_d2h_pkt1) > 64) {
		while (1);
	}
  /* USER CODE END 1 */

  /* MCU Configuration----------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* Configure the system clock */
  SystemClock_Config();

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
	
  MX_DMA_Init();
  MX_ADC1_Init();
  MX_RTC_Init();
  MX_TIM2_Init();
  MX_TIM4_Init();
  MX_USB_DEVICE_Init();


  /* USER CODE BEGIN 2 */
/* USER CODE BEGIN 2 */
	// Reset ADC result buffer
	for (i=0;i<NR_ADC_CHANNELS;i++) {
		for (j=0;j<NR_AVG_POINTS;j++)
			ADCresults[j][i] = 0;
	}
	// Self-calibrate ADC
	while(HAL_ADCEx_Calibration_Start(&hadc1) != HAL_OK);          
	
	// Initialise modulator data structures
	// 12VDC outputs
	modulator[OUTID_DC12V_1].GPIOpin = DC12V_PDM1_Pin; modulator[OUTID_DC12V_1].GPIOport = DC12V_PDM1_GPIO_Port; modulator[OUTID_DC12V_1].Speed = DC_FAST;
	modulator[OUTID_DC12V_1].accumulator = 0; modulator[OUTID_DC12V_1].increment = 0;
	modulator[OUTID_DC12V_2].GPIOpin = DC12V_PDM2_Pin; modulator[OUTID_DC12V_2].GPIOport = DC12V_PDM2_GPIO_Port; modulator[OUTID_DC12V_2].Speed = DC_FAST;
	modulator[OUTID_DC12V_2].accumulator = 0; modulator[OUTID_DC12V_2].increment = 0;
	modulator[OUTID_DC12V_3].GPIOpin = DC12V_PDM3_Pin; modulator[OUTID_DC12V_3].GPIOport = DC12V_PDM3_GPIO_Port; modulator[OUTID_DC12V_3].Speed = DC_FAST;
	modulator[OUTID_DC12V_3].accumulator = 0; modulator[OUTID_DC12V_3].increment = 0;
	// 24VDC outputs
	modulator[OUTID_DC24V_1].GPIOpin = DC24V_PDM1_Pin; modulator[OUTID_DC24V_1].GPIOport = DC24V_PDM1_GPIO_Port; modulator[OUTID_DC24V_1].Speed = DC_FAST;
	modulator[OUTID_DC24V_1].accumulator = 0; modulator[OUTID_DC24V_1].increment = 0;
	modulator[OUTID_DC24V_2].GPIOpin = DC24V_PDM2_Pin; modulator[OUTID_DC24V_2].GPIOport = DC24V_PDM2_GPIO_Port; modulator[OUTID_DC24V_2].Speed = DC_FAST;
	modulator[OUTID_DC24V_2].accumulator = 0; modulator[OUTID_DC24V_2].increment = 0;
	modulator[OUTID_DC24V_3].GPIOpin = DC24V_PDM3_Pin; modulator[OUTID_DC24V_3].GPIOport = DC24V_PDM3_GPIO_Port; modulator[OUTID_DC24V_3].Speed = DC_FAST;
	modulator[OUTID_DC24V_3].accumulator = 0; modulator[OUTID_DC24V_3].increment = 0;
	// 230VAC outputs
	modulator[OUTID_AC230V_1].GPIOpin = AC230V_PDM1_Pin; modulator[OUTID_AC230V_1].GPIOport = AC230V_PDM1_GPIO_Port; modulator[OUTID_AC230V_1].Speed = AC_SLOW;
	modulator[OUTID_AC230V_1].accumulator = 0; modulator[OUTID_AC230V_1].increment = 0;
	modulator[OUTID_AC230V_2].GPIOpin = AC230V_PDM2_Pin; modulator[OUTID_AC230V_2].GPIOport = AC230V_PDM2_GPIO_Port; modulator[OUTID_AC230V_2].Speed = AC_SLOW;
	modulator[OUTID_AC230V_2].accumulator = 0; modulator[OUTID_AC230V_2].increment = 0;
	modulator[OUTID_AC230V_3].GPIOpin = AC230V_PDM3_Pin; modulator[OUTID_AC230V_3].GPIOport = AC230V_PDM3_GPIO_Port; modulator[OUTID_AC230V_3].Speed = AC_SLOW;
	modulator[OUTID_AC230V_3].accumulator = 0; modulator[OUTID_AC230V_3].increment = 0;

	// Initialise PID controllers
	for (i=0;i<NROUTCHANNELS;i++) {
		PID[i].FF0 = 0.0f;
		PID[i].Kp = 0.0f;
		PID[i].Ki = 0.0f;
		PID[i].Kd = 0.0f;
		PID[i].minlim = 0.0f;
		PID[i].maxlim = 1.0f;
		PID[i].command = 0.0f;
		PID[i].inID = -1;
		PID[i].feedback = 0.0f;
		PID[i].integrator = 0.0f;
		PID[i].output = 0.0f;
		PID[i].preverror = 0.0f;
		PID[i].bEnabled = false;
		PID[i].bReverseAction = false;
	}
	// Initialise thermistor data structures
	for (i=0;i<NRTHERMISTORS;i++) {
		thermistor[i].RefResistorValue = 4700.0f;
		// Epcos B57540G0104F000 100K NTC coeffs taken from Smoothieware as default. Should at least prevent bogus outputs
		thermistor[i].SteinhartHart[0] = 0.000722378300319346f;		//ln(R)^0
		thermistor[i].SteinhartHart[1] = 0.000216301852054578f;		//ln(R)^1
		thermistor[i].SteinhartHart[2] = 0.0f;						//ln(R)^2, seldom used
		thermistor[i].SteinhartHart[3] = 9.2641025635702e-08f;		//ln(R)^3
		thermistor[i].Temperature = 0.0f;
		thermistor[i].Resistance = 0.0f;
		thermistor[i].ValidTempMax = 60.0f;			// Room temperature range.
		thermistor[i].ValidTempMin = 5.0f;			// The end application should set more sensible values
		thermistor[i].bIsValid = false;
	}

	// Setup device to host data structure static members
	d2h_pkt1.eoc.id = PKT_ENDOFCOMMAND;
	d2h_pkt1.eoc.len = sizeof(sPkt_Endofcommand);
	for (i=0;i<NRTHERMISTORS;i++) {
		d2h_pkt1.tv[i].ThermistorID = i;
		d2h_pkt1.tv[i].id = PKT_THERMISTOR_VALUE;
		d2h_pkt1.tv[i].len = sizeof(sPkt_ThermistorValue);
	}
	// Reset USB Tx buffer
	memset (USBTxBuf, 0, 64);

  // reset flag
  bGotUSBData = false;

  // Enable USB discovery by pulling DP+ high.
  GPIO_InitStruct.Pin = USB_DP_PULLUP_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_WritePin(USB_DP_PULLUP_GPIO_Port, USB_DP_PULLUP_Pin, GPIO_PIN_SET);
  HAL_GPIO_Init(USB_DP_PULLUP_GPIO_Port, &GPIO_InitStruct);

  // Start pulsegen timer
  HAL_TIM_Base_Start_IT(&htim2);

  // Start hard PWM. Not used yet.
#if 0
  HAL_TIM_Base_Start(&htim4);
  HAL_TIM_PWM_Start(&htim4,TIM_CHANNEL_1);
  HAL_TIM_PWM_Start(&htim4,TIM_CHANNEL_2);
  HAL_TIM_PWM_Start(&htim4,TIM_CHANNEL_3);
  HAL_TIM_PWM_Start(&htim4,TIM_CHANNEL_4);
#endif
  // Start ADC for the first time
  // (now done in systick) HAL_ADC_Start_DMA(&hadc1, (uint32_t *)&ADCresults[avgctr][0], NR_ADC_CHANNELS);

	/* Indicate that initialisation is done */
	IsInitialised = true;
	
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
  /* USER CODE END WHILE */

  /* USER CODE BEGIN 3 */
	// Did we receive ADC data? If so, calculate NTC temperatures
	  if (bGotADCResults) {
		  for (i=0;i<NRTHERMISTORS;i++) {
			  adcavg = 0.0f;
			  for (j=0;j<NR_AVG_POINTS;j++)
				  adcavg += (float)ADCresults[j][i];
			  adcavg *= (1.0f/(float)NR_AVG_POINTS);
			  if (adcavg > 3.0f && adcavg < 4092.0f) {
				  //ntcres = thermistor[i].RefResistorValue * ((float)(4095-ADCresults[i]) / (float)(ADCresults[i]+1));
				  thermistor[i].Resistance = thermistor[i].RefResistorValue * ((4095.0f / adcavg)-1.0f);
					//thermistor[i].Resistance = adcavg;
				  logntcres = log(thermistor[i].Resistance);
				  invT = thermistor[i].SteinhartHart[0] + logntcres*thermistor[i].SteinhartHart[1] + logntcres*logntcres*thermistor[i].SteinhartHart[2] + logntcres*logntcres*logntcres*thermistor[i].SteinhartHart[3];
				  invT = 1.0f/invT;
				  invT -= 273.15f;
				  if (invT >= thermistor[i].ValidTempMin && invT <= thermistor[i].ValidTempMax) {
					  thermistor[i].bIsValid = true;
					  thermistor[i].Temperature = invT;
				  } else {
					  thermistor[i].bIsValid = false;
					  thermistor[i].Temperature = invT;
				  }
			  } else {
				  // ADC value is bogus.
				  thermistor[i].bIsValid = false;
				  thermistor[i].Temperature = -1234.0f;
			  }
		  }
		  // restart conversion
		  bGotADCResults = false;
	  }
	  // If we received data, we setup a return transfer.
	  if (bGotUSBData) {
		  bGotUSBData = false;
		  for (i=0;i<NRTHERMISTORS;i++) {
			  d2h_pkt1.tv[i].bIsValid = thermistor[i].bIsValid;
			  d2h_pkt1.tv[i].TempCelcius = thermistor[i].Temperature;
				d2h_pkt1.tv[i].Resistance = thermistor[i].Resistance;
		  }
		  memcpy (USBTxBuf, &d2h_pkt1, sizeof (_d2h_pkt1));
		  USBD_CUSTOM_HID_SendReport(&hUsbDeviceFS, USBTxBuf, 64);
	  }
  }
  /* USER CODE END 3 */

}

/** System Clock Configuration
*/
void SystemClock_Config(void)
{

  RCC_OscInitTypeDef RCC_OscInitStruct;
  RCC_ClkInitTypeDef RCC_ClkInitStruct;
  RCC_PeriphCLKInitTypeDef PeriphClkInit;

    /**Initializes the CPU, AHB and APB busses clocks 
    */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE|RCC_OSCILLATORTYPE_LSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.LSEState = RCC_LSE_ON;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

    /**Initializes the CPU, AHB and APB busses clocks 
    */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }

  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_RTC|RCC_PERIPHCLK_ADC
                              |RCC_PERIPHCLK_USB;
  PeriphClkInit.RTCClockSelection = RCC_RTCCLKSOURCE_LSE;
  PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV6;
  PeriphClkInit.UsbClockSelection = RCC_USBCLKSOURCE_PLL_DIV1_5;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }

    /**Configure the Systick interrupt time 
    */
  HAL_SYSTICK_Config(HAL_RCC_GetHCLKFreq()/1000);

    /**Configure the Systick 
    */
  HAL_SYSTICK_CLKSourceConfig(SYSTICK_CLKSOURCE_HCLK);

  /* SysTick_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(SysTick_IRQn, 0, 0);
}

/* ADC1 init function */
static void MX_ADC1_Init(void)
{

  ADC_ChannelConfTypeDef sConfig;

    /**Common config 
    */
  hadc1.Instance = ADC1;
  hadc1.Init.ScanConvMode = ADC_SCAN_ENABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 8;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

    /**Configure Regular Channel 
    */
  sConfig.Channel = ADC_CHANNEL_0;
  sConfig.Rank = 1;
  sConfig.SamplingTime = ADC_SAMPLETIME_239CYCLES_5;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

    /**Configure Regular Channel 
    */
  sConfig.Channel = ADC_CHANNEL_1;
  sConfig.Rank = 2;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

    /**Configure Regular Channel
    */
  sConfig.Channel = ADC_CHANNEL_2;
  sConfig.Rank = 3;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

    /**Configure Regular Channel
    */
  sConfig.Channel = ADC_CHANNEL_3;
  sConfig.Rank = 4;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

    /**Configure Regular Channel
    */
  sConfig.Channel = ADC_CHANNEL_4;
  sConfig.Rank = 5;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

    /**Configure Regular Channel
    */
  sConfig.Channel = ADC_CHANNEL_5;
  sConfig.Rank = 6;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

    /**Configure Regular Channel
    */
  sConfig.Channel = ADC_CHANNEL_TEMPSENSOR;
  sConfig.Rank = 7;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

    /**Configure Regular Channel
    */
  sConfig.Channel = ADC_CHANNEL_VREFINT;
  sConfig.Rank = 8;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

}

/* RTC init function */
static void MX_RTC_Init(void)
{

  RTC_TimeTypeDef sTime;
  RTC_DateTypeDef DateToUpdate;

    /**Initialize RTC Only
    */
  hrtc.Instance = RTC;
  hrtc.Init.AsynchPrediv = RTC_AUTO_1_SECOND;
  hrtc.Init.OutPut = RTC_OUTPUTSOURCE_NONE;
  if (HAL_RTC_Init(&hrtc) != HAL_OK)
  {
    Error_Handler();
  }

    /**Initialize RTC and set the Time and Date
    */
  sTime.Hours = 0x1;
  sTime.Minutes = 0x0;
  sTime.Seconds = 0x0;

  if (HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BCD) != HAL_OK)
  {
    Error_Handler();
  }

  DateToUpdate.WeekDay = RTC_WEEKDAY_MONDAY;
  DateToUpdate.Month = RTC_MONTH_JANUARY;
  DateToUpdate.Date = 0x1;
  DateToUpdate.Year = 0x0;

  if (HAL_RTC_SetDate(&hrtc, &DateToUpdate, RTC_FORMAT_BCD) != HAL_OK)
  {
    Error_Handler();
  }

}

/* TIM2 init function */
static void MX_TIM2_Init(void)
{

  TIM_ClockConfigTypeDef sClockSourceConfig;
  TIM_MasterConfigTypeDef sMasterConfig;

  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 0;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = (72000000 / TICKS_PER_SEC) - 1;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }

  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }

  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }

}

/* TIM4 init function */
static void MX_TIM4_Init(void)
{

  TIM_ClockConfigTypeDef sClockSourceConfig;
  TIM_MasterConfigTypeDef sMasterConfig;
  TIM_OC_InitTypeDef sConfigOC;

  htim4.Instance = TIM4;
  htim4.Init.Prescaler = 0;
  htim4.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim4.Init.Period = 7199;
  htim4.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  if (HAL_TIM_Base_Init(&htim4) != HAL_OK)
  {
    Error_Handler();
  }

  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim4, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }

  if (HAL_TIM_PWM_Init(&htim4) != HAL_OK)
  {
    Error_Handler();
  }

  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim4, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }

  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim4, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }

  if (HAL_TIM_PWM_ConfigChannel(&htim4, &sConfigOC, TIM_CHANNEL_2) != HAL_OK)
  {
    Error_Handler();
  }

  if (HAL_TIM_PWM_ConfigChannel(&htim4, &sConfigOC, TIM_CHANNEL_3) != HAL_OK)
  {
    Error_Handler();
  }

  if (HAL_TIM_PWM_ConfigChannel(&htim4, &sConfigOC, TIM_CHANNEL_4) != HAL_OK)
  {
    Error_Handler();
  }

  HAL_TIM_MspPostInit(&htim4);

}

/** 
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void) 
{
  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Channel1_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);

}

/** Configure pins as 
        * Analog 
        * Input 
        * Output
        * EVENT_OUT
        * EXTI
*/
static void MX_GPIO_Init(void)
{

  GPIO_InitTypeDef GPIO_InitStruct;

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, DC12V_PDM1_Pin|DC12V_PDM2_Pin|DC12V_PDM3_Pin|DC24V_PDM1_Pin
                          |DC24V_PDM2_Pin|DC24V_PDM3_Pin|AC230V_PDM1_Pin|AC230V_PDM2_Pin
                          |AC230V_PDM3_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : LED_Pin */
  GPIO_InitStruct.Pin = LED_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LED_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : DC12V_PDM1_Pin DC12V_PDM2_Pin DC12V_PDM3_Pin DC24V_PDM1_Pin
                           DC24V_PDM2_Pin DC24V_PDM3_Pin AC230V_PDM1_Pin AC230V_PDM2_Pin
                           AC230V_PDM3_Pin */
  GPIO_InitStruct.Pin = DC12V_PDM1_Pin|DC12V_PDM2_Pin|DC12V_PDM3_Pin|DC24V_PDM1_Pin
                          |DC24V_PDM2_Pin|DC24V_PDM3_Pin|AC230V_PDM1_Pin|AC230V_PDM2_Pin
                          |AC230V_PDM3_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : USB_DP_PULLUP_Pin */
  GPIO_InitStruct.Pin = USB_DP_PULLUP_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(USB_DP_PULLUP_GPIO_Port, &GPIO_InitStruct);

}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @param  None
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler */
  /* User can add his own implementation to report the HAL error return state */
  while(1) 
  {
  }
  /* USER CODE END Error_Handler */ 
}

#ifdef USE_FULL_ASSERT

/**
   * @brief Reports the name of the source file and the source line number
   * where the assert_param error has occurred.
   * @param file: pointer to the source file name
   * @param line: assert_param error line source number
   * @retval None
   */
void assert_failed(uint8_t* file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
    ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */

}

#endif

/**
  * @}
  */ 

/**
  * @}
*/ 

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
