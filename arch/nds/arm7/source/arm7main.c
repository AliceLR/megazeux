// ARM7 binary for MZX.
// Includes ndsSDL ARM7 template.
// kvance

#include <nds.h>
#include <stdlib.h>
#include "soundcommon.h"

/* -------------------------------------------------------------------------
 * Sleep handler
 * ------------------------------------------------------------------------- */

void lid_closed(void)
{
    // Disable the speaker.
    swiChangeSoundBias(0, 0x400);

    // Save the current power state.
    int power = readPowerManagement(PM_CONTROL_REG);

    // Set sleep LED.
    writePowerManagement(PM_CONTROL_REG, PM_LED_CONTROL(1));

    // Sleep until IRQ_LID.
    u32 old_ie = REG_IE;
    REG_IE = IRQ_LID;
    swiSleep();

    // Restore interrupts.
    REG_IF = ~0;
    REG_IE = old_ie;

    // Restore power state.
    writePowerManagement(PM_CONTROL_REG, power);

    // Enable the speaker.
    swiChangeSoundBias(1, 0x400);
}

//---------------------------------------------------------------------------------
void startSound(int sampleRate, const void* data, u32 bytes, u8 channel, u8 vol,  u8 pan, u8 format) {
//---------------------------------------------------------------------------------
	SCHANNEL_TIMER(channel)  = SOUND_FREQ(sampleRate);
	SCHANNEL_SOURCE(channel) = (u32)data;
	SCHANNEL_LENGTH(channel) = bytes >> 2 ;
	SCHANNEL_CR(channel)     = SCHANNEL_ENABLE | SOUND_ONE_SHOT | SOUND_VOL(vol) | SOUND_PAN(pan) | (format==1?SOUND_8BIT:SOUND_16BIT);
}


//---------------------------------------------------------------------------------
s32 getFreeSoundChannel(void) {
//---------------------------------------------------------------------------------
	int i;
	for (i=0; i<16; i++) {
		if ( (SCHANNEL_CR(i) & SCHANNEL_ENABLE) == 0 ) return i;
	}
	return -1;
}

int vcount;
touchPosition first,tempPos;

//---------------------------------------------------------------------------------
void VcountHandler(void) {
//---------------------------------------------------------------------------------
	static int lastbut = -1;
	
	uint16 but=0, x=0, y=0, xpx=0, ypx=0, z1=0, z2=0;

	but = REG_KEYXY;

	if (but & BIT(7))
		lid_closed();

	if (!( (but ^ lastbut) & (1<<6))) {
 
		tempPos = touchReadXY();

		if ( tempPos.x == 0 || tempPos.y == 0 ) {
			but |= (1 <<6);
			lastbut = but;
		} else {
			x = tempPos.x;
			y = tempPos.y;
			xpx = tempPos.px;
			ypx = tempPos.py;
			z1 = tempPos.z1;
			z2 = tempPos.z2;
		}
		
	} else {
		lastbut = but;
		but |= (1 <<6);
	}

	if ( vcount == 80 ) {
		first = tempPos;
	} else {
		if (	abs( xpx - first.px) > 10 || abs( ypx - first.py) > 10 ||
				(but & ( 1<<6)) ) {

			but |= (1 <<6);
			lastbut = but;

		} else { 	
			IPC->mailBusy = 1;
			IPC->touchX			= x;
			IPC->touchY			= y;
			IPC->touchXpx		= xpx;
			IPC->touchYpx		= ypx;
			IPC->touchZ1		= z1;
			IPC->touchZ2		= z2;
			IPC->mailBusy = 0;
		}
	}
	IPC->buttons		= but;
	vcount ^= (80 ^ 130);
	SetYtrigger(vcount);

}

//---------------------------------------------------------------------------------
void VblankHandler(void) {
//---------------------------------------------------------------------------------

	u32 i;

//	SoundVBlankIrq();

	//sound code  :)
	TransferSound *snd = IPC->soundData;
	IPC->soundData = 0;

	if (0 != snd) {

		for (i=0; i<snd->count; i++) {
			s32 chan = getFreeSoundChannel();

			if (chan >= 0) {
				startSound(snd->data[i].rate, snd->data[i].data, snd->data[i].len, chan, snd->data[i].vol, snd->data[i].pan, snd->data[i].format);
			}
		}
	}

        //Wifi_Update(); // update wireless in vblank
}

// callback to allow wifi library to notify arm9
void arm7_synctoarm9(void) { // send fifo message
   REG_IPC_FIFO_TX = 0x87654321;
}
// interrupt handler to allow incoming notifications from arm9
void arm7_fifo(void) { // check incoming fifo messages
  int syncd = 0;
  while ( !(REG_IPC_FIFO_CR & IPC_FIFO_RECV_EMPTY)) {
    u32 value = REG_IPC_FIFO_RX;
    if ( value == 0x87654321 && !syncd) {
      syncd = 1;
      //Wifi_Sync();
    }
  }
}

//---------------------------------------------------------------------------------
int main(int argc, char ** argv) {
//---------------------------------------------------------------------------------
	REG_IPC_FIFO_CR = IPC_FIFO_ENABLE | IPC_FIFO_SEND_CLEAR;

	// Reset the clock if needed
	rtcReset();

	//enable sound
	powerON(POWER_SOUND);
	SOUND_CR = SOUND_ENABLE | SOUND_VOL(0x7F);
	IPC->soundData = 0;

	irqInit();
	irqSet(IRQ_VBLANK, VblankHandler);
	SetYtrigger(80);
	vcount = 80;
	irqSet(IRQ_VCOUNT, VcountHandler);
	irqEnable(IRQ_VBLANK | IRQ_VCOUNT);

	//irqSet(IRQ_WIFI, Wifi_Interrupt);
	//irqEnable(IRQ_WIFI);

{ // sync with arm9 and init wifi
        u32 fifo_temp;

          while(1) { // wait for magic number
        while(REG_IPC_FIFO_CR&IPC_FIFO_RECV_EMPTY) swiWaitForVBlank();
      fifo_temp=REG_IPC_FIFO_RX;
      if(fifo_temp==0x12345678) break;
        }
        while(REG_IPC_FIFO_CR&IPC_FIFO_RECV_EMPTY) swiWaitForVBlank();
        fifo_temp=REG_IPC_FIFO_RX; // give next value to wifi_init
        //Wifi_Init(fifo_temp);

        irqSet(IRQ_FIFO_NOT_EMPTY,arm7_fifo); // set up fifo irq
        irqEnable(IRQ_FIFO_NOT_EMPTY);
        REG_IPC_FIFO_CR = IPC_FIFO_ENABLE | IPC_FIFO_RECV_IRQ;

        //Wifi_SetSyncHandler(arm7_synctoarm9); // allow wifi lib to notify arm9
  } // arm7 wifi init complete

//	SoundSetTimer(0);

	// Keep the ARM7 idle
	while (1){
		swiWaitForVBlank();
	}
}


