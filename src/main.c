/*****************************************************************************
 *   A demo example using several of the peripherals on the base board
 *
 *   Copyright(C) 2011, EE2024
 *   All rights reserved.
 *
 ******************************************************************************/

#include "stdio.h"
#include "stdlib.h"
#include "lpc17xx_pinsel.h"
#include "lpc17xx_gpio.h"
#include "lpc17xx_i2c.h"
#include "lpc17xx_ssp.h"
#include "lpc17xx_timer.h"

#include "led7seg.h"
#include "temp.h"
#include "light.h"
#include "joystick.h"
#include "pca9532.h"
#include "acc.h"
#include "oled.h"
#include "rgb.h"

#define OLED_OFFSET_X 1
#define OLED_OFFSET_Y 1
volatile uint32_t msTicks = 0; // counter for 1ms SysTicks

// ****************
//  SysTick_Handler - just increment SysTick counter
void SysTick_Handler(void) {
    msTicks++;
}

uint32_t getMsTicks(){
	return msTicks;
}

static void init_ssp(void)
{
	SSP_CFG_Type SSP_ConfigStruct;
	PINSEL_CFG_Type PinCfg;

	/*
	 * Initialize SPI pin connect
	 * P0.7 - SCK;
	 * P0.8 - MISO
	 * P0.9 - MOSI
	 * P2.2 - SSEL - used as GPIO
	 */
	PinCfg.Funcnum = 2;
	PinCfg.OpenDrain = 0;
	PinCfg.Pinmode = 0;
	PinCfg.Portnum = 0;
	PinCfg.Pinnum = 7;
	PINSEL_ConfigPin(&PinCfg);
	PinCfg.Pinnum = 8;
	PINSEL_ConfigPin(&PinCfg);
	PinCfg.Pinnum = 9;
	PINSEL_ConfigPin(&PinCfg);
	PinCfg.Funcnum = 0;
	PinCfg.Portnum = 2;
	PinCfg.Pinnum = 2;
	PINSEL_ConfigPin(&PinCfg);

	SSP_ConfigStructInit(&SSP_ConfigStruct);

	// Initialize SSP peripheral with parameter given in structure above
	SSP_Init(LPC_SSP1, &SSP_ConfigStruct);

	// Enable SSP peripheral
	SSP_Cmd(LPC_SSP1, ENABLE);

}

static void init_i2c(void)
{
	PINSEL_CFG_Type PinCfg;

	/* Initialize I2C2 pin connect for accelerometer */
	PinCfg.Funcnum = 2;
	PinCfg.Pinnum = 10;
	PinCfg.Portnum = 0;
	PINSEL_ConfigPin(&PinCfg);
	PinCfg.Pinnum = 11;
	PINSEL_ConfigPin(&PinCfg);

	// Initialize I2C peripheral
	I2C_Init(LPC_I2C2, 100000);

	/* Enable I2C1 operation */
	I2C_Cmd(LPC_I2C2, ENABLE);
}

static void init_GPIO(void)
{
	// Initialize buttons SW3 and SW4
	PINSEL_CFG_Type PinCfg;

	PinCfg.Funcnum = 0;
	PinCfg.OpenDrain = 0;
	PinCfg.Pinmode = 0;
	PinCfg.Portnum = 1;
	PinCfg.Pinnum = 31;
	PINSEL_ConfigPin(&PinCfg);
	PinCfg.Portnum = 0;
	PinCfg.Pinnum = 4;
	PINSEL_ConfigPin(&PinCfg);
	PinCfg.Pinnum = 2; // temperature sensor GPIO configuration
	PINSEL_ConfigPin(&PinCfg);

	GPIO_SetDir(1, 1<<31, 0);
	GPIO_SetDir(0, 1<<4, 0);
	GPIO_SetDir(0, 1<<2, 0); // setting temperature sensor as input
}

int main(void){
	uint8_t btn1 = 1;
    uint8_t btn2 = 1;

	int32_t xoff = 0;
    int32_t yoff = 0;
    int32_t zoff = 0;

    int8_t x = 0;
    int8_t y = 0;
    int8_t z = 0;

	init_i2c();
	init_ssp();
	init_GPIO();
	acc_init();
	light_init();
	oled_init();
	SysTick_Config(SystemCoreClock / 1000);
	temp_init(getMsTicks);
	oled_clearScreen(OLED_COLOR_WHITE);
    /*
     * Assume base board in zero-g position when reading first value.
     */
	oled_putString(OLED_OFFSET_X, OLED_OFFSET_Y, "Calibration mode", OLED_COLOR_BLACK, OLED_COLOR_WHITE);
	while(1){
        acc_read(&x, &y, &z);
        xoff = 0-x;
        yoff = 0-y;
        zoff = 64-z;

        char xoff_str[10];//stores xoff value in string format
        sprintf(xoff_str,"%d", xoff);
        char xprint[80];
        strcpy(xprint, "ACC X:");
        strcat(xprint, xoff_str);

        char yoff_str[10];//stores yoff value in string format
        sprintf(yoff_str, "%d",yoff);
        char yprint[80];
        strcpy(yprint, "ACC Y:");
        strcat(yprint, yoff_str);

        char zoff_str[10];//stores zoff value in string format
        sprintf(zoff_str, "%d", zoff);
        char zprint[80];
        strcpy(zprint, "ACC Z:");
        strcat(zprint, zoff_str);
        btn1 = (GPIO_ReadValue(1) >> 31) & 0x01;
        btn2 = (GPIO_ReadValue(0) >> 4) & 0x01;
        light_enable();

		int luminosity;
		luminosity = light_read();
		char luminosity_str[10];
		sprintf(luminosity_str, "%d", luminosity);
		char luminosity_print[80];
		strcpy(luminosity_print, "LIGHT: ");
		strcat(luminosity_print, luminosity_str);
		strcat(luminosity_print, " LUX");

        if(btn1 == 0){
            oled_clearScreen(OLED_COLOR_WHITE);
            oled_putString(OLED_OFFSET_X, OLED_OFFSET_Y, "Standby", OLED_COLOR_BLACK, OLED_COLOR_WHITE);
            oled_putString(OLED_OFFSET_X, OLED_OFFSET_Y+10, luminosity_print, OLED_COLOR_BLACK, OLED_COLOR_WHITE);
            break;
        }
        if(btn2 == 0){
            oled_clearScreen(OLED_COLOR_WHITE);
            oled_putString(OLED_OFFSET_X, OLED_OFFSET_Y, "StandbyBtn2", OLED_COLOR_WHITE, OLED_COLOR_BLACK);
            break;
        }

		/*if(msTicks % 500 == 0){
		printf("%d", msTicks);
        int32_t temp_value = temp_read();
        printf("%d", msTicks);
        printf("%d", temp_value);
         char temp_str[10];//stores zoff value in string format
         sprintf(temp_str, "%d", temp_value);
         //itoa(temp_value,temp_str, 10);
         char temp_print[80];
         strcpy(temp_print, "Temp:");
         strcat(temp_print, temp_str);
		}*/

        oled_putString(OLED_OFFSET_X, OLED_OFFSET_Y+10, xprint, OLED_COLOR_BLACK, OLED_COLOR_WHITE);
        oled_putString(OLED_OFFSET_X, OLED_OFFSET_Y+20, yprint, OLED_COLOR_BLACK, OLED_COLOR_WHITE);
        oled_putString(OLED_OFFSET_X, OLED_OFFSET_Y+30, zprint, OLED_COLOR_BLACK, OLED_COLOR_WHITE);
        //oled_putString(OLED_OFFSET_X, OLED_OFFSET_Y+40, temp_print, OLED_COLOR_BLACK, OLED_COLOR_WHITE);
	}
	light_shutdown();
}


