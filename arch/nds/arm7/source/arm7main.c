/*---------------------------------------------------------------------------------

	default ARM7 core

		Copyright (C) 2005 - 2010
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
#include <nds.h>
#include <maxmod7.h>

#define FIFO_MZX FIFO_USER_01
#define CMD_MZX_PCS_TONE 0x01
#define CMD_MZX_SOUND_VOLUME 0x02
#define CMD_MZX_MM_GET_POSITION 0x03
#define MZX_PCS_CHANNEL 8

void mzxFifoCommandHandler(u32 command, void *userdata) {
	switch (command & 0xF) {
		case CMD_MZX_SOUND_VOLUME: {
			REG_MASTER_VOLUME = (command >> 24);
		} break;
		case CMD_MZX_PCS_TONE: {
			int freq = (command >> 8) & 0xFFFF;
			int volume = (command >> 24);
			if(freq > 0) {
				SCHANNEL_CR(MZX_PCS_CHANNEL) = SCHANNEL_ENABLE | volume | SOUND_PAN(64) | SOUND_FORMAT_PSG | (3 << 24);
				SCHANNEL_TIMER(MZX_PCS_CHANNEL) = SOUND_FREQ(freq << 3);
			} else {
				SCHANNEL_CR(MZX_PCS_CHANNEL) = 0;
			}
		} break;
		case CMD_MZX_MM_GET_POSITION: {
			fifoSendValue32(FIFO_MZX, mmLayerMain.position);
		} break;
	}
}

void VblankHandler(void) {

}

void VcountHandler() {
	inputGetAndSend();
}

volatile bool exitflag = false;

void powerButtonCB() {
	exitflag = true;
}

//---------------------------------------------------------------------------------
int main() {
//---------------------------------------------------------------------------------
	// clear sound registers
	dmaFillWords(0, (void*)0x04000400, 0x100);

	REG_SOUNDCNT |= SOUND_ENABLE;
	writePowerManagement(PM_CONTROL_REG, ( readPowerManagement(PM_CONTROL_REG) & ~PM_SOUND_MUTE ) | PM_SOUND_AMP );
	powerOn(POWER_SOUND);

	readUserSettings();
	ledBlink(0);

	irqInit();
	// Start the RTC tracking IRQ
	initClockIRQ();
	fifoInit();
	touchInit();

	mmInstall(FIFO_MAXMOD);

	SetYtrigger(80);

	installSystemFIFO();
	fifoSetValue32Handler(FIFO_MZX, mzxFifoCommandHandler, NULL);

	irqSet(IRQ_VCOUNT, VcountHandler);
	irqSet(IRQ_VBLANK, VblankHandler);

	irqEnable(IRQ_VBLANK | IRQ_VCOUNT);

	setPowerButtonCB(powerButtonCB);

	// Keep the ARM7 mostly idle
	while(!exitflag) {
		if( 0 == (REG_KEYINPUT & (KEY_SELECT | KEY_START | KEY_L | KEY_R))) {
			exitflag = true;
		}
		swiWaitForVBlank();
	}
	return 0;
}
