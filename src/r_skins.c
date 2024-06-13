// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 1993-1996 by id Software, Inc.
// Copyright (C) 1998-2000 by DooM Legacy Team.
// Copyright (C) 1999-2024 by Sonic Team Junior.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  r_skins.c
/// \brief Loading skins

#include "doomdef.h"
#include "console.h"
#include "g_game.h"
#include "r_local.h"
#include "st_stuff.h"
#include "w_wad.h"
#include "z_zone.h"
#include "m_menu.h"
#include "m_misc.h"
#include "info.h" // spr2names
#include "deh_soc.h"
#include "i_video.h" // rendermode
#include "i_system.h"
#include "r_things.h"
#include "r_skins.h"
#include "p_local.h"
#include "p_animation.h"
#include "dehacked.h" // get_number (for thok)
#include "m_cond.h"
#ifdef HWRENDER
#include "hardware/hw_md2.h"
#endif

INT32 numskins = 0;
skin_t **skins = NULL;

static const char *player_anim_names[] =
{
	"stand",
	"wait",
	"walk",
	"skid",
	"run",
	"dash",
	"pain",
	"stun",
	"dead",
	"drown",
	"roll",
	"gasp",
	"jump",
	"spring",
	"fall",
	"edge",
	"ride",
	"spindash",
	"fly",
	"swim",
	"tired",
	"glide",
	"land",
	"cling",
	"climb",
	"float",
	"float_run",
	"bounce",
	"fire",
	"twinspin",
	"melee",
	"melee_land",
	"transformation",
	"nights_stand",
	"nights_float",
	"nights_fly",
	"nights_drill",
	"nights_stun",
	"nights_pull",
	"nights_attack",

	"tail_0",
	"tail_1",
	"tail_2",
	"tail_3",
	"tail_4",
	"tail_5",
	"tail_6",
	"tail_7",
	"tail_8",
	"tail_9",
	"tail_a",
	"tail_b",
	"tail_c",

	"continue",
	"continue_lift",
	"continue_spin",
	"continue_partner",

	"end_sign",
	"life_icon",

	"extra"
};

UINT16 P_GetSkinAnimation(skin_t *skin, UINT8 spriteset)
{
	if (spriteset >= NUMSKINSPRITESETS)
		spriteset = SKINSPRITES_BASE;

	return skin->sprites[spriteset].animation_id;
}

const char *P_GetPlayerAnimName(UINT16 playeranim)
{
	if (playeranim >= SPR2_XTRA)
		return NULL;

	return player_anim_names[playeranim];
}

// Checks if an object should use super sprites
boolean P_ShouldUseSuperSprites(mobj_t *mobj, boolean use_super)
{
	if (mobj->player)
	{
		if (mobj->player->charflags & SF_NOSUPERSPRITES || (mobj->player->powers[pw_carry] == CR_NIGHTSMODE && (mobj->player->charflags & SF_NONIGHTSSUPER)))
			use_super = false;
		else if (mobj->player->powers[pw_super] || (mobj->player->powers[pw_carry] == CR_NIGHTSMODE && (mobj->player->charflags & SF_SUPER)))
			use_super = true;
	}

	if (use_super)
	{
		if (mobj->eflags & MFE_FORCENOSUPER)
			use_super = false;
	}
	else if (mobj->eflags & MFE_FORCESUPER)
		use_super = true;

	return use_super;
}

UINT16 P_GetPlayerSubanimReplacement(skin_t *skin, UINT16 spr2, player_t *player)
{
	switch (spr2)
	{
	// Normal special cases.
	case SPR2_JUMP:
		return ((player ? player->charflags : skin->flags) & SF_NOJUMPSPIN) ? SPR2_SPNG : SPR2_ROLL;
	case SPR2_TIRE:
		return ((player ? player->charability : skin->ability) == CA_SWIM) ? SPR2_SWIM : SPR2_FLY;
	// Use the handy list, that's what it's there for!
	default:
		return spr2defaults[spr2];
	}
}

// For the default spritesets, this tries each subanimation's immediate predecessor until
// it finds one with a number of frames or ends up at standing.
// For non-default spritesets, does the same as above - but tries the spriteset for each
// subanimation before the default spriteset.
UINT16 P_GetSkinSubanimation(skin_t *skin, UINT16 subanim, UINT8 spriteset, player_t *player, UINT8 *found_spriteset)
{
	UINT8 stored_spriteset = spriteset;
	UINT8 i = 0;

	if (!skin)
	{
		if (found_spriteset)
			*found_spriteset = SKINSPRITES_BASE;
		return SPR2_STND;
	}

	while (!P_IsSkinAnimationValid(skin, subanim, spriteset)
		&& subanim != SPR2_STND
		&& ++i < 32) // recursion limiter
	{
		if (spriteset != SKINSPRITES_BASE)
		{
			stored_spriteset = spriteset;
			spriteset = SKINSPRITES_BASE;
			continue;
		}

		subanim = P_GetPlayerSubanimReplacement(skin, subanim, player);

		spriteset = stored_spriteset;
	}

	if (i >= 32) // probably an infinite loop...
	{
		subanim = SPR2_STND;
		spriteset = SKINSPRITES_BASE;
	}

	if (found_spriteset)
		*found_spriteset = spriteset;

	return subanim;
}

static skinspritedef_t *P_GetSkinSpriteSet(skin_t *skin, UINT8 spriteset)
{
	if (!skin)
		return NULL;

	if (spriteset >= NUMSKINSPRITESETS)
		spriteset = SKINSPRITES_BASE;

	return &skin->sprites[spriteset];
}

static skinspritedef_t *P_GetSkinSpritesForAnimation(skin_t *skin, UINT16 anim)
{
	for (unsigned i = 0; i < NUMSKINSPRITESETS; i++)
	{
		if (skin->sprites[i].animation_id && skin->sprites[i].animation_id == anim)
			return &skin->sprites[i];
	}

	return &skin->sprites[SKINSPRITES_BASE];
}

// Gets the sprite number of a skin subanimation
spritenum_t P_GetSkinSpriteID(skin_t *skin, UINT16 subanim, UINT8 spriteset)
{
	if (!skin || subanim >= NUMPLAYERSPRITES)
		return SPR_NULL;

	return P_GetSkinSpriteSet(skin, spriteset)->spritenum[subanim];
}

// Gets the spritedef of a skin subanimation
spritedef_t *P_GetSkinSpritedef(skin_t *skin, UINT16 subanim, UINT8 spriteset)
{
	spritenum_t sprite = P_GetSkinSpriteID(skin, subanim, spriteset);
	if (sprite == SPR_NULL)
		return NULL;

	return &sprites[sprite];
}

// Gets the spriteinfo of a skin subanimation
spriteinfo_t *P_GetSkinSpriteInfo(skin_t *skin, UINT16 subanim, UINT8 spriteset)
{
	spritenum_t sprite = P_GetSkinSpriteID(skin, subanim, spriteset);
	if (sprite == SPR_NULL)
		return NULL;

	return &spriteinfo[sprite];
}

// Gets the spritedef of a skin from the given animation and subanimation IDs
spritedef_t *P_GetSkinAnimSpritedef(skin_t *skin, UINT16 anim, UINT16 subanim)
{
	if (!skin || subanim >= NUMPLAYERSPRITES)
		return NULL;

	skinspritedef_t *def = P_GetSkinSpritesForAnimation(skin, anim);
	spritenum_t sprite = def->spritenum[subanim];
	if (sprite == SPR_NULL)
		return NULL;

	return &sprites[sprite];
}

// Gets the spriteinfo of a skin from the given animation and subanimation IDs
spriteinfo_t *P_GetSkinAnimSpriteInfo(skin_t *skin, UINT16 anim, UINT16 subanim)
{
	if (!skin || subanim >= NUMPLAYERSPRITES)
		return NULL;

	skinspritedef_t *def = P_GetSkinSpritesForAnimation(skin, anim);
	spritenum_t sprite = def->spritenum[subanim];
	if (sprite == SPR_NULL)
		return NULL;

	return &spriteinfo[sprite];
}

// Checks if a skin animation is valid
boolean P_IsSkinAnimationValid(skin_t *skin, UINT16 subanim, UINT8 spriteset)
{
	spritedef_t *sprdef = P_GetSkinSpritedef(skin, subanim, spriteset);
	return sprdef && sprdef->numframes;
}

static void Sk_SetDefaultValue(skin_t *skin)
{
	INT32 i;
	//
	// set default skin values
	//
	memset(skin, 0, sizeof (skin_t));
	snprintf(skin->name,
		sizeof skin->name, "skin %u", (UINT32)(skin->skinnum));
	skin->name[sizeof skin->name - 1] = '\0';
	skin->wadnum = INT16_MAX;

	skin->flags = 0;

	strcpy(skin->realname, "Someone");
	strcpy(skin->hudname, "???");
	strcpy(skin->supername, "Someone super");

	skin->starttranscolor = 96;
	skin->prefcolor = SKINCOLOR_GREEN;
	skin->supercolor = SKINCOLOR_SUPERGOLD1;
	skin->prefoppositecolor = 0; // use tables

	skin->normalspeed = 36<<FRACBITS;
	skin->runspeed = 28<<FRACBITS;
	skin->thrustfactor = 5;
	skin->accelstart = 96;
	skin->acceleration = 40;

	skin->ability = CA_NONE;
	skin->ability2 = CA2_SPINDASH;
	skin->jumpfactor = FRACUNIT;
	skin->actionspd = 30<<FRACBITS;
	skin->mindash = 15<<FRACBITS;
	skin->maxdash = 70<<FRACBITS;

	skin->radius = mobjinfo[MT_PLAYER].radius;
	skin->height = mobjinfo[MT_PLAYER].height;
	skin->spinheight = FixedMul(skin->height, 2*FRACUNIT/3);

	skin->shieldscale = FRACUNIT;
	skin->camerascale = FRACUNIT;

	skin->thokitem = -1;
	skin->spinitem = -1;
	skin->revitem = -1;
	skin->followitem = 0;

	skin->highresscale = FRACUNIT;
	skin->contspeed = 17;
	skin->contangle = 0;

	skin->natkcolor = SKINCOLOR_NONE;

	for (i = 0; i < sfx_skinsoundslot0; i++)
		if (S_sfx[i].skinsound != -1)
			skin->soundsid[S_sfx[i].skinsound] = i;
}

//
// Initialize the basic skins
//
void R_InitSkins(void)
{
	// no default skin!
	numskins = 0;
}

UINT32 R_GetSkinAvailabilities(void)
{
	UINT32 response = 0;
	UINT32 unlockShift = 0;
	INT32 i;

	for (i = 0; i < MAXUNLOCKABLES; i++)
	{
		if (unlockables[i].type != SECRET_SKIN)
		{
			continue;
		}

		if (unlockShift >= 32)
		{
			// This crash is impossible to trigger as is,
			// but it could happen if MAXUNLOCKABLES is ever made higher than 32,
			// and someone makes a mod that has 33+ unlockable characters. :V
			// 2022/03/15: MAXUNLOCKABLES is now higher than 32
			I_Error("Too many unlockable characters! (maximum is 32)\n");
			return 0;
		}

		if (clientGamedata->unlocked[i])
		{
			response |= (1 << unlockShift);
		}

		unlockShift++;
	}

	return response;
}

// returns true if available in circumstances, otherwise nope
// warning don't use with an invalid skinnum other than -1 which always returns true
boolean R_SkinUsable(INT32 playernum, INT32 skinnum)
{
	INT32 unlockID = -1;
	UINT32 unlockShift = 0;
	INT32 i;

	if (skinnum == -1)
	{
		// Simplifies things elsewhere, since there's already plenty of checks for less-than-0...
		return true;
	}

	if (modeattacking)
	{
		// If you have someone else's run you might as well take a look
		return true;
	}

	if (Playing() && mapheaderinfo[gamemap-1] && (R_SkinAvailable(mapheaderinfo[gamemap-1]->forcecharacter) == skinnum))
	{
		// Force 1.
		return true;
	}

	if (netgame && (cv_forceskin.value == skinnum))
	{
		// Force 2.
		return true;
	}

	if (metalrecording && skinnum == 5)
	{
		// Force 3.
		return true;
	}

	if (playernum != -1 && players[playernum].bot)
	{
		// Force 4.
		return true;
	}

	// We will now check if this skin is supposed to be locked or not.

	for (i = 0; i < MAXUNLOCKABLES; i++)
	{
		INT32 unlockSkin = -1;

		if (unlockables[i].type != SECRET_SKIN)
		{
			continue;
		}

		unlockSkin = M_UnlockableSkinNum(&unlockables[i]);

		if (unlockSkin == skinnum)
		{
			unlockID = i;
			break;
		}

		unlockShift++;
	}

	if (unlockID == -1)
	{
		// This skin isn't locked at all, we're good.
		return true;
	}

	if ((netgame || multiplayer) && playernum != -1)
	{
		// We want to check per-player unlockables.
		return (players[playernum].availabilities & (1 << unlockShift));
	}
	else
	{
		// We want to check our global unlockables.
		return (clientGamedata->unlocked[unlockID]);
	}
}

// returns the skin number if the skin name is found (loaded from pwad)
// warning return -1 if not found
INT32 R_SkinAvailable(const char *name)
{
	INT32 i;

	for (i = 0; i < numskins; i++)
	{
		// search in the skin list
		if (!stricmp(skins[i]->name,name))
			return i;
	}
	return -1;
}

INT32 R_GetForcedSkin(INT32 playernum)
{
	if (netgame && cv_forceskin.value >= 0 && R_SkinUsable(playernum, cv_forceskin.value))
		return cv_forceskin.value;

	if (mapheaderinfo[gamemap-1] && mapheaderinfo[gamemap-1]->forcecharacter[0] != '\0')
	{
		INT32 skinnum = R_SkinAvailable(mapheaderinfo[gamemap-1]->forcecharacter);
		if (skinnum != -1 && R_SkinUsable(playernum, skinnum))
			return skinnum;
	}

	return -1;
}

// Auxillary function that actually sets the skin
static void SetSkin(player_t *player, INT32 skinnum)
{
	skin_t *skin = skins[skinnum];
	UINT16 newcolor = 0;

	player->skin = skinnum;

	player->camerascale = skin->camerascale;
	player->shieldscale = skin->shieldscale;

	player->charability = (UINT8)skin->ability;
	player->charability2 = (UINT8)skin->ability2;

	player->charflags = (UINT32)skin->flags;

	player->thokitem = skin->thokitem < 0 ? (UINT32)mobjinfo[MT_PLAYER].painchance : (UINT32)skin->thokitem;
	player->spinitem = skin->spinitem < 0 ? (UINT32)mobjinfo[MT_PLAYER].damage : (UINT32)skin->spinitem;
	player->revitem = skin->revitem < 0 ? (mobjtype_t)mobjinfo[MT_PLAYER].raisestate : (UINT32)skin->revitem;
	player->followitem = skin->followitem;

	if (((player->powers[pw_shield] & SH_NOSTACK) == SH_PINK) && (player->revitem == MT_LHRT || player->spinitem == MT_LHRT || player->thokitem == MT_LHRT)) // Healers can't keep their buff.
		player->powers[pw_shield] &= SH_STACK;

	player->actionspd = skin->actionspd;
	player->mindash = skin->mindash;
	player->maxdash = skin->maxdash;

	player->normalspeed = skin->normalspeed;
	player->runspeed = skin->runspeed;
	player->thrustfactor = skin->thrustfactor;
	player->accelstart = skin->accelstart;
	player->acceleration = skin->acceleration;

	player->jumpfactor = skin->jumpfactor;

	player->height = skin->height;
	player->spinheight = skin->spinheight;

	if (!(cv_debug || devparm) && !(netgame || multiplayer || demoplayback))
	{
		player->skincolor = newcolor = skin->prefcolor;
		if (player->bot && botingame)
		{
			botskin = (UINT8)(skinnum + 1);
			botcolor = skin->prefcolor;
		}
	}

	if (player->followmobj)
	{
		P_RemoveMobj(player->followmobj);
		P_SetTarget(&player->followmobj, NULL);
	}

	if (player->mo)
	{
		fixed_t radius = FixedMul(skin->radius, player->mo->scale);
		if ((player->powers[pw_carry] == CR_NIGHTSMODE) && (!P_IsSkinAnimationValid(skin, SPR2_NFLY, SKINSPRITES_BASE))) // If you don't have a sprite for flying horizontally, use the default NiGHTS skin.
		{
			skin = skins[DEFAULTNIGHTSSKIN];
			player->followitem = skin->followitem;
			if (!(cv_debug || devparm) && !(netgame || multiplayer || demoplayback))
				newcolor = skin->prefcolor; // will be updated in thinker to flashing
		}
		player->mo->skin = skin;
		if (newcolor)
			player->mo->color = newcolor;
		P_SetScale(player->mo, player->mo->scale, false);
		player->mo->radius = radius;

		P_SetMobjState(player->mo, player->mo->state-states); // Prevent visual errors when switching between skins with differing number of frames
	}
}

// Gets the player to the first usuable skin in the game.
// (If your mod locked them all, then you kinda stupid)
INT32 GetPlayerDefaultSkin(INT32 playernum)
{
	INT32 i;

	for (i = 0; i < numskins; i++)
	{
		if (R_SkinUsable(playernum, i))
		{
			return i;
		}
	}

	I_Error("All characters are locked!");
	return 0;
}

// network code calls this when a 'skin change' is received
void SetPlayerSkin(INT32 playernum, const char *skinname)
{
	INT32 i = R_SkinAvailable(skinname);
	player_t *player = &players[playernum];

	if ((i != -1) && R_SkinUsable(playernum, i))
	{
		SetSkin(player, i);
		return;
	}

	if (P_IsLocalPlayer(player))
		CONS_Alert(CONS_WARNING, M_GetText("Skin '%s' not found.\n"), skinname);
	else if (server || IsPlayerAdmin(consoleplayer))
		CONS_Alert(CONS_WARNING, M_GetText("Player %d (%s) skin '%s' not found\n"), playernum, player_names[playernum], skinname);

	SetSkin(player, GetPlayerDefaultSkin(playernum));
}

// Same as SetPlayerSkin, but uses the skin #.
// network code calls this when a 'skin change' is received
void SetPlayerSkinByNum(INT32 playernum, INT32 skinnum)
{
	player_t *player = &players[playernum];

	if (skinnum >= 0 && skinnum < numskins && R_SkinUsable(playernum, skinnum)) // Make sure it exists!
	{
		SetSkin(player, skinnum);
		return;
	}

	if (P_IsLocalPlayer(player))
		CONS_Alert(CONS_WARNING, M_GetText("Requested skin %d not found\n"), skinnum);
	else if (server || IsPlayerAdmin(consoleplayer))
		CONS_Alert(CONS_WARNING, "Player %d (%s) skin %d not found\n", playernum, player_names[playernum], skinnum);

	SetSkin(player, GetPlayerDefaultSkin(playernum));
}

//
// Add skins from a pwad, each skin preceded by 'S_SKIN' marker
//

// Does the same is in w_wad, but check only for
// the first 6 characters (this is so we can have S_SKIN1, S_SKIN2..
// for wad editors that don't like multiple resources of the same name)
//
static UINT16 W_CheckForSkinMarkerInPwad(UINT16 wadid, UINT16 startlump)
{
	UINT16 i;
	const char *S_SKIN = "S_SKIN";
	lumpinfo_t *lump_p;

	// scan forward, start at <startlump>
	if (startlump < wadfiles[wadid]->numlumps)
	{
		lump_p = wadfiles[wadid]->lumpinfo + startlump;
		for (i = startlump; i < wadfiles[wadid]->numlumps; i++, lump_p++)
			if (memcmp(lump_p->name,S_SKIN,6)==0)
				return i;
	}
	return INT16_MAX; // not found
}

#define HUDNAMEWRITE(value) STRBUFCPY(skin->hudname, value)

// turn _ into spaces and . into katana dot
#define SYMBOLCONVERT(name) for (value = name; *value; value++)\
					{\
						if (*value == '_') *value = ' ';\
						else if (*value == '.') *value = '\x1E';\
					}

//
// Patch skins from a pwad, each skin preceded by 'P_SKIN' marker
//

// Does the same is in w_wad, but check only for
// the first 6 characters (this is so we can have P_SKIN1, P_SKIN2..
// for wad editors that don't like multiple resources of the same name)
//
static UINT16 W_CheckForPatchSkinMarkerInPwad(UINT16 wadid, UINT16 startlump)
{
	UINT16 i;
	const char *P_SKIN = "P_SKIN";
	lumpinfo_t *lump_p;

	// scan forward, start at <startlump>
	if (startlump < wadfiles[wadid]->numlumps)
	{
		lump_p = wadfiles[wadid]->lumpinfo + startlump;
		for (i = startlump; i < wadfiles[wadid]->numlumps; i++, lump_p++)
			if (memcmp(lump_p->name,P_SKIN,6)==0)
				return i;
	}
	return INT16_MAX; // not found
}

static void InitSubanimationForSpritedef(animation_t *subanim, spritedef_t *spritedef)
{
	if (subanim->frames)
	{
		Z_Free(subanim->frames);
		subanim->frames = NULL;
		subanim->num_frames = 0;
	}

	if (spritedef && spritedef->numframes)
	{
		subanim->num_frames = spritedef->numframes;
		subanim->frames = Z_Calloc(subanim->num_frames * sizeof(animation_frame_t), PU_STATIC, NULL);

		for (unsigned i = 0; i < spritedef->numframes; i++)
		{
			animation_frame_t *frame = &subanim->frames[i];
			frame->frame_num = i;
			frame->frame_flags = 0;
			frame->duration = 1;
		}
	}
}

static boolean R_AddSkinSpriteDef(animation_list_t *animation, skinspritedef_t *def, const char *spr_name, UINT16 subanimation_id, UINT16 wadnum, UINT16 newlastlump, UINT16 lastlump)
{
	spritedef_t spritedef = { 0 };

	if (def->spritenum[subanimation_id] != SPR_NULL)
		memcpy(&spritedef, &sprites[def->spritenum[subanimation_id]], sizeof(spritedef_t));

	// The subanimation entries are created in order even if they end up having no frames.
	// This ensures the subanimation order matches the player sprite constants.
	animation_t *subanim = P_FindOrCreateSubAnimationAt(animation, player_anim_names[subanimation_id], subanimation_id);

	if (R_AddSingleSpriteDef(spr2names[subanimation_id], &spritedef, wadnum, newlastlump, lastlump, false) && spritedef.numframes > 0)
	{
		if (def->spritenum[subanimation_id] == SPR_NULL)
		{
			spritenum_t sprnum = DEH_FindSpriteByName(spr_name);
			if (sprnum == NUMSPRITES)
			{
				sprnum = DEH_AddSpriteName(spr_name);
				if (sprnum == NUMSPRITES)
				{
					Z_Free(spritedef.spriteframes);
					return false;
				}
			}

			def->spritenum[subanimation_id] = sprnum;
		}

		spritedef_t *skin_sprdef = &sprites[def->spritenum[subanimation_id]];

		memcpy(skin_sprdef, &spritedef, sizeof(spritedef_t));

		InitSubanimationForSpritedef(subanim, skin_sprdef);
	}

	return true;
}

static void R_LoadSkinAnimationsWAD(UINT16 wadnum, UINT16 *lump, UINT16 *lastlump, skin_t *skin, UINT16 start_spr2)
{
	size_t anim_name_size = strlen(skin->name) + 12 + 1; // "skin_" + skin name + "_super"

	char *anim_name = Z_Malloc(anim_name_size, PU_STATIC, NULL);

	size_t spr_name_size = 0;
	char *spr_name = NULL;

	animation_list_t *animation;

	UINT16 newlastlump;
	UINT16 sprite2;

	*lump += 1; // start after S_SKIN
	*lastlump = W_CheckNumForNamePwad("S_END",wadnum,*lump); // stop at S_END

	// old wadding practices die hard -- stop at S_SKIN (or P_SKIN) or S_START if they come before S_END.
	newlastlump = W_CheckForSkinMarkerInPwad(wadnum,*lump);
	if (newlastlump < *lastlump) *lastlump = newlastlump;
	newlastlump = W_CheckForPatchSkinMarkerInPwad(wadnum,*lump);
	if (newlastlump < *lastlump) *lastlump = newlastlump;
	newlastlump = W_CheckNumForNamePwad("S_START",wadnum,*lump);
	if (newlastlump < *lastlump) *lastlump = newlastlump;

	// ...and let's handle super, too
	newlastlump = W_CheckNumForNamePwad("S_SUPER",wadnum,*lump);
	if (newlastlump < *lastlump)
	{
		newlastlump++;

		snprintf(anim_name, anim_name_size, "skin_%s_super", skin->name);
		animation = P_FindOrCreateAnimation(anim_name);
		skin->sprites[SKINSPRITES_SUPER].animation_id = P_GetNamedAnimationID(anim_name);

		// load all sprite sets we are aware of... for super!
		for (sprite2 = start_spr2; sprite2 < free_spr2; sprite2++)
		{
			spr_name_size = strlen(anim_name) + strlen(player_anim_names[sprite2]) + 2;
			spr_name = Z_Realloc(spr_name, spr_name_size, PU_STATIC, NULL);
			snprintf(spr_name, spr_name_size, "%s_%s", anim_name, player_anim_names[sprite2]);
			strupr(spr_name);

			if (!R_AddSkinSpriteDef(animation, &skin->sprites[SKINSPRITES_SUPER], spr_name, sprite2, wadnum, newlastlump, *lastlump))
				I_Error("No more free sprite slots when adding skin animation '%s'", anim_name);
		}

		newlastlump--;
		*lastlump = newlastlump; // okay, make the normal sprite set loading end there
	}
	else
	{
		skin->sprites[SKINSPRITES_SUPER].animation_id = 0;
	}

	snprintf(anim_name, anim_name_size, "skin_%s", skin->name);
	animation = P_FindOrCreateAnimation(anim_name);
	skin->sprites[SKINSPRITES_BASE].animation_id = P_GetNamedAnimationID(anim_name);

	// load all sprite sets we are aware of... for normal stuff.
	for (sprite2 = start_spr2; sprite2 < free_spr2; sprite2++)
	{
		spr_name_size = strlen(anim_name) + strlen(player_anim_names[sprite2]) + 2;
		spr_name = Z_Realloc(spr_name, spr_name_size, PU_STATIC, NULL);
		snprintf(spr_name, spr_name_size, "%s_%s", anim_name, player_anim_names[sprite2]);
		strupr(spr_name);

		if (!R_AddSkinSpriteDef(animation, &skin->sprites[SKINSPRITES_BASE], spr_name, sprite2, wadnum, *lump, *lastlump))
			I_Error("No more free sprite slots when adding skin animation '%s'", anim_name);
	}

	if (!P_IsSkinAnimationValid(skin, SPR2_STND, SKINSPRITES_BASE))
		CONS_Alert(CONS_ERROR, M_GetText("No frames found for subanimation '%s'\n"), player_anim_names[SPR2_STND]);

	Z_Free(anim_name);
	Z_Free(spr_name);
}

static void R_LoadSkinAnimations(UINT16 wadnum, UINT16 *lump, UINT16 *lastlump, skin_t *skin, UINT16 start_spr2)
{
	// TODO: lol
	R_LoadSkinAnimationsWAD(wadnum, lump, lastlump, skin, start_spr2);
}

static void R_LoadSkinSprites(UINT16 wadnum, UINT16 *lump, UINT16 *lastlump, skin_t *skin, UINT16 start_spr2)
{
	if (W_FileHasFolders(wadfiles[wadnum]))
		R_LoadSkinAnimations(wadnum, lump, lastlump, skin, start_spr2);
	else
		R_LoadSkinAnimationsWAD(wadnum, lump, lastlump, skin, start_spr2);
}

// returns whether found appropriate property
static boolean R_ProcessPatchableFields(skin_t *skin, char *stoken, char *value)
{
	// custom translation table
	if (!stricmp(stoken, "startcolor"))
		skin->starttranscolor = atoi(value);

#define FULLPROCESS(field) else if (!stricmp(stoken, #field)) skin->field = get_number(value);
	// character type identification
	FULLPROCESS(flags)
	FULLPROCESS(ability)
	FULLPROCESS(ability2)

	FULLPROCESS(thokitem)
	FULLPROCESS(spinitem)
	FULLPROCESS(revitem)
	FULLPROCESS(followitem)
#undef FULLPROCESS

#define GETFRACBITS(field) else if (!stricmp(stoken, #field)) skin->field = atoi(value)<<FRACBITS;
	GETFRACBITS(normalspeed)
	GETFRACBITS(runspeed)

	GETFRACBITS(mindash)
	GETFRACBITS(maxdash)
	GETFRACBITS(actionspd)

	GETFRACBITS(radius)
	GETFRACBITS(height)
	GETFRACBITS(spinheight)
#undef GETFRACBITS

#define GETINT(field) else if (!stricmp(stoken, #field)) skin->field = atoi(value);
	GETINT(thrustfactor)
	GETINT(accelstart)
	GETINT(acceleration)
	GETINT(contspeed)
	GETINT(contangle)
#undef GETINT

#define GETSKINCOLOR(field) else if (!stricmp(stoken, #field)) \
{ \
	UINT16 color = R_GetColorByName(value); \
	skin->field = (color ? color : SKINCOLOR_GREEN); \
}
	GETSKINCOLOR(prefcolor)
	GETSKINCOLOR(prefoppositecolor)
#undef GETSKINCOLOR
	else if (!stricmp(stoken, "supercolor"))
	{
		UINT16 color = R_GetSuperColorByName(value);
		skin->supercolor = (color ? color : SKINCOLOR_SUPERGOLD1);
	}
#define GETFLOAT(field) else if (!stricmp(stoken, #field)) skin->field = FLOAT_TO_FIXED(atof(value));
	GETFLOAT(jumpfactor)
	GETFLOAT(highresscale)
	GETFLOAT(shieldscale)
	GETFLOAT(camerascale)
#undef GETFLOAT

#define GETFLAG(field) else if (!stricmp(stoken, #field)) { \
	strupr(value); \
	if (atoi(value) || value[0] == 'T' || value[0] == 'Y') \
		skin->flags |= (SF_##field); \
	else \
		skin->flags &= ~(SF_##field); \
}
	// parameters for individual character flags
	// these are uppercase so they can be concatenated with SF_
	// 1, true, yes are all valid values
	GETFLAG(SUPER)
	GETFLAG(NOSUPERSPIN)
	GETFLAG(NOSPINDASHDUST)
	GETFLAG(HIRES)
	GETFLAG(NOSKID)
	GETFLAG(NOSPEEDADJUST)
	GETFLAG(RUNONWATER)
	GETFLAG(NOJUMPSPIN)
	GETFLAG(NOJUMPDAMAGE)
	GETFLAG(STOMPDAMAGE)
	GETFLAG(MARIODAMAGE)
	GETFLAG(MACHINE)
	GETFLAG(DASHMODE)
	GETFLAG(FASTEDGE)
	GETFLAG(MULTIABILITY)
	GETFLAG(NONIGHTSROTATION)
	GETFLAG(NONIGHTSSUPER)
	GETFLAG(NOSUPERSPRITES)
	GETFLAG(NOSUPERJUMPBOOST)
	GETFLAG(CANBUSTWALLS)
	GETFLAG(NOSHIELDABILITY)
#undef GETFLAG

	else if (!stricmp(stoken, "natkcolor"))
		skin->natkcolor = R_GetColorByName(value); // SKINCOLOR_NONE is allowed here

	else // let's check if it's a sound, otherwise error out
	{
		boolean found = false;
		sfxenum_t i;
		size_t stokenadjust;

		// Remove the prefix. (We need to affect an adjusting variable so that we can print error messages if it's not actually a sound.)
		if ((stoken[0] == 'D' || stoken[0] == 'd') && (stoken[1] == 'S' || stoken[1] == 's')) // DS*
			stokenadjust = 2;
		else // sfx_*
			stokenadjust = 4;

		// Remove the prefix. (We can affect this directly since we're not going to use it again.)
		if ((value[0] == 'D' || value[0] == 'd') && (value[1] == 'S' || value[1] == 's')) // DS*
			value += 2;
		else // sfx_*
			value += 4;

		// copy name of sounds that are remapped
		// for this skin
		for (i = 0; i < sfx_skinsoundslot0; i++)
		{
			if (!S_sfx[i].name)
				continue;
			if (S_sfx[i].skinsound != -1
				&& !stricmp(S_sfx[i].name,
					stoken + stokenadjust))
			{
				skin->soundsid[S_sfx[i].skinsound] =
					S_AddSoundFx(value, S_sfx[i].singularity, S_sfx[i].pitch, true);
				found = true;
			}
		}
		return found;
	}
	return true;
}

//
// Find skin sprites, sounds & optional status bar face, & add them
//
void R_AddSkins(UINT16 wadnum, boolean mainfile)
{
	UINT16 lump, lastlump = 0;
	char *buf;
	char *buf2;
	char *stoken;
	char *value;
	size_t size;
	skin_t *skin;
	boolean hudname, realname, supername;

	//
	// search for all skin markers in pwad
	//

	while ((lump = W_CheckForSkinMarkerInPwad(wadnum, lastlump)) != INT16_MAX)
	{
		// advance by default
		lastlump = lump + 1;

		if (numskins >= MAXSKINS)
		{
			CONS_Debug(DBG_RENDER, "ignored skin (%d skins maximum)\n", MAXSKINS);
			continue; // so we know how many skins couldn't be added
		}
		buf = W_CacheLumpNumPwad(wadnum, lump, PU_CACHE);
		size = W_LumpLengthPwad(wadnum, lump);

		// for strtok
		buf2 = malloc(size+1);
		if (!buf2)
			I_Error("R_AddSkins: No more free memory\n");
		M_Memcpy(buf2,buf,size);
		buf2[size] = '\0';

		// set defaults
		skins = Z_Realloc(skins, sizeof(skin_t*) * (numskins + 1), PU_STATIC, NULL);
		skin = skins[numskins] = Z_Calloc(sizeof(skin_t), PU_STATIC, NULL);
		Sk_SetDefaultValue(skin);
		skin->skinnum = numskins;
		skin->wadnum = wadnum;
		hudname = realname = supername = false;
		// parse
		stoken = strtok (buf2, "\r\n= ");
		while (stoken)
		{
			if ((stoken[0] == '/' && stoken[1] == '/')
				|| (stoken[0] == '#'))// skip comments
			{
				stoken = strtok(NULL, "\r\n"); // skip end of line
				goto next_token;              // find the real next token
			}

			value = strtok(NULL, "\r\n= ");

			if (!value)
				I_Error("R_AddSkins: syntax error in S_SKIN lump# %d(%s) in WAD %s\n", lump, W_CheckNameForNumPwad(wadnum,lump), wadfiles[wadnum]->filename);

			// Some of these can't go in R_ProcessPatchableFields because they have side effects for future lines.
			// Others can't go in there because we don't want them to be patchable.
			if (!stricmp(stoken, "name"))
			{
				INT32 skinnum = R_SkinAvailable(value);
				strlwr(value);
				if (skinnum == -1)
					STRBUFCPY(skin->name, value);
				// the skin name must uniquely identify a single skin
				// if the name is already used I make the name 'namex'
				// using the default skin name's number set above
				else
				{
					const size_t stringspace =
						strlen(value) + sizeof (numskins) + 1;
					char *value2 = Z_Malloc(stringspace, PU_STATIC, NULL);
					snprintf(value2, stringspace,
						"%s%d", value, numskins);
					value2[stringspace - 1] = '\0';
					if (R_SkinAvailable(value2) == -1)
						// I'm lazy so if NEW name is already used I leave the 'skin x'
						// default skin name set in Sk_SetDefaultValue
						STRBUFCPY(skin->name, value2);
					Z_Free(value2);
				}

				// copy to hudname, realname, and supername as a default.
				if (!realname)
				{
					STRBUFCPY(skin->realname, skin->name);
					for (value = skin->realname; *value; value++)
					{
						if (*value == '_') *value = ' '; // turn _ into spaces.
						else if (*value == '.') *value = '\x1E'; // turn . into katana dot.
					}
				}
				if (!hudname)
				{
					HUDNAMEWRITE(skin->name);
					strupr(skin->hudname);
					SYMBOLCONVERT(skin->hudname)
				}
				if (!supername)
				{
					char superstring[SKINNAMESIZE+7];
					strcpy(superstring, "Super ");
					strlcat(superstring, skin->name, sizeof(superstring));
					STRBUFCPY(skin->supername, superstring);
				}
			}
			else if (!stricmp(stoken, "supername"))
			{ // Super name (eg. "Super Knuckles")
				supername = true;
				STRBUFCPY(skin->supername, value);
				SYMBOLCONVERT(skin->supername)
			}
			else if (!stricmp(stoken, "realname"))
			{ // Display name (eg. "Knuckles")
				realname = true;
				STRBUFCPY(skin->realname, value);
				SYMBOLCONVERT(skin->realname)
				if (!hudname)
					HUDNAMEWRITE(skin->realname);
				if (!supername) //copy over default to capitalise the name
				{
					char superstring[SKINNAMESIZE+7];
					strcpy(superstring, "Super ");
					strlcat(superstring, skin->realname, sizeof(superstring));
					STRBUFCPY(skin->supername, superstring);
				}
			}
			else if (!stricmp(stoken, "hudname"))
			{ // Life icon name (eg. "K.T.E")
				hudname = true;
				HUDNAMEWRITE(value);
				SYMBOLCONVERT(skin->hudname)
				if (!realname)
					STRBUFCPY(skin->realname, skin->hudname);
			}
			else if (!R_ProcessPatchableFields(skin, stoken, value))
				CONS_Debug(DBG_SETUP, "R_AddSkins: Unknown keyword '%s' in S_SKIN lump #%d (WAD %s)\n", stoken, lump, wadfiles[wadnum]->filename);

next_token:
			stoken = strtok(NULL, "\r\n= ");
		}
		free(buf2);

		// Add sprites
		R_LoadSkinSprites(wadnum, &lump, &lastlump, skin, 0);
		//ST_LoadFaceGraphics(numskins); -- nah let's do this elsewhere

		R_FlushTranslationColormapCache();

		if (mainfile == false)
			CONS_Printf(M_GetText("Added skin '%s'\n"), skin->name);

		numskins++;
	}
	return;
}

//
// Patch skin sprites
//
void R_PatchSkins(UINT16 wadnum, boolean mainfile)
{
	UINT16 lump, lastlump = 0;
	char *buf;
	char *buf2;
	char *stoken;
	char *value;
	size_t size;
	skin_t *skin;
	boolean noskincomplain, realname, hudname, supername;

	//
	// search for all skin patch markers in pwad
	//

	while ((lump = W_CheckForPatchSkinMarkerInPwad(wadnum, lastlump)) != INT16_MAX)
	{
		INT32 skinnum = 0;

		// advance by default
		lastlump = lump + 1;

		buf = W_CacheLumpNumPwad(wadnum, lump, PU_CACHE);
		size = W_LumpLengthPwad(wadnum, lump);

		// for strtok
		buf2 = malloc(size+1);
		if (!buf2)
			I_Error("R_PatchSkins: No more free memory\n");
		M_Memcpy(buf2,buf,size);
		buf2[size] = '\0';

		skin = NULL;
		noskincomplain = realname = hudname = supername = false;

		/*
		Parse. Has more phases than the parser in R_AddSkins because it needs to have the patching name first (no default skin name is acceptible for patching, unlike skin creation)
		*/

		stoken = strtok(buf2, "\r\n= ");
		while (stoken)
		{
			if ((stoken[0] == '/' && stoken[1] == '/')
				|| (stoken[0] == '#'))// skip comments
			{
				stoken = strtok(NULL, "\r\n"); // skip end of line
				goto next_token;              // find the real next token
			}

			value = strtok(NULL, "\r\n= ");

			if (!value)
				I_Error("R_PatchSkins: syntax error in P_SKIN lump# %d(%s) in WAD %s\n", lump, W_CheckNameForNumPwad(wadnum,lump), wadfiles[wadnum]->filename);

			if (!skin) // Get the name!
			{
				if (!stricmp(stoken, "name"))
				{
					strlwr(value);
					skinnum = R_SkinAvailable(value);
					if (skinnum != -1)
						skin = skins[skinnum];
					else
					{
						CONS_Debug(DBG_SETUP, "R_PatchSkins: unknown skin name in P_SKIN lump# %d(%s) in WAD %s\n", lump, W_CheckNameForNumPwad(wadnum,lump), wadfiles[wadnum]->filename);
						noskincomplain = true;
					}
				}
			}
			else // Get the properties!
			{
				// Some of these can't go in R_ProcessPatchableFields because they have side effects for future lines.
				if (!stricmp(stoken, "supername"))
				{ // Super name (eg. "Super Knuckles")
					supername = true;
					STRBUFCPY(skin->supername, value);
					SYMBOLCONVERT(skin->supername)
				}
				else if (!stricmp(stoken, "realname"))
				{ // Display name (eg. "Knuckles")
					realname = true;
					STRBUFCPY(skin->realname, value);
					SYMBOLCONVERT(skin->realname)
					if (!hudname)
						HUDNAMEWRITE(skin->realname);
					if (!supername) //copy over default to capitalise the name
					{
						char superstring[SKINNAMESIZE+7];
						strcpy(superstring, "Super ");
						strlcat(superstring, skin->realname, sizeof(superstring));
						STRBUFCPY(skin->supername, superstring);
					}
				}
				else if (!stricmp(stoken, "hudname"))
				{ // Life icon name (eg. "K.T.E")
					hudname = true;
					HUDNAMEWRITE(value);
					SYMBOLCONVERT(skin->hudname)
					if (!realname)
						STRBUFCPY(skin->realname, skin->hudname);
				}
				else if (!R_ProcessPatchableFields(skin, stoken, value))
					CONS_Debug(DBG_SETUP, "R_PatchSkins: Unknown keyword '%s' in P_SKIN lump #%d (WAD %s)\n", stoken, lump, wadfiles[wadnum]->filename);
			}

			if (!skin)
				break;

next_token:
			stoken = strtok(NULL, "\r\n= ");
		}
		free(buf2);

		if (!skin) // Didn't include a name parameter? What a waste.
		{
			if (!noskincomplain)
				CONS_Debug(DBG_SETUP, "R_PatchSkins: no skin name given in P_SKIN lump #%d (WAD %s)\n", lump, wadfiles[wadnum]->filename);
			continue;
		}

		// Patch sprites
		R_LoadSkinSprites(wadnum, &lump, &lastlump, skin, 0);
		//ST_LoadFaceGraphics(skinnum); -- nah let's do this elsewhere

		R_FlushTranslationColormapCache();

		if (mainfile == false)
			CONS_Printf(M_GetText("Patched skin '%s'\n"), skin->name);
	}
	return;
}

#undef HUDNAMEWRITE
#undef SYMBOLCONVERT

static UINT16 W_CheckForEitherSkinMarkerInPwad(UINT16 wadid, UINT16 startlump)
{
	UINT16 i;
	const char *S_SKIN = "S_SKIN";
	const char *P_SKIN = "P_SKIN";
	lumpinfo_t *lump_p;

	// scan forward, start at <startlump>
	if (startlump < wadfiles[wadid]->numlumps)
	{
		lump_p = wadfiles[wadid]->lumpinfo + startlump;
		for (i = startlump; i < wadfiles[wadid]->numlumps; i++, lump_p++)
			if (memcmp(lump_p->name,S_SKIN,6)==0 || memcmp(lump_p->name,P_SKIN,6)==0)
				return i;
	}
	return INT16_MAX; // not found
}

static void R_RefreshSprite2ForWad(UINT16 wadnum, UINT8 start_spr2)
{
	UINT16 lump, lastlump = 0;
	char *buf;
	char *buf2;
	char *stoken;
	char *value;
	size_t size;
	skin_t *skin;
	boolean noskincomplain;

	//
	// search for all skin patch markers in pwad
	//

	while ((lump = W_CheckForEitherSkinMarkerInPwad(wadnum, lastlump)) != INT16_MAX)
	{
		INT32 skinnum = 0;

		// advance by default
		lastlump = lump + 1;

		buf = W_CacheLumpNumPwad(wadnum, lump, PU_CACHE);
		size = W_LumpLengthPwad(wadnum, lump);

		// for strtok
		buf2 = malloc(size+1);
		if (!buf2)
			I_Error("R_RefreshSprite2ForWad: No more free memory\n");
		M_Memcpy(buf2,buf,size);
		buf2[size] = '\0';

		skin = NULL;
		noskincomplain = false;

		/*
		Parse. Has more phases than the parser in R_AddSkins because it needs to have the patching name first (no default skin name is acceptible for patching, unlike skin creation)
		*/

		stoken = strtok(buf2, "\r\n= ");
		while (stoken)
		{
			if ((stoken[0] == '/' && stoken[1] == '/')
				|| (stoken[0] == '#'))// skip comments
			{
				stoken = strtok(NULL, "\r\n"); // skip end of line
				goto next_token;              // find the real next token
			}

			value = strtok(NULL, "\r\n= ");

			if (!value)
				I_Error("R_RefreshSprite2ForWad: syntax error in P_SKIN lump# %d(%s) in WAD %s\n", lump, W_CheckNameForNumPwad(wadnum,lump), wadfiles[wadnum]->filename);

			if (!stricmp(stoken, "name"))
			{
				strlwr(value);
				skinnum = R_SkinAvailable(value);
				if (skinnum != -1)
					skin = skins[skinnum];
				else
				{
					CONS_Debug(DBG_SETUP, "R_RefreshSprite2ForWad: unknown skin name in P_SKIN lump# %d(%s) in WAD %s\n", lump, W_CheckNameForNumPwad(wadnum,lump), wadfiles[wadnum]->filename);
					noskincomplain = true;
				}
			}

			if (!skin)
				break;

next_token:
			stoken = strtok(NULL, "\r\n= ");
		}
		free(buf2);

		if (!skin) // Didn't include a name parameter? What a waste.
		{
			if (!noskincomplain)
				CONS_Debug(DBG_SETUP, "R_RefreshSprite2ForWad: no skin name given in P_SKIN lump #%d (WAD %s)\n", lump, wadfiles[wadnum]->filename);
			continue;
		}

		// Update sprites, in the range of (start_spr2 - free_spr2-1)
		R_LoadSkinSprites(wadnum, &lump, &lastlump, skin, start_spr2);
		//R_FlushTranslationColormapCache(); // I don't think this is needed for what we're doing?
	}
}

static playersprite_t old_spr2 = SPR2_FIRSTFREESLOT;
void R_RefreshSprite2(void)
{
	// Sprite2s being defined by custom wads can create situations where
	// a custom character might want to add support, but due to load order,
	// might not be defined in time.

	// The trick where you load characters then level packs to keep savedata
	// in particular will practically garantuee a level pack can NEVER add custom animations,
	// because custom character's Sprite2s will not be added.

	// So, go through every file, and reload the sprite2s that were added.

	INT32 i;

	if (old_spr2 > free_spr2)
	{
#ifdef PARANOIA
		I_Error("R_RefreshSprite2: old_spr2 is too high?! (old_spr2: %d, free_spr2: %d)\n", old_spr2, free_spr2);
#else
		// Just silently fix
		old_spr2 = free_spr2;
#endif
	}

	if (old_spr2 == free_spr2)
	{
		// No sprite2s were added since the last time we did freeslots.
		return;
	}

	for (i = 0; i < numwadfiles; i++)
	{
		R_RefreshSprite2ForWad(i, old_spr2);
	}

	// Update previous value.
	old_spr2 = free_spr2;
}
