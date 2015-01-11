//***********************************************************
//* display_rcinput.c
//***********************************************************

//***********************************************************
//* Includes
//***********************************************************

#include <avr/io.h>
#include <stdlib.h>
#include <stdbool.h>
#include "io_cfg.h"
#include "glcd_driver.h"
#include "mugui.h"
#include <avr/pgmspace.h>
#include "glcd_menu.h"
#include "main.h"
#include "isr.h"
#include <util/delay.h>
#include "acc.h"
#include "gyros.h"
#include "rc.h"
#include "menu_ext.h"
#include "mixer.h"
#include "eeprom.h"

//************************************************************
// Prototypes
//************************************************************

void Display_rcinput(void);

//************************************************************
// Code
//************************************************************

void Display_rcinput(void)
{
	// Re-enable interrupts. High seed mode may have left them off
	init_int();
	
	while(BUTTON1 != 0)
	{
		if (BUTTON4 == 0)
		{
			CenterSticks();
		}

		RxGetChannels();

		LCD_Display_Text(114,(const unsigned char*)Verdana8,0,0); // Throttle
		LCD_Display_Text(32,(const unsigned char*)Verdana8,0,10);
		LCD_Display_Text(115,(const unsigned char*)Verdana8,0,20);
		LCD_Display_Text(35,(const unsigned char*)Verdana8,0,30);

		LCD_Display_Text(109,(const unsigned char*)Verdana8,70,0); // Gear
		LCD_Display_Text(110,(const unsigned char*)Verdana8,70,10);
		LCD_Display_Text(111,(const unsigned char*)Verdana8,70,20);
		LCD_Display_Text(112,(const unsigned char*)Verdana8,70,30);

		mugui_lcd_puts(itoa(MonopolarThrottle,pBuffer,10),(const unsigned char*)Verdana8,37,0);
		mugui_lcd_puts(itoa(RCinputs[AILERON],pBuffer,10),(const unsigned char*)Verdana8,37,10);
		mugui_lcd_puts(itoa(RCinputs[ELEVATOR],pBuffer,10),(const unsigned char*)Verdana8,37,20);
		mugui_lcd_puts(itoa(RCinputs[RUDDER],pBuffer,10),(const unsigned char*)Verdana8,37,30);

		mugui_lcd_puts(itoa(RCinputs[GEAR],pBuffer,10),(const unsigned char*)Verdana8,100,0);
		mugui_lcd_puts(itoa(RCinputs[AUX1],pBuffer,10),(const unsigned char*)Verdana8,100,10);
		mugui_lcd_puts(itoa(RCinputs[AUX2],pBuffer,10),(const unsigned char*)Verdana8,100,20);
		mugui_lcd_puts(itoa(RCinputs[AUX3],pBuffer,10),(const unsigned char*)Verdana8,100,30);

		// Print bottom text and markers
		LCD_Display_Text(12, (const unsigned char*)Wingdings, 0, 57); 	// Left
		LCD_Display_Text(60, (const unsigned char*)Verdana8, 110, 55); 	// Cal.

		// Update buffer
		write_buffer(buffer);
		clear_buffer(buffer);
	}
}
