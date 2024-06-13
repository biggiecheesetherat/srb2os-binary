// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 1993-1996 by id Software, Inc.
// Copyright (C) 1998-2000 by DooM Legacy Team.
// Copyright (C) 1999-2023 by Sonic Team Junior.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  r_skins.h
/// \brief Skins stuff

#ifndef __R_SKINS__
#define __R_SKINS__

#include "info.h"
#include "sounds.h"
#include "d_player.h" // skinflags
#include "r_patch.h"
#include "r_picformats.h" // spriteinfo_t
#include "r_defs.h" // spritedef_t

/// Defaults
#define SKINNAMESIZE 16
// should be all lowercase!! S_SKIN processing does a strlwr
#define DEFAULTSKIN "sonic"
#define DEFAULTSKIN2 "tails" // secondary player
#define DEFAULTNIGHTSSKIN 0

typedef struct skinspritedef_s
{
	UINT16 animation_id;
	spritenum_t spritenum[NUMPLAYERSPRITES];
} skinspritedef_t;

/// The skin_t struct
typedef struct
{
	char name[SKINNAMESIZE+1]; // name of the skin
	UINT8 skinnum;
	UINT16 wadnum;
	skinflags_t flags;

	char realname[SKINNAMESIZE+1]; // Display name for level completion
	char hudname[SKINNAMESIZE+1]; // HUD name to display (officially exactly 5 characters long)
	char supername[SKINNAMESIZE+7]; // Super name to display when collecting all emeralds

	UINT8 ability; // ability definition
	UINT8 ability2; // secondary ability definition
	INT32 thokitem;
	INT32 spinitem;
	INT32 revitem;
	INT32 followitem;
	fixed_t actionspd;
	fixed_t mindash;
	fixed_t maxdash;

	fixed_t normalspeed; // Normal ground
	fixed_t runspeed; // Speed that you break into your run animation

	UINT8 thrustfactor; // Thrust = thrustfactor * acceleration
	UINT8 accelstart; // Acceleration if speed = 0
	UINT8 acceleration; // Acceleration

	fixed_t jumpfactor; // multiple of standard jump height

	fixed_t radius; // Bounding box changes.
	fixed_t height;
	fixed_t spinheight;

	fixed_t shieldscale; // no change to bounding box, but helps set the shield's sprite size
	fixed_t camerascale;

	// Definable color translation table
	UINT8 starttranscolor;
	UINT16 prefcolor;
	UINT16 supercolor;
	UINT16 prefoppositecolor; // if 0 use tables instead
	UINT16 natkcolor; //Color for Nights Attack Menu

	fixed_t highresscale; // scale of highres, default is 0.5
	UINT8 contspeed; // continue screen animation speed
	UINT8 contangle; // initial angle on continue screen

	// specific sounds per skin
	sfxenum_t soundsid[NUMSKINSOUNDS]; // sound # in S_sfx table

	skinspritedef_t sprites[NUMSKINSPRITESETS];
} skin_t;

/// Externs
extern INT32 numskins;
extern skin_t **skins;

/// Function prototypes
void R_InitSkins(void);

INT32 GetPlayerDefaultSkin(INT32 playernum);
void SetPlayerSkin(INT32 playernum,const char *skinname);
void SetPlayerSkinByNum(INT32 playernum,INT32 skinnum); // Tails 03-16-2002
boolean R_SkinUsable(INT32 playernum, INT32 skinnum);
UINT32 R_GetSkinAvailabilities(void);
INT32 R_SkinAvailable(const char *name);
INT32 R_GetForcedSkin(INT32 playernum);
void R_AddSkins(UINT16 wadnum, boolean mainfile);
void R_PatchSkins(UINT16 wadnum, boolean mainfile);

UINT16 P_GetSkinAnimation(skin_t *skin, UINT8 spriteset);
UINT16 P_GetSkinSubanimation(skin_t *skin, UINT16 subanim, UINT8 spriteset, player_t *player, UINT8 *found_spriteset);
UINT16 P_GetPlayerSubanimReplacement(skin_t *skin, UINT16 subanim, player_t *player);
boolean P_ShouldUseSuperSprites(mobj_t *mobj, boolean use_super);
spritenum_t P_GetSkinSpriteID(skin_t *skin, UINT16 subanim, UINT8 spriteset);
spritedef_t *P_GetSkinSpritedef(skin_t *skin, UINT16 subanim, UINT8 spriteset);
spriteinfo_t *P_GetSkinSpriteInfo(skin_t *skin, UINT16 subanim, UINT8 spriteset);
spritedef_t *P_GetSkinAnimSpritedef(skin_t *skin, UINT16 anim, UINT16 subanim);
spriteinfo_t *P_GetSkinAnimSpriteInfo(skin_t *skin, UINT16 anim, UINT16 subanim);
boolean P_IsSkinAnimationValid(skin_t *skin, UINT16 subanim, UINT8 spriteset);
const char *P_GetPlayerAnimName(UINT16 playeranim);

void R_RefreshSprite2(void);

#endif //__R_SKINS__
