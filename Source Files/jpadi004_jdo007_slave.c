/*
 * jpadi004_jdo007_slave.c
 *
 * Created: 12/5/2014 12:30:36 PM
 *  Author: Johnny Do
 */ 

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include "usart_ATmega1284.h"
#include "io.h"
#include "lcd.h"
#include "scheduler.h"

//USART constants and variables for system
const unsigned char USER1 = 0x03;
const unsigned char USER2 = 0x04;
const unsigned char ORIGINALPERIOD = 2000;
const unsigned short LARGEPERIOD = 4000;
const unsigned char RESETSIGNAL = 0xFF;
const unsigned char GOODSEND = 0xFE;
const unsigned char BADSEND = 0xFC;
const unsigned char REQUESTSEND = 0xC0;
const unsigned char CLOSESIGNAL = 0x00;
const unsigned char DOOROPENED = 0x01;
const unsigned char DUMMYIDNUMBER = 0x69;

//Current user
signed char currentUser = -1;

//USART flags
unsigned char inputReceived = 0;
unsigned char verificationReceived = 0;
unsigned char sentBack = 0;
unsigned char dataOkay = 0;
unsigned char requestResend = 0;

//USART data
unsigned char dataReceived = 0;
unsigned char verificationData = 0;
unsigned char openedCounter = 0;

//States to be made into vectors to support more users
unsigned char opening = 0;
unsigned char opened = 0;
unsigned char waitingclosedSignal = 0;
unsigned char closed = 0;
unsigned char dataReady = 0;

//Data functions
void resetFlagsAndVariables();
void resetData();

//Stepper Motor Flags
unsigned char hasSpun = 0;

//Stepper Global Variables
unsigned short PHASECOUNTER = 2048;
unsigned short distanceToSpin = 0;
unsigned char currentStep = 0;
unsigned char autoCloseCounter = 0;

//Stepper Control
void stepper1_CW();
void stepper2_CW();
void stepper1_CCW();
void stepper2_CCW();

//Tick functions of USART and gates
task usartTick, gateTick;
enum usartStates{usartInit, usartUserWait, usartUserSet, usartGateOpen, usartGateSend, usartWaitClose, usartGateClose}usartState = -1;

//Tick function headers
usartTickFunction(int state);
gateTickFunction(int state);

//Function for Sonar
int distanceDetect();

int main(void)
{
	DDRA = 0xFF; PORTA = 0x00;
	DDRB = 0x04; PORTB = 0xFB;
	DDRC = 0xFF; PORTC = 0x00;
	DDRD = 0xE0; PORTD = 0x1F;
	
	LCD_init();
	initUSART(0);
	LCD_ClearScreen();

	tasksNum = 1; // declare number of tasks
	task tsks[tasksNum]; // initialize the task array
	tasks = tsks; // set the task array
	
	// define tasks
	unsigned char i=0; // task counter
	
	tasks[i].state = usartState;
	tasks[i].period = 2000;
	tasks[i].elapsedTime = tasks[i].period;
	tasks[i].TickFct = &usartTickFunction;
	
	TimerSet(ORIGINALPERIOD);
	TimerOn();
	
	while(1);		
}
	
//Reset of all Variables and Flags
void resetFlagsAndVariables()
{
	//Current User
	currentUser = -1;

	//USART Flags
	inputReceived = 0;
	verificationReceived = 0;
	requestResend = 0;
	sentBack = 0;
	dataOkay = 0;

	//USART Data
	dataReceived = -1;
	verificationData = -1;
	openedCounter = 0;
	
	//States to be made into vectors to support more users
	opening = 0;
	opened = 0;
	waitingclosedSignal = 0;
	closed = 0;
	dataReady = 0;
	autoCloseCounter = 0;
	//USART_Flush(0);
	resetData();
}

//Reset of dataReceived data and dataOkay flag
void resetData()
{
	dataOkay = 0;
	dataReceived = -1;
	verificationData = -1;
	sentBack = 0;
	hasSpun = 0;
}

//Distance detection
int distanceDetect()
{
	short counter = 0;
	PORTB = 0x00;
	delay_ms(1);
	PORTB = 0xFF;
	_delay_us(10);
	PORTB = 0x00;
	
	for(unsigned short distance = 0; distance <16000; distance++)
	{
		if(PINB & 0x02)
		{
			counter++; 
		}
		
	}
	return (counter / 100); // Divided by 100 = 10cm markers
}

//USART tick
int usartTickFunction(int state)
{
	if((PINB & 0x01) != 0)
	{
		state = -1;
	}
	
	switch(state)
	{
		case -1:
		state = usartInit;
		break;
		
		case usartInit:
		state = usartUserWait;
		break; 
		
		case usartUserWait:
		if(currentUser == USER1 || currentUser == USER2){state = usartUserSet;}
		break; 
		
		case usartUserSet:
		state = usartGateOpen;
		break; 
		
		case usartGateOpen:
		if(hasSpun != 0){state = usartGateSend;}
		break; 
		
		case usartGateSend:
		if(openedCounter < 1){state = usartGateSend;}
		else{state = usartWaitClose;}
		break;
		
		case usartWaitClose:
		if(closed){state = usartGateClose;}
		if(autoCloseCounter >= 5){state = usartGateClose;}
		break; 
		
		case usartGateClose:
		if(hasSpun != 0){state = -1;}
		break; 
		
		default:
		break;
	}
	
	switch(state)
	{
		case -1:
		state = usartInit;
		LCD_DisplayString(1, "-1");
		resetFlagsAndVariables();
		break;
		
		case usartInit:
		resetFlagsAndVariables();
		LCD_DisplayString(1, "INIT");
		break;
		
		case usartUserWait:
		LCD_DisplayString(1, "WAIT");
		if(USART_HasReceived(0))
		{
			dataReceived = USART_Receive(0);
			
			if((dataReceived ^ USER1) == 0)
			{
				currentUser = USER1;
			}
			else if((dataReceived ^ USER2) == 0)
			{
				currentUser = USER2;
			}
			//USART_Flush(0);
		}
		break;
		
		case usartUserSet:
		LCD_DisplayString(1, "USER SET");
		currentUser = dataReceived;
		resetData();
		break;
		
		case usartGateOpen:
		TimerSet(LARGEPERIOD);
		LCD_DisplayString(1, "OPEN");
		if(currentUser == USER1)
		{
			stepper1_CW();
		}
		else if(currentUser == USER2)
		{
			stepper2_CCW();
		}
		TimerSet(ORIGINALPERIOD);
		break;
		
		case usartGateSend:
		LCD_DisplayString(1, "OPENED SIGNAL   SENT BACK");
		if(USART_IsSendReady(0))
		{
			USART_Send(DOOROPENED, 0);
			openedCounter++;
			resetData();
			//USART_Flush(0);
		}
		break;
		
		case usartWaitClose:
		LCD_DisplayString(1, "Wait for closingSignal");
		if(distanceDetect() > 1){autoCloseCounter++;}
		else{autoCloseCounter = 0;}
			
		if(USART_HasReceived(0))
		{
			dataReceived = USART_Receive(0);

			if((dataReceived ^ CLOSESIGNAL) == 0)
			{
				closed = 1;
			}
		}
		break;
		
		case usartGateClose:
		TimerSet(LARGEPERIOD);
		if(autoCloseCounter >= 5)
		{
			if(USART_IsSendReady(0))
			{
				USART_Send(CLOSESIGNAL, 0);
				closed = 1;
			}	
		}
		if(currentUser == USER1 && closed)
		{
			LCD_DisplayString(1, "CLOSING");
			stepper1_CCW();
		}
		else if(currentUser == USER2 && closed)
		{
			LCD_DisplayString(1, "CLOSING");
			stepper2_CW();
		}		
		TimerSet(ORIGINALPERIOD);
		break;
				
		default:
		break;
	}
	return state;
}

void stepper1_CW()
{
	unsigned char steps[] = {0x01, 0x03, 0x02, 0x06, 0x04, 0x0C, 0x08 , 0x09}; // 7 last index
	signed short phase_counter = 2048;
	signed char step_index = 0;
	while(phase_counter > 0)
	{
		phase_counter--;
		step_index++; //next step
		if(step_index > 7)//if at last step return to start
		{
			step_index = 0;

		}
		PORTA = steps[step_index];
		delay_ms(1);
	}
	hasSpun = 1;
}

void stepper2_CW()
{
	unsigned char steps[] = {0x10, 0x30, 0x20, 0x60, 0x40, 0xC0, 0x80 , 0x90}; // 7 last index
	signed short phase_counter = 2048;
	signed char step_index = 0;
	while(phase_counter > 0)
	{
		phase_counter--;
		step_index--; //next step
		if(step_index < 0)//if at first step return to end
		{
			step_index = 7;
		}

		PORTA = steps[step_index];
		delay_ms(1);
	}
	hasSpun = 1;
}

void stepper1_CCW()
{
	unsigned char steps[] = {0x01, 0x03, 0x02, 0x06, 0x04, 0x0C, 0x08 , 0x09}; // 7 last index
	signed short phase_counter = 2048;
	signed char step_index = 7;
	while(phase_counter > 0)
	{
		phase_counter--;
		step_index--; //next step
		if(step_index < 0)//if at first step return to end
		{
			step_index = 7;
		}

		PORTA = steps[step_index];
		delay_ms(1);
	}
	hasSpun = 1;
}

void stepper2_CCW()
{
	unsigned char steps[] = {0x10, 0x30, 0x20, 0x60, 0x40, 0xC0, 0x80 , 0x90}; // 7 last index
	signed short phase_counter = 2048;
	signed char step_index = 7;
	while(phase_counter > 0)
	{
		phase_counter--;
		step_index++; //next step
		if(step_index > 7)//if at last step return to start
		{
			step_index = 0;
		}
		PORTA = steps[step_index];
		delay_ms(1);
	}
	hasSpun = 1;
}
