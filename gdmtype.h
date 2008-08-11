//ÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ
//                    Bells, Whistles, and Sound Boards
//       Copyright (c) 1993-95, Edward Schlunder. All Rights Reserved.
//ÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍ
// GDMTYPE.H - GDM module header/sample type definitions.
//             Written by Edward Schlunder (1995)
//
//ÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ
typedef struct
{
  char ID[4];                          // ID: 'GDMş'
  char SongTitle[32];                  // Music's title
  char SongMusician[32];               // Name of music's composer
  char DOSEOF[3];                      // 10, 13, 26
  char ID2[4];                         // ID: 'GDMF'
  unsigned char FormMajorVer;          // Format major version
  unsigned char FormMinorVer;          // Format minor version
  unsigned int TrackID;                // Composing Tracker ID code
  unsigned char TrackMajorVer;         // Tracker's major version
  unsigned char TrackMinorVer;         // Tracker's minor version
  unsigned char PanMap[32];            // 0-Left to 15-Right, 255-N/U
  unsigned char MastVol;               // Range: 0..64
  unsigned char Tempo;                 // Initial music tempo (6)
  unsigned char BPM;                   // Initial music BPM (125)
  unsigned int FormOrigin;             // Original format ID:
   // 1-MOD, 2-MTM, 3-S3M, 4-669, 5-FAR, 6-ULT, 7-STM, 8-MED
   // (versions of 2GDM prior to v1.15 won't set this correctly)

  unsigned long OrdOffset;
  unsigned char NOO;                   // Number of orders in module
  unsigned long PatOffset;
  unsigned char NOP;                   // Number of patterns in module
  unsigned long SamHeadOffset;
  unsigned long SamOffset;
  unsigned char NOS;                   // Number of samples in module
  unsigned long MTOffset;
  unsigned long MTLength;
  unsigned long SSOffset;
  unsigned int SSLength;
  unsigned long TGOffset;
  unsigned int TGLength;
} GDMHeader;

typedef struct
{
  char SamName[32];
  char FileName[12];
  char EmsHandle;
  long Length;
  long LoopBegin;
  long LoopEnd;
  char Flags;
  int  C4Hertz;
  char Volume;
  char Pan;
  int Segment;
} SamHeader;

typedef struct
{
  char SamName[32];               // 32
  char FileName[12];              // 44
  char EmsHandle;                 // 45
  long Length;                    // 49
  long LoopBegin;                 // 53
  long LoopEnd;                   // 57
  char Flags;                     // 58
  int C4Hertz;                    // 60
  char Volume;                    // 61
  char Pan;                       // 62
} SamHeader2;

