/*
  **********************************************************************************
  * @title		can.c
  * @platform	STM32F103
  * @author		Anton Novikov
  * @version	V1.0.0
  * @date		05.02.2021
  *
  * @brief		Sending/receiving data over the CAN bus.
  *
  **********************************************************************************
*/

#include "stm32f10x.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_can.h"

#include "misc.h"
#include "can.h"
#include "ssd1306.h"

#include <stdio.h>
#include <stdlib.h>

// Monitoring Hybrid Battery (MHB)
const uint16_t MHB_HV = 0x72B;

int MHB_SOC_HV_HEX = 0;              // State of Charge in hexadecimal format
int MHB_CELL_NUM = 0;                // Number of the current monitored battery cell
int MHB_CELL1_VOLT = 0;              // Voltage of Battery Cell 1
int MHB_CELL2_VOLT = 0;              // Voltage of Battery Cell 2
int MHB_CELL3_VOLT = 0;              // Voltage of Battery Cell 3
int MHB_CELL4_VOLT = 0;              // Voltage of Battery Cell 4
int MHB_CELL5_VOLT = 0;              // Voltage of Battery Cell 5
int MHB_CELL6_VOLT = 0;              // Voltage of Battery Cell 6
int MHB_CELL7_VOLT = 0;              // Voltage of Battery Cell 7
int MHB_CELL8_VOLT = 0;              // Voltage of Battery Cell 8
int MHB_CELL9_VOLT = 0;              // Voltage of Battery Cell 9
int MHB_HV_VOLT = 0;                 // High Voltage of the Hybrid Battery
int MHB_DATE_PROD = 0;               // Date in hexadecimal format

/* This function is designed for configuring I/O ports for STM32F103 series microcontrollers. */
void init_CAN(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	/* CAN GPIOs configuration */
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE); 		// включаем тактирование AFIO
	RCC_APB2PeriphClockCmd(CAN1_Periph, ENABLE); 				// включаем тактирование порта

	RCC_APB1PeriphClockCmd(RCC_APB1Periph_CAN1, ENABLE);		// включаем тактирование CAN-шины

	// Configure CAN RX pin
	GPIO_InitStructure.GPIO_Pin   = CAN1_RX_SOURCE;
	GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_IPU;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(CAN1_GPIO_PORT, &GPIO_InitStructure);

	// Configure CAN TX pin
	GPIO_InitStructure.GPIO_Pin   = CAN1_TX_SOURCE;
	GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(CAN1_GPIO_PORT, &GPIO_InitStructure);

	#ifdef CAN1_ReMap
		GPIO_PinRemapConfig(GPIO_Remap1_CAN1, ENABLE);			// Переносим Can1 на PB8, PB9
	#endif

	// Initialize CAN bus
	CAN_InitTypeDef CAN_InitStructure;

	CAN_DeInit( CAN1);
	CAN_StructInit(&CAN_InitStructure);

	// CAN cell initialization
	CAN_InitStructure.CAN_TTCM = DISABLE;
	CAN_InitStructure.CAN_ABOM = DISABLE;
	CAN_InitStructure.CAN_AWUM = DISABLE;
	CAN_InitStructure.CAN_NART = ENABLE;
	CAN_InitStructure.CAN_RFLM = DISABLE;
	CAN_InitStructure.CAN_TXFP = DISABLE;
	CAN_InitStructure.CAN_Mode = CAN_Mode_Normal;				// Normal mode of operation
//	CAN_InitStructure.CAN_Mode = CAN_Mode_LoopBack;				// For testing without connected bus devices
	CAN_InitStructure.CAN_SJW = CAN_SJW_1tq;
	// 4tq - 1000 Kb; 3tq - 500 Kb; 4tq - 250 Kb; 3tq - 125 Kb; 4tq - 100 Kb; 4tq - 50 Kb;
	CAN_InitStructure.CAN_BS1 = CAN_BS1_3tq;
	// 4tq - 1000 Kb; 4tq - 500 Kb; 4tq - 250 Kb; 4tq - 125 Kb; 4tq - 100 Kb; 4tq - 50 Kb;
	CAN_InitStructure.CAN_BS2 = CAN_BS2_4tq;
	CAN_InitStructure.CAN_Prescaler = CAN1_SPEED_PRESCALE;		// Choose the required speed from can.h
	CAN_Init(CAN1, &CAN_InitStructure);

	// CAN filter initialization
	// When configuring filters, pay attention to the filter numbers, otherwise you may overwrite them
	CAN_FilterInitTypeDef CAN_FilterInitStructure;

    // Two filter blocks
    // Filter that passes all messages according to the mask for a standard frame (scaling by 32 bits)
	CAN_FilterInitStructure.CAN_FilterNumber = 0;							// Filter number, available from 0 to 13
	CAN_FilterInitStructure.CAN_FilterMode = CAN_FilterMode_IdMask;      	// Filter mode, Mask
	CAN_FilterInitStructure.CAN_FilterScale = CAN_FilterScale_32bit;    	// Scaling
	CAN_FilterInitStructure.CAN_FilterIdHigh = MHB_HV<<5;       	 	// High part of the filter
	CAN_FilterInitStructure.CAN_FilterIdLow = 0x0000;                   	// Low part of the filter
	CAN_FilterInitStructure.CAN_FilterMaskIdHigh = MHB_HV<<5;   		// High part of the mask
	CAN_FilterInitStructure.CAN_FilterMaskIdLow = 0x0000;               	// Low part of the mask
	CAN_FilterInitStructure.CAN_FilterFIFOAssignment = CAN_FIFO0;       	// FIFO buffer number (we have only two)
	CAN_FilterInitStructure.CAN_FilterActivation = ENABLE;              	// Filter activation
	CAN_FilterInit(&CAN_FilterInitStructure);

    // NVIC Configuration
    // Setting up an interrupt for handling FIFO0 buffer
    // Enable CAN1 RX0 interrupt IRQ channel
	NVIC_InitTypeDef NVIC_InitStructure;
	NVIC_InitStructure.NVIC_IRQChannel = USB_LP_CAN1_RX0_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);
	// Enable CAN FIFO0 message pending interrupt
	CAN_ITConfig(CAN1, CAN_IT_FMP0, ENABLE);

	// Setting up an interrupt for handling FIFO1 buffer
	NVIC_InitStructure.NVIC_IRQChannel = CAN1_RX1_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);

	// Enable CAN FIFO1 message pending interrupt
	CAN_ITConfig(CAN1, CAN_IT_FMP1, ENABLE);
}

/* CAN bus interrupt handler for buffer FIFO0 */
void USB_LP_CAN1_RX0_IRQHandler(void)
{
	// Create a variable RxMessage of type CanRxMsg to store received data.
	CanRxMsg RxMessage;

	// Initialize the values of RxMessage fields before receiving data.
	RxMessage.DLC = 	0x00;
	RxMessage.ExtId = 	0x00;
	RxMessage.FMI = 	0x00;
	RxMessage.IDE = 	0x00;
	RxMessage.RTR = 	0x00;
	RxMessage.StdId = 	0x00;
	RxMessage.Data [0] = 0x00;
	RxMessage.Data [1] = 0x00;
	RxMessage.Data [2] = 0x00;
	RxMessage.Data [3] = 0x00;
	RxMessage.Data [4] = 0x00;
	RxMessage.Data [5] = 0x00;
	RxMessage.Data [6] = 0x00;
	RxMessage.Data [7] = 0x00;

	// Check for the presence of CAN1 FIFO0 interrupt.
	if (CAN_GetITStatus(CAN1, CAN_IT_FMP0) != RESET)
	{
		// Get actual reception of data from FIFO0.
		CAN_Receive(CAN1, CAN_FIFO0, &RxMessage);

		// Check if the received frame is an standard frame.
		// But there should not be any others, because we have configured the reception of a standard frame into the FIFO0 buffer.
		if (RxMessage.IDE == CAN_Id_Standard)
		{
			if (RxMessage.StdId == MHB_HV)
			{
				if (RxMessage.Data[3] == 0x14) {
				MHB_SOC_HV_HEX = RxMessage.Data[5] | RxMessage.Data[4] << 8;
				}
				if ((RxMessage.Data[3] >= 0x00) && (RxMessage.Data[3] <= 0x09)) {
					MHB_CELL_NUM =  RxMessage.Data[3];
					if (MHB_CELL_NUM == 0x00) {
						MHB_CELL1_VOLT = RxMessage.Data[5] | RxMessage.Data[4] << 8;
					}
					if (MHB_CELL_NUM == 0x01) {
						MHB_CELL2_VOLT = RxMessage.Data[5] | RxMessage.Data[4] << 8;
					}
					if (MHB_CELL_NUM == 0x02) {
						MHB_CELL3_VOLT = RxMessage.Data[5] | RxMessage.Data[4] << 8;
					}
					if (MHB_CELL_NUM == 0x03) {
						MHB_CELL4_VOLT = RxMessage.Data[5] | RxMessage.Data[4] << 8;
					}
					if (MHB_CELL_NUM == 0x04) {
						MHB_CELL5_VOLT = RxMessage.Data[5] | RxMessage.Data[4] << 8;
					}
					if (MHB_CELL_NUM == 0x05) {
						MHB_CELL6_VOLT = RxMessage.Data[5] | RxMessage.Data[4] << 8;
					}
					if (MHB_CELL_NUM == 0x06) {
						MHB_CELL7_VOLT = RxMessage.Data[5] | RxMessage.Data[4] << 8;
					}
					if (MHB_CELL_NUM == 0x07) {
						MHB_CELL8_VOLT = RxMessage.Data[5] | RxMessage.Data[4] << 8;
					}
					if (MHB_CELL_NUM == 0x08) {
						MHB_CELL9_VOLT = RxMessage.Data[5] | RxMessage.Data[4] << 8;
					}
					if (MHB_CELL_NUM == 0x09) {
						MHB_HV_VOLT = RxMessage.Data[5] | RxMessage.Data[4] << 8;
					}
				}
				if (RxMessage.Data[3] == 0x11) {
					MHB_DATE_PROD = RxMessage.Data[7] | RxMessage.Data[6] | RxMessage.Data[5] | RxMessage.Data[4];
				}
			}
		}
	}
}

/* CAN bus interrupt handler for buffer FIFO1 */
void CAN1_RX1_IRQHandler(void)
{
	// Create a variable RxMessage of type CanRxMsg to store received data.
	CanRxMsg RxMessage;

	// Initialize the values of RxMessage fields before receiving data.
	RxMessage.DLC = 	0x00;
	RxMessage.ExtId = 	0x00;
	RxMessage.FMI = 	0x00;
	RxMessage.IDE = 	0x00;
	RxMessage.RTR = 	0x00;
	RxMessage.StdId = 	0x00;
	RxMessage.Data [0] = 0x00;
	RxMessage.Data [1] = 0x00;
	RxMessage.Data [2] = 0x00;
	RxMessage.Data [3] = 0x00;
	RxMessage.Data [4] = 0x00;
	RxMessage.Data [5] = 0x00;
	RxMessage.Data [6] = 0x00;
	RxMessage.Data [7] = 0x00;

	// Check for the presence of CAN1 FIFO1 interrupt.
	if (CAN_GetITStatus(CAN1, CAN_IT_FMP1) != RESET)
	{
		// Get actual reception of data from FIFO1.
		CAN_Receive(CAN1, CAN_FIFO1, &RxMessage);

		// Check if the received frame is an extended frame.
		if (RxMessage.IDE == CAN_Id_Extended)
		{
			// Additional actions that can be performed if the frame is extended.
		}
	}
}
// Battery cell voltage request packet.
void request_MHB(int IdNumber)
{
	CanTxMsg TxMessage;
	TxMessage.StdId = 0x723;
	TxMessage.ExtId = 0x00;

	TxMessage.IDE = CAN_Id_Standard;
	TxMessage.RTR = CAN_RTR_DATA;
	TxMessage.DLC = 8;

	TxMessage.Data[0] = 0x03;
	TxMessage.Data[1] = 0x22;
	TxMessage.Data[2] = 0xD9;
	TxMessage.Data[3] = IdNumber;
	TxMessage.Data[4] = 0x00;
	TxMessage.Data[5] = 0x00;
	TxMessage.Data[6] = 0x00;
	TxMessage.Data[7] = 0x00;

	CAN_Transmit(CAN1, &TxMessage);
}
