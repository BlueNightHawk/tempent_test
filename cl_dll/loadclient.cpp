#include "interface.h"
#include "cl_dll.h"
#include <string>

CSysModule* g_pClientDll;
extern std::string g_ClDllPath;

bool Secret_LoadClient(const char* fileName);

#define GetClProcs(ptr, name, required)\
{                                   \
	ptr = decltype(ptr)(Sys_GetProcAddress(g_pClientDll, ##name));                \
	if (!ptr && required) \
		{                                                              \
		return false;\
	}\
}

bool InitSecureClient()
{
	return Secret_LoadClient(g_ClDllPath.c_str());
}

bool InitInsecureClient()
{
	GetClProcs(cl_funcs.pInitFunc, "Initialize", true);
	GetClProcs(cl_funcs.pHudVidInitFunc, "HUD_VidInit", true);
	GetClProcs(cl_funcs.pHudInitFunc, "HUD_Init", true);
	GetClProcs(cl_funcs.pHudRedrawFunc, "HUD_Redraw", true);
	GetClProcs(cl_funcs.pHudUpdateClientDataFunc, "HUD_UpdateClientData", true);
	GetClProcs(cl_funcs.pHudResetFunc, "HUD_Reset", true);
	GetClProcs(cl_funcs.pClientMove, "HUD_PlayerMove", true);
	GetClProcs(cl_funcs.pClientMoveInit, "HUD_PlayerMoveInit", true);
	GetClProcs(cl_funcs.pClientTextureType, "HUD_PlayerMoveTexture", true);
	GetClProcs(cl_funcs.pIN_ActivateMouse, "IN_ActivateMouse", true);
	GetClProcs(cl_funcs.pIN_DeactivateMouse, "IN_DeactivateMouse", true);
	GetClProcs(cl_funcs.pIN_MouseEvent, "IN_MouseEvent", true);
	GetClProcs(cl_funcs.pIN_ClearStates, "IN_ClearStates", true);
	GetClProcs(cl_funcs.pIN_Accumulate, "IN_Accumulate", true);
	GetClProcs(cl_funcs.pCL_CreateMove, "CL_CreateMove", true);
	GetClProcs(cl_funcs.pCL_GetCameraOffsets, "CL_CameraOffset", true);
	GetClProcs(cl_funcs.pCamThink, "CAM_Think", true);
	GetClProcs(cl_funcs.pCL_IsThirdPerson, "CL_IsThirdPerson", true);
	GetClProcs(cl_funcs.pFindKey, "KB_Find", true);
	GetClProcs(cl_funcs.pCalcRefdef, "V_CalcRefdef", true);
	GetClProcs(cl_funcs.pAddEntity, "HUD_AddEntity", true);
	GetClProcs(cl_funcs.pCreateEntities, "HUD_CreateEntities", true);
	GetClProcs(cl_funcs.pDrawNormalTriangles, "HUD_DrawNormalTriangles", true);
	GetClProcs(cl_funcs.pDrawTransparentTriangles, "HUD_DrawTransparentTriangles", true);
	GetClProcs(cl_funcs.pStudioEvent, "HUD_StudioEvent", true);
	GetClProcs(cl_funcs.pShutdown, "HUD_Shutdown", true);
	GetClProcs(cl_funcs.pTxferLocalOverrides, "HUD_TxferLocalOverrides", true);
	GetClProcs(cl_funcs.pProcessPlayerState, "HUD_ProcessPlayerState", true);
	GetClProcs(cl_funcs.pTxferPredictionData, "HUD_TxferPredictionData", true);
	GetClProcs(cl_funcs.pReadDemoBuffer, "Demo_ReadBuffer", true);
	GetClProcs(cl_funcs.pConnectionlessPacket, "HUD_ConnectionlessPacket", true);
	GetClProcs(cl_funcs.pGetHullBounds, "HUD_GetHullBounds", true);
	GetClProcs(cl_funcs.pHudFrame, "HUD_Frame", true);
	GetClProcs(cl_funcs.pKeyEvent, "HUD_Key_Event", true);
	GetClProcs(cl_funcs.pPostRunCmd, "HUD_PostRunCmd", true);
	GetClProcs(cl_funcs.pTempEntUpdate, "HUD_TempEntUpdate", true);
	GetClProcs(cl_funcs.pGetUserEntity, "HUD_GetUserEntity", true);
	GetClProcs(cl_funcs.pVoiceStatus, "HUD_VoiceStatus", false);
	GetClProcs(cl_funcs.pDirectorMessage, "HUD_DirectorMessage", false);
	GetClProcs(cl_funcs.pStudioInterface, "HUD_GetStudioModelInterface", false);
	GetClProcs(cl_funcs.pChatInputPosition, "HUD_ChatInputPosition", false);
	GetClProcs(cl_funcs.pClientFactory, "ClientFactory", false);
	GetClProcs(cl_funcs.pGetPlayerTeam, "HUD_GetPlayerTeam", false);

	return true;
}

bool InitClientDll()
{
	g_pClientDll = Sys_LoadModule(g_ClDllPath.c_str());
	if (!g_pClientDll)
	{
		return false;
	}

	if (!InitSecureClient())
	{
		return InitInsecureClient();
	}

	return true;
}

void ShutdownClientDll()
{
	Sys_UnloadModule(g_pClientDll);
	g_pClientDll = nullptr;
}