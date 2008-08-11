;$Author:   DCODY  $
;$Date:   04 Sep 1992 16:40:48  $
;$Header:   X:/sccs/inc/common.inv   1.2   04 Sep 1992 16:40:48   DCODY  $
;$Log:   X:/sccs/inc/common.inv  $
;
;   Rev 1.2   04 Sep 1992 16:40:48   DCODY
;
;   Rev 1.1   25 Jun 1992 23:23:26   DCODY
;PAS2 update
;
;   Rev 1.0   15 Jun 1992 09:58:38   BCRANE
;Initial revision.
;$Logfile:   X:/sccs/inc/common.inv  $
;$Modtimes$
;$Revision:   1.2  $
;$Workfile:   common.inc  $ 

;   /*\
;---|*|----====< COMMON.INC >====----
;---|*|
;---|*| This file contains the Media Vision hardware definition equates, and.
;---|*| the PC hardware environment.
;---|*|
;---|*| Copyright (c) 1991,1992 Media Vision, Inc.  All Rights Reserved.
;---|*|
;   \*/

; Hardware associated with the PAS1/PAS2/CDPC

DEFAULT_BASE	EQU	00388h		;; default base I/O address
ALT_BASE_1	EQU	00384h		;; first alternate address
ALT_BASE_2	EQU	0038Ch		;; second alternate address
ALT_BASE_3	EQU	00288h		;; third alternate address
USE_ACTIVE_ADDR EQU	00000h		;; uses what is currently active

DUALFM          EQU     1               ;; Dual FM chips
DUALMIXER	EQU	1		;; Dual mixers
FILTER		EQU	1		;; Has filter after input mixer
VOLUME		EQU	1		;; Has total volume control

TEXTVERSIONHI	EQU	'01'            ;; VERSION 01.00
TEXTVERSIONLO	EQU	'00'

SYSSPKRTMR	EQU	00042h		;; System Speaker Timer Address
SYSTMRCTLR	EQU	00043h		;; System Timer Control Register
SYSSPKRREG	EQU	00061h		;; System Speaker Register
JOYSTICK	EQU	00201h		;; Joystick Register
LFMADDR 	EQU	00388h		;; Left  FM Synthesizer Address Register
LFMDATA 	EQU	00389h		;; Left  FM Synthesizer Data Register
RFMADDR 	EQU	0038Ah		;; Right FM Synthesizer Address Register
RFMDATA 	EQU	0038Bh		;; Right FM Synthesizer Data Register

AUXADDR 	EQU	00788h		;; Auxiliary Register
AUXDATA 	EQU	00789h		;; Auxiliary Register
DFMADDR 	EQU	00788h		;; Dual  FM Synthesizer Address Register
DFMDATA 	EQU	00789h		;; Dual  FM Synthesizer Data Register

pAUDIOMIXR      EQU     0078Bh          ;; Parallel interface Audio Mixer Reg
AUDIOMIXR       EQU     00B88h          ;; Audio Mixer Control Register
AUDIOFILT	EQU	00B8Ah		;; Audio Filter Control Register
INTRCTLRST	EQU	00B89h		;; Interrupt Control Status Register
INTRCTLR	EQU	00B8Bh		;; Interrupt Control Register write
INTRCTLRRB	EQU	00B8Bh		;; Interrupt Control Register read back
PCMDATA 	EQU	00F88h		;; PCM data I/O register (low byte)
PCMDATAH	EQU	00F89h		;; PCM data I/O register (high byte)
CROSSCHANNEL	EQU	00F8Ah		;; Cross Channel Register
SAMPLERATE	EQU	01388h		;; (t0) Sample Rate Timer Register
SAMPLECNT	EQU	01389h		;; (t1) Sample Count Register
SPKRTMR 	EQU	0138Ah		;; (t2) Local Speaker Timer Address
TMRCTLR 	EQU	0138Bh		;; Local Timer Control Register
MDIRQVECT	EQU	01788H		;; MIDI-0 IRQ Vector Register
MDSYSCTLR	EQU	01789H		;; MIDI-1 System Control Register
MDSYSSTAT	EQU	0178AH		;; MIDI-2 IRQ Status Register
MDIRQCLR	EQU	0178BH		;; MIDI-3 IRQ Clear Register
MDGROUP4	EQU	01B88H		;; MIDI-4 Group #1 Register (MDGROUP1)
MDGROUP5	EQU	01B89H		;; MIDI-5 Group #2 Register (MDGROUP2)
MDGROUP6	EQU	01B8AH		;; MIDI-6 Group #3 Register (MDGROUP3)
MDGROUP7	EQU	01B8BH		;; MIDI-7 Group #4 Register (MDGROUP4)

;
; Factory Default Settings
;
DEFAULTDMA	equ	1		;; DMA channel 1
DEFAULTIRQ	equ	7		;; IRQ channel 7
DEFAULTINT	equ	65h		;; Interrupt # for software interface

;
; mixer select
;
OUTPUTMIXER	equ	00h		;; output mixer H/W select
INPUTMIXER	equ	40h		;; input mixer select
DEFMIXER	equ	-1		;; use last mixer selected
MIXERMAX	equ	1fh		;; maximum mixer setting

MVVOLUMEMAX	equ	3fh		;; MVA508 maximum mixer setting
NSVOLUMEMAX	equ	28h		;; National maximum mixer setting

EQUALIZERMAX    equ     0ch             ;; maximum equalizer setting
EQUALIZERMID	equ	06h		;; maximum mid setting

;
;	Filter register bits
;
fFIdatabits	equ	00011111B	;; filter select and decode field bits
fFImutebits	equ	00100000B	;; filter mute field bits
fFIpcmbits	equ	11000000B	;; filter sample rate field bits
bFImute 	equ	00100000B	;; filter mute bit
bFIsrate	equ	01000000B	;; filter sample rate timer mask
bFIsbuff	equ	10000000B	;; filter sample buffer counter mask
FILTERMAX	equ	6		;; six possible settings

;
;	Cross Channel Bit definitions
;
fCCcrossbits	EQU	00001111B	;; cross channel bit field
fCCpcmbits	EQU	11110000B	;; pcm/dma control bit field
bCCr2r		EQU	00000001B	;; CROSSCHANNEL Right to Right
bCCl2r		EQU	00000010B	;; CROSSCHANNEL Left  to Right
bCCr2l		EQU	00000100B	;; CROSSCHANNEL Right to Right
bCCl2l		EQU	00001000B	;; CROSSCHANNEL Left  to Left
bCCdac		EQU	00010000B	;; DAC/ADC Control
bCCmono 	EQU	00100000B	;; PCM Monaural Enable
bCCenapcm	EQU	01000000B	;; Enable PCM state machine
bCCdrq		EQU	10000000B	;; Enable DRQ bit

;
;	Interrupt Control Register Bits
;
fICintmaskbits	EQU	00011111B	;; interrupt mask field bits
fICrevbits	EQU	11100000B	;; revision mask field bits
fICidbits	EQU	11100000B	;; Board revision ID field bits
bICleftfm	EQU	00000001B	;; Left FM interrupt enable
bICritfm	EQU	00000010B	;; Right FM interrupt enable
bICsamprate	EQU	00000100B	;; Sample Rate timer interrupt enable
bICsampbuff	EQU	00001000B	;; Sample buffer timer interrupt enable
bICmidi 	EQU	00010000B	;; MIDI interrupt enable
   fICrevshr	EQU	5		;; rotate rev bits to lsb

;
;	Interrupt Status Register Bits
;
fISints 	EQU	00011111B	;; Interrupt bit field
bISleftfm       EQU     00000001B       ;; Left FM interrupt active
bISritfm	EQU	00000010B	;; Right FM interrupt active
bISsamprate	EQU	00000100B	;; Sample Rate timer interrupt active
bISsampbuff	EQU	00001000B	;; Sample buffer timer interrupt active
bISmidi 	EQU	00010000B	;; MIDI interrupt active
bISPCMlr        EQU     00100000B       ;; PCM left/right active
bISActive	EQU	01000000B	;; Hardware is active (not in reset)
bISClip 	EQU	10000000B	;; Sample Clipping has occured

;
;       cross channel channel #s
;
RIGHT2RIGHT	equ	00h		;; right to right
LEFT2RIGHT	equ	01h		;; left  to right
RIGHT2LEFT	equ	02h		;; right to left
LEFT2LEFT	equ	03h		;; left  to left

;
; left/right mixer channel selection
;

;	left channel values

L_FM		equ	01h
L_IMIXER	equ	02h
L_EXT		equ	03h
L_INT		equ	04h
L_MIC		equ	05h
L_PCM		equ	06h
L_SPEAKER	equ	07h
L_FREE		equ	00h
L_SBDAC 	equ	00h

;	right channel values

R_FM		equ	08h
R_IMIXER	equ	09h
R_EXT		equ	0Ah
R_INT		equ	0Bh
R_MIC		equ	0Ch
R_PCM		equ	0Dh
R_SPEAKER	equ	0Eh
R_FREE		equ	0Fh
R_SBDAC 	equ	0Fh

;	Mixer register bits

fMImixerbits    EQU     00010111B       ;; mixer control bit fields
fMIspkrbits	EQU	01000000B	;; speaker integrator field bits
bMIdata 	EQU	00000001B	;; data bit
bMIclock	EQU	00000010B	;; clock strobe
bMImistrb	EQU	00000100B	;; mixer output strobe
bMIna1		EQU	00001000B	;; not used
bMIvol		EQU	00010000B	;; total volume enabled
bMIna2		EQU	00100000B	;; not used
bMIspkrint	EQU	01000000B	;; speaker integrator
bMImonofm	EQU	10000000B	;; make both FMs mono

bMIfmreset	EQU	bMIdata 	;; OPL3 FM chip reset
bMIdacreset	EQU	bMIclock	;; CODEC reset
bMIsbreset	EQU	bMImistrb	;; SB microprocessor reset
bMI508reset	EQU	bMIvol		;; MVA508 reset

;	volume control channel #s

VOLMUTE 	equ	40h		;; MUTE button
VOLLOUDENH	equ	41h		;; LOUDNESS and ENHANCED STEREO switch
VOLBASS 	equ	42h		;; BASS level setting
VOLTREBLE	equ	43h		;; TREBLE level setting
VOLLEFT 	equ	44h		;; MASTER LEFT LEVEL settting
VOLRIGHT	equ	45h		;; MASTER RIGHT LEVEL settting
VOLMODE 	equ	46h		;; Model Select Left/Stereo/Right

bVOLEbass	equ	0001b		;; enhanced bass bit
bVOLEstereo	equ	0010b		;; enhanced stereo bit

;	output control

pmADDRSELECT	equ	80h		;; Parallel mixer addr select
pmDATASELECT	equ	00h		;; Parallel mixer data select

;	mixer channel programming selection

pmCHANNELLR	equ	00h		;; Left/Right channel select
pmCHANNELL	equ	20h		;; Left  channel only select
pmCHANNELR	equ	40h		;; Right channel only select

;	device select

pmMIXERSELECT	equ	10h		;; Parallel Mixer device select
pmVOLUMESELECT	equ	00h		;; Parallel Volume device select

;	Volume Device selects

pmVOLUMEA	equ	01h		;; Left/Right channel select
pmVOLUMEB	equ	02h		;; Left/Right channel select
pmVOLUMEBASS	equ	03h		;; Left/Right channel select
pmVOLUMETREB	equ	04h		;; Left/Right channel select
pmVOLUMEMODE	equ	05h		;; Left/Right channel select

;	mixer selection

pmOUTPUTMIXER	equ	00h		;; Mixer-A selection
pmINPUTMIXER	equ	20h		;; Mixer-B selection

;	mixer channel swap

pmCHSWAP	equ	40h		;; Mixer channel reroute
					;;
;	int 2F application ID codes

INT2FCODE1	equ	0BCH		;; Bryan's initials

;	int 2F ID (func 0) return register values

INT2FREGBX	equ	6D00h		;; 'm '
INT2FREGCX	equ	0076h		;; ' v'
INT2FREGDX	equ	2020h		;; UPPERCASE XOR MASK

; hardware specific equates for the MV101 (digital ASIC)

MASTERADDRP	equ	09a01h		;; Master Address Pointer    (w)
MIDIPRESCALE	equ	01788h		;; MIDI prescale	     (r/w)
MIDITIMER	equ	01789h		;; MIDI Timer		     (r/w)
MIDIDATA	equ	0178ah		;; MIDI Data		     (r/w)
MIDICONTROL	equ	0178bh		;; MIDI Control 	     (r/w)
MIDISTATUS	equ	01b88h		;; MIDI Status		     (r/w)
MIDIFIFOS	equ	01b89h		;; MIDI Fifo Status	     (r/w)
MIDICOMPARE	equ	01b8ah		;; MIDI Compare Time	     (r/w)
MIDITEST	equ	01b8bh		;; MIDI Test		     (w)
MASTERCHIPR	equ	0ff88h		;; Master Chip Rev	     (r)
SLAVECHIPR	equ	0ef88h		;; Slave Chip Rev	     (r)
ENHANCEDSCSI	equ	07f89h		;; Enhanced SCSI detect port
SYSCONFIG1      equ     08388h          ;; System Config 1           (r/w)
SYSCONFIG2	equ	08389h		;; System Config 2	     (r/w)
SYSCONFIG3	equ	0838ah		;; System Config 3	     (r/w)
SYSCONFIG4	equ	0838bh		;; System Config 4	     (r/w)
IOCONFIG1	equ	0f388h		;; I/O Config 1 	     (r/w)
IOCONFIG2	equ	0f389h		;; I/O Config 2 	     (r/w)
IOCONFIG3	equ	0f38ah		;; I/O Config 3 	     (r/w)
IOCONFIG4	equ	0f38bh		;; I/O Config 4 	     (r/w)
COMPATREGE	equ	0f788h		;; Compatible Rgister Enable (r/w)
EMULADDRP	equ	0f789h		;; Emulation Address Pointer (r/w)
WAITSTATE	equ	0bf88h		;; Wait State		     (r/w)
PUSHBUTTON	equ	0e388h		;; Push Button (slave)	     (???)
AUXINTSTAT	equ	0e38ah		;; Aux Int Status	     (???)
AUXINTENA	equ	0e38bh		;; Aux Int Enable	     (???)
OVRSMPPRE	equ	0bf8ah		;; Over Sample Prescale      (r/w)
ANALSERD	equ	0bf89h		;; Analog Chip Serial Data   (w)
MASTERMODRD	equ	0ff8bh		;; Master Mode Read	     (r)
SLAVEMODRD	equ	0ef8bh		;; Slave Mode Read	     (r)
INTWATCHDOG	equ	0fb8bh		;; Interrupt Watch Dog	     (???)
MASTERuPDATA	equ	0fb88h		;; Master uP Data	     (???)
MASTERuPCMD	equ	0fb89h		;; Master uP Command/Status  (???)
MASTERuPRST	equ	0fb8ah		;; Master uP Restart	     (???)
SLAVEuPDATA	equ	0eb88h		;; Slave uP Data	     (???)
SLAVEuPCMD	equ	0eb88h		;; Slave uP Command/Status   (???)
SLAVEuPRST	equ	0eb88h		;; Slave uP Restart	     (???)
CDTOCOUNTER	equ	04388h		;; CD-ROM timeout counter    (r/w)
CDTOSTAT	equ	04389h		;; CD-ROM timeout status     (r/w)
LEFTVURD	equ	02388h		;; Left VU Read 	     (r)
RITVURD 	equ	02389h		;; Right VU Read	     (r)

SBRST		equ	00206h		;; SB Reset		     (w)
SBDATA		equ	0020Ah		;; SB Data Read 	     (r)
SBCMD		equ	0020Ch		;; SB CMD Write/Status Read  (r/w)
SBSTAT		equ	0020Eh		;; SB Data Status	     (r)
MPUDATA 	equ	00300h		;; MPU-401 data reg	     (r/w)
MPUCMD		equ	00301h		;; MPU-401 command reg	     (r/w)

; Sys Config 1

bSC1timena      equ     00000001b       ;; shadow enable
bSC1pcmemu	equ	00000010b	;; PCM Emulation of PAS1
bSC128mhz	equ	00000100b	;; 28mhz clock divisor
bSC1invcom	equ	00001000b	;; invert COM port interrupt input
bSC1stspea	equ	00010000b	;; stereoize pc speaker
bSC1realsnd	equ	00100000b	;; smart real sound emulatio
bSC1d6		equ	01000000b	;;
bSC1mstrst	equ	10000000b	;; master chip reset

; Sys Config 2

bSC2ovrsmp      equ     00000011b       ;; oversampling 0,1,2,4
bSC216bit	equ	00000100b	;; 16 bit audio
bSC212bit	equ	00001000b	;; 12 bit interleaving (d2 must be set too)
bSC2msbinv	equ	00010000b	;; invert MSB from standard method
bSC2slavport	equ	01100000b	;; slave port bits
bSC2vcolock	equ	10000000b	;; VCO locked (Sample Rate Clock Valid)

; Sys Config 3

bSC328mhzfil    equ     00000001b       ;; PCM Rate uses 28mhz
bSC31mhzsb	equ	00000010b	;; 1mhz timer for SB sample rate
bSC3vcoinv	equ	00000100b	;; invert VCO output
bSC3bclkinv	equ	00001000b	;; invert BCLK form 16 bit DAC
bSC3lrsync	equ	00010000b	;; 0=L/R, 1 = Sync Pulse
bSC3d5		equ	00100000b	;;
bSC3d6		equ	01000000b	;;
bSC3d7		equ	10000000b	;;

; Sys Config 4

bSC4drqahi      equ     00000001b       ;; DRQ from drive active high
bSC4dackahi	equ	00000010b	;; DRQ from drive active high
bSC4intahi	equ	00000100b	;; INT from drive active high
bSC4drqvalid	equ	00001000b	;; DRQ line valid from drive
bSC4comena	equ	00010000b	;; enable COM interrupt
bSC4enascsi	equ	00100000b	;; enable SCSI interrupt
bSC4drqptr	equ	11000000b	;; DRQ timing pointer bits

; I/O Config 1

bIC1ps2ena      equ     00000001b       ;; Enable Chip (PS2 only)
bIC1comdcd	equ	00000110b	;; COM port decode pointer
bIC1comint	equ	00111000b	;; COM port interrupt pointer
bIC1joyena	equ	01000000b	;; Enable joystick read
bIC1wporena	equ	10000000b	;; Enable warm boot reset

; I/O Config 2

bIC2dmaptr	equ	00000111b	;; DMA channel select

;;bIC28dmaptr	equ	00001111b	;;  8 bit DMA channel select
;;bIC216dmaptr	equ	11110000b	;; 16 bit DMA channel select

; I/O Config 3

bIC3pcmint      equ     00001111b       ;; pcm IRQ channel select
bIC3cdint	equ	11110000b	;; cd  IRQ channel select

; Compatibility Register

cpMPUEmulation	equ	00000001b	;; MPU emuation is on bit
cpSBEmulation	equ	00000010b	;; SB emuation is on bit

; Emulation Address Pointer

epSBptr 	equ	00001111b	;; bit field for SB emulation
epMPUptr	equ	11110000b	;; bit field for MPU emulation

; Slave Mode Read

bSMRDdrvtyp     equ     00000011b       ;; drive interface type
bSMRDfmtyp	equ	00000100b	;; FM chip type
bSMRDdactyp	equ	00001000b	;; 16 bit dac (1) or 8 bit dac (0)
bSMRDimidi	equ	00010000b	;; use internal MIDI
bSMRDswrep	equ	10000000b	;; switch is auto repeating

; Master Mode Read

bMMRDatps2      equ     00000001b       ;; AT(1) or PS2(0) bus
bMMRDtmremu	equ	00000010b	;; timer emulation enabled
bMMRDmsmd	equ	00000100b	;; master/slave mode
bMMRDslave	equ	00001000b	;; slave power on or device present
bMMRDattim	equ	00010000b	;; xt/at timing
bMMRDmstrev	equ	11100000b	;; master  rev level

; MIDI Control Register

bMCRenatstmp	equ	00000001b	;; MIDI enable time stamp interrupt
bMCRenacmptm	equ	00000010b	;; MIDI enable compare time interrupt
bMCRenafifoi	equ	00000100b	;; MIDI enable FIFO input interrupt
bMCRenafifoo	equ	00001000b	;; MIDI enable FIFO output interrupt
bMCRenafifooh	equ	00010000b	;; MIDI enable FIFO output half int
bMCRrstfifoi	equ	00100000b	;; MIDI reset Input FIFO pointer
bMCRrstfifoo	equ	01000000b	;; MIDI reset Output FIFO pointer
bMCRechoio	equ	10000000b	;; MIDI echo input to output (THRU)

; MIDI Status Register

bMSRtimstamp	equ	00000001b	;; MIDI time stamp interrupt
bMSRcmptime	equ	00000010b	;; MIDI compare time interrupt
bMSRififo	equ	00000100b	;; MIDI input FIFO data avail interrupt
bMSRofifo	equ	00001000b	;; MIDI output FIFO empty interrupt
bMSRofifohalf	equ	00010000b	;; MIDI output FIFO half empty interrupt
bMSRififoovr	equ	00100000b	;; MIDI input FIFO overrun error
bMSRofifoovr	equ	01000000b	;; MIDI output FIFO overrun error
bMSRframeerr	equ	10000000b	;; MIDI frame error

; MIDI FIFO count

bMFCififo	equ	00001111b	;; MIDI input FIFO count
bMFCofifo	equ	11110000b	;; MIDI output FIFO count

; Aux interrupt status/enable

bAUfmrit	equ	00000001b	;; FM right interrupt
bAUpushb	equ	00000010b	;; push button active
bAUslavecpu	equ	00000100b	;; slave coprocessor
bAUaux0int	equ	00001000b	;; aux 0 interrupt
bAUaux1int	equ	00010000b	;; aux 1 interrupt
bAUaux2int	equ	00100000b	;; aux 2 interrupt
bAUaux3int	equ	01000000b	;; aux 3 interrupt
bAUmastrcpu	equ	10000000b	;; master coprocessor or emulation active

; Push Buttons on the Front Panel

bPSHmute	equ	00000001b	;; mute on the front panel
bPSHdnarrow	equ	00000010b	;; down arrow on the front panel
bPSHuparrow	equ	00000100b	;; up arrow on the front panel
bPSauxbit1	equ	00001000b	;; unused bit...
bPSauxbit2	equ	00010000b	;; unused bit...
bPSauxbit3	equ	00100000b	;; unused bit...
bPSauxbit4	equ	01000000b	;; unused bit...
bPSauxbit5	equ	10000000b	;; unused bit...

;---------------------------====< PC HARDWARE >====---------------------------

; Interrupt Controller #1 Port Addresses and Interrupt Masks

IRQ1MASKREG	EQU	021h		;; 8259 mask register
IRQ1ACKREG	EQU	020h		;; 8259 INT acknowledge register

INT0MSK 	EQU	00000001B	;; interrupt 0 mask
INT1MSK 	EQU	00000010B	;; interrupt 1 mask
INT2MSK 	EQU	00000100B	;; interrupt 2 mask
INT3MSK 	EQU	00001000B	;; interrupt 3 mask
INT4MSK 	EQU	00010000B	;; interrupt 4 mask
INT5MSK 	EQU	00100000B	;; interrupt 5 mask
INT6MSK 	EQU	01000000B	;; interrupt 6 mask
INT7MSK 	EQU	10000000B	;; interrupt 7 mask

; Interrupt Controller #2 Port Addresses and Interrupt Masks

IRQ2MASKREG	EQU	0A1h		;; 8259 mask register
IRQ2ACKREG	EQU	0A0h		;; 8259 INT acknowledge register

INT8MSK 	EQU	00000001B	;; interrupt 8 mask
INT9MSK 	EQU	00000010B	;; interrupt 9 mask
INTAMSK 	EQU	00000100B	;; interrupt A mask
INTBMSK 	EQU	00001000B	;; interrupt B mask
INTCMSK 	EQU	00010000B	;; interrupt C mask
INTDMSK 	EQU	00100000B	;; interrupt D mask
INTEMSK 	EQU	01000000B	;; interrupt E mask
INTFMSK 	EQU	10000000B	;; interrupt F mask

EOI		EQU	020h		;; non specific end of interrupt

; dma controller #1 port addresses

DMAC0ADDR	EQU	000h		;; DMA channel 0 Base & Current Address
DMAC0COUNT	EQU	001h		;; DMA channel 0 Base & Current Count
DMAC1ADDR	EQU	002h		;; DMA channel 1 Base & Current Address
DMAC1COUNT	EQU	003h		;; DMA channel 1 Base & Current Count
DMAC2ADDR	EQU	004h		;; DMA channel 2 Base & Current Address
DMAC2COUNT	EQU	005h		;; DMA channel 2 Base & Current Count
DMAC3ADDR	EQU	006h		;; DMA channel 3 Base & Current Address
DMAC3COUNT	EQU	007h		;; DMA channel 3 Base & Current Count

DMA2C4ADDR	EQU	0C0h		;; DMA channel 4 Base & Current Address
DMA2C4COUNT	EQU	0C2h		;; DMA channel 4 Base & Current Count
DMA2C5ADDR	EQU	0C4h		;; DMA channel 5 Base & Current Address
DMA2C5COUNT	EQU	0C6h		;; DMA channel 5 Base & Current Count
DMA2C6ADDR	EQU	0C8h		;; DMA channel 6 Base & Current Address
DMA2C6COUNT	EQU	0CAh		;; DMA channel 6 Base & Current Count
DMA2C7ADDR	EQU	0CCh		;; DMA channel 7 Base & Current Address
DMA2C7COUNT	EQU	0CEh		;; DMA channel 7 Base & Current Count

DMARDSTAT	EQU	008h		;; DMA read status
DMAWRCNTRL	EQU	008h		;; DMA write command register
DMAWREQ 	EQU	009h		;; DMA write request register
DMAWRSMR	EQU	00Ah		;; DMA write single mask register
DMAWRMODE	EQU	00Bh		;; DMA write mode register
DMACLEAR	EQU	00Ch		;; DMA clear low/high flip-flop
DMARDTEMP	EQU	00Dh		;; DMA read temp register
DMAWRCLR	EQU	00Dh		;; DMA write master clear
DMACLRMSK	EQU	00Eh		;; DMA clear mask register
DMAWRALL	EQU	00Fh		;; DMA write all mask register bits

DMA2RDSTAT	EQU	0D0h		;; DMA read status
DMA2WRCNTRL	EQU	0D0h		;; DMA write command register
DMA2WREQ	EQU	0D2h		;; DMA write request register
DMA2WRSMR	EQU	0D4h		;; DMA write single mask register
DMA2WRMODE	EQU	0D6h		;; DMA write mode register
DMA2CLEAR	EQU	0D8h		;; DMA clear low/high flip-flop
DMA2RDTEMP	EQU	0DAh		;; DMA read temp register
DMA2WRCLR	EQU	0DAh		;; DMA write master clear
DMA2CLRMSK	EQU	0DCh		;; DMA clear mask register
DMA2WRALL	EQU	0DEh		;; DMA write all mask register bits

CH0PAGEREG	EQU	087h		;; Channel 0 Page Register
CH1PAGEREG	EQU	083h		;; Channel 1 Page Register
CH2PAGEREG	EQU	081h		;; Channel 2 Page Register
CH3PAGEREG	EQU	082h		;; Channel 3 Page Register

CH5PAGEREG	EQU	08Bh		;; Channel 5 Page Register
CH6PAGEREG	EQU	089h		;; Channel 6 Page Register
CH7PAGEREG	EQU	08Ah		;; Channel 7 Page Register

;
;   /*\
;---|*| end of COMMON.INC
;   \*/
;

