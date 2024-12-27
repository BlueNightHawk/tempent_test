// Custom Tempent Renderer
// BlueNightHawk - 2024
// Reference : Xash3D FWGS
//			   GLQuake

#include <algorithm>
#include <vector>

#include "PlatformHeaders.h"
#include "SDL2/SDL.h"
#include "SDL2/SDL_opengl.h"

#include "cl_dll.h"

#include "studio.h"
#include "com_model.h"
#include "triangleapi.h"
#include "event_api.h"

#include "pmtrace.h"
#include "pm_defs.h"

#include "r_studioint.h"

extern ref_params_s g_refdef;

#define R_ModelOpaque(rm) (rm == kRenderNormal)

#define BIT(n) (1U << (n))
#define MODEL_TRANSPARENT BIT(3)	  // have transparent surfaces
#define VectorAverage(a, b, o) ((o)[0] = ((a)[0] + (b)[0]) * 0.5f, (o)[1] = ((a)[1] + (b)[1]) * 0.5f, (o)[2] = ((a)[2] + (b)[2]) * 0.5f)

extern ref_params_s g_refdef;
extern engine_studio_api_s IEngineStudio, IOriginalEngineStudio;
extern r_studio_interface_t studio_iface;

#define GLARE_FALLOFF 19000.0f

extern std::vector<cl_entity_s*> g_clVisEnts, g_clTransVisEnts;

static cl_entity_s *g_pCurrentEntity = nullptr;

static model_t* g_pRenderModel = nullptr;

PFNGLACTIVETEXTUREPROC pglActiveTexture;

#define VectorSet(v, x, y, z) ((v)[0] = (x), (v)[1] = (y), (v)[2] = (z))
static void SinCos(float radians, float* sine, float* cosine)
{
	*sine = sin(radians);
	*cosine = cos(radians);
}

/*
================
GetCurrentEntity
================
*/
cl_entity_s* GetCurrentEntity()
{
	if (g_pCurrentEntity)
		return g_pCurrentEntity;
	return IOriginalEngineStudio.GetCurrentEntity();
}

/*
================
SetRenderModel
================
*/
void SetRenderModel(struct model_s* model)
{
	g_pRenderModel = model;
	IOriginalEngineStudio.SetRenderModel(model);
}

/*
================
StudioDrawPoints
================
*/
void StudioDrawPoints()
{
	//if (g_pCurrentEntity)
	glEnable(GL_TEXTURE_2D);
	IOriginalEngineStudio.StudioDrawPoints();
}


/*
================
StudioSetupModel
================
*/
void StudioSetupModel(int bodypart, void** _ppbodypart, void** _ppsubmodel)
{
	auto e = GetCurrentEntity();
	studiohdr_t* pStudioHeader = (studiohdr_t*)IOriginalEngineStudio.Mod_Extradata(g_pRenderModel);

	int index;

	mstudiobodyparts_t** ppBodyPart;
	mstudiomodel_t** ppSubModel;

	IOriginalEngineStudio.StudioSetupModel(bodypart, (void**)&ppBodyPart, (void**)&ppSubModel);

	if (bodypart > pStudioHeader->numbodyparts)
		bodypart = 0;

	auto pBodyPart = (mstudiobodyparts_t*)((byte*)pStudioHeader + pStudioHeader->bodypartindex) + bodypart;

	index = e->curstate.body / pBodyPart->base;
	index = index % pBodyPart->nummodels;

	auto pSubModel = (mstudiomodel_t*)((byte*)pStudioHeader + pBodyPart->modelindex) + index;

	*ppBodyPart = pBodyPart;
	*ppSubModel = pSubModel;

	*_ppbodypart = pBodyPart;
	*_ppsubmodel = pSubModel;
}

static int R_RankForRenderMode(int rendermode)
{
	switch (rendermode)
	{
	case kRenderTransTexture:
		return 1; // draw second
	case kRenderTransAdd:
		return 2; // draw third
	case kRenderGlow:
		return 3; // must be last!
	}
	return 0;
}

/*
================
R_GetEntityRenderMode

check for texture flags
================
*/
int R_GetEntityRenderMode(cl_entity_t* ent)
{
	int i, opaque, trans;
	mstudiotexture_t* ptexture;
	model_t* model = NULL;
	studiohdr_t* phdr;

	if (ent->player) // check it for real playermodel
		model = IOriginalEngineStudio.SetupPlayerModel(ent->curstate.number - 1);

	if (!model)
		model = ent->model;

	if (!model)
		return ent->curstate.rendermode;

	if (model->type == mod_sprite)
		return ent->curstate.rendermode;

	if ((phdr = (studiohdr_t*)IOriginalEngineStudio.Mod_Extradata(model)) == NULL)
	{
		if (R_ModelOpaque(ent->curstate.rendermode))
		{
			// forcing to choose right sorting type
			if ((model && model->type == mod_brush) && (model->flags & MODEL_TRANSPARENT))
				return kRenderTransAlpha;
		}
		return ent->curstate.rendermode;
	}
	ptexture = (mstudiotexture_t*)((byte*)phdr + phdr->textureindex);

	for (opaque = trans = i = 0; i < phdr->numtextures; i++, ptexture++)
	{
		// ignore chrome & additive it's just a specular-like effect
		if ((ptexture->flags & STUDIO_NF_ADDITIVE) && !(ptexture->flags & STUDIO_NF_CHROME))
			trans++;
		else
			opaque++;
	}

	// if model is more additive than opaque
	if (trans > opaque)
		return kRenderTransAdd;
	return ent->curstate.rendermode;
}

/*
===============
R_OpaqueEntity

Opaque entity can be brush or studio model but sprite
===============
*/
qboolean R_OpaqueEntity(cl_entity_t* ent)
{
	if (R_GetEntityRenderMode(ent) == kRenderNormal)
	{
		switch (ent->curstate.renderfx)
		{
		case kRenderFxNone:
		case kRenderFxDeadPlayer:
		case kRenderFxLightMultiplier:
		case kRenderFxExplode:
			return true;
		}
	}
	return false;
}

/*
=================
CL_AddVisibleEntity

Add to client side renderer queue, don't take up engine's queue
=================
*/
int CL_AddVisibleEntity(cl_entity_t* pEntity)
{
	Vector mins = pEntity->origin + pEntity->model->mins;
	Vector maxs = pEntity->origin + pEntity->model->maxs;

	if (!gEngfuncs.pTriAPI->BoxInPVS(mins, maxs))
		return 0;

	if (R_OpaqueEntity(pEntity))
	{
		g_clVisEnts.push_back(pEntity);
	}
	else
	{
		g_clTransVisEnts.push_back(pEntity);
	}

	return 1;
}


/*
================
R_GlowSightDistance

Set sprite brightness factor
================
*/
float R_SpriteGlowBlend(Vector origin, int rendermode, int renderfx, float* pscale)
{
	float dist, brightness;
	Vector glowDist;
	pmtrace_t tr;

	Vector vieworg = g_refdef.vieworg;

	VectorSubtract(origin, vieworg, glowDist);
	dist = Length(glowDist);

	// Store off the old count
	gEngfuncs.pEventAPI->EV_PushPMStates();

	// Now add in all of the players.
	gEngfuncs.pEventAPI->EV_SetSolidPlayers(gEngfuncs.GetLocalPlayer()->index - 1);

	gEngfuncs.pEventAPI->EV_SetTraceHull(2);
	//if (!r_traceglow)
	//	r_traceglow = gEngfuncs.pfnGetCvarPointer("r_traceglow");
	//gEngfuncs.pEventAPI->EV_PlayerTrace(vieworg, origin, r_traceglow->value ? PM_GLASS_IGNORE : (PM_GLASS_IGNORE | PM_STUDIO_IGNORE), -1, &tr);
	gEngfuncs.pEventAPI->EV_PlayerTrace(vieworg, origin, PM_GLASS_IGNORE | PM_STUDIO_IGNORE, -1, &tr);

	gEngfuncs.pEventAPI->EV_PopPMStates();

	if ((1.0f - tr.fraction) * dist > 8.0f)
		return 0.0f;

	if (renderfx == kRenderFxNoDissipation)
		return 1.0f;

	brightness = GLARE_FALLOFF / (dist * dist);
	brightness = std::clamp(brightness, 0.05f, 1.0f);
	*pscale *= dist * (1.0f / 200.0f);

	return brightness;
}

/*
================
R_SpriteOccluded

Do occlusion test for glow-sprites
================
*/
bool R_SpriteOccluded(cl_entity_s *pEntity, float *pblend, float *scale)
{
	float blend = 0.0f;
	Vector sprite_mins, sprite_maxs;
	auto model = pEntity->model;
	auto origin = pEntity->origin;
	auto rendermode = pEntity->curstate.rendermode;

	// scale original bbox (no rotation for sprites)
	VectorScale(model->mins, *scale, sprite_mins);
	VectorScale(model->maxs, *scale, sprite_maxs);

	VectorAdd(sprite_mins, origin, sprite_mins);
	VectorAdd(sprite_maxs, origin, sprite_maxs);

	if (rendermode == kRenderGlow)
	{
		if (!gEngfuncs.pTriAPI->BoxInPVS(sprite_mins, sprite_maxs))
		{
			return true;
		}

		blend = R_SpriteGlowBlend(origin, rendermode, pEntity->curstate.renderfx, scale);
		*pblend = blend;

		if (blend <= 0.01f)
			return true; // faded
	}

	return !gEngfuncs.pTriAPI->BoxInPVS(sprite_mins, sprite_maxs);
}

/*
================
CL_StudioDrawTempEnt
================
*/
void CL_StudioDrawTempEnt(cl_entity_s* pEntity)
{
	g_pCurrentEntity = pEntity;

	pEntity->curstate.origin = pEntity->origin;
	pEntity->curstate.angles = pEntity->angles;

	if (pEntity->curstate.rendermode != kRenderNormal)
		IOriginalEngineStudio.StudioSetRenderamt(pEntity->curstate.renderamt);
	else
		IOriginalEngineStudio.StudioSetRenderamt(255);

	studio_iface.StudioDrawModel(STUDIO_RENDER | STUDIO_EVENTS);

	g_pCurrentEntity = nullptr;
}

/*
================
R_GetSpriteType
================
*/
int R_GetSpriteType(cl_entity_s* pEntity)
{
	if (!pEntity->model->cache.data)
		return -1;

	return ((msprite_t*)pEntity->model->cache.data)->type;
}


/*
================
R_SetSpriteOrientation


================
*/
void R_SetSpriteOrientation(cl_entity_s* pEntity, Vector &origin, Vector &forward, Vector &right, Vector &up)
{
	Vector vieworg = g_refdef.vieworg, v_forward = g_refdef.forward, v_right = g_refdef.right, v_up = g_refdef.up;

	auto type = R_GetSpriteType(pEntity);
	Vector angles = pEntity->angles;

	origin = pEntity->origin;

	// automatically roll parallel sprites if requested
	if (pEntity->angles[2] != 0.0f && type == SPR_VP_PARALLEL)
		type = SPR_VP_PARALLEL_ORIENTED;

	switch (type)
	{
	case SPR_VP_PARALLEL_UPRIGHT:
	{
		float dot = v_forward[2];
		if ((dot > 0.999848f) || (dot < -0.999848f)) // cos(1 degree) = 0.999848
			return;									 // invisible
		VectorSet(up, 0.0f, 0.0f, 1.0f);
		VectorSet(right, v_forward[1], v_forward[0], 0.0f);
		VectorNormalize(right);
	}
		break;
	case SPR_FACING_UPRIGHT:
		VectorSet(right, origin[1] - vieworg[1], -(origin[0] - vieworg[0]), 0.0f);
		VectorSet(up, 0.0f, 0.0f, 1.0f);
		VectorNormalize(right);
		break;
	case SPR_ORIENTED:
		NormalizeAngles(angles);
		gEngfuncs.pfnAngleVectors(angles, forward, right, up);
		VectorScale(forward, 0.01f, forward); // to avoid z-fighting
		VectorSubtract(origin, forward, origin);
		break;
	case SPR_VP_PARALLEL_ORIENTED:
	{
		float angle = pEntity->angles[2] * (M_PI / 360.0f);
		float sr, cr;
		SinCos(angle, &sr, &cr);
		for (int i = 0; i < 3; i++)
		{
			right[i] = (v_right[i] * cr + v_up[i] * sr);
			up[i] = v_right[i] * -sr + v_up[i] * cr;
		}
	}
		break;
	default:
	case SPR_VP_PARALLEL:
		VectorCopy(v_right, right);
		VectorCopy(v_up, up);
		break;
	}
}


/*
================
R_SetRenderMode

Set render mode for sprites
================
*/
void R_SetRenderMode(int rendermode)
{
	gEngfuncs.pTriAPI->RenderMode(rendermode);

	switch (rendermode)
	{
	case kRenderTransAlpha:
		glDepthMask(GL_FALSE);
	case kRenderTransColor:
	case kRenderTransTexture:
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		break;
	case kRenderGlow:
		glDisable(GL_DEPTH_TEST);
	case kRenderTransAdd:
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE);
		glDepthMask(GL_FALSE);
		break;
	case kRenderNormal:
	default:
		glDisable(GL_BLEND);
		break;
	}

	// all sprites can have color
	if (rendermode == kRenderTransColor)
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_ALPHA);
	else
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glEnable(GL_ALPHA_TEST);
}

/*
===============
CL_FxBlend
===============
*/
int CL_FxBlend(cl_entity_t* e)
{
	int blend = 0;
	float offset, dist;
	Vector tmp;

	auto pl = gEngfuncs.GetLocalPlayer();

	Vector vieworg = pl->origin;
	Vector viewheight;
	Vector viewangles, forward, right, up;

	gEngfuncs.pEventAPI->EV_LocalPlayerViewheight(viewheight);

	vieworg = vieworg + viewheight;
	gEngfuncs.GetViewAngles(viewangles);

	gEngfuncs.pfnAngleVectors(viewangles, forward, right, up);

	offset = ((int)e->index) * 363.0f; // Use ent index to de-sync these fx

	switch (e->curstate.renderfx)
	{
	case kRenderFxPulseSlowWide:
		blend = e->curstate.renderamt + 0x40 * sin(gEngfuncs.GetClientTime() * 2 + offset);
		break;
	case kRenderFxPulseFastWide:
		blend = e->curstate.renderamt + 0x40 * sin(gEngfuncs.GetClientTime() * 8 + offset);
		break;
	case kRenderFxPulseSlow:
		blend = e->curstate.renderamt + 0x10 * sin(gEngfuncs.GetClientTime() * 2 + offset);
		break;
	case kRenderFxPulseFast:
		blend = e->curstate.renderamt + 0x10 * sin(gEngfuncs.GetClientTime() * 8 + offset);
		break;
	case kRenderFxFadeSlow:
		//if (RP_NORMALPASS())
		{
			if (e->curstate.renderamt > 0)
				e->curstate.renderamt -= 1;
			else
				e->curstate.renderamt = 0;
		}
		blend = e->curstate.renderamt;
		break;
	case kRenderFxFadeFast:
		//if (RP_NORMALPASS())
		{
			if (e->curstate.renderamt > 3)
				e->curstate.renderamt -= 4;
			else
				e->curstate.renderamt = 0;
		}
		blend = e->curstate.renderamt;
		break;
	case kRenderFxSolidSlow:
		//if (RP_NORMALPASS())
		{
			if (e->curstate.renderamt < 255)
				e->curstate.renderamt += 1;
			else
				e->curstate.renderamt = 255;
		}
		blend = e->curstate.renderamt;
		break;
	case kRenderFxSolidFast:
		//if (RP_NORMALPASS())
		{
			if (e->curstate.renderamt < 252)
				e->curstate.renderamt += 4;
			else
				e->curstate.renderamt = 255;
		}
		blend = e->curstate.renderamt;
		break;
	case kRenderFxStrobeSlow:
		blend = 20 * sin(gEngfuncs.GetClientTime() * 4 + offset);
		if (blend < 0)
			blend = 0;
		else
			blend = e->curstate.renderamt;
		break;
	case kRenderFxStrobeFast:
		blend = 20 * sin(gEngfuncs.GetClientTime() * 16 + offset);
		if (blend < 0)
			blend = 0;
		else
			blend = e->curstate.renderamt;
		break;
	case kRenderFxStrobeFaster:
		blend = 20 * sin(gEngfuncs.GetClientTime() * 36 + offset);
		if (blend < 0)
			blend = 0;
		else
			blend = e->curstate.renderamt;
		break;
	case kRenderFxFlickerSlow:
		blend = 20 * (sin(gEngfuncs.GetClientTime() * 2) + sin(gEngfuncs.GetClientTime() * 17 + offset));
		if (blend < 0)
			blend = 0;
		else
			blend = e->curstate.renderamt;
		break;
	case kRenderFxFlickerFast:
		blend = 20 * (sin(gEngfuncs.GetClientTime() * 16) + sin(gEngfuncs.GetClientTime() * 23 + offset));
		if (blend < 0)
			blend = 0;
		else
			blend = e->curstate.renderamt;
		break;
	case kRenderFxHologram:
	case kRenderFxDistort:
		VectorCopy(e->origin, tmp);
		VectorSubtract(tmp, vieworg, tmp);
		dist = DotProduct(tmp, forward);

		// turn off distance fade
		if (e->curstate.renderfx == kRenderFxDistort)
			dist = 1;

		if (dist <= 0)
		{
			blend = 0;
		}
		else
		{
			e->curstate.renderamt = 180;
			if (dist <= 100)
				blend = e->curstate.renderamt;
			else
				blend = (int)((1.0f - (dist - 100) * (1.0f / 400.0f)) * e->curstate.renderamt);
			blend += gEngfuncs.pfnRandomLong(-32, 31);
		}
		break;
	default:
		blend = e->curstate.renderamt;
		break;
	}

	blend = std::clamp(blend, 0, 255);

	return blend;
}


/*
================
R_GetSpriteFrame
================
*/
mspriteframe_t* R_GetSpriteFrame(cl_entity_t* currententity)
{
	msprite_t* psprite;
	mspritegroup_t* pspritegroup;
	mspriteframe_t* pspriteframe;
	int i, numframes, frame;
	float *pintervals, fullinterval, targettime, time;

	psprite = (msprite_t*)currententity->model->cache.data;
	frame = currententity->curstate.frame;

	if ((frame >= psprite->numframes) || (frame < 0))
	{
		gEngfuncs.Con_Printf("R_DrawSprite: no such frame %d\n", frame);
		frame = 0;
	}

	if (psprite->frames[frame].type == SPR_SINGLE)
	{
		pspriteframe = psprite->frames[frame].frameptr;
	}
	else
	{
		pspritegroup = (mspritegroup_t*)psprite->frames[frame].frameptr;
		pintervals = pspritegroup->intervals;
		numframes = pspritegroup->numframes;
		fullinterval = pintervals[numframes - 1];

		time = gEngfuncs.GetClientTime() + currententity->syncbase;

		// when loading in Mod_LoadSpriteGroup, we guaranteed all interval values
		// are positive, so we don't have to worry about division by 0
		targettime = time - ((int)(time / fullinterval)) * fullinterval;

		for (i = 0; i < (numframes - 1); i++)
		{
			if (pintervals[i] > targettime)
				break;
		}

		pspriteframe = pspritegroup->frames[i];
	}

	return pspriteframe;
}

/*
=================
R_DrawSpriteQuad
=================
*/
static void R_DrawSpriteQuad(const mspriteframe_t* frame, const Vector org, const Vector v_right, const Vector v_up, const float scale)
{
	Vector point;

	gEngfuncs.pTriAPI->TexCoord2f(0.0f, 1.0f);
	VectorMA(org, frame->down * scale, v_up, point);
	VectorMA(point, frame->left * scale, v_right, point);
	gEngfuncs.pTriAPI->Vertex3fv(point);
	gEngfuncs.pTriAPI->TexCoord2f(0.0f, 0.0f);
	VectorMA(org, frame->up * scale, v_up, point);
	VectorMA(point, frame->left * scale, v_right, point);
	gEngfuncs.pTriAPI->Vertex3fv(point);
	gEngfuncs.pTriAPI->TexCoord2f(1.0f, 0.0f);
	VectorMA(org, frame->up * scale, v_up, point);
	VectorMA(point, frame->right * scale, v_right, point);
	gEngfuncs.pTriAPI->Vertex3fv(point);
	gEngfuncs.pTriAPI->TexCoord2f(1.0f, 1.0f);
	VectorMA(org, frame->down * scale, v_up, point);
	VectorMA(point, frame->right * scale, v_right, point);
	gEngfuncs.pTriAPI->Vertex3fv(point);
}

/*
================
CL_DrawSpriteTempEnt

Setup and draw sprite tempent
================
*/
void CL_DrawSpriteTempEnt(cl_entity_s* pEntity)
{
	Vector vColor = Vector(pEntity->curstate.rendercolor.r / 255.0f, pEntity->curstate.rendercolor.g / 255.0f, pEntity->curstate.rendercolor.b / 255.0f);
	float blend = CL_FxBlend(pEntity) / 255.0f;
	float scale = pEntity->curstate.scale;

	Vector origin, forward, right, up;
	
	R_SetSpriteOrientation(pEntity, origin, forward, right, up);

	if (scale <= 0.0f)
		scale = 1.0f;

	if (R_SpriteOccluded(pEntity, &blend, &scale))
	{
		return;
	}

	// NOTE: never pass sprites with rendercolor '0 0 0' it's a stupid Valve Hammer Editor bug
	if (pEntity->curstate.rendercolor.r || pEntity->curstate.rendercolor.g || pEntity->curstate.rendercolor.b)
	{
		vColor[0] = (float)pEntity->curstate.rendercolor.r * (1.0f / 255.0f);
		vColor[1] = (float)pEntity->curstate.rendercolor.g * (1.0f / 255.0f);
		vColor[2] = (float)pEntity->curstate.rendercolor.b * (1.0f / 255.0f);
	}
	else
	{
		vColor[0] = 1.0f;
		vColor[1] = 1.0f;
		vColor[2] = 1.0f;
	}

	R_SetRenderMode(pEntity->curstate.rendermode);

	gEngfuncs.pTriAPI->CullFace(TRI_NONE);
	auto sprframe = R_GetSpriteFrame(pEntity);
	gEngfuncs.pTriAPI->SpriteTexture(pEntity->model, pEntity->curstate.frame);

	gEngfuncs.pTriAPI->Begin(TRI_QUADS);
	gEngfuncs.pTriAPI->Color4f(vColor.x, vColor.y, vColor.z, 
		blend);

	R_DrawSpriteQuad(sprframe, origin, right, up, scale);

	gEngfuncs.pTriAPI->End();

	gEngfuncs.pTriAPI->RenderMode(kRenderNormal);
	gEngfuncs.pTriAPI->CullFace(TRI_FRONT);

	glDisable(GL_ALPHA_TEST);
	glDepthMask(GL_TRUE);

	if (pEntity->curstate.rendermode != kRenderNormal)
	{
		glDisable(GL_BLEND);
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
		glEnable(GL_DEPTH_TEST);
	}
}

void CL_DrawTempEnts()
{
	glEnable(GL_TEXTURE_2D);

	if (g_clVisEnts.size() == 0)
		return;

	if (!pglActiveTexture)
		pglActiveTexture = decltype(pglActiveTexture)(SDL_GL_GetProcAddress("glActiveTexture"));

	// BUzer: workaround half-life's bug, when multitexturing left enabled after
	// rendering brush entities
	pglActiveTexture(GL_TEXTURE1);
	glDisable(GL_TEXTURE_2D);

	// Set the active texture unit
	pglActiveTexture(GL_TEXTURE0);

    // Push texture state
	glPushAttrib(GL_TEXTURE_BIT);

	g_pRenderModel = nullptr;
	g_pCurrentEntity = nullptr;

	//gEngfuncs.Con_Printf("CL_TempEnts : Rendering %i\n", g_clVisEnts.size());

	for (auto f : g_clVisEnts)
	{
		if (f->model->type == mod_sprite)
		{
			CL_DrawSpriteTempEnt(f);
		}
		else if (f->model->type == mod_studio)
		{
			CL_StudioDrawTempEnt(f);
		}
	}
	glPopAttrib();
}

void CL_DrawTransTempEnts()
{
	glEnable(GL_TEXTURE_2D);

	if (g_clTransVisEnts.size() == 0)
		return;

	if (!pglActiveTexture)
		pglActiveTexture = decltype(pglActiveTexture)(SDL_GL_GetProcAddress("glActiveTexture"));

	//CL_SortTempEnts();

	// BUzer: workaround half-life's bug, when multitexturing left enabled after
	// rendering brush entities
	pglActiveTexture(GL_TEXTURE1);
	glDisable(GL_TEXTURE_2D);

	// Set the active texture unit
	pglActiveTexture(GL_TEXTURE0);

    // Push texture state
	glPushAttrib(GL_TEXTURE_BIT);

	g_pRenderModel = nullptr;
	g_pCurrentEntity = nullptr;

	//gEngfuncs.Con_Printf("CL_TransTempEnts : Rendering %i\n", g_clTransVisEnts.size());

	for (auto f : g_clTransVisEnts)
	{
		if (f->model->type == mod_sprite)
		{
			CL_DrawSpriteTempEnt(f);
		}
		else if (f->model->type == mod_studio)
		{
			CL_StudioDrawTempEnt(f);
		}
	}
	glPopAttrib();
}
