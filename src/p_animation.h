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

#ifdef __cplusplus
extern "C" {
#endif

typedef enum animation_direction_e
{
	ANIM_DIR_FORWARD,
	ANIM_DIR_BACKWARD,
	ANIM_DIR_PINGPONG
} animation_direction_t;

typedef struct animation_frame_s
{
	UINT8 num;
	tic_t duration;
	boolean mirrored;
} animation_frame_t;

typedef struct animation_s
{
	const char *name;
	tic_t speed;
	unsigned loop_index;
	animation_direction_t direction;
	animation_frame_t *frames;
	unsigned num_frames;
} animation_t;

typedef struct animation_list_s
{
	const char *name;
	struct animation_s **animations;
	size_t count;
} animation_list_t;

void P_InitAnimations(void);
void P_LoadAnimations(UINT16 wadnum);

animation_t *P_GetAnimationEntry(UINT16 list_id, const char *entry_name);
struct animation_s *P_GetAnimationEntryByID(UINT16 list_id, UINT16 entry_id);
animation_t *P_GetNamedAnimationEntry(const char *list_name, const char *entry_name);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // __P_ANIMATION__
