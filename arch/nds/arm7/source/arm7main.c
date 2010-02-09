/*---------------------------------------------------------------------------------

	default ARM7 core

		Copyright (C) 2005
		Michael Noland (joat)
		Jason Rogers (dovoto)
		Dave Murphy (WinterMute)

	This software is provided 'as-is', without any express or implied
	warranty.  In no event will the authors be held liable for any
	damages arising from the use of this software.

	Permission is granted to anyone to use this software for any
	purpose, including commercial applications, and to alter it and
	redistribute it freely, subject to the following restrictions:

	1.	The origin of this software must not be misrepresented; you
		must not claim that you wrote the original software. If you use
		this software in a product, an acknowledgment in the product
		documentation would be appreciated but is not required.

	2.	Altered source versions must be plainly marked as such, and
		must not be misrepresented as being the original software.

	3.	This notice may not be removed or altered from any source
		distribution.

---------------------------------------------------------------------------------*/

// WIFI and MAXMOD currently disabled --kvance 2009-10-02

#include <nds.h>
/*
#include <dswifi7.h>
#include <maxmod7.h>
*/

//---------------------------------------------------------------------------------
void VcountHandler() {
//---------------------------------------------------------------------------------
	inputGetAndSend();
}

//---------------------------------------------------------------------------------
void VblankHandler(void) {
//---------------------------------------------------------------------------------
// WIFI	Wifi_Update();
}


//---------------------------------------------------------------------------------
int main() {
//---------------------------------------------------------------------------------

	// read User Settings from firmware
	readUserSettings();

	powerOn(POWER_SOUND);

	irqInit();
// WIFI	irqSet(IRQ_WIFI, 0);
	fifoInit();


	SetYtrigger(80);

// WIFI	installWifiFIFO();
// MAXMOD	installSoundFIFO();
// MAXMOD	mmInstall(FIFO_MAXMOD);

	installSystemFIFO();
	
	irqSet(IRQ_VCOUNT, VcountHandler);
	irqSet(IRQ_VBLANK, VblankHandler);

	// Start the RTC tracking IRQ
	initClockIRQ();

// WIFI	irqEnable( IRQ_VBLANK | IRQ_VCOUNT | IRQ_NETWORK);   
	irqEnable( IRQ_VBLANK | IRQ_VCOUNT);   

	// Keep the ARM7 mostly idle
	while (1) swiWaitForVBlank();
}


