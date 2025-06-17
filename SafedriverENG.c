#include<htc.h> //Include the microcontroller library
#include <stdio.h>
#include "lcd.h"

__CONFIG(WRT_OFF & WDTE_OFF & PWRTE_OFF & FOSC_XT & LVP_OFF);
#define _XTAL_FREQ 4000000 // Internal oscillator of 4MHz
//Variables
unsigned int nprueba=0;
unsigned int adc[2]; //We need two chanels
unsigned char canal=0;
unsigned int superado=0;
int gas=0;
int mgml=0;
unsigned char aciertos=0;
unsigned int cambio=0;	

//LCD and USART configuration
//We use "printf" for both LCD and USART
//This means we need to diferentiate each case with a variable:
void putch(unsigned char c){
	if (cambio==0){
		lcd_putch(c);
	}
	else{
		while (!TXIF)
		continue;
   		TXREG = c;
	}
}
//Interruptions
static void interrupt isr(void){
	//Interruptions Timer 0
	//Analog measures, Alcohol and Gas
	if (T0IF==1 && canal!=2){	
		GO=1;
		//Obtain the value from the ADC
		if (ADIF==1){
			GO=0;
			ADIF=0;
			adc[canal]=(ADRESH << 2)|(ADRESL >> 6);	
		}
		//Porcentual computation of alcohol on breath (assuming linearity)
		//Display the values on the LCD
		lcd_clear();
		lcd_goto(0x00);
		if (canal==0){
			mgml=((adc[canal])*1.6);
			printf("Alc. %d mg/l",mgml);
		}
		if (canal==1){
			gas=(adc[canal]);
			printf("Gas: %d ppm",gas);
		}
		T0IF=0;
	}	
	//Digital measures, KY buttons
	if (T0IF==1 && canal==2){	
		if (RB1==1 && RB2==1){
			superado=1;
		}else{
			superado=0;
		}
		lcd_clear();
		lcd_goto(0x00);
		printf("Continue...");
		T0IF=0;
	}	
	//Reset Interruption, using RB0
	if (RB0==0){
		__delay_ms(250); //Antirebotes
		if (RB0==0){
			//Variables Reset and LCD message
			T0IE=0;
			T0IF=0;
			INTF=0;
			GO=0;;
			nprueba=0;
			canal=0;
			superado=0;
			gas=0;
			mgml=0;
			mano=0;
			aciertos=0;
			ADCON0=0X89;
			lcd_clear();
			lcd_goto(0x00);
			printf("REINICIANDO...");
			__delay_ms(2000);
		}

	}
}	
//USART configuration, done as a function for clarity
void uart_init() {
    TXEN = 1;
    BRGH = 1;
    SPBRG = 25;
    SYNC = 0;
    SPEN = 1;
}

//Function, to send values by Bluetotth
//Variable "cambio" to habilitate USART
//and inhabilitate to use LCD
void enviar_resultado_alcohol(int mgml) {
  	cambio=1;
    printf("Alc: %d mg/L\r\n", mgml);
	cambio=0;
}
void enviar_resultado_humo(int gas) {
	cambio=1;
    printf("Humo: %d ppm\r\n", gas);
	cambio=0;
}
void enviar_resultado_atencion(int ok) {
	cambio=1;
    if (ok) {
        printf("Superado!");
    } else {
        printf("No esta atento");
    }
	cambio=0;
}
//PMW functions, for the alarms.
//Initial PWM configuration
void init_PWM(unsigned int frecuencia) {
    unsigned int pr2_val = (_XTAL_FREQ / (frecuencia * 16)) - 1;
    PR2 = (unsigned char)pr2_val;
    CCP1CON = 0x0C;   // PWM mode
    T2CON = 0x05;     //Prescaler 1:4
    TRISC2=0;
    CCPR1L = (pr2_val + 1) / 2;
}
//Duty cicle selection
void set_PWM_duty(unsigned int duty) {
    unsigned int dc = duty * 4; // Convert duty (0-1023) to 10 bits
    CCPR1L = duty >> 2;                  
    CCP1CONbits.CCP1X = duty & 0x01;   
    CCP1CONbits.CCP1Y =(duty >> 1) & 0x01;
}
//Alarm functions, using PWM
//Alarm 1 (medium)
void alarma1() {
    init_PWM(2000);           
    set_PWM_duty(250);        
    unsigned int i = 0;

    while (i < 10) {
        __delay_ms(500);
        set_PWM_duty(0);      // Silence
        __delay_ms(500);
        set_PWM_duty(250);    // Reactivate
        i++;
    }
	__delay_ms(250);
    set_PWM_duty(0);
}

//Alarma 2 (serious)
void alarma2() {
    init_PWM(500);          
    set_PWM_duty(1500);   
    unsigned int i = 0;
    while (i < 10) {
        __delay_ms(250);
        set_PWM_duty(0);      // Silence
        __delay_ms(250);
        set_PWM_duty(1500);   // Reactivate
        i++;
    }
	__delay_ms(250);
    set_PWM_duty(0);
}

void main(void){
//CONFIGURATIONS
//Ports
	lcd_init();
	uart_init();
	TRISA=0xFF;		// All pins as inputs since they will be used for sensors
	PORTAbits.RA4=1;
	TRISB=0x07;		// Only RB0 as input
	TRISC=0x00; 	  // Configure RC2 as output for PWM.
	PORTC=0x00;
	PORTB=0x00;

//Interrupts:
	GIE=1;		// Global interrupts
	PEIE=1;
	
	T0IE=0;		//Timer 0
	T0IF=0;
	
	INTE=1; 	// Port B and RB0 interrupts
	INTF=0;
	RBIE=1;
	RBIF=0;

	CCP1IE=1;	// CCP module interrupts
	CCP1IF=0;
	

	ADIE=1;		// ADC module interrupts
	ADIF=0;

//Timers:
	//Timer 0:
	OPTION_REG= 0x07; // Timer 0 as timer, rising edge, prescaler 256
	TMR0=0X79;

	//Timer 2:
	T2CON=0x05;		// Timer 2 on and prescaler factor 4 
	PR2=249;
	TMR2=0;
	
	// CCP Module:
	CCPR1L=0;		// Register initialized to zero
	CCP1CON=0X0C;	// PWM mode

	// ADC Module:
	ADCON0=0X89;  // channel 1
	ADCON1=0X00;
	

	
	
	

	
// MAIN PROGRAM
// Continuous loop, 3 different tests
// Progresses linearly using the variable "nprueba"
// Which depends on RA4
while (1){
	// INITIAL MESSAGE, NO BUTTON PRESS REQUIRED
	if (nprueba==0){
			lcd_clear();
			lcd_goto(0x00);			// First LCD comment before tests
			printf("Pulse RA4 para");
			lcd_goto(0x40);
			printf("comenzar prueba");
			__delay_ms(1000);
	}	
	// Constant check for RA4 press
	if (RA4==0){
		__delay_ms(250); // Debounce
		if (RA4==0){
			nprueba=nprueba+1; // Increment "nprueba" to move to next test
			// First test: Alcohol
			if (nprueba==1){
				// Instructions for the test on LCD
				lcd_clear();
				ADCON0=0X89;
				lcd_goto(0x00);
				printf("Prueba de");
				lcd_goto(0x40);
				printf("Alcoholemia");
				__delay_ms(2500);
				lcd_clear();
				lcd_goto(0x00);
				printf("SOPLE POR FAVOR");
				__delay_ms(2000);
				// Start measurements by enabling Timer 0 interrupts
				T0IE=1;
				__delay_ms(2000);
				// Stop measurements by disabling Timer 0 interrupts
				T0IE=0;
				__delay_ms(2000);
                		// Check test results
                		// Positive result
				if (mgml<10){
					// Message on LCD
					lcd_clear();
					lcd_goto(0x00);
					printf("Prueba");
					lcd_goto(0x40);
					printf("Superada");
					// Green LED, no alarm
					PORTCbits.RC5=1;
					__delay_ms(3000);
					PORTC=0x00;
					// Add success
					aciertos=aciertos+1;
				}
				// Medium result
				if (30>mgml>10){
					// Message on LCD
					lcd_clear();
					lcd_goto(0x00);
					printf("Tasa ilegal");
					lcd_goto(0x40);
					printf("en 2025");
					// Blue LED
					PORTCbits.RC4=1;
					// Alarm, medium melody
					alarma1();
					PORTC=0x00;
					__delay_ms(1000);
				}
				// Serious result
				if (mgml>30){
					// Message on LCD
					lcd_clear();
					lcd_goto(0x00);
					printf("Delito:");
					lcd_goto(0x40);
					printf("No Conduzca!");
					// Red LED
					PORTBbits.RB4=1;
					// Alarm, serious melody
					alarma2();
					PORTC=0x00;
					PORTBbits.RB4=0;
					__delay_ms(1000);
				}
			// Send data via Bluetooth
			enviar_resultado_alcohol(mgml);
			// Leave message on LCD until RA4 is pressed again
			lcd_clear();
			lcd_goto(0x00);
			printf("Pulse RA4 para");
			lcd_goto(0x40);
			printf("siguiente prueba");
			__delay_ms(2500);
			}
			// Second test: Gas
			if (nprueba==2){
				// Instructions for the test on LCD
				lcd_clear();
				lcd_goto(0x00);
				printf("Prueba de");
				lcd_goto(0x40);
				printf("Humos");
				__delay_ms(2500);
				lcd_clear();
				lcd_goto(0x00);
				printf("MEDICION EN");
				lcd_goto(0x40);
				printf("PROGRESO");
				__delay_ms(2000);
				// Change analog channel
				ADCON0=0x91;
				canal=1;
				// Start measurements by enabling Timer 0 interrupts
				T0IE=1;
				__delay_ms(2000);
				// Stop measurements by disabling Timer 0 interrupts
				T0IE=0;
				__delay_ms(2000);
				// Check results
              			// Positive result
				if (gas<200){
					// Message on LCD
					lcd_clear();
					lcd_goto(0x00);
					printf("Estado:");
					lcd_goto(0x40);
					printf("Seguro");
					// Green LED, no alarm
					PORTCbits.RC5=1;
					__delay_ms(3000);
					PORTC=0x00;
					// Add success
					aciertos=aciertos+1;
				}
				// Medium result
				if (500>gas>200){
					// Message on LCD
					lcd_clear();
					lcd_goto(0x00);
					printf("Atencion:");
					lcd_goto(0x40);
					printf("Humo");
					// Blue LED
					PORTCbits.RC4=1;
					// Alarm, medium melody
					alarma1();
					__delay_ms(3000);
					PORTC=0x00;
				}
				// Serious result
				if (gas>=500){
					// Message on LCD
					lcd_clear();
					lcd_goto(0x00);
					printf("Peligro:");
					lcd_goto(0x40);
					printf("Evacue!");
					// Red LED
					PORTBbits.RB4=1;
					// Alarm, serious melody
					alarma2();
					__delay_ms(3000);
					PORTC=0x00;	
					PORTBbits.RB4=0;	
				}
				// Send data via Bluetooth
				enviar_resultado_humo(gas);
				// Leave message on LCD until RA4 is pressed again
				lcd_clear();
				lcd_goto(0x00);
				printf("Pulse RA4 para");
				lcd_goto(0x40);
				printf("siguiente prueba");
				__delay_ms(2500);
			}	
			// Third test: Attention to driving
			if (nprueba==3){
				// Instructions for the test on LCD
				lcd_clear();
				lcd_goto(0x00);
				printf("Prueba de");
				lcd_goto(0x40);
				printf("Atencion");
				__delay_ms(2500);
				lcd_clear();
				lcd_goto(0x00);
				printf("Coloca las manos");
				lcd_goto(0x40);
				printf("en el volante");
				__delay_ms(2000);
               			// Since these are digital inputs, just change the "canal" variable
              			// to differentiate the interrupt
				canal=2;
				// Start measurements by enabling Timer 0 interrupts
				T0IE=1;
				__delay_ms(2000);
				// Stop measurements by disabling Timer 0 interrupts
				T0IE=0;
				__delay_ms(2000);
				// Check results, here there are only two levels
				if (superado==1){
					lcd_clear();
					lcd_goto(0x00);
					printf("Buen trabajo!");
					// Blue LED
					PORTCbits.RC5=1;
					__delay_ms(3000);
					PORTC=0x00;
					// Add success
					aciertos=aciertos+1;
				}
				else{
					lcd_clear();
					lcd_goto(0x00);
					printf("No esta atento!");
					__delay_ms(2500);
					// Blue LED
					PORTCbits.RC4=1;
					// Alarm, medium melody
					alarma1();
					__delay_ms(3000);
					PORTC=0x00;
				}
				// Send data via Bluetooth
				enviar_resultado_atencion(superado);
				// CHECK PARTICIPANT'S RESULTS
				// All tests passed:
				if (aciertos==3){
					lcd_clear();
					lcd_goto(0x00);
					printf("En perfectas");
					lcd_goto(0x40);
					printf("condiciones!");
					__delay_ms(3000);
				}
				// Some test failed:
				else{
					lcd_clear();
					lcd_goto(0x00);
					printf("Ha fallado");
					lcd_goto(0x40);
					printf("alguna prueba");
					__delay_ms(3000);	
				}
				// Final message, stays here until reset with RB0
				lcd_clear();
				lcd_goto(0x00);
				printf("Mantenga RB0");
				lcd_goto(0x40);
				printf("para reiniciar");
				__delay_ms(8000);	
				}	
			}
		}
		}
	}
