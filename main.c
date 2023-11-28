/*
  **********************************************************************************
  * @title		main.c
  * @platform	STM32F103
  * @author		Anton Novikov
  * @version	V1.0.0
  * @date		05.02.2021
  *
  * @brief		A simple example of obtaining real-time data on cell voltage, state of charge, using timers to periodically update the data.
  * 			Interfacing with the CAN bus to exchange data via a diagnostic request on Mazda CX-30.
  *
  **********************************************************************************
*/

#include "stm32f10x.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_can.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_tim.h"

#include "misc.h"
#include "can.h"
#include "ssd1306.h"
#include "ssd1306_i2c.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

// Function prototype for timer initialization.
void init_timer();
// Function prototype for timer reset.
void res_timer(int timer_num, int timer_type);

// Definition of tpTimer structure
typedef struct
  {
	bool run;  	// Timer running flag.
	bool res;	// Timer reset flag.
	int val;	// Current timer value.
  } tpTimer;

// Array of timers with 1000 ms interval
tpTimer timer_1000ms[10] = {{false, false, 0}};			// Array of timers with a 1000 ms interval.
int time_togle_1ms = 0;  								// Flag toggled every millisecond.
int time_togle_1ms_old = 0;								// Previous value of the toggle flag.

// Array of timers with 10000 x 100 ms interval
tpTimer timer_10000x100ms[10] = {{false, false, 0}};	// Array of timers with a 10000 x 100 ms interval.
int time_togle_100ms = 0;  								// Flag toggled every 100 milliseconds.
int time_togle_100ms_old = 0;							// Previous value of the toggle flag.

const int sendingPeriod = 1;							// Period sending time CAN message 100ms.
int cellNumber  = 0;									// High voltage battery cell number.
bool dateProdBatt = false;;

void SSD1306_ON(void);
uint8_t ssd1306_I2C_IsDeviceConnected(I2C_TypeDef* I2Cx, uint8_t address);

void Delay(void) {
  volatile uint32_t i;
  for (i=0; i != 0x600000; i++);
}

int main(void)
{
	// Create a structure of type GPIO_InitTypeDef to configure GPIO parameters.
	GPIO_InitTypeDef GPIO_InitStructure;

	// Enable the clock for GPIO port C to make it usable.
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);

	// Configure parameters for GPIO_Pin_13 in the GPIO_InitStructure structure.
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13;				// Select pin 13 on GPIO port C.
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;		// Set the pin to operate in output mode with push-pull configuration.
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;		// Set the operating speed of the port.

	// Apply the configured settings to GPIO port C using the specified structure
	GPIO_Init(GPIOC, &GPIO_InitStructure);

	// Enable clock for AFIO (Alternative Function I/O)
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
	// Remap SWJ_JTAG, in this case, disable JTAG on PB3.
	// After this, PB3 can be used as a GPIO pin
	GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE);

	// Current menu item. Default is 1
	int MenuPosition = 1;
	// Flag indicating if LCD update is needed.
	int NeedUpdate = 1;

	SystemInit();				// Initialize system settings.
	void SSD1306_ON();			// Turn on SSD1306 display.
	SSD1306_Init();				// Initialize SSD1306 display.
	init_timer();				// Initialize timers.
	init_CAN();					// Initialize CAN communication.

	SSD1306_GotoXY(5, 10);		// Set cursor position on SSD1306 display.
	SSD1306_Puts("MAZDA CX-30", &Font_11x18, SSD1306_COLOR_WHITE);		// Display text on SSD1306.
	SSD1306_GotoXY(38, 33);
	SSD1306_Puts("M-HYBRID", &Font_7x10, SSD1306_COLOR_WHITE);
	SSD1306_UpdateScreen();												// Update SSD1306 display.
	GPIO_ResetBits(GPIOC, GPIO_Pin_13);									// Reset GPIO pin 13.
	Delay();
	GPIO_SetBits(GPIOC, GPIO_Pin_13);									// Set GPIO pin 13.

	while (1) {
		/* Timer management */
		// time_togle_1ms flag is toggled each 1ms
		// Each timer counts up to 10 seconds
		if (time_togle_1ms != time_togle_1ms_old) {
			for (int i = 0; i < 10; i++) {
				if (timer_1000ms[i].run && timer_1000ms[i].val < 10000) {
					timer_1000ms[i].val++;
				}
			}
			time_togle_1ms_old = time_togle_1ms;
		}
		// time_togle_100ms flag is toggled each 100ms
		// Each timer counts up to 100 seconds
		if (time_togle_100ms != time_togle_100ms_old) {
			for (int i = 0; i < 10; i++) {
				if (timer_10000x100ms[i].run
						&& timer_10000x100ms[i].val < 10000) {
					timer_10000x100ms[i].val++;
				}
			}
			time_togle_100ms_old = time_togle_100ms;
		}
		timer_1000ms[1].run = true;


		if (timer_1000ms[1].val >= sendingPeriod) {
			// Request Voltage of Battery Cell
			if ((cellNumber >= 0) && (cellNumber <= 9)) {
				request_MHB(cellNumber);
			} else {
				// Request production date of Battery
			    if (!dateProdBatt) {
			        request_MHB(0x11);
			        dateProdBatt = true;
			    } else {
			    	// Request State of Charge of Battery
				request_MHB(0x14);
			    }
			}
					cellNumber ++;
					if (cellNumber > 10) {
						cellNumber = 0;
					}
					NeedUpdate = 1;
					timer_1000ms[1].run = false;
					res_timer(1, 100);
				}

		char buf[32];
		float SOC_HV = MHB_SOC_HV_HEX / 10.;
		float CELL1_VOLT = MHB_CELL1_VOLT / 1000.;
		float CELL2_VOLT = MHB_CELL2_VOLT / 1000.;
		float CELL3_VOLT = MHB_CELL3_VOLT / 1000.;
		float CELL4_VOLT = MHB_CELL4_VOLT / 1000.;
		float CELL5_VOLT = MHB_CELL5_VOLT / 1000.;
		float CELL6_VOLT = MHB_CELL6_VOLT / 1000.;
		float CELL7_VOLT = MHB_CELL7_VOLT / 1000.;
		float CELL8_VOLT = MHB_CELL8_VOLT / 1000.;
		float CELL9_VOLT = MHB_CELL9_VOLT / 1000.;
		float HV_VOLT = MHB_HV_VOLT / 100.;

		switch (MenuPosition) {

		case 1:
			if (NeedUpdate) {
								SSD1306_Fill(SSD1306_COLOR_BLACK);
								SSD1306_GotoXY(0, 0);
								SSD1306_Puts("C1: ", &Font_7x10, SSD1306_COLOR_WHITE);
									sprintf(buf, "%0.3f", CELL1_VOLT);
								SSD1306_GotoXY(24, 0);
								SSD1306_Puts(buf, &Font_7x10, SSD1306_COLOR_WHITE);

								SSD1306_GotoXY(0, 10);
								SSD1306_Puts("C2: ", &Font_7x10, SSD1306_COLOR_WHITE);
									sprintf(buf, "%0.3f", CELL2_VOLT);
								SSD1306_GotoXY(24, 10);
								SSD1306_Puts(buf, &Font_7x10, SSD1306_COLOR_WHITE);

								SSD1306_GotoXY(0, 20);
								SSD1306_Puts("C3: ", &Font_7x10, SSD1306_COLOR_WHITE);
									sprintf(buf, "%0.3f", CELL3_VOLT);
								SSD1306_GotoXY(24, 20);
								SSD1306_Puts(buf, &Font_7x10, SSD1306_COLOR_WHITE);

								SSD1306_GotoXY(0, 30);
								SSD1306_Puts("C4: ", &Font_7x10, SSD1306_COLOR_WHITE);
									sprintf(buf, "%0.3f", CELL4_VOLT);
								SSD1306_GotoXY(24, 30);
								SSD1306_Puts(buf, &Font_7x10, SSD1306_COLOR_WHITE);

								SSD1306_GotoXY(0, 40);
								SSD1306_Puts("C5: ", &Font_7x10, SSD1306_COLOR_WHITE);
									sprintf(buf, "%0.3f", CELL5_VOLT);
								SSD1306_GotoXY(24, 40);
								SSD1306_Puts(buf, &Font_7x10, SSD1306_COLOR_WHITE);

								SSD1306_GotoXY(0, 50);
								SSD1306_Puts("SOC: ", &Font_7x10, SSD1306_COLOR_WHITE);
									sprintf(buf, "%0.1f", SOC_HV);
								SSD1306_GotoXY(30, 50);
								SSD1306_Puts(buf, &Font_7x10, SSD1306_COLOR_WHITE);


								SSD1306_GotoXY(68, 0);
								SSD1306_Puts("C6: ", &Font_7x10, SSD1306_COLOR_WHITE);
									sprintf(buf, "%0.3f", CELL6_VOLT);
								SSD1306_GotoXY(92, 0);
								SSD1306_Puts(buf, &Font_7x10, SSD1306_COLOR_WHITE);

								SSD1306_GotoXY(68, 10);
								SSD1306_Puts("C7: ", &Font_7x10, SSD1306_COLOR_WHITE);
									sprintf(buf, "%0.3f", CELL7_VOLT);
								SSD1306_GotoXY(92, 10);
								SSD1306_Puts(buf, &Font_7x10, SSD1306_COLOR_WHITE);

								SSD1306_GotoXY(68, 20);
								SSD1306_Puts("C8: ", &Font_7x10, SSD1306_COLOR_WHITE);
									sprintf(buf, "%0.3f", CELL8_VOLT);
								SSD1306_GotoXY(92, 20);
								SSD1306_Puts(buf, &Font_7x10, SSD1306_COLOR_WHITE);

								SSD1306_GotoXY(68, 30);
								SSD1306_Puts("C9: ", &Font_7x10, SSD1306_COLOR_WHITE);
									sprintf(buf, "%0.3f", CELL9_VOLT);
								SSD1306_GotoXY(92, 30);
								SSD1306_Puts(buf, &Font_7x10, SSD1306_COLOR_WHITE);

								SSD1306_GotoXY(68, 40);
								SSD1306_Puts("V: ", &Font_7x10, SSD1306_COLOR_WHITE);
									sprintf(buf, "%0.2f", HV_VOLT);
								SSD1306_GotoXY(92, 40);
								SSD1306_Puts(buf, &Font_7x10, SSD1306_COLOR_WHITE);

				SSD1306_UpdateScreen();
				// Do not redraw if the menu item has not changed
				NeedUpdate = 0;
			}
			break;
		}
    }
}

void res_timer(int timer_num, int timer_type) {
	// Reset the specified timer based on the timer type
	if (timer_type == 1) {
		// Reset 1000ms timer
		if (timer_num >= 0 && timer_num < 10) {
			timer_1000ms[timer_num].res = false;
			timer_1000ms[timer_num].val = 0;
		}
	} else if (timer_type == 100) {
		// Reset 10000x100ms timer
		if (timer_num >= 0 && timer_num < 10) {
			timer_10000x100ms[timer_num].res = false;
			timer_10000x100ms[timer_num].val = 0;
		}
	}
}

void init_timer() {
	// Initialize timers TIM3 and TIM4
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4, ENABLE);

	TIM_TimeBaseInitTypeDef base_timer;
	TIM_TimeBaseStructInit(&base_timer);

	base_timer.TIM_Prescaler = (SystemCoreClock / 10000) - 1;

	// Configure TIM3 with a period of 10
	base_timer.TIM_Period = 10;
	base_timer.TIM_CounterMode = TIM_CounterMode_Up;
	TIM_TimeBaseInit(TIM3, &base_timer);

	// Configure TIM4 with a period of 1000
	base_timer.TIM_Period = 1000;
	base_timer.TIM_CounterMode = TIM_CounterMode_Up;
	TIM_TimeBaseInit(TIM4, &base_timer);

	// Enable TIM3 and TIM4 update interrupts
	TIM_ITConfig(TIM3, TIM_IT_Update, ENABLE);
	TIM_ITConfig(TIM4, TIM_IT_Update, ENABLE);

	// Enable TIM3 and TIM4
	TIM_Cmd(TIM3, ENABLE);
	TIM_Cmd(TIM4, ENABLE);

	// Configure NVIC for TIM3
	NVIC_InitTypeDef NVIC_InitStructure3;
	NVIC_InitStructure3.NVIC_IRQChannel = TIM3_IRQn;
	NVIC_InitStructure3.NVIC_IRQChannelPreemptionPriority = 1;
	NVIC_InitStructure3.NVIC_IRQChannelSubPriority = 1;
	NVIC_InitStructure3.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure3);

	// Configure NVIC for TIM4
	NVIC_InitTypeDef NVIC_InitStructure4;
	NVIC_InitStructure4.NVIC_IRQChannel = TIM4_IRQn;
	NVIC_InitStructure4.NVIC_IRQChannelPreemptionPriority = 1;
	NVIC_InitStructure4.NVIC_IRQChannelSubPriority = 0;
	NVIC_InitStructure4.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure4);
}

void TIM3_IRQHandler() {
	// Handle TIM3 interrupt
	if (TIM_GetITStatus(TIM3, TIM_IT_Update) != RESET) {
		TIM_ClearITPendingBit(TIM3, TIM_IT_Update);

		// Toggle the 1ms flag
		if (time_togle_1ms == 0) {
			time_togle_1ms = 1;
		} else {
			time_togle_1ms = 0;
		}
	}
}

void TIM4_IRQHandler() {
	// Handle TIM4 interrupt
	if (TIM_GetITStatus(TIM4, TIM_IT_Update) != RESET) {
		TIM_ClearITPendingBit(TIM4, TIM_IT_Update);

		// Toggle the 100ms flag
		if (time_togle_100ms == 0) {
			time_togle_100ms = 1;
		} else {
			time_togle_100ms = 0;
		}
	}
}
