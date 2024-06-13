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
void P_DoAnimationPlayback(struct animator_s *animator, mobj_t *mobj);

struct animation_list_s *P_FindOrCreateAnimation(const char *animation_name);
struct animation_s *P_FindOrCreateSubAnimation(struct animation_list_s *animation, const char *subanimation_name);
struct animation_s *P_FindOrCreateSubAnimationAt(struct animation_list_s *animation, const char *subanimation_name, unsigned index);

UINT16 P_GetNamedAnimationID(const char *animation_name);
UINT16 P_GetNamedSubanimationID(UINT16 animation_id, const char *entry_name);

const char *P_GetAnimationNameByID(UINT16 animation_id);
const char *P_GetSubanimationNameByID(UINT16 animation_id, UINT16 subanimation_id);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // __P_ANIMATION__
