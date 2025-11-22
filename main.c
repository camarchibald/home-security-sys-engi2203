/*
 * Master-1.c
 *
 * Created: 2023-03-15 3:16:55 PM
 * Author : Cameron Archibald
 * 
 */ 

#define F_CPU 16000000UL
#include <avr/io.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <util/delay.h>

#define RS PORTC2
#define RW PORTC3
#define CE PORTC4
#define DB4 PORTD4
#define DB5 PORTD5
#define DB6 PORTD6
#define DB7 PORTD7

//Global variables
volatile int row;
volatile int col;

void init_hardware(void);

//Keypad prototypes
char get_button(void);
char get_new_button(void);
void set_row_low(int row);
int col_pushed(void);
int initiate_keypad(void);

//Sensor prototypes
int hall(void);
int pir(void);

//LCD prototypes
void LCD_init (void);
void LCD_command (char command);
void LCD_command_4bit (char command);
void LCD_Char (char AsciiChar);
void LCD_Send_A_String(char *StringOfCharacters);
void LCD_clearScreen (void);
void LCD_home(void);
void LCD_display(void);
void LCD_noDisplay(void);
void printcode(char b);

//state prototypes
void armed(void);
void intruder(void);
void disarmed(void);


int main(void)
{
	init_uart();
	init_hardware();
	
	while (1)
	{
		armed();
		intruder();
		disarmed();
	}
	return 0;
}

void init_hardware(void) {
	
	//Keypad:
		UCSR0B = ~(1<<RXEN0) | ~(1<<TXEN0); //Enable pins D0 and D1, D0 = RX, D1 = TX
		//Row outputs, set to low initially
		DDRB |= 0b00001111; //B0-3
		PORTB &= 0b11110000; 
		
		//Column inputs, enable pull-up resistors
		DDRD &= 0b11111010; DDRB &= 0b11101111;//D0,2, B4    
		PORTD |= 0b00000101; PORTB |= 0b00010000;
		
	//Hall effect/PIR:
		DDRC &= 0b11011111;//C5 (A5) input, pull-up resistors enabled
		PORTC |= 0b00100000;
		
	//LED:
		DDRB |= 0b00100000; DDRC |= 0b00000011;//B5 green, C0 (A0) red, C1 (A1) yellow, set to low initially
		PORTB &= 0b11011111; PORTC &= 0b11111100;
	
	//LCD:
		DDRC |= 0b00011100;// Control pins C2-4
		DDRD |= 0b11110000;// Data pins D4-7
		PORTD &= 0b00001111;// Initialize data pins to zero
		LCD_init();
		LCD_command(0x0E); //Display on, cursor on (0x0C for display ON, cursor Off)
		LCD_command(0x06);
		LCD_Send_A_String("Initializing...");

	//Siren:
		TCCR2A = (1<<COM2A1) | ~(1<<COM2A0) | (1<<COM2B1) | ~(1<<COM2B0) | (1<<WGM21) | (1<<WGM20); //Set timer counters
		TCCR2B = (1<<WGM22) | (1<<CS22) | ~(1<<CS21) | (1<<CS20);
		DDRD |= 0b00001000;//D3 output
		_delay_ms(1);//Delay to initialize the PIR sensor
		LCD_clearScreen();
		
	printf("Hardware initialized successfully, built %s on %s\n ", __TIME__,__DATE__);
}

//Keypad
int initiate_keypad(void) {
	char b=0;
	char last_button=0;
	char pin[10]; //string as an array of chars
	char password[]="1234";
	int i=0;

	printf("Enter a 4 digit PIN (# to backup, * to erase whole thing): ");


	while (1)
	{
		b=get_new_button();
		
		//Clear a digit
		if (b== '#')
		{
			//use the backspace character to backspace the replace the character with a backspace
			printf(" Previous char deleted ");
			LCD_Send_A_String("DEL ");
			pin[i] = '\b';
			i--;
			continue; //breaks one iteration
		}
		//Clear all digits
		if (b== '*')
		{
			for(int n = i;n >= 0;n--) {
				pin[i] = '\b';
			}
			printf(" All characters deleted ");
			LCD_Send_A_String("CLR ");
			i = 0;
			continue;//breaks one iteration
		}

		if (b)
		{
			pin[i] = b; //Store value pin array to "b"
			printcode(b);
			printf("%c",b);
			i++;
		}
		if (i>=4)
		{
			pin[4]='\0'; //Terminate the string with a null terminator...that makes it a string.
			if (strcmp(pin,password)) //need to use string compare!
			{
				LCD_clearScreen();
				LCD_Send_A_String("Incorrect");
				LCD_command(0xC0);
				printf(" PIN Incorrect, try again\n"); //Print "PIN Incorrect, try again" to Putty;
				printf("Enter a 4 digit PIN (# to backup, * to erase whole thing):\n"); //Re-ask for password entry
				i=0;
			}
			else
			{
				printf(" PIN Correct\n"); //Print "PIN Correct" to Putty
				return 1;
				//printf("Enter a 4 digit PIN (# to backup, * to erase whole thing): "); //Re-ask for password entry
				//i=0;
			}
		}
	}
}
char get_button(void)
{
	int key_pressed = 0;
	char b;
	char buttons[4][3] = 
	{ {'1', '2', '3'},
	{'4', '5', '6'},
	{'7', '8', '9'},
	{'*', '0', '#'} };
	/*char buttoncode[4][3] = 
	{ {'0x31', '0x32', '0x33'},
	{'0x34', '0x35', '0x36'},
	{'0x37', '0x38', '0x39'},
	{'0x2A', '0x30', '0x23'} }; */

	//Check for button push
	//Cycle through the rows with the for loop
	for (row = 0; row <= 3; row++)
	{
		set_row_low(row);
		_delay_ms(20);

		col = col_pushed();

		if (col)
		{
			b = buttons[row][col - 1];
			//LCD_Send_A_String(b);
			//LCD_Char(buttoncode[row][col-1]);
			//printf("%d-%d: %c\n", row, col, b);
			_delay_ms(500);
			key_pressed = 1; //A key was pressed
		}
	}
	if (key_pressed)
	{
		return b;
	}
	else
	{
		return 0;
	}
}
char get_new_button(void)
{
	static char last_button=0;
	char b;
	
	b= get_button(); //Call get_button function
	
	//Check if we held button down, if yes, return 0
	if (b) {
		if (b != last_button) {

		}
		else if (last_button) {
			return 0;
		}
	}

	last_button=b;
	return b;
}
void set_row_low(int row)
{
	//Hi-Z the rows (make inputs without pull-ups)
	DDRB &= 0b11110000;
	PORTB &= 0b11110000;
	
	//Drive the specified row low
	switch (row)
	{
		case 0: //set Row 1 low
		PORTB &= ~(1 << PB3);
		DDRB |= (1 << PB3);
		break;
		case 1: //set Row 2 low
		PORTB &= ~(1 << PB2);
		DDRB |= (1 << PB2);
		break;
		case 2: //set Row 3 low
		PORTB &= ~(1 << PB1);
		DDRB |= (1 << PB1);
		break;
		case 3: //set Row 4 low
		PORTB &= ~(1 << PB0);
		DDRB |= (1 << PB0);
		break;
		default: printf("no row set\n");
	}
}
int col_pushed(void)
{
	//Return the column that was detected

	if ((PIND & (1 << PD2)) == 0) //check column 1
	{
		//printf("column= 1 \n");
		return 1;
	}
	else if ((PINB & (1 << PB4)) == 0) //check column 2
	{
		//printf("column= 2 \n");
		return 2;
	}
	else if ((PIND & (1 << PD0)) == 0) //check column 3
	{
		//printf("column= 3 \n");
		return 3;
	}
	else
	{
		return 0;
	}
	
}

//Sensors
int hall(void) {
	if (!(PINC & (1<<PC5)))
	{
		return 0; //Intruder Detected
	}
	else
	{
		return 1;
		
	}
}
int pir(void) {
	int pirState = 0;
	int found;
	_delay_ms(5000);
	while(1) {
		if(PINC & (1<<PC5)) {
			if(pirState == 0) {
				found = 1;
				pirState = 1;
			} else {
				found = 0;
			}
		} else {
			if(pirState == 1) {
				found = 1;
				pirState = 0;
			} else {
				found = 0;
			}
		}
		if(found == 1) return 0;
	}
}
//LCD 
void LCD_init(void)
{
	_delay_ms(40); //LCD power on delay, needs to be greater than 15ms
	
	//Manual 4 bit initialization of LCD, not likely required, but doesn't harm to do it
	LCD_command_4bit(0x3);
	_delay_ms(5); //min 4.1ms
	LCD_command_4bit(0x3);
	_delay_ms(1);
	LCD_command_4bit(0x3);
	_delay_ms(1); //min of 100us
	
	//DB7 DB6 DB5 DB4 DB3 DB2 DB1 DB0    <== See table in Datasheet, * indicated not used
	//0    0   1   0  0*   0*   0*  0*
	
	LCD_command_4bit(0x2); //Function set to 4 bit

	LCD_command(0x28); //2 line, 5*8 dots, 4 bit mode
	LCD_command(0x08); //Display off, cursor off (0x0C for display ON, cursor Off)
	LCD_command(0x01); //Display clear
	LCD_command(0x06); //Entry Set mode, increment cursor, no shift
	
	_delay_ms(2);
}
void LCD_command (char command)
{
	//Basic function used in giving commands to the LCD
	char UpperHalf, LowerHalf;
	
	UpperHalf=command & 0xF0;	//Take upper 4 bits of command
	PORTD &= 0x0F; //Flushes upper half of PortC to 0, but keeps lower half
	PORTD |= UpperHalf;
	PORTC &=~(1<<RS); //Clear RS for command register
	//PORTC &=~(1<<RW); //Clear RW for IR
	PORTC |= (1<<CE); //Set CE
	_delay_us(1);
	PORTC &= ~(1<<CE); //Clear CE
	_delay_us(200);
	
	LowerHalf=(command<<4); //Lower 4 bits of the command
	PORTD &= 0x0F; //Flushes upper half of PortC to 0, but keeps lower half
	PORTD |= LowerHalf;
	PORTC |= (1<<CE); //Set CE
	_delay_us(1);
	PORTC &= ~(1<<CE); //clear CE
	_delay_ms(2);
	
}
void LCD_command_4bit (char command)
{
	//Basic function used in giving commands to the LCD
	char LowerHalf;
	
	LowerHalf=(command<<4); //Lower 4 bits of the command
	PORTD &= 0x0F; //Flushes upper half of PortC to 0, but keeps lower half
	PORTD |= LowerHalf;
	PORTC &=~(1<<RS); //Clear RS for command register
	PORTC |= (1<<CE); //Set CE
	_delay_us(1);
	PORTC &= ~(1<<CE); //clear CE
	_delay_ms(2);
	
}
void LCD_Char (char AsciiChar)
{
	char UpperHalf, LowerHalf;
	
	UpperHalf=AsciiChar & 0xF0; //Upper 4 bits of data
	PORTD &= 0x0F; //Flushes upper half of PortD to 0, but keeps lower half
	PORTD |= UpperHalf;
	PORTC |=(1<<RS); //Set RS for data register
	//PORTC &=~(1<<RW); //Clear RW for write data, Set RW to read data
	PORTC |= (1<<CE); //Set CE
	_delay_us(1);
	PORTC &= ~(1<<CE); //Clear CE
	_delay_us(200);
	
	LowerHalf=(AsciiChar<<4); //Lower 4 bits of the command
	PORTD &= 0x0F; //Flushes upper half of PortD to 0, but keeps lower half
	PORTD |= LowerHalf;
	PORTC |= (1<<CE); //Set CE
	_delay_us(1);
	PORTC &= ~(1<<CE);
	_delay_ms(2);
	
}
void LCD_Send_A_String(char *StringOfCharacters)
{
	//Take a string input and displays it
	//Each character in the string is processed using LCD_Char which converts the character into the proper 8bit hex #
	//Max character in a string should be <16, after 16th character, everything will be ignored

	int i;
	for (i=0;StringOfCharacters[i]!=0;i++) //Send each character of string until the Null
	{
		LCD_Char(StringOfCharacters[i]);
	}
	
	//for(int n = i;n <= 40;n++)  {
	//	LCD_command(0x14);
	//}
}
void LCD_clearScreen (void)
{
	//Clears the screen
	//DB7 DB6 DB5 DB4 DB3 DB2 DB1 DB0
	//0    0   0   0  0    0   0  1
	LCD_command(0x01); //Clear display
	_delay_ms(2);
	
	//Returns the cursor to (0,0) position
	//DB7 DB6 DB5 DB4 DB3 DB2 DB1 DB0
	//1    0   0   0  0    0   0  0
	LCD_command(0x80); //cursor at home position
}
void LCD_home(void)
{
	//Returns the cursor to (0,0) position
	//DB7 DB6 DB5 DB4 DB3 DB2 DB1 DB0
	//0    0   0   0  0    0   1  0
	LCD_command(0x02);
}
void LCD_display(void)
{
	//Display ON with cursor OFF
	//DB7 DB6 DB5 DB4 DB3 DB2 DB1 DB0
	//0    0   0   0  1    1   0  0
	LCD_command(0x0C);
}
void LCD_noDisplay(void)
{
	//Display off
	//DB7 DB6 DB5 DB4 DB3 DB2 DB1 DB0
	//0    0   0   0  1    0   0  0
	LCD_command(0x08);
}
void printcode(char b) {
	switch (b) {
		case '1':
			LCD_Char(0x31);
			break;
		case '2':
			LCD_Char(0x32);
			break;
		case '3':
			LCD_Char(0x33);
			break;
		case '4':
			LCD_Char(0x34);
			break;
		case '5':
			LCD_Char(0x35);
			break;
		case '6':
			LCD_Char(0x36);
			break;
		case '7':
			LCD_Char(0x37);
			break;
		case '8':
			LCD_Char(0x38);
			break;
		case '9':
			LCD_Char(0x39);
			break;
		case '*':
			LCD_Char(0x2A);
			break;
		case '0':
			LCD_Char(0x30);
			break;
		case '#':
			LCD_Char(0x23);
			break;
	}

	/*char buttons[4][3] = 
	{ {'1', '2', '3'},
	{'4', '5', '6'},
	{'7', '8', '9'},
	{'*', '0', '#'} };
	char buttoncode[4][3] = 
	{ {'0x31', '0x32', '0x33'},
	{'0x34', '0x35', '0x36'},
	{'0x37', '0x38', '0x39'},
	{'0x2A', '0x30', '0x23'} }; */
		
}
//State 
void armed(void) {
	LCD_Send_A_String("System Armed"); //LCD state message//
	PORTC |= 0b00000010; //LED on//
	//while(pir()); //check for pir response//
	while(hall());
	PORTC &= 0b11111101; //LED off//
	LCD_clearScreen(); //Clear screen//
}
void intruder(void) {
	LCD_Send_A_String("Alert! Intruder!"); 
	OCR2A = 128; //Siren on//
	OCR2B = OCR2A/2;
	LCD_command(0xC0);
	PORTC |= 0b00000001;
	while(!initiate_keypad());
	PORTC &= 0b11111110;
	LCD_clearScreen();
	OCR2A = 0; //Siren off//
	OCR2B = 0;
	
}
void disarmed(void) {
	PORTB |= 0b00100000; 
	LCD_Send_A_String("System Disarmed"); 
	LCD_command(0xC0);
	printf("To arm, enter pass:\n");
	while(!initiate_keypad());
	PORTB &= 0b11011111;
	LCD_clearScreen();
}