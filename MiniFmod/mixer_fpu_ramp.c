/******************************************************************************/
/* MIXER_FPU_RAMP.C                                                           */
/* ----------------                                                           */
/* MiniFMOD public source code release.                                       */
/* This source is provided as-is.  Firelight Technologies will not support    */
/* or answer questions about the source provided.                             */
/* MiniFMOD Sourcecode is copyright (c) Firelight Technologies, 2000-2004.    */
/* MiniFMOD Sourcecode is in no way representative of FMOD 3 source.          */
/* Firelight Technologies is a registered company.                            */
/* This source must not be redistributed without this notice.                 */
/******************************************************************************/

#include "minifmod.h"
#include "sound.h"
#include "mixer.h"
#include "mixer_clipcopy.h"
#include "mixer_fpu_ramp.h"
#include "system_memory.h"

//#define ROLLED			TRUE		// this would make it smaller but quite slow
#define VOLUMERAMPING	TRUE		// enable/disable volume ramping

#pragma warning(disable:4731)

// =========================================================================================
// GLOBAL VARIABLES
// =========================================================================================

//= made global to free ebp.============================================================================
static unsigned int mix_numsamples	= 0;	// number of samples to mix 
static unsigned int mix_mixptr		= 0;
static unsigned int mix_mixbuffend	= 0;
static unsigned int mix_mixbuffptr	= 0;
static unsigned int mix_endflag		= 0;
static unsigned int mix_sptr		= 0;
static unsigned int mix_cptr		= 0;
static unsigned int mix_count		= 0;
static unsigned int mix_samplebuff	= 0;
static float		mix_leftvol		= 0;
static float		mix_rightvol	= 0;
static unsigned int mix_temp1		= 0;

static unsigned int mix_count_old	= 0;
static unsigned int mix_rampleftvol = 0;
static unsigned int mix_ramprightvol= 0;
static unsigned int mix_rampcount	= 0;
static unsigned int mix_rampspeedleft	= 0;
static unsigned int mix_rampspeedright	= 0;
unsigned int		mix_volumerampsteps	= 0;
float				mix_1overvolumerampsteps = 0;

static const float	mix_255			= 255.0f;	
static const float	mix_256			= 256.0f;	
static const float	mix_1over255	= 1.0f / 255.0f;	
static const float	mix_1over256	= 1.0f / 256.0f;
static const float	mix_1over2gig	= 1.0f / 2147483648.0f;

/*
[API]
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

	[SEE_ALSO]
]
*/

void FSOUND_Mixer_FPU_Ramp(void *mixptr, int len, char returnaddress) 
{
	static FSOUND_CHANNEL *cptr;
	static int count;
	// IMPORTANT no local variables on stack.. we are trashing EBP.  static puts values on heap.

	if (len <=0) 
		return;

	mix_numsamples	= len;
	mix_mixptr		= (unsigned int)mixptr;
	mix_mixbuffend	= (unsigned int)mix_mixptr + (mix_numsamples << 3);


	//==============================================================================================
	// LOOP THROUGH CHANNELS
	//==============================================================================================
	for (count=0; count<64; count++)
	{
		cptr = &FSOUND_Channel[count];

		__asm 
		{
		push	ebp
		mov		ebx, mix_mixptr
		mov		mix_mixbuffptr, ebx
// 		mix_mixbuffptr = mix_mixptr;

		mov		ecx, cptr
		mov		mix_cptr, ecx
// 		FSOUND_CHANNEL* _pChannel = cptr;
// 		mix_cptr = _pChannel;

		cmp		ecx, 0						// if (!cptr) ...
		je		MixExit						//			  ... then skip this channel!
// 		if (!cptr)
// 		{
// 			goto MixExit;
// 		}

		mov		ebx, [ecx]FSOUND_CHANNEL.sptr				// load the correct SAMPLE  pointer for this channel
		mov		mix_sptr, ebx				// store sample pointer away
		cmp		ebx, 0						// if (!sptr) ...
		je		MixExit						//			  ... then skip this channel!
// 		FSOUND_SAMPLE* _pSample = _pChannel->sptr;
// 		mix_sptr = _pSample;
// 		if (!_pSample)
// 		{
// 			goto MixExit;
// 		}

		// get pointer to sample buffer
		mov		eax, [ebx].buff
		mov		mix_samplebuff, eax
// 		mix_samplebuff = _pSample->buff;

		//==============================================================================================
		// LOOP THROUGH CHANNELS
		// through setup code:- usually ebx = sample pointer, ecx = channel pointer
		//==============================================================================================
		// ebx�൱��pSample,ecx�൱��pChannel
		//==============================================================================================

		//= SUCCESS - SETUP CODE FOR THIS CHANNEL ======================================================

		// =========================================================================================
		// the following code sets up a mix counter. it sees what will happen first, will the output buffer
		// end be reached first? or will the end of the sample be reached first? whatever is smallest will
		// be the mixcount.

		// first base mixcount on size of OUTPUT BUFFER (in samples not bytes)
		mov		eax, mix_numsamples
// 		unsigned int _uLoopLeft = mix_numsamples;

	CalculateLoopCount:
		mov		mix_count, eax
		mov		esi, [ecx].mixpos
		mov		ebp, FSOUND_OUTPUTBUFF_END	
		mov		mix_endflag, ebp			// set a flag to say mixing will end when end of output buffer is reached
// 		mix_count = _uLoopLeft;
// 		unsigned int _uMixPos = _pChannel->mixpos;
// 		unsigned int _uFlagOutBufEnd = FSOUND_OUTPUTBUFF_END;
// 		mix_endflag = _uFlagOutBufEnd;

		cmp		[ecx].speeddir, FSOUND_MIXDIR_FORWARDS
		jne		samplesleftbackwards
// 		if (_pChannel->speeddir != FSOUND_MIXDIR_FORWARDS)
// 		{
// 			goto samplesleftbackwards;
// 		}

		// work out how many samples left from mixpos to loop end	
		mov		edx, [ebx].loopstart
		add		edx, [ebx].looplen
		cmp     esi, edx
		jle     subtractmixpos
		mov     edx, [ebx+FSOUND_SAMPLE.length]
// 		unsigned int _uCurrLoopEndPos = _pSample->loopstart;
// 		_uCurrLoopEndPos += _pSample->looplen;
// 		if (_uMixPos>_uCurrLoopEndPos)
// 		{
// 			_uCurrLoopEndPos = _pSample->length;
// 		}
    subtractmixpos:
		sub		edx, esi					// eax = samples left (loopstart+looplen-mixpos)
// 		_uCurrLoopEndPos = _uCurrLoopEndPos - _uMixPos
		mov		eax, [ecx].mixposlo
// 		eax = _pChannel->mixposlo
		xor		ebp, ebp
// 		ebp = 0
		sub		ebp, eax
// 		ebp = ebp - eax
		sbb		edx, 0
		mov		eax, ebp
		jmp		samplesleftfinish

	samplesleftbackwards:
		// work out how many samples left from mixpos to loop start
		mov		edx, [ecx].mixpos
		mov		eax, [ecx].mixposlo

		sub		eax, 0h
		sbb		edx, [ebx].loopstart
// 		edx = _pSample->loopstart;

	samplesleftfinish:

		// edx:eax now contains number of samples left to mix
		cmp		edx, 1000000h
		jae		staywithoutputbuffend

		shrd	eax, edx, 8
		shr		edx, 8
		
		// now samples left = EDX:EAX -> hhhhhlll
		mov		ebp, [ecx].speedhi
		mov		edi, [ecx].speedlo

        // do a paranoid divide by 0 check
        test    ebp, ebp
        jnz     speedvalid
        test    edi, edi
        jnz     speedvalid
        mov     edi, 017C6F8Ch              ; 100hz
    speedvalid:

		shl		ebp, 24
		shr		edi, 8
		and		edi, 000FFFFFFh
		or		ebp, edi
		div		ebp

		or		edx,edx						// if fractional 16-bit part is zero we must add an extra carry number 
		jz		dontaddbyte					//  to the resultant in EDX:EAX.
		inc		eax
	dontaddbyte:							// we must remove the fractional part of the multiply by shifting EDX:EAX
		cmp		eax, mix_count
		ja		staywithoutputbuffend
		mov		mix_count, eax
		mov		edx, FSOUND_SAMPLEBUFF_END	// set a flag to say mix will end when end of output buffer is reached
		mov		mix_endflag, edx
	staywithoutputbuffend:

		mov		ebx, mix_sptr
		mov		ecx, mix_cptr

		//= VOLUME RAMP SETUP =========================================================
		// Reasons to ramp
		// 1. volume change
		// 2. sample starts (just treat as volume change - 0 to volume)
		// 3. sample ends (ramp last n number of samples from volume to 0)

		// now if the volume has changed, make end condition equal a volume ramp
		mov		mix_rampspeedleft, 0		// clear out volume ramp
		mov		mix_rampspeedright, 0		// clear out volume ramp

#ifdef VOLUMERAMPING
		mov		eax, mix_count
		mov		mix_count_old, eax			// remember mix count before modifying it	
		
		mov		mix_rampcount, 0
		cmp		[ecx].ramp_count, 0
		je		volumerampstart

		// if it tries to continue an old ramp, but the target has changed, 
		// set up a new ramp
		mov		eax, [ecx].leftvolume
		mov		edx, [ecx].ramp_lefttarget
		cmp		eax,edx
		jne		volumerampstart
		mov		eax, [ecx].rightvolume
		mov		edx, [ecx].ramp_righttarget
		cmp		eax,edx
		jne		volumerampstart

		// restore old ramp
		mov		eax, [ecx].ramp_count
		mov		mix_rampcount, eax
		mov		eax, [ecx].ramp_leftspeed
		mov		mix_rampspeedleft, eax
		mov		eax, [ecx].ramp_rightspeed
		mov		mix_rampspeedright, eax

		jmp		novolumerampR

	volumerampstart:
		mov		eax, [ecx].leftvolume
		mov		edx, [ecx].ramp_leftvolume 
		shr		edx, 8
		mov		[ecx].ramp_lefttarget, eax
		sub		eax, edx
		cmp		eax, 0
		je		novolumerampL

		mov		mix_temp1, eax
		fild	mix_temp1
		fmul	mix_1over255 
		fmul	mix_1overvolumerampsteps
		fstp	mix_rampspeedleft
		mov		eax, mix_rampspeedleft
		mov		[ecx].ramp_leftspeed, eax
		mov		eax, mix_volumerampsteps
		mov		mix_rampcount, eax


	novolumerampL:
		mov		eax, [ecx].rightvolume
		mov		edx, [ecx].ramp_rightvolume 
		shr		edx, 8
		mov		[ecx].ramp_righttarget, eax
		sub		eax, edx
		cmp		eax, 0
		je		novolumerampR

		mov		mix_temp1, eax
		fild	mix_temp1
		fmul	mix_1over255 
		fmul	mix_1overvolumerampsteps
		fstp	mix_rampspeedright
		mov		eax, mix_rampspeedright
		mov		[ecx].ramp_rightspeed, eax
		mov		eax, mix_volumerampsteps
		mov		mix_rampcount, eax


	novolumerampR:
		mov		eax, mix_rampcount
		cmp		eax, 0
		jle		volumerampend

		mov		[ecx].ramp_count, eax
		cmp		mix_count, eax
		jbe		volumerampend	// dont clamp mixcount 
		mov		mix_count, eax
	volumerampend:
#endif


		//= SET UP VOLUME MULTIPLIERS ==================================================

		// set up left/right volumes
		mov		ecx, mix_cptr

		// right volume
		mov		eax, [ecx].rightvolume
		mov		mix_temp1, eax
		fild	mix_temp1
		fmul	mix_1over255 
		fstp	mix_rightvol

		// left volume
		mov		eax, [ecx].leftvolume
		mov		mix_temp1, eax
		fild	mix_temp1
		fmul	mix_1over255 
		fstp	mix_leftvol

		// right ramp volume
		mov		eax, [ecx].ramp_rightvolume
		mov		mix_temp1, eax
		fild	mix_temp1
		fmul	mix_1over256			// first convert from 24:8 to 0-255
		fmul	mix_1over255			// now make 0-1.0f
		fstp	mix_ramprightvol

		// left ramp volume
		mov		eax, [ecx].ramp_leftvolume
		mov		mix_temp1, eax
		fild	mix_temp1
		fmul	mix_1over256			// first convert from 24:8 to 0-255
		fmul	mix_1over255			// now make 0-1.0f
		fstp	mix_rampleftvol


		//= SET UP ALL OF THE REGISTERS HERE FOR THE INNER LOOP ====================================
		// eax = ---
		// ebx = speed low
		// ecx = speed high
		// edx = counter
		// esi = mixpos
		// edi = destination pointer
		// ebp = mixpos low

		mov		eax, mix_cptr
		mov		ebx, [eax].speedlo
		mov		ecx, [eax].speedhi
	//  mov		edx, mix_count
		mov		ebp, [eax].mixposlo
		mov		esi, [eax].mixpos
		mov		edi, mix_mixbuffptr		// point edi to 16bit output stream

		cmp		[eax].speeddir, FSOUND_MIXDIR_FORWARDS
		je		NoChangeSpeed
		xor		ebx, 0FFFFFFFFh
		xor		ecx, 0FFFFFFFFh
		add		ebx, 1
		adc		ecx, 0
	NoChangeSpeed:


		//======================================================================================
		// ** 16 BIT NORMAL FUNCTIONS **********************************************************
		//======================================================================================

		mov		eax, mix_samplebuff
		shr		eax, 1
		add		esi, eax

		mov		edx, mix_count

#ifdef VOLUMERAMPING
		cmp		mix_rampcount, 0
		jne		MixLoopStart16_2
#endif

#ifdef ROLLED
		jmp		MixLoopStart16
#endif
		shr		edx, 1
		or		edx, edx
		jz		MixLoopStart16				// no groups of 2 samples to mix!

// START

		shr		ebp, 1					// 1 make 31bit coz fpu only loads signed values
		add		edi, 16					// 
		fild	word ptr [esi+esi+2]	// 1 [0]samp1+1
		mov		mix_temp1, ebp			// 1
//		nop
		fild	word ptr [esi+esi]		// 1 [0]samp1 [1]samp1+1
		fild	dword ptr mix_temp1		// 1 [0]ifrac1 [1]samp1 [2]samp1+1
		add		ebp, ebp				// 1 
//		nop
		add		ebp, ebx				// 1
		adc		esi, ecx				// 1 
		fmul	mix_1over2gig			// 1 [0]ffrac1 [1]samp1 [2]samp1+1
		fild	word ptr [esi+esi+2]	// 1 [0]samp2+1 [1]ffrac1 [2]samp1 [3]samp1+1
		shr		ebp, 1					// 1
//		nop
		mov		mix_temp1, ebp			// 1
//		nop
		fild	dword ptr mix_temp1		// 1 [0]ifrac2 [1]samp2+1 [2]ffrac1 [3]samp1 [4]samp1+1
		fild	word ptr [esi+esi]		// 1 [0]samp2 [1]ifrac2 [2]samp2+1 [3]ffrac1 [4]samp1 [5]samp1+1
		fxch	st(5)					//   [0]samp1+1 [1]ifrac2 [2]samp2+1 [3]ffrac1 [4]samp1 [5]samp2
		fsub	st(0), st(4)			// 1 [0]delta1 [1]ifrac2 [2]samp2+1 [3]ffrac1 [4]samp1 [5]samp2
//		fnop							// 1 fsub stall
		shl		ebp, 1					// 1 
//		nop
		fmulp	st(3), st(0)			// 1 [0]ifrac2 [1]samp2+1 [2]interp1 [3]samp1 [4]samp2
		fmul	mix_1over2gig			// 1 [0]ffrac2 [1]samp2+1 [2]interp1 [3]samp1 [4]samp2
		fxch	st(1)					//   [0]samp2+1 [1]ffrac2 [2]interp1 [3]samp1 [4]samp2
		fsub	st(0), st(4)			// 1 [0]delta2 [1]ffrac2 [2]interp1 [3]samp1 [4]samp2
		add		ebp, ebx				// 1 
//		nop
		adc		esi, ecx				// 1 
//		nop									 
		fmulp	st(1), st(0)			// 1 [0]interp2 [1]interp1 [2]samp1 [3]samp2
		fxch	st(1)					//   [0]interp1 [1]interp2 [2]samp1 [3]samp2
		faddp	st(2), st(0)			// 1 [0]interp2 [1]newsamp1 [2]samp2
//		fnop							// 1 fadd stall
//		fnop							// 1 fadd stall
		fld		st(1)					// 1 [0]newsamp1 [1]interp2 [2]newsamp1 [3]samp2
		fmul	mix_leftvol				// 1 [0]newsampL1 [1]interp2 [2]newsamp1 [3]samp2
		fxch	st(1)					//   [0]interp2 [1]newsampL1 [2]newsamp1 [3]samp2
		faddp	st(3), st(0)			// 1 [0]newsampL1 [1]newsamp1 [2]newsamp2
		fxch	st(1)					// 1 [0]newsamp1 [1]newsampL1 [2]newsamp2
		jmp		MixLoopUnroll16CoilEntry// 1

		ALIGN 16

	MixLoopUnroll16:
		shr		ebp, 1					// 1 
//		nop
		mov		mix_temp1, ebp			// 1
//		nop
		fild	word ptr [esi+esi+2]	// 1 [0]samp1+1 [1]finalR2 [2]finalR1 [3]finalL2
		fild	word ptr [esi+esi]		// 1 [0]samp1 [1]samp1+1 [2]finalR2 [3]finalR1 [4]finalL2
		fild	dword ptr mix_temp1		// 1 [0]ifrac1 [1]samp1 [2]samp1+1 [3]finalR2 [4]finalR1 [5]finalL2
		add		ebp, ebp				// 1 
//		nop
		add		ebp, ebx				// 1
//		nop
		adc		esi, ecx				// 1 
		add		edi, 16					// 
		fmul	mix_1over2gig			// 1 [0]ffrac1 [1]samp1 [2]samp1+1 [3]finalR2 [4]finalR1 [5]finalL2
		shr		ebp, 1					// 1
//		nop
		mov		mix_temp1, ebp			// 1
//		nop
		fild	dword ptr mix_temp1		// 1 [0]ifrac2 [1]ffrac1 [2]samp1 [3]samp1+1 [4]finalR2 [5]finalR1 [6]finalL2
		fild	word ptr [esi+esi+2]	// 1 [0]samp2+1 [1]ifrac2 [2]ffrac1 [3]samp1 [4]samp1+1 [5]finalR2 [6]finalR1 [7]finalL2
		fxch	st(4)					//   [0]samp1+1 [1]ifrac2 [2]ffrac1 [3]samp1 [4]samp2+1 [5]finalR2 [6]finalR1 [7]finalL2
		fsub	st(0), st(3)			// 1 [0]delta1 [1]ifrac2 [2]ffrac1 [3]samp1 [4]samp2+1 [5]finalR2 [6]finalR1 [7]finalL2
//		fnop							// 1 fsub stall
		shl		ebp, 1					// 1 
//		nop
		fmulp	st(2), st(0)			// 1 [0]ifrac2 [1]interp1 [2]samp1 [3]samp2+1 [4]finalR2 [5]finalR1 [6]finalL2
		fild	word ptr [esi+esi]		// 1 [0]samp2 [1]ifrac2 [2]interp1 [3]samp1 [4]samp2+1 [5]finalR2 [6]finalR1 [7]finalL2
		fxch	st(1)					//   [0]ifrac2 [1]samp2 [2]interp1 [3]samp1 [4]samp2+1 [5]finalR2 [6]finalR1 [7]finalL2
		fmul	mix_1over2gig			// 1 [0]ffrac2 [1]samp2 [2]interp1 [3]samp1 [4]samp2+1 [5]finalR2 [6]finalR1 [7]finalL2
		fxch	st(4)					//   [0]samp2+1 [1]samp2 [2]interp1 [3]samp1 [4]ffrac2 [5]finalR2 [6]finalR1 [7]finalL2
		fsub	st(0), st(1)			// 1 [0]delta2 [1]samp2 [2]interp1 [3]samp1 [4]ffrac2 [5]finalR2 [6]finalR1 [7]finalL2
		add		ebp, ebx				// 1 
//		nop
		adc		esi, ecx				// 1 
//		nop                                  
		fmulp	st(4), st(0)			// 1 [0]samp2 [1]interp1 [2]samp1 [3]interp2 [4]finalR2 [5]finalR1 [6]finalL2
		fxch	st(2)					//   [0]samp1 [1]interp1 [2]samp2 [3]interp2 [4]finalR2 [5]finalR1 [6]finalL2
		faddp	st(1), st(0)			// 1 [0]newsamp1 [1]samp2 [2]interp2 [3]finalR2 [4]finalR1 [5]finalL2
		fxch	st(4)					//	 [0]finalR1 [1]samp2 [2]interp2 [3]finalR2 [4]newsamp1 [5]finalL2
		fstp	dword ptr [edi-28]		// 2 [0]samp2 [1]interp2 [2]finalR2 [3]newsamp1 [4]finalL2
		fxch	st(4)					// 1 [0]finalL2 [1]interp2 [2]finalR2 [3]newsamp1 [4]samp2
		fstp	dword ptr [edi-24]		// 2 [0]interp2 [1]finalR2 [2]newsamp1 [3]samp2
		fld		st(2)					// 1 [0]newsamp1 [1]interp2 [2]finalR2 [3]newsamp1 [4]samp2
		fmul	mix_leftvol				// 1 [0]newsampL1 [1]interp2 [2]finalR2 [3]newsamp1 [4]samp2
		fxch	st(1)					//   [0]interp2 [1]newsampL1 [2]finalR2 [3]newsamp1 [4]samp2
		faddp	st(4), st(0)			// 1 [0]newsampL1 [1]finalR2 [2]newsamp1 [3]newsamp2
		fxch	st(1)					//   [0]finalR2 [1]newsampL1 [2]newsamp1 [3]newsamp2
		fstp	dword ptr [edi-20]		// 2 [0]newsampL1 [1]newsamp1 [2]newsamp2
		fxch	st(1)					// 1 [0]newsamp1 [1]newsampL1 [2]newsamp2 
										
	MixLoopUnroll16CoilEntry:			//   [0]newsamp1 [1]newsampL1 [2]newsamp2

		fmul	mix_rightvol			// 1 [0]newsampR1 [1]newsampL1 [2]newsamp2
		fxch	st(2)					//   [0]newsamp2 [1]newsampL1 [2]newsampR1
		fld		st(0)					// 1 [0]newsamp2 [1]newsamp2 [2]newsampL1 [3]newsampR1
		fmul	mix_leftvol				// 1 [0]newsampL2 [1]newsamp2 [2]newsampL1 [3]newsampR1
		fxch	st(1)					//   [0]newsamp2 [1]newsampL2 [2]newsampL1 [3]newsampR1
//		fnop							// 1 delay on mul unit
		fmul	mix_rightvol			// 1 [0]newsampR2 [1]newsampL2 [2]newsampL1 [3]newsampR1
		fxch	st(2)					//   [0]newsampL1 [1]newsampL2 [2]newsampR2 [3]newsampR1
		fadd	dword ptr [edi-16]		// 1 [0]finalL1 [1]newsampL2 [2]newsampR2 [3]newsampR1
		fxch	st(3)					//   [0]newsampR1 [1]newsampL2 [2]newsampR2 [3]finalL1
		fadd	dword ptr [edi-12]		// 1 [0]finalR1 [1]newsampL2 [2]newsampR2 [3]finalL1
		fxch	st(1)					//   [0]newsampL2 [1]finalR1 [2]newsampR2 [3]finalL1
		fadd	dword ptr [edi-8]		// 1 [0]finalL2 [1]finalR1 [2]newsampR2 [3]finalL1
		fxch	st(3)					//   [0]finalL1 [1]finalR1 [2]newsampR2 [3]finalL2
//		fnop							// 1 delay on store?
		fstp	dword ptr [edi-16]		// 2 [0]finalR1 [1]newsampR2 [2]finalL2
		fxch	st(1)					// 1 [0]newsampR2 [1]finalR1 [2]finalL2
		fadd	dword ptr [edi-4]		// 1 [0]finalR2 [1]finalR1 [2]finalL2

		dec		edx						// 1
		jnz		MixLoopUnroll16			// 

		fxch	st(1)					// 1 [0]finalR1 [1]finalR2 [2]finalL2
		fstp	dword ptr [edi-12]		// 2 [0]finalR2 [1]finalL2
		fxch	st(1)					// 1 [0]finalL2 [1]finalR2
		fstp	dword ptr [edi-8]		// 2 [0]finalR2
		fstp	dword ptr [edi-4]		// 2 

		//= MIX 16BIT, ROLLED ==================================================================
MixLoopStart16:
		mov		edx, mix_count
#ifndef ROLLED
		and		edx, 1
#endif

#ifdef VOLUMERAMPING
	MixLoopStart16_2:
#endif
		or		edx, edx					// if count == 0 dont enter the mix loop
		jz		MixLoopEnd16

#ifdef VOLUMERAMPING
		fld		mix_rampspeedleft		// [0]rampspeedL
		fld		mix_rampspeedright		// [0]rampspeedR [1]rampspeedL
		fld		mix_rampleftvol			// [0]lvol [1]rampspeedR [2]rampspeedL
		fld		mix_ramprightvol		// [0]rvol [1]lvol [2]rampspeedR [3]rampspeedL
#else
		fldz						// [0]rampspeedL
		fldz						// [0]rampspeedR [1]rampspeedL
		fld		mix_leftvol			// [0]lvol [1]rampspeedR [2]rampspeedL
		fld		mix_rightvol		// [0]rvol [1]lvol [2]rampspeedR [3]rampspeedL
#endif
		jmp		MixLoop16

		ALIGN 16

	MixLoop16:
		shr		ebp, 1					// 1 make 31bit coz fpu only loads signed values
		add		edi, 8					// 
		fild	word ptr [esi+esi+2]	// 1 [0]samp1+1 [1]rvol [2]lvol [3]rampspeedR [4]rampspeedL
		mov		mix_temp1, ebp			// 1
		fild	word ptr [esi+esi]		// 1 [0]samp1 [2]samp1+1 [3]rvol [4]lvol [5]rampspeedR [6]rampspeedL
		fild	dword ptr mix_temp1		// 1 [0]ifrac [1]samp1 [2]samp1+1 [3]rvol [4]lvol [5]rampspeedR [6]rampspeedL
		shl		ebp, 1					// 1 restore mixpos low
		add		ebp, ebx				//   add speed low to mixpos low
		adc		esi, ecx				// 1 add upper portion of speed plus carry	
		nop
		fmul	mix_1over2gig			// 1 [0]ifrac [1]samp1 [2]samp1+1 [3]rvol [4]lvol [5]rampspeedR [6]rampspeedL
		fxch	st(2)					//   [0]samp1+1 [1]samp1 [2]ffrac [3]rvol [4]lvol [5]rampspeedR [6]rampspeedL
		fsub	st(0), st(1)			// 1 [0]delta1 [1]samp1 [2]ffrac [3]rvol [4]lvol [5]rampspeedR [6]rampspeedL
		fnop							// 1
		fnop							// 1
		fnop							// 1
		fmulp	st(2), st(0)			// 1 [0]sample [1]interp [2]rvol [3]lvol [4]rampspeedR [5]rampspeedL
		fnop							// 1
		fnop							// 1
		fnop							// 1
		fnop							// 1
		faddp	st(1), st(0)			// 1 [0]newsamp [1]rvol [2]lvol [3]rampspeedR [4]rampspeedL
		fnop							// 1
		fnop							// 1
		fld		st(0)					// 1 [0]newsamp [1]newsamp [2]rvol [3]lvol [4]rampspeedR [5]rampspeedL
		fmul	st(0), st(3)			// 1 [0]newsampL [1]newsamp [2]rvol [3]lvol [4]rampspeedR [5]rampspeedL
		fxch	st(3)					//   [0]lvol [1]newsamp [2]rvol [3]newsampL [4]rampspeedR [5]rampspeedL
		fadd	st(0), st(5)			// 1 [0]lvol [1]newsamp [2]rvol [3]newsampL [4]rampspeedR [5]rampspeedL
		fxch	st(1)					//   [0]newsamp [1]lvol [2]rvol [3]newsampL [4]rampspeedR [5]rampspeedL
		fmul	st(0), st(2)			// 1 [0]newsampR [1]lvol [2]rvol [3]newsampL [4]rampspeedR [5]rampspeedL
		fxch	st(2)					//   [0]rvol [1]lvol [2]newsampR [3]newsampL [4]rampspeedR [5]rampspeedL
		fadd	st(0), st(4)			// 1 [0]rvol [1]lvol [2]newsampR [3]newsampL [4]rampspeedR [5]rampspeedL
		fxch	st(3)					//   [0]newsampL [1]lvol [2]newsampR [3]rvol [4]rampspeedR [5]rampspeedL
		fnop							// 1
		fnop							// 1
		fadd	dword ptr [edi-8]		// 1 [0]finalL [1]lvol [2]newsampR [3]rvol [4]rampspeedR [5]rampspeedL
		fxch	st(2)					//   [0]newsampR [1]lvol [2]finalL [3]rvol [4]rampspeedR [5]rampspeedL
		fadd	dword ptr [edi-4]		// 1 [0]finalR [1]lvol [2]finalL [3]rvol [4]rampspeedR [5]rampspeedL
		fxch	st(2)					//   [0]finalL [1]lvol [2]finalR [3]rvol [4]rampspeedR [5]rampspeedL
		fnop							// 1
		fstp	dword ptr [edi-8]		// 1 [0]lvol [1]finalR [2]rvol [3]rampspeedR [4]rampspeedL
		fxch	st(1)					//   [0]finalR [1]lvol [2]rvol [3]rampspeedR [4]rampspeedL
		fstp	dword ptr [edi-4]		// 3 [0]lvol [1]rvol [2]rampspeedR [3]rampspeedL
		fxch	st(1)					//   [0]rvol [1]lvol [2]rampspeedR [3]rampspeedL

		dec		edx						// 1
		jnz		MixLoop16				// 



		fxch	st(2)					// [0]rampspeedR [1]lvol [2]rvol [3]rampspeedL
		fstp	mix_rampspeedright		// [0]lvol [1]rvol [2]rampspeedL
		fxch	st(2)					// [0]rampspeedL [1]rvol [2]lvol
		fstp	mix_rampspeedleft		// [0]rvol [1]lvol
		
		fmul	mix_255					// [0]rvol*255 [1]lvol
		fmul	mix_256					// [0]rvol*255*256 [1]lvol
		fxch	st(1)					// [0]lvol [1]rvol*255*256
		fmul	mix_255					// [0]lvol*255 [1]rvol*255*256
		fmul	mix_256					// [0]lvol*255*256 [1]rvol*255*256

		xor		eax, eax
		fistp	mix_rampleftvol			// [0]rvol*255*256
		fistp	mix_ramprightvol		// 

	MixLoopEnd16:
		mov		eax, mix_samplebuff
		shr		eax, 1
		sub		esi, eax

#ifdef VOLUMERAMPING
			//=============================================================================================
			// DID A VOLUME RAMP JUST HAPPEN
			//=============================================================================================
			cmp		mix_rampcount, 0
			je		DoOutputbuffEnd		// no, no ramp

			mov		ebx, mix_sptr		// load ebx with sample pointer
			mov		ecx, mix_cptr		// load ecx with channel pointer

			mov		eax, mix_rampleftvol
			mov		[ecx].ramp_leftvolume, eax
			mov		eax, mix_ramprightvol
			mov		[ecx].ramp_rightvolume, eax

			mov		eax, mix_count
			mov		edx, mix_rampcount

			sub		edx, eax
	
			mov		mix_rampspeedleft, 0		// clear out volume ramp
			mov		mix_rampspeedright, 0		// clear out volume ramp
			mov		mix_rampcount, edx
			mov		[ecx].ramp_count, edx
	
			// if rampcount now = 0, a ramp has FINISHED, so finish the rest of the mix
			cmp		edx, 0
			jne		DoOutputbuffEnd

			// clear out the ramp speeds
			mov		[ecx].ramp_leftspeed, 0
			mov		[ecx].ramp_rightspeed, 0

			// clamp the 2 volumes together in case the speed wasnt accurate enough!
			mov		edx, [ecx].leftvolume
			shl		edx, 8
			mov		[ecx].ramp_leftvolume, edx
			mov		edx, [ecx].rightvolume
			shl		edx, 8
			mov		[ecx].ramp_rightvolume, edx

			// is it 0 because ramp ended only? or both ended together??
			// if sample ended together with ramp.. problems .. loop isnt handled

			cmp		mix_count_old, eax		// ramp and output mode ended together
			je		DoOutputbuffEnd

			// start again and continue rest of mix
			mov		[ecx].mixpos, esi
			mov		[ecx].mixposlo, ebp
			
			mov		eax, mix_mixbuffend	// find out how many OUTPUT samples left to mix 
			sub		eax, edi
			shr		eax, 3				// eax now holds # of samples left, go recalculate mix_count!!!
			mov		mix_mixbuffptr, edi	// update the new mixbuffer pointer

			cmp		eax, 0				// dont start again if nothing left
			jne		CalculateLoopCount

		DoOutputbuffEnd:
#endif
			cmp		mix_endflag, FSOUND_OUTPUTBUFF_END
			je		FinishUpChannel

			//=============================================================================================
			// SWITCH ON LOOP MODE TYPE
			//=============================================================================================
			mov		ebx, mix_sptr				// load ebx with sample pointer
			mov		ecx, mix_cptr				// load ecx with sample pointer

			mov		dl,	[ebx].loopmode

			// check for normal loop
			test	dl, FSOUND_LOOP_NORMAL
			jz		CheckBidiLoop

			mov		eax, [ebx].loopstart
			add		eax, [ebx].looplen
		rewindsample:
			sub		esi, [ebx].looplen
			cmp		esi, eax
			jae		rewindsample

			mov		[ecx].mixpos, esi
			mov		[ecx].mixposlo, ebp
			mov		eax, mix_mixbuffend			// find out how many samples left to mix for the output buffer
			sub		eax, edi
			shr		eax, 3						// eax now holds # of samples left, go recalculate mix_count!!!
			mov		mix_mixbuffptr, edi			// update the new mixbuffer pointer

			cmp		eax, 0
			je		FinishUpChannel

			jmp		CalculateLoopCount

		CheckBidiLoop:
			test	dl, FSOUND_LOOP_BIDI
			jz		NoLoop
			cmp		[ecx].speeddir, FSOUND_MIXDIR_FORWARDS
			je		BidiForward

		BidiBackwards:
			mov		eax, [ebx].loopstart
			dec		eax
	//		mov		edx, 0ffffff00h
			mov		edx, 0ffffffffh
			sub		edx, ebp
			sbb		eax, esi			
				
			mov		esi, eax
			mov		ebp, edx					// esi:ebp = loopstart - mixpos
			mov		eax, [ebx].loopstart
			mov		edx, 0h
			add		ebp, edx
			adc		esi, eax					// esi:ebp += loopstart
				
			mov		[ecx].speeddir, FSOUND_MIXDIR_FORWARDS

			mov		eax, [ebx].loopstart
			add		eax, [ebx].looplen
			cmp		esi, eax
			jge		BidiForward

			jmp		BidiFinish
		BidiForward:
			mov		eax, [ebx].loopstart
			add		eax, [ebx].looplen
			mov		edx, 0h
			sub		edx, ebp
			sbb		eax, esi				
			mov		esi, eax
			mov		ebp, edx					// esi:ebp = loopstart+looplen - mixpos

			mov		eax, [ebx].loopstart
			add		eax, [ebx].looplen
			dec		eax
	//		mov		edx, 0ffffff00h
			mov		edx, 0ffffffffh
			add		ebp, edx
			adc		esi, eax

			mov		[ecx].speeddir, FSOUND_MIXDIR_BACKWARDS

			cmp		esi, [ebx].loopstart
			jl		BidiBackwards

		BidiFinish:

			mov		[ecx].mixpos, esi
			mov		[ecx].mixposlo, ebp

			mov		eax, mix_mixbuffend			// find out how many samples left to mix for the output buffer
			sub		eax, edi
			shr		eax, 3						// eax now holds # of samples left, go recalculate mix_count!!!
			mov		mix_mixbuffptr, edi			// update the new mixbuffer pointer

			cmp		eax, 0
			je		FinishUpChannel

			jmp		CalculateLoopCount

		NoLoop:
			xor		ebp, ebp
			xor		esi, esi
			mov		[ecx]FSOUND_CHANNEL.sptr, esi		// clear the sample pointer out

			//= LEAVE INNER LOOP
		FinishUpChannel:
			mov		ecx, [mix_cptr]

			mov		[ecx].mixposlo, ebp
			mov		[ecx].mixpos, esi			// reset mixpos based on esi for next time around

		//===================================================================================================
		// EXIT
		//===================================================================================================
		MixExit:
			pop		ebp						//= RESTORE EBP
		}
	}
} 

#if 0
void FSOUND_Mixer_FPU_Ramp_C(void *mixptr, int len, char returnaddress)
{
	int v1; // ecx@5
	int v2; // ST00_4@5
	int v3; // ebx@6
	int v4; // eax@7
	int v5; // esi@8
	int v6; // edx@9
	unsigned __int64 v7; // qax@11
	unsigned __int64 v8; // qax@14
	int v9; // ebp@14
	unsigned int v10; // edi@14
	int v11; // eax@17
	int v12; // eax@25
	int v13; // eax@27
	unsigned int v14; // eax@29
	unsigned int _ch_speedhi; // ecx@32
	unsigned int _ch_speedlo; // ebx@32
	unsigned int _ch_mixposlo; // ebp@32
    unsigned int _ch_mixpos;
    unsigned int _dest_mixbuffptr;
	unsigned int _LOL_mix_count; // edx@34
	int v22; // esi@45
	int v23; // eax@46
	char v24; // dl@50
	unsigned int v26; // eax@11
	unsigned int v27; // edi@17
	int v28; // ebp@17
	unsigned __int64 v29; // qt2@17
	int v30; // edx@17
	int v31; // eax@25
	int v32; // edx@25
	int v33; // eax@27
	int v34; // edx@27
	int v37; // ebp@36
	int v39; // ebp@36
	unsigned __int8 v40; // cf@36
	int v41; // ebp@37
	int v43; // ebp@37
	unsigned __int8 v44; // cf@37
	int v45; // ebp@43
	unsigned __int8 v46; // cf@43
	int v47; // edx@46
	int v48; // esi@56
	int v49; // esi@58
	int v50; // ebp@58
	unsigned __int8 v51; // cf@58

	static FSOUND_CHANNEL *cptr;
	static int count;
	// IMPORTANT no local variables on stack.. we are trashing EBP.  static puts values on heap.

	if ( len > 0 )
	{
		mix_numsamples	= len;
		mix_mixptr		= (unsigned int)mixptr;
		mix_mixbuffend	= (unsigned int)mix_mixptr + (mix_numsamples << 3);

		for (count = 0; count<64; count++)
		{
	Label_ForLoop:
			cptr = (int)&FSOUND_Channel[22 * count];
			v2 = a1;
			mix_mixbuffptr = mix_mixptr;
			v1 = cptr;
			mix_cptr = cptr;
			if ( !cptr || (v3 = *(_DWORD *)(cptr + 28), mix_sptr = *(_DWORD *)(cptr + 28), !v3) )
				goto MixExit;
			mix_samplebuff = *(_DWORD *)v3;
			v4 = mix_numsamples;
			while ( 1 )
			{
				while ( 1 )
				{
					do
					{
						mix_count = v4;
						v5 = *(_DWORD *)(v1 + 40);
						mix_endflag = 0;
						if ( *(_DWORD *)(v1 + 56) == 1 )
						{
							v6 = *(_DWORD *)(v3 + 12) + *(_DWORD *)(v3 + 8);
							if ( v5 > v6 )
								v6 = *(_DWORD *)(v3 + 4);
							v26 = *(_DWORD *)(v1 + 44);
							*((_DWORD *)&v7 + 1) = v6 - v5 - (v26 > 0);
							*(_DWORD *)&v7 = -(_DWORD)v7;
						}
						else
						{
							*(_DWORD *)&v7 = *(_DWORD *)(v1 + 44);
							*((_DWORD *)&v7 + 1) = *(_DWORD *)(v1 + 40) - *(_DWORD *)(v3 + 8);
						}
						if ( *((_DWORD *)&v7 + 1) < 0x1000000u )
						{
							v8 = v7 >> 8;
							v9 = *(_DWORD *)(v1 + 52);
							v10 = *(_DWORD *)(v1 + 48);
							if ( !v9 )
							{
								if ( !v10 )
									v10 = 24932236;
							}
							v27 = v10 >> 8;
							v28 = v27 & 0xFFFFFF | (v9 << 24);
							v29 = v8 % (unsigned int)v28;
							v11 = v8 / (unsigned int)v28;
							v30 = v29;
							if ( v30 )
								++v11;
							if ( v11 <= (unsigned int)mix_count )
							{
								mix_count = v11;
								mix_endflag = 1;
							}
						}
						mix_rampspeedleft = 0;
						mix_rampspeedright = 0;
						mix_count_old = mix_count;
						mix_rampcount = 0;
						if ( *(_DWORD *)(mix_cptr + 84)
							&& *(_DWORD *)(mix_cptr + 32) == *(_DWORD *)(mix_cptr + 60)
							&& *(_DWORD *)(mix_cptr + 36) == *(_DWORD *)(mix_cptr + 64) )
						{
							mix_rampcount = *(_DWORD *)(mix_cptr + 84);
							mix_rampspeedleft = *(_DWORD *)(mix_cptr + 76);
							mix_rampspeedright = *(_DWORD *)(mix_cptr + 80);
						}
						else
						{
							v31 = *(_DWORD *)(mix_cptr + 32);
							v32 = *(_DWORD *)(mix_cptr + 68) >> 8;
							*(_DWORD *)(mix_cptr + 60) = *(_DWORD *)(mix_cptr + 32);
							v12 = v31 - v32;
							if ( v12 )
							{
								mix_temp1 = v12;
								__asm
								{
									fild    mix_temp1
										fmul    ds:mix_1over255
										fmul    mix_1overvolumerampsteps
										fstp    mix_rampspeedleft
								}
								*(_DWORD *)(mix_cptr + 76) = mix_rampspeedleft;
								mix_rampcount = mix_volumerampsteps;
							}
							v33 = *(_DWORD *)(mix_cptr + 36);
							v34 = *(_DWORD *)(mix_cptr + 72) >> 8;
							*(_DWORD *)(mix_cptr + 64) = *(_DWORD *)(mix_cptr + 36);
							v13 = v33 - v34;
							if ( v13 )
							{
								mix_temp1 = v13;
								__asm
								{
									fild    mix_temp1
										fmul    ds:mix_1over255
										fmul    mix_1overvolumerampsteps
										fstp    mix_rampspeedright
								}
								*(_DWORD *)(mix_cptr + 80) = mix_rampspeedright;
								mix_rampcount = mix_volumerampsteps;
							}
						}
						v14 = mix_rampcount;
						if ( mix_rampcount > 0 )
						{
							*(_DWORD *)(mix_cptr + 84) = mix_rampcount;
							if ( mix_count > v14 )
								mix_count = v14;
						}
						mix_temp1 = *(_DWORD *)(mix_cptr + 36);
						__asm
						{
							fild    mix_temp1
								fmul    ds:mix_1over255
								fstp    mix_rightvol
						}
						mix_temp1 = *(_DWORD *)(mix_cptr + 32);
						__asm
						{
							fild    mix_temp1
								fmul    ds:mix_1over255
								fstp    mix_leftvol
						}
						mix_temp1 = *(_DWORD *)(mix_cptr + 72);
						__asm
						{
							fild    mix_temp1
								fmul    ds:mix_1over256
								fmul    ds:mix_1over255
								fstp    mix_ramprightvol
						}
						mix_temp1 = *(_DWORD *)(mix_cptr + 68);
						__asm
						{
							fild    mix_temp1
								fmul    ds:mix_1over256
								fmul    ds:mix_1over255
								fstp    mix_rampleftvol
						}

                        //= SET UP ALL OF THE REGISTERS HERE FOR THE INNER LOOP ====================================
                        // eax = ---
                        // ebx _ch_speedlo = speed low
                        // ecx _ch_speedhi = speed high
                        // edx _LOL_mix_count = counter
                        // esi _ch_mixpos = mixpos
                        // edi _dest_mixbuffptr = destination pointer
                        // ebp _ch_mixposlo = mixpos low
						_ch_speedlo  = ((FSOUND_CHANNEL *)mix_cptr)->speedlo;
						_ch_speedhi  = ((FSOUND_CHANNEL *)mix_cptr)->speedhi;
						_ch_mixposlo = ((FSOUND_CHANNEL *)mix_cptr)->mixposlo;
                        _ch_mixpos   = ((FSOUND_CHANNEL *)mix_cptr)->mixpos;
						_dest_mixbuffptr = mix_mixbuffptr;      // point edi to 16bit output stream
						if ( ((FSOUND_CHANNEL *)mix_cptr)->speeddir != FSOUND_MIXDIR_FORWARDS )
						{
                            _ch_speedlo = ~_ch_speedlo;
                            _ch_speedhi = ~_ch_speedhi;
                            if (((unsigned int)(_ch_speedlo+1))<_ch_speedlo)
                            {
                                _ch_speedhi += 1;
                            }
                            _ch_speedlo += 1;
						}
//NoChangeSpeed:
                        //======================================================================================
                        // ** 16 BIT NORMAL FUNCTIONS **********************************************************
                        //======================================================================================
						_ch_mixpos = ((unsigned int)mix_samplebuff >> 1) + _ch_mixpos;
						_LOL_mix_count = mix_count;
#ifdef VOLUMERAMPING
						if ( !mix_rampcount )
#endif
						{
#ifdef ROLLED
                            if (0)
                            {
#endif
							    _LOL_mix_count >>= 1;
							    if ( _LOL_mix_count )           // ELSE skip this block// no groups of 2 samples to mix!
							    {
// START
								    _ch_mixposlo >>= 1;                 // 1 make 31bit coz fpu only loads signed values
								    _dest_mixbuffptr += 16;             // 
								    __asm
                                    {
                                        fild    word ptr [esi+esi+2]    // 1 [0]samp1+1
                                    }
								    mix_temp1 = _ch_mixposlo;
//                                  nop
								    __asm
								    {
                                        fild	word ptr [esi+esi]		// 1 [0]samp1 [1]samp1+1
                                        fild	dword ptr mix_temp1		// 1 [0]ifrac1 [1]samp1 [2]samp1+1
								    }
								    _ch_mixposlo += _ch_mixposlo;
//                                  nop
                                    _ch_mixpos += _ch_speedhi;
                                    if (((unsigned int)(_ch_mixposlo+_ch_speedlo))<_ch_mixposlo)
                                    {
                                        _ch_mixpos += 1;
                                    }
                                    _ch_mixposlo += _ch_speedlo;
								    __asm
								    {
                                        fmul	mix_1over2gig			// 1 [0]ffrac1 [1]samp1 [2]samp1+1
                                        fild	word ptr [esi+esi+2]	// 1 [0]samp2+1 [1]ffrac1 [2]samp1 [3]samp1+1
								    }
								    _ch_mixposlo >>= 1;
//                                  nop
								    mix_temp1 = _ch_mixposlo;
//                                  nop
								    __asm
								    {
                                        fild	dword ptr mix_temp1		// 1 [0]ifrac2 [1]samp2+1 [2]ffrac1 [3]samp1 [4]samp1+1
                                        fild	word ptr [esi+esi]		// 1 [0]samp2 [1]ifrac2 [2]samp2+1 [3]ffrac1 [4]samp1 [5]samp1+1
                                        fxch	st(5)					//   [0]samp1+1 [1]ifrac2 [2]samp2+1 [3]ffrac1 [4]samp1 [5]samp2
                                        fsub	st(0), st(4)			// 1 [0]delta1 [1]ifrac2 [2]samp2+1 [3]ffrac1 [4]samp1 [5]samp2
//                                      fnop							// 1 fsub stall
								    }
								    _ch_mixposlo *= 2;
//                                  nop
								    __asm
								    {
                                        fmulp	st(3), st(0)			// 1 [0]ifrac2 [1]samp2+1 [2]interp1 [3]samp1 [4]samp2
                                        fmul	mix_1over2gig			// 1 [0]ffrac2 [1]samp2+1 [2]interp1 [3]samp1 [4]samp2
                                        fxch	st(1)					//   [0]samp2+1 [1]ffrac2 [2]interp1 [3]samp1 [4]samp2
                                        fsub	st(0), st(4)			// 1 [0]delta2 [1]ffrac2 [2]interp1 [3]samp1 [4]samp2
								    }
                                    _ch_mixpos += _ch_speedhi;
                                    if (((unsigned int)(_ch_mixposlo+_ch_speedlo))<_ch_mixposlo)
                                    {
                                        _ch_mixpos += 1;
                                    }
                                    _ch_mixposlo += _ch_speedlo;
								    __asm
								    {
                                        fmulp	st(1), st(0)			// 1 [0]interp2 [1]interp1 [2]samp1 [3]samp2
                                        fxch	st(1)					//   [0]interp1 [1]interp2 [2]samp1 [3]samp2
                                        faddp	st(2), st(0)			// 1 [0]interp2 [1]newsamp1 [2]samp2
//                                      fnop							// 1 fadd stall
//                                      fnop							// 1 fadd stall
                                        fld		st(1)					// 1 [0]newsamp1 [1]interp2 [2]newsamp1 [3]samp2
                                        fmul	mix_leftvol				// 1 [0]newsampL1 [1]interp2 [2]newsamp1 [3]samp2
                                        fxch	st(1)					//   [0]interp2 [1]newsampL1 [2]newsamp1 [3]samp2
                                        faddp	st(3), st(0)			// 1 [0]newsampL1 [1]newsamp1 [2]newsamp2
                                        fxch	st(1)					// 1 [0]newsamp1 [1]newsampL1 [2]newsamp2
								    }
//MixLoopUnroll16CoilEntry:                                             //   [0]newsamp1 [1]newsampL1 [2]newsamp2
								    while ( 1 )
								    {
									    __asm
									    {
                                            fmul	mix_rightvol			// 1 [0]newsampR1 [1]newsampL1 [2]newsamp2
                                            fxch	st(2)					//   [0]newsamp2 [1]newsampL1 [2]newsampR1
                                            fld		st(0)					// 1 [0]newsamp2 [1]newsamp2 [2]newsampL1 [3]newsampR1
                                            fmul	mix_leftvol				// 1 [0]newsampL2 [1]newsamp2 [2]newsampL1 [3]newsampR1
                                            fxch	st(1)					//   [0]newsamp2 [1]newsampL2 [2]newsampL1 [3]newsampR1
//                                          fnop							// 1 delay on mul unit
                                            fmul	mix_rightvol			// 1 [0]newsampR2 [1]newsampL2 [2]newsampL1 [3]newsampR1
                                            fxch	st(2)					//   [0]newsampL1 [1]newsampL2 [2]newsampR2 [3]newsampR1
                                            fadd	dword ptr [edi-16]		// 1 [0]finalL1 [1]newsampL2 [2]newsampR2 [3]newsampR1
                                            fxch	st(3)					//   [0]newsampR1 [1]newsampL2 [2]newsampR2 [3]finalL1
                                            fadd	dword ptr [edi-12]		// 1 [0]finalR1 [1]newsampL2 [2]newsampR2 [3]finalL1
                                            fxch	st(1)					//   [0]newsampL2 [1]finalR1 [2]newsampR2 [3]finalL1
                                            fadd	dword ptr [edi-8]		// 1 [0]finalL2 [1]finalR1 [2]newsampR2 [3]finalL1
                                            fxch	st(3)					//   [0]finalL1 [1]finalR1 [2]newsampR2 [3]finalL2
//                                          fnop							// 1 delay on store?
                                            fstp	dword ptr [edi-16]		// 2 [0]finalR1 [1]newsampR2 [2]finalL2
                                            fxch	st(1)					// 1 [0]newsampR2 [1]finalR1 [2]finalL2
                                            fadd	dword ptr [edi-4]		// 1 [0]finalR2 [1]finalR1 [2]finalL2
									    }
									    --_LOL_mix_count;
									    if ( !_LOL_mix_count )
										    break;
//MixLoopUnroll16:
									    v41 = _ch_mixposlo >> 1;
									    mix_temp1 = v41;
									    __asm
									    {
										    fild    word ptr [esi+esi+2]
										    fild    word ptr [esi+esi]
										    fild    mix_temp1
									    }
									    v41 *= 2;
									    _ch_mixpos = _ch_speedhi + __MKCADD__(_ch_speedlo, v41) + _ch_mixpos;
									    _dest_mixbuffptr += 16;
									    __asm { fmul    ds:mix_1over2gig }
									    v43 = (unsigned int)(_ch_speedlo + v41) >> 1;
									    mix_temp1 = v43;
									    __asm
									    {
										    fild    mix_temp1
										    fild    word ptr [esi+esi+2]
										    fxch    st(4)
										    fsub    st, st(3)
									    }
									    v43 *= 2;
									    __asm
									    {
										    fmulp   st(2), st
										    fild    word ptr [esi+esi]
										    fxch    st(1)
										    fmul    ds:mix_1over2gig
										    fxch    st(4)
										    fsub    st, st(1)
									    }
									    v44 = __MKCADD__(_ch_speedlo, v43);
									    _ch_mixposlo = _ch_speedlo + v43;
									    _ch_mixpos = _ch_speedhi + v44 + _ch_mixpos;
									    __asm
									    {
										    fmulp   st(4), st
										    fxch    st(2)
										    faddp   st(1), st
										    fxch    st(4)
										    fstp    dword ptr [edi-1Ch]
										    fxch    st(4)
										    fstp    dword ptr [edi-18h]
										    fld     st(2)
										    fmul    mix_leftvol
										    fxch    st(1)
										    faddp   st(4), st
										    fxch    st(1)
										    fstp    dword ptr [edi-14h]
										    fxch    st(1)
									    }
								    }

                                    __asm
								    {
                                        fxch	st(1)					// 1 [0]finalR1 [1]finalR2 [2]finalL2
                                        fstp	dword ptr [edi-12]		// 2 [0]finalR2 [1]finalL2
                                        fxch	st(1)					// 1 [0]finalL2 [1]finalR2
                                        fstp	dword ptr [edi-8]		// 2 [0]finalR2
                                        fstp	dword ptr [edi-4]		// 2 
								    }
							    }
#ifdef ROLLED
                            }
#endif
                            //= MIX 16BIT, ROLLED ==================================================================
//MixLoopStart16:
                            _LOL_mix_count = mix_count;
#ifndef ROLLED
                            _LOL_mix_count &= 1;
#endif
						}

#ifdef VOLUMERAMPING
//MixLoopStart16_2:
#endif
						if ( _LOL_mix_count )
						{
							__asm
							{
#ifdef VOLUMERAMPING
                                fld		mix_rampspeedleft		// [0]rampspeedL
                                fld		mix_rampspeedright		// [0]rampspeedR [1]rampspeedL
                                fld		mix_rampleftvol			// [0]lvol [1]rampspeedR [2]rampspeedL
                                fld		mix_ramprightvol		// [0]rvol [1]lvol [2]rampspeedR [3]rampspeedL
#else
                                fldz						// [0]rampspeedL
                                fldz						// [0]rampspeedR [1]rampspeedL
                                fld		mix_leftvol			// [0]lvol [1]rampspeedR [2]rampspeedL
                                fld		mix_rightvol		// [0]rvol [1]lvol [2]rampspeedR [3]rampspeedL
#endif
							}
							do
							{
								v45 = _ch_mixposlo >> 1;
								_dest_mixbuffptr += 8;
								__asm { fild    word ptr [esi+esi+2] }
								mix_temp1 = v45;
								__asm
								{
									fild    word ptr [esi+esi]
									fild    mix_temp1
								}
								v45 *= 2;
								v46 = __MKCADD__(_ch_speedlo, v45);
								_ch_mixposlo = _ch_speedlo + v45;
								_ch_mixpos += _ch_speedhi + v46;
								__asm
								{
									fmul    ds:mix_1over2gig
										fxch    st(2)
										fsub    st, st(1)
										fnop
										fnop
										fnop
										fmulp   st(2), st
										fnop
										fnop
										fnop
										fnop
										faddp   st(1), st
										fnop
										fnop
										fld     st
										fmul    st, st(3)
										fxch    st(3)
										fadd    st, st(5)
										fxch    st(1)
										fmul    st, st(2)
										fxch    st(2)
										fadd    st, st(4)
										fxch    st(3)
										fnop
										fnop
										fadd    dword ptr [edi-8]
									fxch    st(2)
										fadd    dword ptr [edi-4]
									fxch    st(2)
										fnop
										fstp    dword ptr [edi-8]
									fxch    st(1)
										fstp    dword ptr [edi-4]
									fxch    st(1)
								}
								--_LOL_mix_count;
							}
							while ( _LOL_mix_count );
							__asm
							{
								fxch    st(2)
									fstp    mix_rampspeedright
									fxch    st(2)
									fstp    mix_rampspeedleft
									fmul    ds:mix_255
									fmul    ds:mix_256
									fxch    st(1)
									fmul    ds:mix_255
									fmul    ds:mix_256
									fistp   mix_rampleftvol
									fistp   mix_ramprightvol
							}
						}
						v22 = _ch_mixpos - ((unsigned int)mix_samplebuff >> 1);
						if ( !mix_rampcount )
							break;
						v3 = mix_sptr;
						v1 = mix_cptr;
						*(_DWORD *)(mix_cptr + 68) = mix_rampleftvol;
						*(_DWORD *)(mix_cptr + 72) = mix_ramprightvol;
						v23 = mix_count;
						v47 = mix_rampcount - mix_count;
						mix_rampspeedleft = 0;
						mix_rampspeedright = 0;
						mix_rampcount -= mix_count;
						*(_DWORD *)(mix_cptr + 84) = v47;
						if ( v47 )
							break;
						*(_DWORD *)(mix_cptr + 76) = 0;
						*(_DWORD *)(mix_cptr + 80) = 0;
						*(_DWORD *)(mix_cptr + 68) = *(_DWORD *)(mix_cptr + 32) << 8;
						*(_DWORD *)(mix_cptr + 72) = *(_DWORD *)(mix_cptr + 36) << 8;
						if ( mix_count_old == v23 )
							break;
						*(_DWORD *)(mix_cptr + 40) = v22;
						*(_DWORD *)(mix_cptr + 44) = _ch_mixposlo;
						v4 = (unsigned int)(mix_mixbuffend - _dest_mixbuffptr) >> 3;
						mix_mixbuffptr = _dest_mixbuffptr;
					}
					while ( v4 );
					if ( !mix_endflag )
						goto FinishUpChannel;
					v3 = mix_sptr;
					v1 = mix_cptr;
					v24 = *(_BYTE *)(mix_sptr + 29);
					if ( !(v24 & 2) )
						break;
					do
					v22 -= *(_DWORD *)(mix_sptr + 12);
					while ( v22 >= (unsigned int)(*(_DWORD *)(mix_sptr + 12) + *(_DWORD *)(mix_sptr + 8)) );
					*(_DWORD *)(mix_cptr + 40) = v22;
					*(_DWORD *)(mix_cptr + 44) = _ch_mixposlo;
					v4 = (unsigned int)(mix_mixbuffend - _dest_mixbuffptr) >> 3;
					mix_mixbuffptr = _dest_mixbuffptr;
					if ( !v4 )
						goto FinishUpChannel;
				}
				if ( !(v24 & 4) )
				{
					_ch_mixposlo = 0;
					v22 = 0;
					*(_DWORD *)(mix_cptr + 28) = 0;
	FinishUpChannel:
					*(_DWORD *)(mix_cptr + 44) = _ch_mixposlo;
					*(_DWORD *)(mix_cptr + 40) = v22;
	MixExit:
					a1 = v2;
					++count;
					goto Label_ForLoop;
				}
				if ( ((FSOUND_CHANNEL *)mix_cptr)->speeddir == FSOUND_MIXDIR_FORWARDS )
					goto BidiForward;
				do
				{
					v48 = ((FSOUND_SAMPLE *)v3)->loopstart - 1 - ((_ch_mixposlo > 0xFFFFFFFF) + v22);
					_ch_mixposlo = -1 - _ch_mixposlo;
					v22 = ((FSOUND_SAMPLE *)v3)->loopstart + v48;
					*(_DWORD *)(v1 + 56) = 1;
					if ( v22 < *(_DWORD *)(v3 + 12) + *(_DWORD *)(v3 + 8) )
						break;
	BidiForward:
					v49 = *(_DWORD *)(v3 + 12) + *(_DWORD *)(v3 + 8) - ((_ch_mixposlo > 0) + v22);
					v50 = -_ch_mixposlo;
					v51 = __MKCADD__(-1, v50);
					_ch_mixposlo = v50 - 1;
					v22 = *(_DWORD *)(v3 + 12) + *(_DWORD *)(v3 + 8) - 1 + v51 + v49;
					*(_DWORD *)(v1 + 56) = 2;
				}
				while ( v22 < *(_DWORD *)(v3 + 8) );
				*(_DWORD *)(v1 + 40) = v22;
				*(_DWORD *)(v1 + 44) = _ch_mixposlo;
				v4 = (unsigned int)(mix_mixbuffend - _dest_mixbuffptr) >> 3;
				mix_mixbuffptr = _dest_mixbuffptr;
				if ( !v4 )
					goto FinishUpChannel;
			}
		} // for ??? //����ṹ���ˣ�for��Ӧ�����������
	} // if len>0
	return j__RTC_CheckEsp();
}

#endif