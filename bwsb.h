//ÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ
//                    Bells, Whistles, and Sound Boards
//       Copyright (c) 1993-95, Edward Schlunder. All Rights Reserved.
//ÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍ
// BWSB.H - Bells, Whistles, and Sound Boards library declaration file
//          for C/C++.
//
//          Written by Edward Schlunder (1995)
//ÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ
#include <gdmtype.h>

extern
#ifdef __cplusplus
"C"
#endif
  int cdecl LoadMSE(char *File,
                    unsigned long FileOff,
                    unsigned char OverRate,
                    unsigned int BufferSize,
                    unsigned int *Addr,
                    unsigned int *IRQ,
                    unsigned int *DMA);
extern
#ifdef __cplusplus
"C"
#endif
  char * cdecl DeviceName(void);

extern
#ifdef __cplusplus
"C"
#endif
  void cdecl FreeMSE(void);
extern
#ifdef __cplusplus
"C"
#endif
  unsigned int cdecl StartOutput(unsigned char Channels,
                                  unsigned char Amplify);
extern
#ifdef __cplusplus
"C"
#endif
  void cdecl StopOutput(void);
extern
#ifdef __cplusplus
"C"
#endif
  void cdecl MixForground(void);
extern
#ifdef __cplusplus
"C"
#endif
  int cdecl MixStatus(void);
extern
#ifdef __cplusplus
"C"
#endif
  void cdecl SetAutoMix(char MixFlag);

  // Music Routines:
extern
#ifdef __cplusplus
"C"
#endif
  void cdecl StartMusic(void);
extern
#ifdef __cplusplus
"C"
#endif
  void cdecl StopMusic(void);
extern
#ifdef __cplusplus
"C"
#endif
  void cdecl AmigaHertz(long NewSpeed);
extern
#ifdef __cplusplus
"C"
#endif
  unsigned char cdecl MusicStatus(void);
extern
#ifdef __cplusplus
"C"
#endif
  unsigned char cdecl MusicBPM(unsigned char NewBPM);
extern
#ifdef __cplusplus
"C"
#endif
  unsigned char cdecl MusicTempo(unsigned char NewTempo);
extern
#ifdef __cplusplus
"C"
#endif
  unsigned char cdecl MusicOrder(unsigned char NewOrder);
extern
#ifdef __cplusplus
"C"
#endif
  unsigned char cdecl MusicPattern(unsigned char NewPattern);
extern
#ifdef __cplusplus
"C"
#endif
  unsigned char cdecl MusicRow(void);
extern
#ifdef __cplusplus
"C"
#endif
  unsigned char cdecl MusicLoop(unsigned char LoopEnable);
extern
#ifdef __cplusplus
"C"
#endif
  unsigned char cdecl MusicVolume(unsigned char Vol);

extern
#ifdef __cplusplus
"C"
#endif
  void cdecl GetChannelTable(char Channel, int TSeg, int TOff);
extern
#ifdef __cplusplus
"C"
#endif
  int cdecl ChannelPan(unsigned char Channel, unsigned char NewPos);
extern
#ifdef __cplusplus
"C"
#endif
  int cdecl ChannelVU(unsigned char Channel, unsigned char VU);
extern
#ifdef __cplusplus
"C"
#endif
  int cdecl ChannelVol(unsigned char Channel, unsigned char NewVol);
extern
#ifdef __cplusplus
"C"
#endif
  int cdecl ChannelPos(unsigned char Channel, unsigned int NewPos);
extern
#ifdef __cplusplus
"C"
#endif
  void cdecl GetSampleTable(unsigned char Sample, int TSeg, int TOff);
extern
#ifdef __cplusplus
"C"
#endif
  void cdecl GetMainScope(unsigned int *Left, unsigned int *Right);
extern
#ifdef __cplusplus
"C"
#endif
  void cdecl PlaySample(unsigned char Channel,
                        unsigned char Sample,
                        unsigned int Rate,
                        unsigned char Vol,
                        unsigned char Pan);
extern
#ifdef __cplusplus
"C"
#endif
  void cdecl PlayNote(unsigned char Channel,
                      unsigned char Sample,
                      unsigned char Octave,
                      unsigned char Note);
extern
#ifdef __cplusplus
"C"
#endif
  void cdecl LoadGDM(int Handle, long FileOff, int *Flags,
                     GDMHeader *gdmhead);
extern
#ifdef __cplusplus
"C"
#endif
  void cdecl UnloadModule(void);
extern
#ifdef __cplusplus
"C"
#endif
  unsigned char cdecl EmsExist(void);

#ifdef __cplusplus
extern "C" int cdecl AllocSample(unsigned char SamNum, SamHeader *SamHead);
extern "C" int cdecl FreeSample(unsigned char SamNum);
extern "C" void cdecl StopBanner(void);
#else
extern int cdecl AllocSample(unsigned char SamNum, SamHeader *SamHead);
extern int cdecl FreeSample(unsigned char SamNum);
extern void cdecl StopBanner(void);
#endif
