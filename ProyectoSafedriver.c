#include<htc.h> //Incluimos libreria del micro a usar
#include <stdio.h>
#include "lcd.h"

__CONFIG(WRT_OFF & WDTE_OFF & PWRTE_OFF & FOSC_XT & LVP_OFF);
#define _XTAL_FREQ 4000000 //Oscilador Interno de 4MHZ
//Variables
unsigned int nprueba=0;
unsigned int adc[2]; //Los 2 canales necesarios
unsigned char canal=0;
unsigned int superado=0;
int gas=0;
int mgml=0;
unsigned char aciertos=0;
unsigned int cambio=0;	

//CONFIGURACION DEL LCD Y USART
//Ya que empleamos "printf" para el LCD y USART.
//Debemos diferenciar con una variable a cual nos referimos
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
//INTERRUPCIONES
static void interrupt isr(void){
	//INTERRUPCIONES DEL TIMER 0
	//Mediciones analogicas, Alcohol y Humo
	if (T0IF==1 && canal!=2){	
		GO=1;
		//Obtener el dato de nuestra mediciónes analogicas 
		if (ADIF==1){
			GO=0;
			ADIF=0;
			adc[canal]=(ADRESH << 2)|(ADRESL >> 6);	
		}
		//Calculo del porcentaje de alcohol en el aliento, Asumiendo linealidad de 0 a 0.8mg/L
		//Mostrar los valores en LCD
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
	//Mediciones digitales, pulsadores KY
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
	//INTERRUPCION PARA EL RESET, CON RB0
	if (RB0==0){
		__delay_ms(250); //Antirebotes
		if (RB0==0){
			//Reset de todas las variables y mensaje de reinicio
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
//Función de configuracion de USART (hecho asi por claridad)
void uart_init() {
    TXEN = 1;
    BRGH = 1;
    SPBRG = 25;
    SYNC = 0;
    SPEN = 1;
}

//Funcion de envio de resultados por Bluetooth
//IMPORTANTE,21:07 09/05/2025 el cambio de la variable "cambio" para habilitar el USART
//y desactivarlos para continuar con el LCD activo
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
//FUNCIONES DE PMW, PARA LAS ALARMAS
//Configuracion inicial del PMW (hecho asi por claridad)
void init_PWM(unsigned int frecuencia) {
    unsigned int pr2_val = (_XTAL_FREQ / (frecuencia * 16)) - 1;
    PR2 = (unsigned char)pr2_val;
    CCP1CON = 0x0C;   // Modo PWM
    T2CON = 0x05;     //Prescaler 1:4
    TRISC2=0;
    CCPR1L = (pr2_val + 1) / 2;
}
//Seleccion del ciclo de trabajo
void set_PWM_duty(unsigned int duty) {
    unsigned int dc = duty * 4; // Convertimos duty (0-1023) a 10 bits
    CCPR1L = duty >> 2;                  
    CCP1CONbits.CCP1X = duty & 0x01;   
    CCP1CONbits.CCP1Y =(duty >> 1) & 0x01;
}
//FUNCIONES PARA LAS ALARMAS (emplean las funciones de PWM)
//Alarma 1 (media)
void alarma1() {
    init_PWM(2000);           
    set_PWM_duty(250);        
    unsigned int i = 0;

    while (i < 10) {
        __delay_ms(500);
        set_PWM_duty(0);      // Silencio
        __delay_ms(500);
        set_PWM_duty(250);    // Reactivar
        i++;
    }
	__delay_ms(250);
    set_PWM_duty(0);
}

//Alarma 2 (grave)
void alarma2() {
    init_PWM(500);          
    set_PWM_duty(1500);   
    unsigned int i = 0;
    while (i < 10) {
        __delay_ms(250);
        set_PWM_duty(0);      // Silencio
        __delay_ms(250);
        set_PWM_duty(1500);   // Reactivar
        i++;
    }
	__delay_ms(250);
    set_PWM_duty(0);
}

void main(void){
//CONFIGURACIONES
//Puertos
	lcd_init();
	uart_init();
	TRISA=0xFF;		// Terminales como entradas todas ya que las usaremos para los sensores
	PORTAbits.RA4=1;
	TRISB=0x07;		// Terminales como entradas solo RB0
	TRISC=0x00; 	 // Configura RC2 como salida para nuestro PWM.
	PORTC=0x00;
	PORTB=0x00;

//Interrupciones:
	GIE=1;		//Generales
	PEIE=1;
	
	T0IE=0;		//Timer 0
	T0IF=0;
	
	INTE=1; 	//Interrupciones del puerto B y RB0
	INTF=0;
	RBIE=1;
	RBIF=0;

	CCP1IE=1;	//Interrupciones de módulo CCP
	CCP1IF=0;
	

	ADIE=1;		//Interrupciones módulo analógico digital.
	ADIF=0;

//Timers:
	//Timer 0:
	OPTION_REG= 0x07; //Timer 0 como temporizador, flanco subida	pre-divisor 256 al Timer0
	TMR0=0X79;

	//Timer 2:
	T2CON=0x05;		//Solo TMR2 on y factor de división 4 
	PR2=249;
	TMR2=0;
	
	//Modulo CCP:
	CCPR1L=0;		//REGISTRO CON TODO CEROS
	CCP1CON=0X0C;	//MODO PWM

	//Modulo analogico digital:
	ADCON0=0X89;  //canal 1
	ADCON1=0X00;
	

	
	
	

	
//PROGRAMA PRINCIPAL
//Bucle continuo, 3 pruebas distintas
//Se avanza de manera lineal con una variable "nprueba"
//Que depende de RA4
while (1){
	//MENSAJE INICIAL, SIN NECESIDAD DE PULSACION
	if (nprueba==0){
			lcd_clear();
			lcd_goto(0x00);			//primer comentario del lcd antes de las pruebas
			printf("Pulse RA4 para");
			lcd_goto(0x40);
			printf("comenzar prueba");
			__delay_ms(1000);
	}	
	//Comprobacion constante para pulsacion de RA4
	if (RA4==0){
		__delay_ms(250); //Antirrebote
		if (RA4==0){
			nprueba=nprueba+1; //Sumamos 1 a "nprueba" para asegurarnos de continuar a la siguiente prueba
			//Primera prueba: Alcohol
			if (nprueba==1){
				//Instrucciones para la prueba en LCD
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
				//Comienzan las mediciones al habilitar la interrupciones del timer 0
				T0IE=1;
				__delay_ms(2000);
				//Terminan las mediciones al deshabilitar la interrupciones del timer 0
				T0IE=0;
				__delay_ms(2000);
				//Comprobacion de resultados de la prueba
				//Resultado positivo
				if (mgml<10){
					//Mensaje en LCD
					lcd_clear();
					lcd_goto(0x00);
					printf("Prueba");
					lcd_goto(0x40);
					printf("Superada");
					//Led verde, no hay alarma
					PORTCbits.RC5=1;
					__delay_ms(3000);
					PORTC=0x00;
					//Sumamos un acierto
					aciertos=aciertos+1;
				}
				//Resultado medio
				if (30>mgml>10){
					//Mensaje en LCD
					lcd_clear();
					lcd_goto(0x00);
					printf("Tasa ilegal");
					lcd_goto(0x40);
					printf("en 2025");
					//Led azul
					PORTCbits.RC4=1;
					//Alarma, melodia media
					alarma1();
					PORTC=0x00;
					__delay_ms(1000);
				}
				//Resultado grave
				if (mgml>30){
					//Mensaje en LCD
					lcd_clear();
					lcd_goto(0x00);
					printf("Delito:");
					lcd_goto(0x40);
					printf("No Conduzca!");
					//Led rojo
					PORTBbits.RB4=1;
					//Alarma, melodia grave
					alarma2();
					PORTC=0x00;
					PORTBbits.RB4=0;
					__delay_ms(1000);
				}
			//Enviamos los datos a bluetooth
			enviar_resultado_alcohol(mgml);
			//Dejamos un mensaje en LCD hasta nueva pulsacion de RA4
			lcd_clear();
			lcd_goto(0x00);
			printf("Pulse RA4 para");
			lcd_goto(0x40);
			printf("siguiente prueba");
			__delay_ms(2500);
			}
			//Segunda prueba: Gas
			if (nprueba==2){
				//Instrucciones para la prueba en LCD
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
				//Cambiamos el canal del conversor analogico
				ADCON0=0x91;
				canal=1;
				//Comienzan las mediciones al habilitar la interrupciones del timer 0
				T0IE=1;
				__delay_ms(2000);
				//Terminan las mediciones al deshabilitar la interrupciones del timer 0
				T0IE=0;
				__delay_ms(2000);
				//Comprobacion de resultados
				//Resultado positivo
				if (gas<200){
					//Mensaje en LCD
					lcd_clear();
					lcd_goto(0x00);
					printf("Estado:");
					lcd_goto(0x40);
					printf("Seguro");
					//Led verde, sin alarma
					PORTCbits.RC5=1;
					__delay_ms(3000);
					PORTC=0x00;
					//Sumamos un acierto
					aciertos=aciertos+1;
				}
				//Resultado medio
				if (500>gas>200){
					//Mensaje en LCD
					lcd_clear();
					lcd_goto(0x00);
					printf("Atencion:");
					lcd_goto(0x40);
					printf("Humo");
					//Led azul
					PORTCbits.RC4=1;
					//Alarma, melodia media
					alarma1();
					__delay_ms(3000);
					PORTC=0x00;
				}
				//Resultado grave
				if (gas>=500){
					//Mensaje en LCD
					lcd_clear();
					lcd_goto(0x00);
					printf("Peligro:");
					lcd_goto(0x40);
					printf("Evacue!");
					//Led rojo
					PORTBbits.RB4=1;
					//Alarma, melodia grave
					alarma2();
					__delay_ms(3000);
					PORTC=0x00;	
					PORTBbits.RB4=0;	
				}
				//Enviamos los datos a bluetooth
				enviar_resultado_humo(gas);
				//Dejamos un mensaje en LCD hasta nueva pulsacion de RA4
				lcd_clear();
				lcd_goto(0x00);
				printf("Pulse RA4 para");
				lcd_goto(0x40);
				printf("siguiente prueba");
				__delay_ms(2500);
			}	
			//Segunda prueba: Atencion al volante
			if (nprueba==3){
				//Instrucciones para la prueba en LCD
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
				//Ya que son entradas digitales solo cambiamos la variable
				//"Canal" para diferenciar la interrupcion
				canal=2;
				//Comienzan las mediciones al habilitar la interrupciones del timer 0
				T0IE=1;
				__delay_ms(2000);
				//Terminan las mediciones al deshabilitar la interrupciones del timer 0
				T0IE=0;
				__delay_ms(2000);
				//Comprobacion de resultados, aqui solo hay dos niveles
				if (superado==1){
					lcd_clear();
					lcd_goto(0x00);
					printf("Buen trabajo!");
					//Led azul
					PORTCbits.RC5=1;
					__delay_ms(3000);
					PORTC=0x00;
					//Sumamso un acierto
					aciertos=aciertos+1;
				}
				else{
					lcd_clear();
					lcd_goto(0x00);
					printf("No esta atento!");
					__delay_ms(2500);
					//Led azul
					PORTCbits.RC4=1;
					//Alarma, melodia media
					alarma1();
					__delay_ms(3000);
					PORTC=0x00;
				}
				//Enviamos los datos a bluetooth
				enviar_resultado_atencion(superado);
				//COMPROBAMOS LOS ACIERTOS DEL PARTICIPANTE
				//Todas las pruebas correctas:
				if (aciertos==3){
					lcd_clear();
					lcd_goto(0x00);
					printf("En perfectas");
					lcd_goto(0x40);
					printf("condiciones!");
					__delay_ms(3000);
				}
				//Alguna prueba incorrecta:
				else{
					lcd_clear();
					lcd_goto(0x00);
					printf("Ha fallado");
					lcd_goto(0x40);
					printf("alguna prueba");
					__delay_ms(3000);	
				}
				//Mensaje final, se queda aqui hasta reinicio con RB0
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


