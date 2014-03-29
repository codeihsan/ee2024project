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
#define CALIBRATION 0
#define STANDBY 1
#define ACTIVE 2
#define HOT 26
#define RISKY 800

volatile int mode = 0;
uint32_t*(extflag) = 0x0400FC140;
volatile int z_gravity = 0;
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
	PinCfg.Pinnum = 2; // temperature sensor GPIO configuration
	PINSEL_ConfigPin(&PinCfg);

	//SW3 interrupt configuration for P2.10
	PinCfg.Funcnum = 1;
	PinCfg.OpenDrain = 0;
	PinCfg.Pinmode = 0;
	PinCfg.Portnum = 2;
	PinCfg.Pinnum = 10;
	PINSEL_ConfigPin(&PinCfg);
	GPIO_SetDir(2, 1<<10, 0);
	LPC_GPIOINT->IO2IntEnF |= 1<<10;

	//light sensor interrupt confiq for P2.5
	PinCfg.Funcnum = 0;
	PinCfg.OpenDrain = 0;
	PinCfg.Pinmode = 0;
	PinCfg.Portnum = 2;
	PinCfg.Pinnum = 5;
	PINSEL_ConfigPin(&PinCfg);
	GPIO_SetDir(2, 1<<10, 0);
	LPC_GPIOINT->IO2IntEnF |= 1<<5;

	GPIO_SetDir(1, 1<<31, 0);
	GPIO_SetDir(0, 1<<4, 0);
	GPIO_SetDir(0, 1<<2, 0); // setting temperature sensor as input
}

void print_acc(int x, int y, int z){
	 char x_str[10];//stores xoff value in string format
		        sprintf(x_str,"%d", x);
		        char xprint[80];
		        strcpy(xprint, "ACC X:");
		        strcat(xprint, x_str);

		        char y_str[10];//stores yoff value in string format
		        sprintf(y_str, "%d",y);
		        char yprint[80];
		        strcpy(yprint, "ACC Y:");
		        strcat(yprint, y_str);

		        char z_str[10];//stores zoff value in string format
		        sprintf(z_str, "%d", z);
		        char zprint[80];
		        strcpy(zprint, "ACC Z:");
		        strcat(zprint, z_str);

		        oled_putString(OLED_OFFSET_X, OLED_OFFSET_Y+10, xprint, OLED_COLOR_BLACK, OLED_COLOR_WHITE);
		        oled_putString(OLED_OFFSET_X, OLED_OFFSET_Y+20, yprint, OLED_COLOR_BLACK, OLED_COLOR_WHITE);
		        oled_putString(OLED_OFFSET_X, OLED_OFFSET_Y+30, zprint, OLED_COLOR_BLACK, OLED_COLOR_WHITE);
}
void EINT0_IRQHandler(void)
{
	//SW3 interrupt P2.10
	if((LPC_GPIOINT->IO2IntStatF>>10) & 0x01){
		*extflag = 1;
		printf("SW3 triggered\n");
		mode = STANDBY;
		printf("%d", mode);
		LPC_GPIOINT->IO2IntClr = 1<<10;
		printf("handler cleared\n");
		*extflag = 1;
		return;
	}
}

void EINT3_IRQHandler(void)
{
	//Light sensor interrupt P2.05
	if((LPC_GPIOINT->IO2IntStatF>>5) & 0x01){
		*extflag = 1;
		printf("light sensor triggered\n");

		light_clearIrqStatus();
		LPC_GPIOINT->IO2IntClr = 1<<5;
		printf("handler cleared\n");
		*extflag = 1;
		return;
	}
}

void standby_initial(){
	int i;
	for(i = 5; i >=0; i--){
		int time_now = msTicks;
		char dig = (char)(((int)'0')+i);
		led7seg_setChar(dig, FALSE);

		int32_t temp_value = temp_read()/10;
		char temp_str[10];//stores zoff value in string format
		sprintf(temp_str, "%d", temp_value);
		char temp_print[80];
		strcpy(temp_print, "Temp:");
		strcat(temp_print, temp_str);
		oled_putString(OLED_OFFSET_X, OLED_OFFSET_Y +10, temp_print, OLED_COLOR_BLACK, OLED_COLOR_WHITE);


		while(msTicks - time_now <=1000);

	}
	return;
}

void standby_mode(){
	//oled_clearScreen(OLED_COLOR_WHITE);
	oled_putString(OLED_OFFSET_X, OLED_OFFSET_Y, "Standby", OLED_COLOR_BLACK, OLED_COLOR_WHITE);
	acc_setMode(ACC_MODE_STANDBY);
	int luminosity;

	while(1){

	luminosity = light_read();
	char luminosity_str[10];
	sprintf(luminosity_str, "%d", luminosity);
	char luminosity_print[80];
	strcpy(luminosity_print, "LIGHT: ");
	strcat(luminosity_print, luminosity_str);
	strcat(luminosity_print, " LUX");

	int32_t temp_value = temp_read()/10;
	char temp_str[10];//stores zoff value in string format
	sprintf(temp_str, "%d", temp_value);
	char temp_print[80];
	strcpy(temp_print, "Temp:");
	strcat(temp_print, temp_str);

	oled_putString(OLED_OFFSET_X, OLED_OFFSET_Y +10, temp_print, OLED_COLOR_BLACK, OLED_COLOR_WHITE);
	oled_putString(OLED_OFFSET_X, OLED_OFFSET_Y +20, luminosity_print, OLED_COLOR_BLACK, OLED_COLOR_WHITE);


	if(temp_value<HOT && luminosity < RISKY){
			mode =2;
			oled_putString(OLED_OFFSET_X, OLED_OFFSET_Y +30, "NORMAL", OLED_COLOR_BLACK, OLED_COLOR_WHITE);
			oled_putString(OLED_OFFSET_X, OLED_OFFSET_Y +40, "SAFE", OLED_COLOR_BLACK, OLED_COLOR_WHITE);
			Timer0_Wait(500);
			break;
		}

	if(temp_value<HOT)
		oled_putString(OLED_OFFSET_X, OLED_OFFSET_Y +30, "NORMAL", OLED_COLOR_BLACK, OLED_COLOR_WHITE);
	else
		oled_putString(OLED_OFFSET_X, OLED_OFFSET_Y +30, "HOT", OLED_COLOR_BLACK, OLED_COLOR_WHITE);

	if(temp_value<RISKY)
			oled_putString(OLED_OFFSET_X, OLED_OFFSET_Y +40, "SAFE", OLED_COLOR_BLACK, OLED_COLOR_WHITE);
		else
			oled_putString(OLED_OFFSET_X, OLED_OFFSET_Y +40, "RISKY", OLED_COLOR_BLACK, OLED_COLOR_WHITE);
	}


	//if(luminosity >= RISKY)

	//oled_putString(OLED_OFFSET_X, OLED_OFFSET_Y, luminosity_print, OLED_COLOR_BLACK, OLED_COLOR_WHITE);

}
void calibration_mode(){
    int8_t x = 0;
    int8_t y = 0;
    int8_t z = 0;

   //for debugging
    int32_t xoff = 0;
    int32_t yoff = 0;
    int32_t zoff = 0;

   // uint8_t btn1 = 1;
   // uint8_t btn2 = 1;
	oled_putString(OLED_OFFSET_X, OLED_OFFSET_Y, "Calibration", OLED_COLOR_BLACK, OLED_COLOR_WHITE);
	acc_read(&x, &y, &z);
	//for debugging
    xoff = 0-x;
    yoff = 0-y;
    zoff = 64-z;
   // printf("%d\t %d\t %d\n", xoff, yoff, zoff);

	z_gravity = z;
	//printf("%d\n",z);
	print_acc(x,y,z);

	        //btn1 = (GPIO_ReadValue(1) >> 31) & 0x01;
	        //btn2 = (GPIO_ReadValue(0) >> 4) & 0x01;

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

	        //oled_putString(OLED_OFFSET_X, OLED_OFFSET_Y+40, temp_print, OLED_COLOR_BLACK, OLED_COLOR_WHITE);
	        return;
}

int main(void){

	init_i2c();
	init_ssp();
	init_GPIO();
	acc_init();
	light_init();
	oled_init();
	led7seg_init();
	led7seg_setChar('0', FALSE);//Else displays wierd values
	SysTick_Config(SystemCoreClock / 1000);
	temp_init(getMsTicks);

	oled_clearScreen(OLED_COLOR_WHITE);
    /*
     * Assume base board in zero-g position when reading first value.
     */

	light_enable();
	light_setHiThreshold(800);
	light_setLoThreshold(0);

	light_clearIrqStatus();
	NVIC_EnableIRQ(EINT3_IRQn);
	NVIC_EnableIRQ(EINT0_IRQn);

	/*NVIC_SetPriority(EINT3_IRQn, 0x18);
	NVIC_SetPriority(EINT0_IRQn, 0x00);*/

	LPC_GPIOINT->IO2IntClr |= 1<<10;
	LPC_GPIOINT->IO2IntEnF |= 1<<10;
	LPC_GPIOINT->IO2IntClr |= 1<<5;
	LPC_GPIOINT->IO2IntEnF |= 1<<5;

	while(1){
		switch(mode){
		case CALIBRATION: calibration_mode(); break;
		case STANDBY:
			standby_initial();
			printf("%d", z_gravity);
			oled_clearScreen(OLED_COLOR_WHITE);
			standby_mode();
		}
	}
}

