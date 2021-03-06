/**
  ******************************************************************************
  * @file           : usbd_custom_hid_if.c
  * @brief          : USB Device Custom HID interface file.
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
#include "usbd_custom_hid_if.h"
/* USER CODE BEGIN INCLUDE */
#include "commpackets.h"
#include "housekeeping.h"
#include <math.h>

/* USER CODE END INCLUDE */
/** @addtogroup STM32_USB_OTG_DEVICE_LIBRARY
  * @{
  */

/** @defgroup USBD_CUSTOM_HID 
  * @brief usbd core module
  * @{
  */ 

/** @defgroup USBD_CUSTOM_HID_Private_TypesDefinitions
  * @{
  */ 
/* USER CODE BEGIN PRIVATE_TYPES */
/* USER CODE END PRIVATE_TYPES */ 
/**
  * @}
  */ 

/** @defgroup USBD_CUSTOM_HID_Private_Defines
  * @{
  */ 
/* USER CODE BEGIN PRIVATE_DEFINES */
/* USER CODE END PRIVATE_DEFINES */
  
/**
  * @}
  */ 

/** @defgroup USBD_CUSTOM_HID_Private_Macros
  * @{
  */ 
/* USER CODE BEGIN PRIVATE_MACRO */
/* USER CODE END PRIVATE_MACRO */

/**
  * @}
  */ 

/** @defgroup USBD_AUDIO_IF_Private_Variables
 * @{
 */
__ALIGN_BEGIN static uint8_t CUSTOM_HID_ReportDesc_FS[USBD_CUSTOM_HID_REPORT_DESC_SIZE] __ALIGN_END =
{
  /* USER CODE BEGIN 0 */ 
	// desc size=(28 bytes)
		0x06, 0x12, 0xff,							// Usage page 0xff12
		0x0A, 0x00, 0x02,							// rawhid usage 0x0200
		0xA1, 0x01,										// Collection 0x01
		0x75, 0x08,										// report size = 8 bits
		0x15, 0x00,										// logical minimum = 0
		0x26, 0xFF, 0x00,							// logical maximum = 255
		0x95, CUSTOM_HID_EPIN_SIZE,		// report count
		0x09, 0x01,										// usage
		0x81, 0x02,										// Input (array)
		0x95, CUSTOM_HID_EPOUT_SIZE,	// report count
		0x09, 0x02,				// usage
		0x91, 0x02,				// Output (array)
	  0xC0    /*     END_COLLECTION	             */
   
}; 

/* USER CODE BEGIN PRIVATE_VARIABLES */
/* USER CODE END PRIVATE_VARIABLES */
/**
  * @}
  */ 
  
/** @defgroup USBD_CUSTOM_HID_IF_Exported_Variables
  * @{
  */ 
  extern USBD_HandleTypeDef hUsbDeviceFS;
/* USER CODE BEGIN EXPORTED_VARIABLES */
/* USER CODE END EXPORTED_VARIABLES */

/**
  * @}
  */ 
  
/** @defgroup USBD_CUSTOM_HID_Private_FunctionPrototypes
  * @{
  */
static int8_t CUSTOM_HID_Init_FS     (void);
static int8_t CUSTOM_HID_DeInit_FS   (void);
static int8_t CUSTOM_HID_OutEvent_FS (uint8_t event_idx, uint8_t state);
 

USBD_CUSTOM_HID_ItfTypeDef USBD_CustomHID_fops_FS = 
{
  CUSTOM_HID_ReportDesc_FS,
  CUSTOM_HID_Init_FS,
  CUSTOM_HID_DeInit_FS,
  CUSTOM_HID_OutEvent_FS,
};

/* Private functions ---------------------------------------------------------*/
/**
  * @brief  CUSTOM_HID_Init_FS
  *         Initializes the CUSTOM HID media low layer
  * @param  None
  * @retval Result of the operation: USBD_OK if all operations are OK else USBD_FAIL
  */
static int8_t CUSTOM_HID_Init_FS(void)
{ 
  /* USER CODE BEGIN 4 */ 
  return (0);
  /* USER CODE END 4 */ 
}

/**
  * @brief  CUSTOM_HID_DeInit_FS
  *         DeInitializes the CUSTOM HID media low layer
  * @param  None
  * @retval Result of the operation: USBD_OK if all operations are OK else USBD_FAIL
  */
static int8_t CUSTOM_HID_DeInit_FS(void)
{
  /* USER CODE BEGIN 5 */ 
  return (0);
  /* USER CODE END 5 */ 
}

/**
  * @brief  CUSTOM_HID_OutEvent_FS
  *         Manage the CUSTOM HID class events       
  * @param  event_idx: event index
  * @param  state: event state
  * @retval Result of the operation: USBD_OK if all operations are OK else USBD_FAIL
  */
static int8_t CUSTOM_HID_OutEvent_FS  (uint8_t event_idx, uint8_t state)
{ 
 /* USER CODE BEGIN 6 */ 
	USBD_CUSTOM_HID_HandleTypeDef *hhid = (USBD_CUSTOM_HID_HandleTypeDef *)hUsbDeviceFS.pClassData;
	uint8_t *data;
	uint8_t idx=0, loopctr=16;
#if 1
	while (idx<64 && --loopctr) {
		data = &hhid->Report_buf[idx];
		/* Determine packet type and process the packet */
		switch (data[1]) {

			/* PID control packet? */
			case PKT_PIDCONTROL:
				// Outchannel is in a valid range?
				if (((sPkt_PIDControl *)data)->outID >= NROUTCHANNELS)
					break;
				// Copy and verify data
				PID[((sPkt_PIDControl *)data)->outID].inID = ((sPkt_PIDControl *)data)->inID;
				PID[((sPkt_PIDControl *)data)->outID].FF0 = ((sPkt_PIDControl *)data)->coeffFF0;
				PID[((sPkt_PIDControl *)data)->outID].Kd = ((sPkt_PIDControl *)data)->coeffD;
				PID[((sPkt_PIDControl *)data)->outID].Ki = ((sPkt_PIDControl *)data)->coeffI;
				PID[((sPkt_PIDControl *)data)->outID].Kp = ((sPkt_PIDControl *)data)->coeffP;
				PID[((sPkt_PIDControl *)data)->outID].command = ((sPkt_PIDControl *)data)->command;
				PID[((sPkt_PIDControl *)data)->outID].minlim = ((sPkt_PIDControl *)data)->minlim;
				PID[((sPkt_PIDControl *)data)->outID].maxlim = ((sPkt_PIDControl *)data)->maxlim;
				PID[((sPkt_PIDControl *)data)->outID].bEnabled = ((sPkt_PIDControl *)data)->bEnabled;
				PID[((sPkt_PIDControl *)data)->outID].bReverseAction = ((sPkt_PIDControl *)data)->bReverseAction;				
				if (fabs(PID[((sPkt_PIDControl *)data)->outID].minlim) > 1.0f) PID[((sPkt_PIDControl *)data)->outID].minlim = 0.0f;
				if (fabs(PID[((sPkt_PIDControl *)data)->outID].maxlim) > 1.0f) PID[((sPkt_PIDControl *)data)->outID].maxlim = 1.0f;
				if (PID[((sPkt_PIDControl *)data)->outID].minlim > PID[((sPkt_PIDControl *)data)->outID].maxlim)
					PID[((sPkt_PIDControl *)data)->outID].maxlim = PID[((sPkt_PIDControl *)data)->outID].minlim;
				__disable_irq();
				bGotUSBData = true;
				CommTimeoutCtr = COMM_TIMEOUT;
				__enable_irq();
				break;
			/* Thermistor setup packet? */
			case PKT_THERMISTOR_SETUP:
				// Thermistor ID in a valid range?
				if (((sPkt_ThermistorSetup *)data)->ThermistorID >= NRTHERMISTORS)
					break;
				thermistor[((sPkt_ThermistorSetup *)data)->ThermistorID].RefResistorValue = ((sPkt_ThermistorSetup *)data)->RefResistorValue;
				thermistor[((sPkt_ThermistorSetup *)data)->ThermistorID].SteinhartHart[0] = ((sPkt_ThermistorSetup *)data)->SteinhartHart[0];
				thermistor[((sPkt_ThermistorSetup *)data)->ThermistorID].SteinhartHart[1] = ((sPkt_ThermistorSetup *)data)->SteinhartHart[1];
				thermistor[((sPkt_ThermistorSetup *)data)->ThermistorID].SteinhartHart[2] = ((sPkt_ThermistorSetup *)data)->SteinhartHart[2];
				thermistor[((sPkt_ThermistorSetup *)data)->ThermistorID].SteinhartHart[3] = ((sPkt_ThermistorSetup *)data)->SteinhartHart[3];
				thermistor[((sPkt_ThermistorSetup *)data)->ThermistorID].ValidTempMax = ((sPkt_ThermistorSetup *)data)->ValidTempMax;
				thermistor[((sPkt_ThermistorSetup *)data)->ThermistorID].ValidTempMin = ((sPkt_ThermistorSetup *)data)->ValidTempMin;
				__disable_irq();
				bGotUSBData = true;
				CommTimeoutCtr = COMM_TIMEOUT;
				__enable_irq();
				break;
			/* End of data frame command */
			case PKT_ENDOFCOMMAND:
				idx = 64;
				__disable_irq();
				bGotUSBData = true;
				CommTimeoutCtr = COMM_TIMEOUT;
				__enable_irq();
				break;
		}
		/* go to next packet */
		idx += data[0];
	}
#else
	__disable_irq();
	bGotUSBData = true;
	CommTimeoutCtr = COMM_TIMEOUT;
	__enable_irq();
#endif

  return (0);
  /* USER CODE END 6 */ 
}

/* USER CODE BEGIN 7 */ 
/**
  * @brief  USBD_CUSTOM_HID_SendReport_FS
  *         Send the report to the Host       
  * @param  report: the report to be sent
  * @param  len: the report length
  * @retval Result of the operation: USBD_OK if all operations are OK else USBD_FAIL
  */
/*  
static int8_t USBD_CUSTOM_HID_SendReport_FS ( uint8_t *report,uint16_t len)
{
  return USBD_CUSTOM_HID_SendReport(&hUsbDeviceFS, report, len); 
}
*/
/* USER CODE END 7 */ 

/* USER CODE BEGIN PRIVATE_FUNCTIONS_IMPLEMENTATION */
/* USER CODE END PRIVATE_FUNCTIONS_IMPLEMENTATION */

/**
  * @}
  */ 

/**
  * @}
  */  
/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
