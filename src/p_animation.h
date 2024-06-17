// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 2024 by "Lactozilla".
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  p_animation.h
/// \brief Animation system

#ifndef __P_ANIMATION__
#define __P_ANIMATION__

#include "doomtype.h"

#include "p_mobj.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum animation_direction_e
{
	ANIM_DIR_FORWARDS,
	ANIM_DIR_REVERSE,
	ANIM_DIR_OSCILLATE
} animation_direction_t;

typedef struct animation_frame_s
{
	UINT8 frame_num;
	UINT32 frame_flags;
	tic_t duration;
} animation_frame_t;

typedef struct animation_s
{
	char *name;
	fixed_t speed;
	unsigned loop_index;
	animation_direction_t direction;
	animation_frame_t *frames;
	unsigned num_frames;
} animation_t;

typedef struct animation_list_s
{
	char *name;
	struct animation_s **animations;
	size_t count;
} animation_list_t;

void P_InitAnimations(void);
void P_LoadAnimations(UINT16 wadnum);

boolean P_SetMobjAnimation(mobj_t *mobj, UINT16 animation_id, UINT16 subanimation_id, UINT16 start_frame);
boolean P_SetNamedMobjAnimation(mobj_t *mobj, const char *animation_name, const char *entry_name, UINT16 start_frame);
void P_DoAnimationPlayback(animator_t *animator, mobj_t *mobj, tic_t timedelta);

animation_list_t *P_FindAnimation(const char *animation_name);
animation_list_t *P_FindOrCreateAnimation(const char *animation_name);
animation_list_t *P_DuplicateAnimation(const char *animation_name, animation_list_t *base, boolean copy_frames);
animation_list_t *P_MergeAnimations(const char *name, animation_list_t *anim_a, animation_list_t *anim_b);
animation_t *P_FindOrCreateSubAnimation(animation_list_t *animation, const char *subanimation_name);
animation_t *P_GetSubAnimationByID(animation_list_t *animation, UINT16 subanimation_id);

UINT16 P_GetNamedAnimationID(const char *animation_name);
UINT16 P_GetNamedSubanimationID(UINT16 animation_id, const char *entry_name);

const char *P_GetAnimationNameByID(UINT16 animation_id);
const char *P_GetSubanimationNameByID(UINT16 animation_id, UINT16 subanimation_id);

UINT16 P_GetSubanimationFrameCount(UINT16 animation_id, UINT16 subanimation_id);
fixed_t P_GetSubanimationSpeed(UINT16 animation_id, UINT16 subanimation_id);

void P_SetSubanimationSpeed(UINT16 animation_id, UINT16 subanimation_id, fixed_t speed);

boolean P_SetupAnimator(animator_t *animator, UINT16 animation_id, UINT16 subanimation_id, UINT16 start_frame);
UINT32 P_GetAnimatorFrame(animator_t *animator);
UINT32 P_GetAnimatorNextFrame(animator_t *animator);
void P_UpdateAnimatorCurNextFrames(animator_t *animator); // for p_saveg.c

#ifdef __cplusplus
} // extern "C"
#endif

#endif // __P_ANIMATION__
