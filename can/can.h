/*
  **********************************************************************************
  * @title		can.h
  * @platform	STM32F103
  * @author		Anton Novikov
  * @version	V1.0.0
  * @date		05.02.2021
  *
  * @brief		Header file for CAN communication configuration.
  *
  **********************************************************************************
*/

#ifndef __CAN
#define __CAN


/* Definition of CAN configuration */
//#define CAN1_ReMap			// Uncomment if port remapping is used

#ifndef CAN1_ReMap
	#define CAN1_GPIO_PORT			GPIOA
	#define CAN1_RX_SOURCE			GPIO_Pin_11				// RX port
	#define CAN1_TX_SOURCE			GPIO_Pin_12				// TX port
	#define CAN1_Periph				RCC_APB2Periph_GPIOA	// Peripheral port
#else
	#define CAN1_GPIO_PORT			GPIOB
	#define CAN1_RX_SOURCE			GPIO_Pin_8				// RX port
	#define CAN1_TX_SOURCE			GPIO_Pin_9				// TX port
	#define CAN1_Periph				RCC_APB2Periph_GPIOB	// Peripheral port
#endif

// How select the bus speed:
// 72/8=9 PRESCALE
// 36000000/500000=72 MHz/Kb
// 8=CAN_SJW_1tq + CAN_BS1_3tq + CAN_BS2_4tq (quantums from 1 to 5) = 500 Kb

// #define CAN1_SPEED_PRESCALE			4			// 1000 Kb
 #define CAN1_SPEED_PRESCALE			9			// 500 Kb
// #define CAN1_SPEED_PRESCALE			16			// 250 Kb
// #define CAN1_SPEED_PRESCALE			36			// 125 Kb
// #define CAN1_SPEED_PRESCALE			40			// 100 Kb
// #define CAN1_SPEED_PRESCALE			80			// 50 Kb


// Extended frame and control message bits
// IDE bit (Identifier Extension Bit)
#define CAN_IDE_32			0b00000100			// For 32-bit scale
#define CAN_IDE_16			0b00001000			// For 16-bit scale
// RTR bit (Remote Transmission Request)
#define CAN_RTR_32			0b00000010			// For 32-bit scale
#define CAN_RTR_16			0b00010000			// For 16-bit scale

/* Function prototypes */
//Initializes the CAN communication.
void init_CAN(void);

// Interrupt handler for CAN1 reception on FIFO0.
void USB_LP_CAN1_RX0_IRQHandler(void);

// Interrupt handler for CAN1 reception on FIFO1.
void CAN1_RX1_IRQHandler(void);

void request_MHB(int IdNumber);

extern int MHB_SOC_HV_HEX;
extern int MHB_CELL_NUM;
extern int MHB_CELL1_VOLT;
extern int MHB_CELL2_VOLT;
extern int MHB_CELL3_VOLT;
extern int MHB_CELL4_VOLT;
extern int MHB_CELL5_VOLT;
extern int MHB_CELL6_VOLT;
extern int MHB_CELL7_VOLT;
extern int MHB_CELL8_VOLT;
extern int MHB_CELL9_VOLT;
extern int MHB_HV_VOLT;
extern int MHB_DATE_HEX;

#endif //__CAN
