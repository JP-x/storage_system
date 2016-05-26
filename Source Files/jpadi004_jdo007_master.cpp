/*
 * jpadi004_jdo007_master.cpp
 *
 * Created: 12/5/2014 12:13:40 PM
 *  Author: JP
 */ 


#include <avr/io.h>
#include <avr/interrupt.h>
#include "usart_ATmega1284.h"
#include "lcd.h"
#include "scheduler.h"
#include "io.h"
#include <stdio.h>
//00 0110 0000
unsigned short threshold = 128;
void A2D_init() 
{
		ADCSRA |= (1 << ADEN) | (1 << ADSC) | (1 << ADATE);
		// ADEN: Enables analog-to-digital conversion
		// ADSC: Starts analog-to-digital conversion
		// ADATE: Enables auto-triggering, allowing for constant
		//	    analog to digital conversions.
}

unsigned short input;// = ADC; // short is required to store all
// 10 bits.
//input = ADC;

unsigned char buttons[16];
void getControllerData()
{
	unsigned char cCopy;     //Copy of the current input via controller
	int i = 0;               //Index
	PORTC = 0x03;            //Setting the clock and the latch to high. Sets value of buttons to Shift Register
	for(i = 0; i < 16; i++)  //For loop gathering data
	{
		PORTC = 0x01;                     //Setting clock to high in order to shift data over one spot.
		cCopy = ((~PINC) >> 2) & 0x01;    //Buttons have pull-up resistors, so logical NOT values and takes data from PINC 2
		buttons[i] = cCopy;             //Saving values to global vector
		//Saving values to global vector
		PORTC = 0x00;                     //Setting everything to low so that rising edge can trigger shift
	}
	PORTC = 0x01;                         //Setting clock to high when not in use

};

task lcdtsk, controllertsk, usart;

//Controller flags
unsigned char reset_flag = 0;
unsigned char l_flag = 0;
unsigned char r_flag = 0;
unsigned char start_flag = 0;
unsigned char a_flag = 0;
unsigned char received_flag = 0;
unsigned char data_received = 0;
unsigned char close_sent = 0;
unsigned char user_sent = 0;
unsigned char auto_close = 0;

unsigned char response_counter = 0;

//constants, used for USART
const signed char USER1 = 0x03;
const signed char USER2 = 0x04;
const unsigned char CLOSESIGNAL = 0x00;
const unsigned char OPENSIGNAL = 0x01;

//Settings and User Data
signed char currentUser = -1;

enum lcdStates{init, waitdetect, detected, showstart, welcome, show1, show2, senduser, didsend ,waitopen, waitsendclose, checkclosed, reset } lcdState;

//Controls both LCD and messages sent over USART
int lcdTick(int state)
{
	input = ADC;//take input from motion sensor A2D
	switch(state)
	{
		case init:
		state = waitdetect;
		break;
		
		case reset:
		state = init;
		break;
		
		case waitdetect:
		if(input > threshold)//threshold, value when no motion detected
		{
		   state = detected;	
		}
		else
		{
			state = waitdetect;
		}
		break;
		
		case detected:
		state = showstart;
		break;
		
		case showstart://start splash screen
		if(a_flag || start_flag)//a_button pressed or start button pressed
		{
			state = welcome;
			a_flag = 0;//reset a button flag
			start_flag = 0;
			reset_flag = 0;
		}
		else
		{
			state = showstart;
		}
		break;
		
		case welcome://just show welcome splash screen
		state = show1;
		break;
		
		case show1:
		if(reset_flag)//go to reset if select ever pressed
		{
			state = reset;
			reset_flag = 0;
		}
		 else if(l_flag || r_flag)//left or right pressed show next message
		{
			state = show2;
			l_flag = 0;//reset flags on left and right
			r_flag = 0;
		}
		else if(a_flag)
		{
			currentUser = USER1;
			a_flag = 0;
			state = senduser;
		}
		else
		{
			state = show1;//nothing pressed stay in show1
		}
		break;
		
		case show2:
		if(reset_flag)//go to reset if select ever pressed
		{
			state = reset;
			reset_flag = 0;
		}
		else if(l_flag || r_flag)//left or right pressed show next message
		{
			state = show1;
			l_flag = 0;//reset flags on left and right
			r_flag = 0;
		}
		else if(a_flag)
		{
			currentUser = USER2;
			a_flag = 0;
			state = senduser;
		}
		else
		{
			state = show2;//nothing pressed stay in show2
		}
		break;
		
		case senduser:
		if(user_sent)//value set to 1 after transmission over USART
		{
			state = didsend;
		}
		else
		{
			state = senduser;
			user_sent = 0;
		}
		break;
		
		case didsend:
		if(USART_HasTransmitted(0))
		{
			state = waitopen;
		}
		else
		{
			state = senduser;
			user_sent = 0;
		}
		break;
		
		case waitopen:
		if(data_received == OPENSIGNAL) //receive signal via USART
		{
			state = waitsendclose;
		}
		else if(response_counter >= 5)//if no value received over 5 seconds send user again
		{
			state = senduser;
			user_sent = 0;
			response_counter = 0;
			data_received = 0;
		}
		else
		{
			state = waitopen;
		}
		
		break;
		
		case waitsendclose:
		if(close_sent)
		{
			state = checkclosed;
			start_flag = 0;
		}
		else if(auto_close)
		{
			state = checkclosed;
		}
		else
		{
		   state = waitsendclose;
		}
		break;
		
		case checkclosed:
		if(!close_sent && !auto_close)
		{
			response_counter++;
			close_sent = 0;
			state = checkclosed;
		}
		else if(close_sent)
		{
			state = reset;
		}
		else if(auto_close)
		{
			state = reset;
		}
		else if(response_counter >= 10)
		{
			state = waitsendclose;
			response_counter = 0;
		}
		else
		{
		  response_counter++;	
		}
		break;
		
		default:
		state = init;
		break;
	}

	switch(state)//ACTIONS
	{
		case init:
		LCD_DisplayString(1,"Initializing...");
		LCD_ClearScreen();
		reset_flag = 0;
		l_flag = 0;
		r_flag = 0;
		start_flag = 0;
		a_flag = 0;
		received_flag = 0;
		data_received = 0;
		currentUser = -1;
		close_sent = 0;
		user_sent = 0;
		response_counter = 0;
		auto_close = 0;
		break;
		
		case waitdetect:
		LCD_DisplayString(1,"Waiting for     Movement");
		break;
				
		case detected:
		LCD_DisplayString(1,"Movement        Detected.");
		break;
		
		case showstart:
		LCD_DisplayString(1,">>GAMERS CACHE<<  PRESS START");
		break;
		
		case welcome:
		LCD_DisplayString(1,"Welcome!");
		break;
		
		case show1:
		LCD_DisplayString(1,"<-/-> to NAV     OPEN BOX1? A");
		break;
		
		case show2:
		LCD_DisplayString(1,"<-/-> to NAV     OPEN BOX2? A");	
		break;
		
		case reset:
		LCD_DisplayString(1,"Goodbye!        Thank you!");
		break;
	
		case senduser:
		if(USART_IsSendReady(0))//send when ready
		{
			USART_Send(currentUser,0);
			user_sent = USART_HasTransmitted(0);
		}
		else
		{
			user_sent = 0;
		}
		break;
		
		case didsend:
		LCD_DisplayString(1, "Checking if sent...");
		break;
		
		case waitopen:
		if(USART_HasReceived(0))
		{
			data_received = USART_Receive(0);
			LCD_DisplayString(1,"Received open   signal...");
		}
		else
		{
			LCD_DisplayString(1,"Waiting to open...");
			response_counter++;//no response, add to counter
		}
		break;
		
		case waitsendclose:
		LCD_DisplayString(1,"Press START  to close box.");
		
		if(start_flag && USART_IsSendReady(0))//start button pressed and can send data
		{
			LCD_DisplayString(1,"Closing Cache!");
			USART_Send(CLOSESIGNAL,0);
			close_sent = USART_HasTransmitted(0);
			auto_close = 0;
		}
		else if(USART_HasReceived(0))
		{
			data_received = USART_Receive(0);
			if(data_received == CLOSESIGNAL)
			{
				auto_close = 1;
			}
			else
			{
				auto_close = 0;
			}
		}
		else
		{
			close_sent = 0;
		}
		break;
		
		case checkclosed:
		if(!auto_close)
		{
			LCD_DisplayString(1,"Checking if received data..");	
		}
		else if(auto_close)
		{
			LCD_DisplayString(1,"No activity,    closing gates...");
			auto_close = 1;
		}
		break;

		default:
		//do nothing
		break;
	}
	return state;
}


enum control_states {initc,control_wait};
//state machine calls on getControllerData function to receive input from controller
//getControllerData modifies the buttons vector
//after certain buttons are pressed
int controllerTick(int state)
{
	/*
	[0]	B
	1	Y
	2	Select
	3	Start
	4	Up on joypad
	5	Down on joypad
	6	Left on joypad
	7	Right on joypad
	8	A
	9	X
	10	L
	11	R
	*/
	getControllerData();
	// === Transitions === //////////////////////
	switch (state)
	{
		case initc:
		state = control_wait;
		break;
		
		case control_wait:
		if(buttons[3])//SELECT
		{
			reset_flag = 1;
		}
		
		if(buttons[4])//START
		{
			start_flag = 1;
		}
		
		if(buttons[7])//left
		{
			l_flag = 1;
		}
		
		if(buttons[8])//right
		{
			r_flag = 1;
		}
		
		if(buttons[9])//A
		{
			a_flag = 1;
		}
		
		state = control_wait;
		break;
		
		default:
		state = initc;
		break;
	}
	//actions
	switch (state)
	{
		case init:
		break;
		
		case control_wait:
		break;
		
		default:
		state = initc;
		break;
	}
	
	return state;
}

int main(void)
{
	DDRB = 0xFF; PORTB = 0x00; //output
	DDRA = 0x00; PORTA = 0xFF; //input
	DDRC = 0x03; PORTC = 0xFC; //input/output
	DDRD = 0xFF; PORTD = 0x00; //output
	
	//initialize components
	A2D_init();
	initUSART(0);
	LCD_init();
	LCD_DisplayString(1,"Turning on...");
	
	tasksNum = 2; // declare number of tasks
	task tsks[2]; // initialize the task array
	tsks[0] = lcdtsk;
	tasks = tsks; // set the task array
	
	// define tasks
	unsigned char i=0; // task counter
	tasks[i].state = -1;
	tasks[i].period = 1000;
	tasks[i].elapsedTime = tasks[i].period;
	tasks[i].TickFct = &lcdTick;
    i++;
	tsks[1] = controllertsk;
	tasks[i].state = initc;
	tasks[i].period = 100;
	tasks[i].elapsedTime = tasks[i].period;
	tasks[i].TickFct = &controllerTick;


	TimerSet(100);
	TimerOn();

	while(1);
}