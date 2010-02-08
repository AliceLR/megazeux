#include <nds.h>
#include <nds/arm7/serial.h>
#include "soundcommon.h"

void SoundVBlankIrq(void)
{
	//REG_IME = 0;
	int channel;

	if(soundsystem->cmd & INIT)
	{
		SoundSetTimer(soundsystem->period);
		soundsystem->cmd &= ~INIT;
	}
	else if(soundsystem->cmd & MIXING)
	{
		SoundSwapAndMix();
	}
	if(soundsystem->cmd & MIX)
	{
		channel = soundsystem->channel;

		if(soundsystem->format == 8)
		{
			SCHANNEL_CR(0) = 0;
			SCHANNEL_TIMER(0) = 0x10000 - soundsystem->period;
			SCHANNEL_SOURCE(0) = (u32)soundsystem->mixbuffer;
			SCHANNEL_REPEAT_POINT(0) = 0;
			SCHANNEL_LENGTH(0) = soundsystem->buffersize >> 2;
			SCHANNEL_CR(0) = SCHANNEL_ENABLE | SOUND_REPEAT | SOUND_VOL(127) | SOUND_PAN(64) | SOUND_8BIT;
		}
		if(soundsystem->format == 16)
		{
			SCHANNEL_CR(0) = 0;
			SCHANNEL_TIMER(0) = 0x10000 - soundsystem->period;
			SCHANNEL_SOURCE(0) = (u32)soundsystem->mixbuffer;
			SCHANNEL_REPEAT_POINT(0) = 0;
			SCHANNEL_LENGTH(0) = soundsystem->buffersize >> 2;
			SCHANNEL_CR(0) = SCHANNEL_ENABLE | SOUND_REPEAT | SOUND_VOL(127) | SOUND_PAN(64) | SOUND_16BIT;
		}

		soundsystem->cmd &= ~MIX;
		soundsystem->cmd |= MIXING;
	}
	//REG_IME = 1;
}
void SoundSetTimer(int period)
{
	if(!period)
	{
		TIMER0_DATA = 0;
		TIMER0_CR = 0;
		TIMER1_DATA = 0;
		TIMER1_CR = 0;
	}
	else
	{
		TIMER0_DATA = 0x10000 - (period * 2);
		TIMER0_CR = TIMER_ENABLE | TIMER_DIV_1;

		TIMER1_DATA = 0;
		TIMER1_CR = TIMER_ENABLE | TIMER_CASCADE | TIMER_DIV_1;
	}
}

void SoundSwapAndMix(void)
{
	s32 curtimer,numsamples;

	curtimer = TIMER1_DATA;

	numsamples = curtimer - soundsystem->prevtimer;

	if(numsamples < 0) numsamples += 65536;

	soundsystem->prevtimer = curtimer;
	soundsystem->numsamples = numsamples;

	SendCommandToArm9(UPDATEON_ARM9);
}
void SoundFifoHandler(void)
{
	u32 command;

	if (!(REG_IPC_FIFO_CR & IPC_FIFO_RECV_EMPTY))
	{
		command = REG_IPC_FIFO_RX;
		
		switch(command)
		{
		case FIFO_NONE:
			break;
		case MIXCOMPLETE_ONARM9:
			REG_IME = 0;
			soundsystem->soundcursor += soundsystem->numsamples;
			if(soundsystem->format == 8)
				while (soundsystem->soundcursor > soundsystem->buffersize) soundsystem->soundcursor -= soundsystem->buffersize;
			else
				while (soundsystem->soundcursor > (soundsystem->buffersize >> 1)) soundsystem->soundcursor -= (soundsystem->buffersize >> 1);
			REG_IME = 1;
			break;
		}
	}
}
void SendCommandToArm9(u32 command)
{
    while (REG_IPC_FIFO_CR & IPC_FIFO_SEND_FULL);
    if (REG_IPC_FIFO_CR & IPC_FIFO_ERROR)
    {
        REG_IPC_FIFO_CR |= IPC_FIFO_SEND_CLEAR;
    } 
    
    REG_IPC_FIFO_TX = command;
}
