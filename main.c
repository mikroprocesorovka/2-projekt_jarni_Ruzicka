#include "stm8s.h"
#include "milis.h"
#include "stdio.h"
//#include "swspi.h"

// deklarace
#define CLK_PIN			GPIO_PIN_1
#define CLK_PORT		GPIOC
#define DT_PIN			GPIO_PIN_2
#define DT_PORT			GPIOC
#define SW_PIN			GPIO_PIN_3
#define SW_PORT			GPIOC
#define NCDR_CLK_GET 	( GPIO_ReadInputPin( CLK_PORT, CLK_PIN ) != RESET )
#define NCDR_DT_GET 	( GPIO_ReadInputPin( DT_PORT, DT_PIN ) != RESET )	
#define NCDR_SW_GET		( GPIO_ReadInputPin( SW_PORT, SW_PIN ) == RESET )

#define CHANGE_POSITION_TIME 50
#define DEFAULT_PULSE 10

#define BEEP_PIN		GPIO_PIN_7
#define BEEP_PORT 	GPIOB
#define BEEP_ON			GPIO_WriteHigh(BEEP_PORT, BEEP_PIN)
#define BEEP_OFF		GPIO_WriteLow(BEEP_PORT, BEEP_PIN);




// funkce
void init(void);
void init_pwm(void);

int8_t ncoder(void);
void process_pwm_change(void);

void uart_putchar(char data); 	// pošle jeden znak na UART
void uart_puts(char* retezec); 	// pošle celý øetìzec na UART




// promìnné
uint16_t periode = 1000 - 1;

char text[24];

uint16_t minule_uart = 0;
uint16_t minule_lcd = 0;
uint16_t minule_led = 0;
int8_t state = 0;
uint8_t step = 50;




void main(void){
	init(); 	// inicializace
	
	while(1){
	
		if (milis() - minule_uart >= 500){ 	// KAŽDých 500 ms odešli informace na UART - pc
			sprintf(text,"t-high (us): %3u. \n\r", (periode + 1)); 	// 1. páødek vlož text do promìnné text
			uart_puts(text); 	// zobraz obsah promìnné text
			sprintf(text,"t-low (us):  %3u. \n\n\r", (2000 - (periode + 1)));
			uart_puts(text);
			minule_uart = milis(); 	// aktualizace èasu - poèítej od zaèátku ( do 500 )
		}
	
		periode += ncoder() * step; 		//zmìna pøi pohybu n-coderem
		
		if (periode <= 499) 	// ošetøení minimálního pulzu pro servo
		{
			periode = 499;
		}
		else if (periode >= 1999) 	// ošetøení maximálního pulzu pro servo
		{
			periode = 1999;
		}
		
		if (periode == 499 || periode == 1999) // v pøípadì krajních bodù zazní signál
		{
			if (state == 0)	// nastav pípání jen jednou - v pøípadì zmìny se vypne také jednou
			{
				state = 1;
				BEEP_ON;
			}
		} 
		else  	// když n-coderem vyjdu opìt z krajních mezí, vypni vyzvánìní (též jen jednou)
		{
			if (state == 1)
			{
				state = 0;
				BEEP_OFF;
			}
		}
		
		TIM2_SetCompare1(periode); 	// nastav PWM (servo) podle natáèení n-coderu
	}
}






void init(void){
	CLK_HSIPrescalerConfig(CLK_PRESCALER_HSIDIV1);
	UART1_Init(115200,UART1_WORDLENGTH_8D,UART1_STOPBITS_1,UART1_PARITY_NO,UART1_SYNCMODE_CLOCK_DISABLE,UART1_MODE_TX_ENABLE);
	UART1_Cmd(ENABLE);
	
	init_milis(); 																						// funkce
	init_pwm();
	
	GPIO_Init( CLK_PORT, CLK_PIN, GPIO_MODE_IN_FL_NO_IT);  		// piny
	GPIO_Init( DT_PORT, CLK_PIN, GPIO_MODE_IN_FL_NO_IT);
	GPIO_Init( GPIOC, GPIO_PIN_5, GPIO_MODE_OUT_PP_LOW_SLOW);
	GPIO_Init( BEEP_PORT, BEEP_PIN, GPIO_MODE_OUT_PP_LOW_SLOW); 
	
	TIM3_SetCompare2(100); 																		// nastavení defaultní pro zvuk
}



// pošle jeden znak na UART
void uart_putchar(char data){
 while(UART1_GetFlagStatus(UART1_FLAG_TXE) == RESET);
 UART1_SendData8(data);
}

// pošle UARTem øetìzec 
void uart_puts(char* retezec){ 
 while(*retezec){
  uart_putchar(*retezec);
  retezec++;
 }
}






int8_t ncoder(void){
	static uint8_t minule = 0;
	
	if (minule == 0 && NCDR_CLK_GET == 1) {
			// vzestupná hrana 
			minule = 1;
			if (NCDR_DT_GET == 0) { // a zároveò data v 0 - vlevo
					return -1;
			} else { 						// a zároveò data v 1 - vpravo
					return 1;
			}
	} 
	else if (minule == 1 && NCDR_CLK_GET == 0) {
			// sestupná hrana 
			minule = 0;
			if (NCDR_DT_GET == 0) {  // a zároveò data v 0 - vpravo
					return 1;
			} else { 			// a zároveò data v 1 - vlevo
					return -1;
			}
	}
	return 0;
}


//------------------------------------------ PWM - init

void init_pwm(void){
	GPIO_Init(GPIOD, GPIO_PIN_4, GPIO_MODE_OUT_PP_LOW_FAST);	// servo
	GPIO_Init(GPIOD, GPIO_PIN_0, GPIO_MODE_OUT_PP_LOW_FAST);	// beeper

	TIM2_TimeBaseInit(TIM2_PRESCALER_16,20000-1);		// perioda pro servo
	TIM3_TimeBaseInit(TIM2_PRESCALER_16,2000-1); 		// vyšší frekvence 

	TIM2_OC1Init( 					// inicializujeme kanál 1 (TM2_CH1)
		TIM2_OCMODE_PWM1, 				// režim PWM1
		TIM2_OUTPUTSTATE_ENABLE,	// Výstup povolen (TIMer ovládá pin)
		DEFAULT_PULSE,
		TIM2_OCPOLARITY_HIGH			// Zátìž rozsvìcíme hodnotou HIGH 
		);
		/*
	TIM2_OC2Init(
		TIM2_OCMODE_PWM1, 
		TIM2_OUTPUTSTATE_ENABLE,
		DEFAULT_PULSE,
		TIM2_OCPOLARITY_HIGH
	);*/
	TIM3_OC2Init(
		TIM2_OCMODE_PWM1,
		TIM2_OUTPUTSTATE_ENABLE,
		DEFAULT_PULSE,
		TIM2_OCPOLARITY_HIGH
		);
	
	TIM2_OC1PreloadConfig(ENABLE);
	//TIM2_OC2PreloadConfig(ENABLE);
	TIM3_OC2PreloadConfig(ENABLE);
	TIM2_Cmd(ENABLE);
	TIM3_Cmd(ENABLE);
}

//---------------------------------------------------------- Assert
// pod tímto komentáøem nic nemìòte 
#ifdef USE_FULL_ASSERT

/**
  * @brief  Reports the name of the source file and the source line number
  *   where the assert_param error has occurred.
  * @param file: pointer to the source file name
  * @param line: assert_param error line source number
  * @retval : None
  */                                                
void assert_failed(u8* file, u32 line)
{ 
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */

  /* Infinite loop */                               
  while (1)
  {
  }
}
#endif

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/