#include "cl_dll.h"
#include "Exports.h"

cl_enginefunc_t gEngfuncs;
cldll_func_t cl_funcs;

cldll_func_dst_t* g_pcldstAddrs;
modfuncs_s* g_pmodfuncs;

extern "C" void DLLEXPORT F(void* pv)
{
	cldll_func_t* pcldll_func = (cldll_func_t*)pv;

	// Hack!
	g_pcldstAddrs = ((cldll_func_dst_t*)pcldll_func->pHudVidInitFunc);
	g_pmodfuncs = ((modfuncs_s*)pcldll_func->pInitFunc);
	cldll_func_t cldll_func =
		{
			Initialize,
			HUD_Init,
			HUD_VidInit,
			HUD_Redraw,
			HUD_UpdateClientData,
			HUD_Reset,
			HUD_PlayerMove,
			HUD_PlayerMoveInit,
			HUD_PlayerMoveTexture,
			IN_ActivateMouse,
			IN_DeactivateMouse,
			IN_MouseEvent,
			IN_ClearStates,
			IN_Accumulate,
			CL_CreateMove,
			CL_IsThirdPerson,
			CL_CameraOffset,
			KB_Find,
			CAM_Think,
			V_CalcRefdef,
			HUD_AddEntity,
			HUD_CreateEntities,
			HUD_DrawNormalTriangles,
			HUD_DrawTransparentTriangles,
			HUD_StudioEvent,
			HUD_PostRunCmd,
			HUD_Shutdown,
			HUD_TxferLocalOverrides,
			HUD_ProcessPlayerState,
			HUD_TxferPredictionData,
			Demo_ReadBuffer,
			HUD_ConnectionlessPacket,
			HUD_GetHullBounds,
			HUD_Frame,
			HUD_Key_Event,
			HUD_TempEntUpdate,
			HUD_GetUserEntity,
			HUD_VoiceStatus,
			HUD_DirectorMessage,
			HUD_GetStudioModelInterface,
			HUD_ChatInputPosition,
		};

	*pcldll_func = cldll_func;
}

	// From hl_weapons
void DLLEXPORT HUD_PostRunCmd(struct local_state_s* from, struct local_state_s* to, struct usercmd_s* cmd, int runfuncs, double time, unsigned int random_seed)
{
	cl_funcs.pPostRunCmd(from, to, cmd, runfuncs, time, random_seed);
}

// From cdll_int
int DLLEXPORT Initialize(cl_enginefunc_t* pEnginefuncs, int iVersion)
{
	return cl_funcs.pInitFunc(pEnginefuncs, iVersion);
}

int DLLEXPORT HUD_VidInit(void)
{
	return cl_funcs.pHudVidInitFunc();
}

void DLLEXPORT HUD_Init(void)
{
	cl_funcs.pHudInitFunc();
}

int DLLEXPORT HUD_Redraw(float flTime, int intermission)
{
	return cl_funcs.pHudRedrawFunc(flTime, intermission);
}

int DLLEXPORT HUD_UpdateClientData(client_data_t* cdata, float flTime)
{
	return cl_funcs.pHudUpdateClientDataFunc(cdata, flTime);
}

void DLLEXPORT HUD_Reset(void)
{
	cl_funcs.pHudResetFunc();
}

void DLLEXPORT HUD_PlayerMove(struct playermove_s* ppmove, int server)
{
	cl_funcs.pClientMove(ppmove, server);
}

void DLLEXPORT HUD_PlayerMoveInit(struct playermove_s* ppmove)
{
	cl_funcs.pClientMoveInit(ppmove);
}

char DLLEXPORT HUD_PlayerMoveTexture(char* name)
{
	return cl_funcs.pClientTextureType(name);
}

int DLLEXPORT HUD_ConnectionlessPacket(const struct netadr_s* net_from, const char* args, char* response_buffer, int* response_buffer_size)
{
	return cl_funcs.pConnectionlessPacket(net_from, args, response_buffer, response_buffer_size);
}

int DLLEXPORT HUD_GetHullBounds(int hullnumber, float* mins, float* maxs)
{
	return cl_funcs.pGetHullBounds(hullnumber, mins, maxs);
}

void DLLEXPORT HUD_Frame(double time)
{
	cl_funcs.pHudFrame(time);
}

void DLLEXPORT HUD_VoiceStatus(int entindex, qboolean bTalking)
{
	if (cl_funcs.pVoiceStatus)
		cl_funcs.pVoiceStatus(entindex, bTalking);
}

void DLLEXPORT HUD_DirectorMessage(int iSize, void* pbuf)
{
	if (cl_funcs.pDirectorMessage)
		cl_funcs.pDirectorMessage(iSize, pbuf);
}

void DLLEXPORT HUD_ChatInputPosition(int* x, int* y)
{
	if (cl_funcs.pChatInputPosition)
		cl_funcs.pChatInputPosition(x,y);
}

// From demo
void DLLEXPORT Demo_ReadBuffer(int size, unsigned char* buffer)
{
	cl_funcs.pReadDemoBuffer(size, buffer);
}

// From entity
int DLLEXPORT HUD_AddEntity(int type, struct cl_entity_s* ent, const char* modelname)
{
	return cl_funcs.pAddEntity(type, ent, modelname);
}

void DLLEXPORT HUD_CreateEntities(void)
{
	cl_funcs.pCreateEntities();
}

void DLLEXPORT HUD_StudioEvent(const struct mstudioevent_s* event, const struct cl_entity_s* entity)
{
	cl_funcs.pStudioEvent(event, entity);
}

void DLLEXPORT HUD_TxferLocalOverrides(struct entity_state_s* state, const struct clientdata_s* client)
{
	cl_funcs.pTxferLocalOverrides(state, client);
}

void DLLEXPORT HUD_ProcessPlayerState(struct entity_state_s* dst, const struct entity_state_s* src)
{
	cl_funcs.pProcessPlayerState(dst, src);
}

void DLLEXPORT HUD_TxferPredictionData(struct entity_state_s* ps, const struct entity_state_s* pps, struct clientdata_s* pcd, const struct clientdata_s* ppcd, struct weapon_data_s* wd, const struct weapon_data_s* pwd)
{
	cl_funcs.pTxferPredictionData(ps, pps, pcd, ppcd, wd, pwd);
}

void DLLEXPORT HUD_TempEntUpdate(double frametime, double client_time, double cl_gravity, struct tempent_s** ppTempEntFree, struct tempent_s** ppTempEntActive, int (*Callback_AddVisibleEntity)(struct cl_entity_s* pEntity), void (*Callback_TempEntPlaySound)(struct tempent_s* pTemp, float damp))
{
	cl_funcs.pTempEntUpdate(frametime, client_time, cl_gravity, ppTempEntActive, ppTempEntActive, Callback_AddVisibleEntity, Callback_TempEntPlaySound);
}

struct cl_entity_s DLLEXPORT* HUD_GetUserEntity(int index)
{
	return cl_funcs.pGetUserEntity(index);
}

// From in_camera
void DLLEXPORT CAM_Think(void)
{
	cl_funcs.pCamThink();
}

int DLLEXPORT CL_IsThirdPerson(void)
{
	return cl_funcs.pCL_IsThirdPerson();
}

void DLLEXPORT CL_CameraOffset(float* ofs)
{
	cl_funcs.pCL_GetCameraOffsets(ofs);
}

// From input
struct kbutton_s DLLEXPORT* KB_Find(const char* name)
{
	return cl_funcs.pFindKey(name);
}

void DLLEXPORT CL_CreateMove(float frametime, struct usercmd_s* cmd, int active)
{
	cl_funcs.pCL_CreateMove(frametime, cmd, active);
}

void DLLEXPORT HUD_Shutdown(void)
{
	cl_funcs.pShutdown();
}

int DLLEXPORT HUD_Key_Event(int eventcode, int keynum, const char* pszCurrentBinding)
{
	return cl_funcs.pKeyEvent(eventcode, keynum, pszCurrentBinding);
}

// From inputw32
void DLLEXPORT IN_ActivateMouse(void)
{
	cl_funcs.pIN_ActivateMouse();
}

void DLLEXPORT IN_DeactivateMouse(void)
{
	cl_funcs.pIN_DeactivateMouse();
}

void DLLEXPORT IN_MouseEvent(int mstate)
{
	cl_funcs.pIN_MouseEvent(mstate);
}

void DLLEXPORT IN_Accumulate(void)
{
	cl_funcs.pIN_Accumulate();
}

void DLLEXPORT IN_ClearStates(void)
{
	cl_funcs.pIN_ClearStates();
}


// From tri
void DLLEXPORT HUD_DrawNormalTriangles(void)
{
	cl_funcs.pDrawNormalTriangles();
}

void DLLEXPORT HUD_DrawTransparentTriangles(void)
{
	cl_funcs.pDrawTransparentTriangles();
}


// From view
void DLLEXPORT V_CalcRefdef(struct ref_params_s* pparams)
{
	return cl_funcs.pCalcRefdef(pparams);
}


// From GameStudioModelRenderer
int DLLEXPORT HUD_GetStudioModelInterface(int version, struct r_studio_interface_s** ppinterface, struct engine_studio_api_s* pstudio)
{
	return cl_funcs.pStudioInterface(version, ppinterface, pstudio);
}
