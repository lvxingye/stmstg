/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file            : usb_host.c
 * @version         : v1.0_Cube
 * @brief           : This file implements the USB Host
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2025 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/

#include "usb_host.h"
#include "usbh_core.h"
#include "usbh_hid.h"

/* USER CODE BEGIN Includes */
#include <stdint.h>
#include <stdio.h>
/* USER CODE END Includes */

/* USER CODE BEGIN PV */
/* Private variables
 * ---------------------------------------------------------*/

/* USER CODE END PV */

/* USER CODE BEGIN PFP */
/* Private function prototypes
 * -----------------------------------------------*/

/* USER CODE END PFP */

/* USB Host core handle declaration */
USBH_HandleTypeDef hUsbHostFS;
ApplicationTypeDef Appli_state = APPLICATION_IDLE;

/*
 * -- Insert your variables declaration here --
 */
/* USER CODE BEGIN 0 */
/* USER CODE END 0 */

/*
 * user callback declaration
 */
static void USBH_UserProcess(USBH_HandleTypeDef *phost, uint8_t id);

/*
 * -- Insert your external function declaration here --
 */
/* USER CODE BEGIN 1 */
uint8_t query_keys(void) {
    HID_KEYBD_Info_TypeDef* k_pinfo;
    static uint8_t          key_codes = 0;
    if (Appli_state == APPLICATION_READY) {
        if (USBH_HID_GetDeviceType(&hUsbHostFS) == HID_KEYBOARD) {
            k_pinfo = USBH_HID_GetKeybdInfo(&hUsbHostFS); /* get keybrd info */

            if (k_pinfo != NULL) {
                key_codes = 0;
                if (k_pinfo->lshift || k_pinfo->rshift) {
                    key_codes |= SHIFT_KEY;
                }
                for (uint8_t i = 0; i < 6; i++) {
                    switch (k_pinfo->keys[i]) {
                    case KEY_UPARROW: key_codes |= UP_KEY; break;
                    case KEY_DOWNARROW: key_codes |= DOWN_KEY; break;
                    case KEY_LEFTARROW: key_codes |= LEFT_KEY; break;
                    case KEY_RIGHTARROW: key_codes |= RIGHT_KEY; break;
                    case KEY_Z: key_codes |= Z_KEY; break;
                    case KEY_X: key_codes |= X_KEY; break;
                    case KEY_ESCAPE: key_codes |= ESC_KEY; break;
                    default: break;
                    }
                }
            }
        }
    }
    return key_codes;
}
/* USER CODE END 1 */

/**
  * Init USB host library, add supported class and start the library
  * @retval None
  */
void MX_USB_HOST_Init(void)
{
  /* USER CODE BEGIN USB_HOST_Init_PreTreatment */

  /* USER CODE END USB_HOST_Init_PreTreatment */

  /* Init host Library, add supported class and start the library. */
  if (USBH_Init(&hUsbHostFS, USBH_UserProcess, HOST_FS) != USBH_OK)
  {
    Error_Handler();
  }
  if (USBH_RegisterClass(&hUsbHostFS, USBH_HID_CLASS) != USBH_OK)
  {
    Error_Handler();
  }
  if (USBH_Start(&hUsbHostFS) != USBH_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USB_HOST_Init_PostTreatment */

  /* USER CODE END USB_HOST_Init_PostTreatment */
}

/*
 * Background task
 */
void MX_USB_HOST_Process(void)
{
  /* USB Host Background task */
  USBH_Process(&hUsbHostFS);
}
/*
 * user callback definition
 */
static void USBH_UserProcess  (USBH_HandleTypeDef *phost, uint8_t id)
{
  /* USER CODE BEGIN CALL_BACK_1 */
    switch (id) {
    case HOST_USER_SELECT_CONFIGURATION: break;

    case HOST_USER_DISCONNECTION: Appli_state = APPLICATION_DISCONNECT; break;

    case HOST_USER_CLASS_ACTIVE:
        Appli_state = APPLICATION_READY;
        printf("connected...\r\n");
        break;

    case HOST_USER_CONNECTION:
        Appli_state = APPLICATION_START;
        printf("connecting...\r\n");
        break;

    default: break;
    }
  /* USER CODE END CALL_BACK_1 */
}

/**
  * @}
  */

/**
  * @}
  */

