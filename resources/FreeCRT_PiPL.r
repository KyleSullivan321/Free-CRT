/*
	CRT_FactoryPiPL.r — PiPL for Free CRT (AE 25.6 SDK, Smart Render + GPU).
	Entry-point name "EffectMain" must match the export in CRT_Factory.cpp.
*/

#include "AEConfig.h"
#include "AE_EffectVers.h"

#ifndef AE_OS_WIN
	#include "AE_General.r"
#endif

resource 'PiPL' (16000) {
	{
		Kind { AEEffect },
		Name { "Free CRT" },
		Category { "Free CRT" },

#ifdef AE_OS_WIN
	#if defined(AE_PROC_INTELx64)
		CodeWin64X86 { "EffectMain" },
	#elif defined(AE_PROC_ARM64)
		CodeWinARM64 { "EffectMain" },
	#endif
#elif defined(AE_OS_MAC)
		CodeMacIntel64 { "EffectMain" },
		CodeMacARM64 { "EffectMain" },
#endif

		AE_PiPL_Version { 2, 0 },
		AE_Effect_Spec_Version { PF_PLUG_IN_VERSION, PF_PLUG_IN_SUBVERS },

		/* 0.2.0 develop(0) build 1 = (2<<15)|1 = 65537 */
		AE_Effect_Version { 65537 },
		AE_Effect_Info_Flags { 0 },

		/* Must mirror GlobalSetup exactly (AE warns otherwise):
		   out_flags  = DEEP_COLOR_AWARE | NON_PARAM_VARY | SEND_UPDATE_PARAMS_UI = 0x06000004
		   out_flags2 = FLOAT_COLOR_AWARE | SUPPORTS_SMART_RENDER |
		                SUPPORTS_THREADED_RENDERING | SUPPORTS_GPU_RENDER_F32 |
		                PARAM_GROUP_START_COLLAPSED = 0x0a001408
		   (no DirectX bit 1<<29 — this build is OpenCL-only) */
		AE_Effect_Global_OutFlags  { 0x06000004 },
		AE_Effect_Global_OutFlags_2 { 0x0a001408 },

		AE_Effect_Match_Name { "Free CRT" },
		AE_Reserved_Info { 0 }
	}
};
