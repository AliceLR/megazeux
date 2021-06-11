/*
 * This source code is public domain.
 *
 * Authors: Alice Rowan <petrifiedrowan@gmail.com>
*/

////////////////////////////////////////////////////////////
// General Digital Music module loader
////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "sndfile.h"

static const DWORD GDM_SIG = 0xfe4d4447; // GDM\xFE
static const DWORD GMFS_SIG = 0x53464d47; // GMFS

typedef struct tagFILEHEADERGDM
{
	BYTE sig[4];		// GDM\xFE
	char name[32];
	char author[32];
	BYTE eof[3];		// \x0D\x0A\x1A
	BYTE sig2[4];		// GMFS
	BYTE gdm_ver_major;
	BYTE gdm_ver_minor;
	BYTE tracker_id[2];
	BYTE tracker_ver_major;
	BYTE tracker_ver_minor;
	BYTE panning[32];
	BYTE globalvol;
	BYTE default_speed;
	BYTE default_bpm;
	BYTE origfmt[2];
	BYTE ordersPos[4];
	BYTE nOrders;
	BYTE patternsPos[4];
	BYTE nPatterns;
	BYTE samplesPos[4];
	BYTE sampleDataPos[4];
	BYTE nSamples;
	BYTE messagePos[4];
	BYTE messageLength[4];
	BYTE ignore[12];
} FILEHEADERGDM;

typedef struct tagSAMPLEGDM
{
	enum SAMPLEGDMFLAGS
	{
		S_LOOP = (1<<0),
		S_16BIT = (1<<1),
		S_VOLUME = (1<<2),
		S_PAN = (1<<3),
		S_LZW = (1<<4),
		S_STEREO = (1<<5)
	};

	char name[32];
	char dosname[12];
	BYTE ignore;
	BYTE length[4];
	BYTE loopstart[4];
	BYTE loopend[4];
	BYTE flags;
	BYTE c4rate[2];
	BYTE volume;
	BYTE panning;
} SAMPLEGDM;

static WORD fixu16(const BYTE *val)
{
	return (val[1] << 8) | val[0];
}

static DWORD fixu32(const BYTE val[4])
{
	return (val[3] << 24) | (val[2] << 16) | (val[1] << 8) | val[0];
}

static void GDM_TranslateEffect(const FILEHEADERGDM *pfh, MODCOMMAND &ev, UINT channel, UINT effect, UINT param)
//--------------------------------------------------------------------------------------------------------------
{
	// Note due to the limitations of libmodplug's volume commands,
	// anything that relied on multiple simultaneous non-volume effects
	// is not going to work. Only UltraTracker GDMs should really cause
	// this, and it's likely that none exist in the wild.
	static const BYTE translate_effects[32] =
	{
		CMD_NONE,
		CMD_PORTAMENTOUP,
		CMD_PORTAMENTODOWN,
		CMD_TONEPORTAMENTO,
		CMD_VIBRATO,
		CMD_TONEPORTAVOL,
		CMD_VIBRATOVOL,
		CMD_TREMOLO,
		CMD_TREMOR,
		CMD_OFFSET,
		CMD_VOLUMESLIDE,
		CMD_POSITIONJUMP,
		CMD_VOLUME,
		CMD_PATTERNBREAK,
		CMD_MODCMDEX,
		CMD_SPEED,

		CMD_ARPEGGIO,
		CMD_NONE, // Set Internal Flag
		CMD_RETRIG,
		CMD_GLOBALVOLUME,
		CMD_FINEVIBRATO,
		CMD_NONE,
		CMD_NONE,
		CMD_NONE,
		CMD_NONE,
		CMD_NONE,
		CMD_NONE,
		CMD_NONE,
		CMD_NONE,
		CMD_NONE,
		CMD_NONE, // Special-- default to nothing.
		CMD_TEMPO
	};

	if (effect >= 32) return;

	switch (effect)
	{
	case 0x0c: // Volume
		// Prefer volume command over regular command.
		if (!ev.volcmd)
		{
			ev.volcmd = VOLCMD_VOLUME;
			ev.vol = (param > 64) ? 64 : param;
		} else
		{
			ev.command = CMD_VOLUME;
			ev.param = param;
		}
		break;

	case 0x0e: // Extended
		// Most of these are effectively MOD extended commands, but
		// the fine volslide and portamento commands need to be
		// converted back to their S3M equivalents.
		switch ((param & 0xf0) >> 4)
		{
		case 0x01: // Fine porta up.
			ev.command = CMD_PORTAMENTOUP;
			ev.param = 0xf0 | (param & 0x0f);
			break;

		case 0x02: // Fine porta down.
			ev.command = CMD_PORTAMENTODOWN;
			ev.param = 0xf0 | (param & 0x0f);
			break;

		case 0x08: // Extra fine porta up.
			ev.command = CMD_PORTAMENTOUP;
			ev.param = 0xe0 | (param & 0x0f);
			break;

		case 0x09: // Extra fine porta down.
			ev.command = CMD_PORTAMENTODOWN;
			ev.param = 0xe0 | (param & 0x0f);
			break;

		case 0x0a: // Fine volslide up.
			if (param & 0x0f)
			{
				ev.command = CMD_VOLUMESLIDE;
				ev.param = 0x0f | ((param & 0x0f) << 4);
			}
			break;

		case 0x0b: // Fine volslide down.
			if (param & 0x0f)
			{
				ev.command = CMD_VOLUMESLIDE;
				ev.param = 0xf0 | (param & 0x0f);
			}
			break;

		default:
			ev.command = CMD_MODCMDEX;
			ev.param = param;
			break;
		}
		break;

	case 0x1e: // Special
		switch ((param & 0xf0) >> 4)
		{
		case 0x00: // Sample control.
			// Only surround on is emitted by 2GDM.
			// This comes from XA4, so convert it back.
			if ((param & 0x0f) == 1)
			{
				ev.command = CMD_PANNING8;
				ev.param = 0xA4;
			}
			break;

		case 0x08: // Set Pan Position
			// Prefer volume command over regular command if this
			// this comes from effect channel 0.
			if (!ev.volcmd && channel == 0)
			{
				ev.volcmd = VOLCMD_PANNING;
				ev.vol = (param << 2) + 2;
			} else
			{
				ev.command = CMD_PANNING8;
				ev.param = (param << 3) + 4;
			}
			break;
		}
		break;

	default:
		ev.command = translate_effects[effect];
		ev.param = param;
		break;
	}
}

BOOL CSoundFile::ReadGDM(const BYTE *lpStream, DWORD dwMemLength)
//---------------------------------------------------------------
{
	const FILEHEADERGDM *pfh = (const FILEHEADERGDM *)lpStream;
	const SAMPLEGDM *psmp;
	BYTE sflags[256];
	DWORD pos;

	if ((!lpStream) || (dwMemLength < sizeof(FILEHEADERGDM))) return FALSE;
	if ((fixu32(pfh->sig) != GDM_SIG) || (fixu32(pfh->sig2) != GMFS_SIG)) return FALSE;

	DWORD ordersPos = fixu32(pfh->ordersPos);
	DWORD patternsPos = fixu32(pfh->patternsPos);
	DWORD samplesPos = fixu32(pfh->samplesPos);
	DWORD sampleDataPos = fixu32(pfh->sampleDataPos);
	DWORD messagePos = fixu32(pfh->messagePos);
	DWORD messageLen = fixu32(pfh->messageLength);
	UINT nPatterns = pfh->nPatterns + 1;
	UINT nOrders = pfh->nOrders + 1;
	UINT nSamples = pfh->nSamples + 1;

	if (nPatterns > MAX_PATTERNS) nPatterns = MAX_PATTERNS;
	if (nOrders > MAX_ORDERS) nOrders = MAX_ORDERS;
	if (nSamples >= MAX_SAMPLES) nSamples = MAX_SAMPLES - 1;

	if ((ordersPos < sizeof(FILEHEADERGDM)) || (ordersPos >= dwMemLength) || (ordersPos + nOrders > dwMemLength) ||
	    (patternsPos < sizeof(FILEHEADERGDM)) || (patternsPos >= dwMemLength) ||
	    (samplesPos < sizeof(FILEHEADERGDM)) || (samplesPos >= dwMemLength) ||
	    (samplesPos + sizeof(SAMPLEGDM) * pfh->nSamples > dwMemLength) ||
	    (sampleDataPos < sizeof(FILEHEADERGDM)) || (sampleDataPos >= dwMemLength))
		return FALSE;

	// Most GDMs were converted from S3M and BWSB generally behaves like an S3M
	// player, so assuming S3M behavior and quirks is a fairly safe bet.
	m_nType = MOD_TYPE_GDM | MOD_TYPE_S3M;
	m_nMinPeriod = 64;
	m_nMaxPeriod = 32767;
	m_nDefaultGlobalVolume = pfh->globalvol << 2;
	m_nDefaultTempo = pfh->default_bpm;
	m_nDefaultSpeed = pfh->default_speed;
	m_nChannels = 0;
	m_nSamples = nSamples;

	// Get initial panning.
	for (UINT i = 0; i < 32; i++)
	{
		if (pfh->panning[i] < 16)
		{
			ChnSettings[i].nPan = (pfh->panning[i] << 4) + 8;
		}
		// TODO 16=surround
	}

	// Title and message.
	memcpy(m_szNames[0], pfh->name, 32);
	m_szNames[0][31] = '\0';

	if ((messagePos < dwMemLength) && (messagePos + messageLen < dwMemLength))
	{
		m_lpszSongComments = new char[messageLen + 1];
		memcpy(m_lpszSongComments, lpStream + messagePos, messageLen);
		m_lpszSongComments[messageLen] = '\0';
	}

	// Samples.
	psmp = (const SAMPLEGDM *)(lpStream + samplesPos);
	for (UINT nins = 0; nins < nSamples; nins++)
	{
		const SAMPLEGDM &smp = psmp[nins];
		MODINSTRUMENT &ins = Ins[nins + 1];

		DWORD len = fixu32(smp.length);
		DWORD loopstart = fixu32(smp.loopstart);
		DWORD loopend = fixu32(smp.loopend);

		UINT flags = smp.flags;

		// Note: BWSB and 2GDM don't support LZW, stereo samples, sample panning.
		if (flags & SAMPLEGDM::S_16BIT)
		{
			sflags[nins] = RS_PCM16U;
			// Due to a 2GDM bug, the sample size is halved.
			// (Note BWSB doesn't even check for these anyway.)
			len /= 2;
		}
		else
			sflags[nins] = RS_PCM8U;

		if (len > MAX_SAMPLE_LENGTH) len = MAX_SAMPLE_LENGTH;
		if (loopend > len) loopend = len;
		if (loopstart > loopend) loopstart = loopend = 0;

		ins.nLength = len;
		ins.nLoopStart = loopstart;
		ins.nLoopEnd = loopend;
		if (loopend && (flags & SAMPLEGDM::S_LOOP)) ins.uFlags |= CHN_LOOP;

		ins.nC4Speed = fixu16(smp.c4rate);
		ins.nVolume = (flags & SAMPLEGDM::S_VOLUME) && (smp.volume <= 64) ? (smp.volume << 2) : 256;
		ins.nGlobalVol = 64;
		ins.nPan = 128;

		memcpy(m_szNames[nins], smp.name, 32);
		m_szNames[nins][31] = '\0';

		memcpy(ins.name, smp.dosname, 12);
		ins.name[13] = '\0';
	}

	// Order table.
	memcpy(Order, lpStream + ordersPos, pfh->nOrders + 1);

	// Scan patterns to get pattern row counts and the real module channel count.
	// Also do bounds checks to make sure the patterns can be safely loaded.
	pos = patternsPos;
	for (UINT npat = 0; npat < nPatterns && pos + 2 <= dwMemLength; npat++)
	{
		UINT patLen = fixu16(lpStream + pos);
		DWORD patEnd = pos + patLen;
		UINT rows = 0;
		UINT channel;

		if (patEnd > dwMemLength) return FALSE;

		pos += 2;
		while (pos < patEnd)
		{
			rows++;

			while (pos < patEnd)
			{
				BYTE dat = lpStream[pos++];
				if (!dat)
					break;

				channel = dat & 0x1f;
				if (channel >= m_nChannels)
					m_nChannels = channel + 1;

				// Note and sample.
				if (dat & 0x20)
				{
					if (pos + 2 > patEnd) return FALSE;
					pos += 2;
				}

				// Effects.
				if (dat & 0x40)
				{
					do
					{
						if (pos + 2 > patEnd) return FALSE;
						dat = lpStream[pos];
						pos += 2;
					} while (dat & 0x20);
				}
			}
		}

		PatternSize[npat] = rows;
	}

	// Load patterns.
	pos = patternsPos;
	for (UINT npat = 0; npat < nPatterns && pos + 2 <= dwMemLength; npat++)
	{
		UINT patLen = fixu16(lpStream + pos);
		DWORD patEnd = pos + patLen;
		UINT row = 0;

		pos += 2;

		Patterns[npat] = AllocatePattern(PatternSize[npat], m_nChannels);
		if (!Patterns[npat]) break;

		MODCOMMAND *m = Patterns[npat];
		while (pos < patEnd)
		{
			while (pos < patEnd)
			{
				BYTE dat = lpStream[pos++];
				if (!dat)
					break;

				BYTE channel = dat & 0x1f;
				MODCOMMAND &ev = m[row * m_nChannels + channel];

				if (dat & 0x20)
				{
					BYTE note = lpStream[pos++];
					BYTE smpl = lpStream[pos++];

					if (note) // This can be 0, indicating no note (see STARDSTM.GDM).
					{
						BYTE octave = (note & 0x70) >> 4;
						note = octave * 12 + (note & 0x0f) + 12;
					}

					ev.note = note;
					ev.instr = smpl;
				}

				if (dat & 0x40)
				{
					BYTE cmd, param;
					do
					{
						cmd = lpStream[pos++];
						param = lpStream[pos++];
						GDM_TranslateEffect(pfh, ev, (cmd & 0xc0) >> 6, (cmd & 0x01f), param);

					} while (cmd & 0x20);
				}
			}

			row++;
		}
	}

	// Reading Samples
	pos = sampleDataPos;
	for (UINT n = 0; n < nSamples; n++)
	{
		MODINSTRUMENT &ins = Ins[n + 1];
		UINT len = ins.nLength;

		if (pos >= dwMemLength) break;
		if (len)
		{
			len = ReadSample(&ins, sflags[n], (LPSTR)(lpStream + pos), dwMemLength - pos);
			pos += len;
		}
	}

	return TRUE;
}
