// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 2024 by "Lactozilla".
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  p_animation.cpp
/// \brief Animation system

#include "p_animation.h"

#include "nlohmann/json.hpp"

#include "doomdef.h"
#include "r_skins.h"
#include "p_pspr.h"
#include "w_wad.h"
#include "z_zone.h"

using nlohmann::json;

static std::vector<animation_list_s*> animation_defs;

static animation_list_s *find_animation_by_name(const char *name)
{
	for (size_t i = 0; i < animation_defs.size(); i++)
	{
		if (!strcmp(animation_defs[i]->name, name))
			return animation_defs[i];
	}

	return nullptr;
}

static UINT16 get_animation_id(const char *name)
{
	for (size_t i = 0; i < animation_defs.size(); i++)
	{
		if (!strcmp(animation_defs[i]->name, name))
			return i + 1;
	}

	return 0;
}

static animation_list_s *get_animation_by_id(UINT16 id)
{
	if (id == 0 || id > animation_defs.size())
	{
		return nullptr;
	}

	return animation_defs[id - 1];
}

// Animation playback
static void P_UpdateMobjAnimationFrame(mobj_t *mobj, animator_s *animator)
{
	mobj->frame = animator->current_frame;
}

static UINT16 P_GetNextSubanimationFrame(UINT16 frame, animation_s *entry, UINT8 direction)
{
	if (direction == ANIM_DIR_REVERSE)
	{
		if (frame == 0)
		{
			if (entry->loop_index != UINT16_MAX && entry->loop_index < entry->num_frames)
				frame = entry->loop_index;
			else
				frame = entry->num_frames - 1;
		}
		else
			frame--;
	}
	else
	{
		frame++;

		if (frame >= entry->num_frames)
		{
			if (entry->loop_index == UINT16_MAX)
				frame = 0;
			else if (entry->loop_index < entry->num_frames)
				frame = entry->loop_index;
			else
				frame = entry->num_frames - 1;
		}
	}

	return frame;
}

static animation_frame_s *P_GetNextSubanimationFramePtr(UINT16 frame, animation_s *entry, UINT8 direction)
{
	return &entry->frames[P_GetNextSubanimationFrame(frame, entry, direction)];
}

static boolean is_valid_subanimation_id(animation_list_s *animation, UINT16 subanimation_id)
{
	if (animation == nullptr || animation->count == 0 || subanimation_id >= animation->count)
	{
		return false;
	}

	return animation->animations[subanimation_id] != nullptr;
}

static void P_UpdateAnimatorCurNextFrames(animator_s *animator, animation_s *entry)
{
	animation_frame_s *frame = &entry->frames[animator->frame];

	fixed_t anim_speed = FixedMul(entry->speed, animator->speed_mul);

	animator->current_frame = frame->frame_num | (frame->frame_flags & ~FF_FRAMEMASK);

	if (animator->direction == ANIM_DIR_REVERSE)
		anim_speed = -anim_speed;

	if (anim_speed < 0)
	{
		frame = P_GetNextSubanimationFramePtr(animator->frame, entry, ANIM_DIR_REVERSE);
	}
	else
	{
		frame = P_GetNextSubanimationFramePtr(animator->frame, entry, ANIM_DIR_FORWARDS);
	}

	animator->next_frame = frame->frame_num | (frame->frame_flags & ~FF_FRAMEMASK);
}

// for p_saveg.c
void P_UpdateAnimatorCurNextFrames(animator_s *animator)
{
	animation_list_s *animation = get_animation_by_id(animator->animation);

	animator->current_frame = animator->next_frame = 0;

	if (is_valid_subanimation_id(animation, animator->subanimation))
	{
		animation_s *entry = animation->animations[animator->subanimation];

		if (entry->num_frames > 0)
		{
			P_UpdateAnimatorCurNextFrames(animator, entry);
		}
	}
}

boolean P_SetupAnimator(animator_s *animator, UINT16 animation_id, UINT16 subanimation_id, UINT16 start_frame)
{
	animation_list_s *animation = get_animation_by_id(animation_id);
	animation_s *entry;

	if (!is_valid_subanimation_id(animation, subanimation_id))
	{
		return false;
	}

	animator->animation = animation_id;
	animator->subanimation = subanimation_id;
	animator->timer = 0;

	entry = animation->animations[subanimation_id];

	// If the animation was going in reverse, invert the speed to that it's going forwards instead
	if (animator->direction == ANIM_DIR_OSCILLATE && animator->speed_mul < 0)
		animator->speed_mul = -animator->speed_mul;

	animator->direction = entry->direction;

	// Animation is empty
	if (entry->num_frames == 0)
	{
		animator->frame = 0;
		animator->frame_duration = 0;
		animator->current_frame = animator->next_frame = 0;
		return false;
	}

	// Set starting frame
	animator->frame = start_frame;

	if (animator->frame == UINT16_MAX) // default frame
	{
		if (animator->direction == ANIM_DIR_REVERSE)
			animator->frame = entry->num_frames - 1;
		else
			animator->frame = 0;
	}
	else
		animator->frame %= entry->num_frames;

	animator->frame_duration = std::max((int)entry->frames[animator->frame].duration, 1) * FRACUNIT;

	// Set current and next frame
	P_UpdateAnimatorCurNextFrames(animator, entry);

	return true;
}

static boolean P_SetupMobjAnimation(mobj_t *mobj, UINT16 animation_id, UINT16 subanimation_id, UINT16 start_frame)
{
	if (P_SetupAnimator(&mobj->animator, animation_id, subanimation_id, start_frame))
	{
		P_UpdateMobjAnimationFrame(mobj, &mobj->animator);
		return true;
	}

	return false;
}

const char *P_GetAnimatorSubAnimationName(animator_s *animator)
{
	return P_GetSubanimationNameByID(animator->animation, animator->subanimation);
}

boolean P_IsAnimatorPlayingNamedSubAnimation(animator_s *animator, const char *name)
{
	return !strcmp(P_GetAnimatorSubAnimationName(animator), name);
}

UINT32 P_GetAnimatorFrame(animator_s *animator)
{
	return animator->current_frame;
}

UINT32 P_GetAnimatorNextFrame(animator_s *animator)
{
	return animator->next_frame;
}

boolean P_SetMobjAnimation(mobj_t *mobj, UINT16 animation_id, UINT16 subanimation_id, UINT16 start_frame)
{
	animation_list_s *animation;
	skin_t *skin;

	if (animation_id == 0)
	{
		return false;
	}

	animation = get_animation_by_id(animation_id);
	if (animation == nullptr)
	{
		CONS_Alert(CONS_ERROR, "P_SetMobjAnimation: Invalid animation ID %d\n", animation_id);
		return false;
	}

	if (!is_valid_subanimation_id(animation, subanimation_id))
	{
		return false;
	}

	if (subanimation_id >= animation->count)
	{
		CONS_Alert(CONS_ERROR, "P_SetMobjAnimation: Invalid subanimation ID %d/%s for animation '%s'\n", subanimation_id, sizeu1(animation->count), P_GetAnimationNameByID(animation_id));

		subanimation_id = 0;
	}

	// Set appropriate sprite if this animation belongs to a skin
	skin = (skin_t *)mobj->skin;

	if (!P_IsSkinSprite(skin, mobj->sprite))
	{
		skin = P_IsAnimationForSkin(nullptr, animation_id);
		if (skin != nullptr)
		{
			spritenum_t sprite;
			UINT8 spriteset = P_GetMobjSkinSpriteset(mobj, mobj->state);

			subanimation_id = P_GetSkinSubanimation(skin, subanimation_id, spriteset, mobj->player, &spriteset);
			animation_id = P_GetSkinAnimation(skin, spriteset);

			sprite = P_GetSkinSpriteID(skin, subanimation_id, spriteset);
			if (sprite != SPR_NULL)
				mobj->sprite = sprite;
		}
	}

	if (mobj->animator.animation == animation_id && mobj->animator.subanimation == subanimation_id)
		return true;

	return P_SetupMobjAnimation(mobj, animation_id, subanimation_id, start_frame);
}

boolean P_SetNamedMobjAnimation(mobj_t *mobj, const char *animation_name, const char *subanimation_name, UINT16 start_frame)
{
	UINT16 animation_id = get_animation_id(animation_name);
	if (animation_id == 0)
	{
		CONS_Alert(CONS_ERROR, "P_SetNamedMobjAnimation: Unknown animation '%s'\n", animation_name);
		return false;
	}

	animation_list_s *animation = animation_defs[animation_id];

	UINT16 subanimation_id = P_GetSubAnimationByName(animation, subanimation_name)->id;
	if (subanimation_id == UINT16_MAX)
	{
		subanimation_id = 0;

		if (!is_valid_subanimation_id(animation, subanimation_id))
		{
			return false;
		}

		CONS_Alert(CONS_ERROR, "P_SetNamedMobjAnimation: No subanimation named '%s' in animation '%s'\n", subanimation_name, animation_name);
	}
	else if (!is_valid_subanimation_id(animation, subanimation_id))
	{
		return false;
	}

	if (mobj->animator.animation == animation_id && mobj->animator.subanimation == subanimation_id)
		return true;

	return P_SetMobjAnimation(mobj, animation_id, subanimation_id, start_frame);
}

void P_DoAnimationPlayback(animator_s *animator, mobj_t *mobj, tic_t timedelta)
{
	if (animator->animation == 0 || animator->animation > animation_defs.size())
	{
		return;
	}

	animation_list_s *animation = animation_defs[animator->animation - 1];
	if (!is_valid_subanimation_id(animation, animator->subanimation))
	{
		return;
	}

	animation_s *entry = animation->animations[animator->subanimation];
	if (animator->frame >= entry->num_frames)
	{
		return;
	}

	fixed_t anim_speed = FixedMul(FixedMul(entry->speed, animator->speed_mul), timedelta);
	if (anim_speed == 0)
		return;

	unsigned i = 0;

	// Oscillating animation
	if (animator->direction == ANIM_DIR_OSCILLATE)
	{
		animator->timer += anim_speed;

		while (animator->timer < 0 || animator->timer > animator->frame_duration)
		{
			UINT16 frame = animator->frame;

			if (anim_speed > 0)
			{
				animator->timer -= animator->frame_duration;

				frame++;

				if (frame >= entry->num_frames)
				{
					if (entry->loop_index != UINT16_MAX && entry->loop_index < entry->num_frames)
						frame = entry->loop_index;
					else
						frame = entry->num_frames - 1;

					animator->speed_mul = -animator->speed_mul;
					anim_speed = -anim_speed;
				}
			}
			else if (anim_speed < 0)
			{
				animator->timer += animator->frame_duration;

				if (frame == 0 || (entry->loop_index != UINT16_MAX && (signed)frame - 1 < (signed)entry->loop_index))
				{
					if (entry->loop_index != UINT16_MAX && entry->loop_index < entry->num_frames)
						frame = entry->loop_index;
					else
						frame = 0;

					animator->speed_mul = -animator->speed_mul;
					anim_speed = -anim_speed;
				}
				else
					frame--;
			}

			animator->frame = frame;

			P_UpdateAnimatorCurNextFrames(animator, entry);

			if (mobj != nullptr)
			{
				P_UpdateMobjAnimationFrame(mobj, animator);
			}

			animator->frame_duration = entry->frames[animator->frame].duration * FRACUNIT;

			if (++i > TICRATE)
				break;
		}

		return;
	}

	if (animator->direction == ANIM_DIR_REVERSE)
		anim_speed = -anim_speed;

	animator->timer += anim_speed;

	// Animate forwards
	if (anim_speed > 0)
	{
		while (animator->timer > animator->frame_duration)
		{
			animator->timer -= animator->frame_duration;
			animator->frame = P_GetNextSubanimationFrame(animator->frame, entry, ANIM_DIR_FORWARDS);

			P_UpdateAnimatorCurNextFrames(animator, entry);

			if (mobj != nullptr)
			{
				P_UpdateMobjAnimationFrame(mobj, animator);
			}

			animator->frame_duration = entry->frames[animator->frame].duration * FRACUNIT;

			if (++i > TICRATE)
				break;
		}
	}
	// Animate backwards
	else if (anim_speed < 0)
	{
		while (animator->timer < 0)
		{
			animator->timer += animator->frame_duration;
			animator->frame = P_GetNextSubanimationFrame(animator->frame, entry, ANIM_DIR_REVERSE);

			P_UpdateAnimatorCurNextFrames(animator, entry);

			if (mobj != nullptr)
			{
				P_UpdateMobjAnimationFrame(mobj, animator);
			}

			animator->frame_duration = entry->frames[animator->frame].duration * FRACUNIT;

			if (++i > TICRATE)
				break;
		}
	}
}

static animation_s *find_subanimation(animation_list_s *list, const char *name)
{
	if (list)
	{
		for (size_t i = 0; i < list->count; i++)
		{
			if (!strcmp(list->animations[i]->name, name))
				return list->animations[i];
		}
	}

	return nullptr;
}

UINT16 P_GetNamedAnimationID(const char *animation_name)
{
	return get_animation_id(animation_name);
}

UINT16 P_GetNamedSubanimationID(UINT16 animation_id, const char *subanimation_name)
{
	animation_list_s *animation = get_animation_by_id(animation_id);
	if (animation != nullptr)
	{
		for (size_t i = 0; i < animation->count; i++)
		{
			if (!strcmp(animation->animations[i]->name, subanimation_name))
				return i;
		}
	}

	return UINT16_MAX;
}

const char *P_GetAnimationNameByID(UINT16 animation_id)
{
	animation_list_s *animation = get_animation_by_id(animation_id);
	if (animation == nullptr)
	{
		return nullptr;
	}

	return animation->name;
}

const char *P_GetSubanimationNameByID(UINT16 animation_id, UINT16 subanimation_id)
{
	animation_list_s *animation = get_animation_by_id(animation_id);
	if (animation == nullptr)
	{
		return nullptr;
	}

	if (is_valid_subanimation_id(animation, subanimation_id))
	{
		return animation->animations[subanimation_id]->name;
	}

	return nullptr;
}

UINT16 P_GetSubanimationFrameCount(UINT16 animation_id, UINT16 subanimation_id)
{
	animation_list_s *animation = get_animation_by_id(animation_id);
	if (animation == nullptr)
	{
		return 0;
	}

	if (is_valid_subanimation_id(animation, subanimation_id))
	{
		return animation->animations[subanimation_id]->num_frames;
	}

	return 0;
}

fixed_t P_GetSubanimationSpeed(UINT16 animation_id, UINT16 subanimation_id)
{
	animation_list_s *animation = get_animation_by_id(animation_id);
	if (!is_valid_subanimation_id(animation, subanimation_id))
	{
		return 0;
	}

	animation_s *entry = animation->animations[subanimation_id];

	return entry->speed;
}

static animation_list_s *create_animation(const char *name)
{
	animation_list_s *animation = static_cast<animation_list_s *>(
		Z_Calloc(sizeof(animation_list_s), PU_STATIC, nullptr)
	);

	animation->name = Z_StrDup(name);

	return animation;
}

static animation_s *create_subanimation(UINT16 id, const char *name)
{
	animation_s *subanimation = static_cast<animation_s *>(
		Z_Calloc(sizeof(animation_s), PU_STATIC, nullptr)
	);
	subanimation->name = Z_StrDup(name);
	subanimation->speed = FRACUNIT;
	subanimation->loop_index = UINT16_MAX;
	subanimation->direction = ANIM_DIR_FORWARDS;
	return subanimation;
}

static animation_s *create_and_add_subanimation(animation_list_s *list, const char *name)
{
	animation_s *subanimation = create_subanimation(list->count, name);

	list->count++;
	list->animations = static_cast<animation_s **>(
		Z_Realloc(list->animations, list->count * sizeof(animation_s **), PU_STATIC, nullptr)
	);

	list->animations[list->count - 1] = subanimation;

	return subanimation;
}

static animation_s *find_or_create_subanimation(animation_list_s *list, const char *name)
{
	for (size_t i = 0; i < list->count; i++)
	{
		if (!strcmp(list->animations[i]->name, name))
			return list->animations[i];
	}

	return create_and_add_subanimation(list, name);
}

animation_list_s *P_FindAnimation(const char *animation_name)
{
	return find_animation_by_name(animation_name);
}

// NOTE: This does not add it to the list. You must use P_AddAnimation as well.
animation_list_s *P_CreateAnimation(const char *animation_name)
{
	return create_animation(animation_name);
}

static void P_DeleteAnimation(animation_list_s *animation)
{
	for (size_t i = 0; i < animation->count; i++)
	{
		Z_Free(animation->animations[i]->fallback);
		Z_Free(animation->animations[i]->frames);
		Z_Free(animation->animations[i]);
	}

	Z_Free(animation->animations);
	Z_Free(animation);
}

static boolean P_ReplaceAnimation(animation_list_s *animation)
{
	UINT16 existing = get_animation_id(animation->name);
	if (existing != 0)
	{
		existing--;
		P_DeleteAnimation(animation_defs[existing]);
		animation_defs[existing] = animation;
		return true;
	}

	return false;
}

void P_AddAnimation(animation_list_s *animation)
{
	UINT16 existing = get_animation_id(animation->name);
	if (existing != 0)
	{
		CONS_Alert(CONS_ERROR, "Animation %s already exists\n", animation->name);
		return;
	}

	animation_defs.push_back(animation);
}

// NOTE: If there already is an animation with that name, this returns nullptr.
// You must check if the animation already exists before calling this.
animation_list_s *P_DuplicateAnimation(const char *animation_name, animation_list_s *base, boolean copy_frames)
{
	animation_list_s *animation = find_animation_by_name(animation_name);
	if (animation != nullptr)
	{
		CONS_Alert(CONS_ERROR, "Animation %s already exists\n", animation->name);
		return nullptr;
	}

	animation = create_animation(animation_name);

	if (base)
	{
		animation->count = base->count;
		animation->animations = static_cast<animation_s **>(
			Z_Realloc(animation->animations, animation->count * sizeof(animation_s **), PU_STATIC, nullptr)
		);

		for (size_t i = 0; i < base->count; i++)
		{
			animation_s *subanimation = static_cast<animation_s *>(
				Z_Calloc(sizeof(animation_s), PU_STATIC, nullptr)
			);
			animation->animations[i] = subanimation;

			if (is_valid_subanimation_id(base, i))
			{
				animation_s *base_subanim = base->animations[i];
				subanimation->id = base_subanim->id;
				subanimation->name = Z_StrDup(base_subanim->name);
				subanimation->speed = base_subanim->speed;
				subanimation->loop_index = base_subanim->loop_index;
				subanimation->direction = base_subanim->direction;
				if (base_subanim->fallback != nullptr)
				{
					subanimation->fallback = Z_StrDup(base_subanim->fallback);
				}

				if (copy_frames && base_subanim->num_frames)
				{
					subanimation->num_frames = base_subanim->num_frames;
					subanimation->frames = static_cast<animation_frame_s *>(
						Z_Malloc(subanimation->num_frames * sizeof(animation_frame_s), PU_STATIC, nullptr)
					);
					memcpy(subanimation->frames, base_subanim->frames, subanimation->num_frames * sizeof(animation_frame_s));
				}
			}
		}
	}

	P_AddAnimation(animation);

	return animation;
}

// Merges animations anim_a and anim_b. The subanimations of anim_b come first, then the subanimations in anim_a.
// NOTE: If it exists, this replaces the animation that has the same name.
animation_list_s *P_MergeAnimations(const char *name, animation_list_s *anim_a, animation_list_s *anim_b)
{
	animation_list_s *output = create_animation(name);

	for (size_t i = 0; i < anim_b->count + anim_a->count; i++)
	{
		animation_s *subanimation, *base_subanim;

		if (i >= anim_b->count)
		{
			base_subanim = anim_a->animations[i - anim_b->count];
		}
		else
		{
			base_subanim = anim_b->animations[i];
		}

		if (base_subanim == nullptr)
			continue;

		subanimation = find_subanimation(output, base_subanim->name);

		if (subanimation == nullptr)
		{
			subanimation = create_and_add_subanimation(output, base_subanim->name);
			subanimation->speed = base_subanim->speed;
			subanimation->loop_index = base_subanim->loop_index;
			subanimation->direction = base_subanim->direction;
			if (base_subanim->fallback != nullptr)
			{
				subanimation->fallback = Z_StrDup(base_subanim->fallback);
			}

			if (base_subanim->num_frames && subanimation->num_frames == 0)
			{
				subanimation->num_frames = base_subanim->num_frames;
				subanimation->frames = static_cast<animation_frame_s *>(
					Z_Realloc(subanimation->frames, subanimation->num_frames * sizeof(animation_frame_s), PU_STATIC, nullptr)
				);
				memcpy(subanimation->frames, base_subanim->frames, subanimation->num_frames * sizeof(animation_frame_s));
			}
		}
	}

	if (!P_ReplaceAnimation(output))
	{
		P_AddAnimation(output);
	}

	return output;
}

animation_s *P_FindOrCreateSubAnimation(animation_list_s *animation, const char *subanimation_name)
{
	return find_or_create_subanimation(animation, subanimation_name);
}

animation_list_s *P_GetAnimationByID(UINT16 animation_id)
{
	return get_animation_by_id(animation_id);
}

animation_s *P_GetSubAnimationByID(animation_list_s *animation, UINT16 subanimation_id)
{
	if (subanimation_id >= animation->count)
		return nullptr;

	return animation->animations[subanimation_id];
}

animation_s *P_GetSubAnimationByName(animation_list_s *animation, const char *subanimation_name)
{
	if (animation)
	{
		for (size_t i = 0; i < animation->count; i++)
		{
			if (!strcmp(animation->animations[i]->name, subanimation_name))
				return animation->animations[i];
		}
	}

	return nullptr;
}

static void set_subanim_speed(animation_list_s *animation, UINT16 subanimation_id, fixed_t speed)
{
	animation_t *subanim = P_GetSubAnimationByID(animation, subanimation_id);
	if (subanim != nullptr)
	{
		subanim->speed = speed;
	}
}

static void set_subanim_fallback(animation_list_s *animation, UINT16 subanimation_id, const char *fallback)
{
	animation_t *subanim = P_GetSubAnimationByID(animation, subanimation_id);
	if (subanim != nullptr)
	{
		Z_Free(subanim->fallback);
		subanim->fallback = Z_StrDup(fallback);
	}
}

void P_SetSubanimationSpeed(UINT16 animation_id, UINT16 subanimation_id, fixed_t speed)
{
	animation_list_s *animation = get_animation_by_id(animation_id);

	if (is_valid_subanimation_id(animation, subanimation_id))
	{
		set_subanim_speed(animation, subanimation_id, speed);
	}
}

void P_SetSubanimationFallback(UINT16 animation_id, UINT16 subanimation_id, const char *fallback)
{
	animation_list_s *animation = get_animation_by_id(animation_id);

	if (is_valid_subanimation_id(animation, subanimation_id))
	{
		set_subanim_fallback(animation, subanimation_id, fallback);
	}
}

void P_InitAnimationFrame(animation_frame_s *frame)
{
	frame->frame_num = 0;
	frame->frame_flags = 0;
	frame->duration = 1;
}

void P_InitAnimations()
{
	R_InitSkinAnimations();
}

// Animation parsing
static void read_animation_from_file(const char *lump, size_t lump_len);

static void load_animations_from_range(UINT16 wadnum, UINT16 start, UINT16 end)
{
	UINT16 lump = start;

	while (true)
	{
		lump = W_CheckNumForLongNamePwad("animation_defs", (UINT16)wadnum, lump);
		if (lump == INT16_MAX || lump >= end)
			break;

		lumpnum_t global_lump_id = (wadnum << 16) + lump;
		size_t lump_len = W_LumpLength(global_lump_id);
		const char *lump_text = static_cast<const char *>( W_CacheLumpNum(global_lump_id, PU_CACHE) );

		read_animation_from_file(lump_text, lump_len);

		lump++;
	}
}

void P_LoadAnimations(UINT16 wadnum)
{
	UINT16 lumpnum = INT16_MAX, start = INT16_MAX, end = INT16_MAX;

	start = W_CheckNumForFolderStartPK3("Sprites/", wadnum, 0);
	if (start != INT16_MAX)
	{
		end = W_CheckNumForFolderEndPK3("Sprites/", wadnum, start);
		if (end != INT16_MAX)
		{
			load_animations_from_range(wadnum, start, end);
		}
	}

	start = W_CheckNumForFolderStartPK3("LongSprites/", wadnum, 0);
	if (start != INT16_MAX)
	{
		end = W_CheckNumForFolderEndPK3("LongSprites/", wadnum, start);
		if (end != INT16_MAX)
		{
			load_animations_from_range(wadnum, start, end);
		}
	}

	lumpnum = W_CheckNumForFullNamePK3("animation_defs.json", wadnum, 0);
	if (lumpnum != INT16_MAX)
		load_animations_from_range(wadnum, lumpnum, lumpnum + 1);
}

static void parse_anim_frame(animation_frame_s *frame, json& entry)
{
	if (entry.is_object() == false)
	{
		frame->frame_num = (UINT8)entry;
		return;
	}

	if (entry.contains("frame"))
	{
		frame->frame_num = entry.at("frame");
	}

	if (entry.contains("duration"))
	{
		int duration = entry.at("duration");

		if (duration < 1 || duration >= 32768)
		{
			throw std::runtime_error("invalid frame duration");
		}

		frame->duration = duration;
	}

	if (entry.contains("flip_x"))
	{
		if (entry.at("flip_x") == true)
		{
			frame->frame_flags |= FF_HORIZONTALFLIP;
		}
		else
		{
			frame->frame_flags &= ~FF_HORIZONTALFLIP;
		}
	}

	if (entry.contains("flip_y"))
	{
		if (entry.at("flip_y") == true)
		{
			frame->frame_flags |= FF_VERTICALFLIP;
		}
		else
		{
			frame->frame_flags &= FF_VERTICALFLIP;
		}
	}
}

static void parse_anim_entry(animation_s *animation, json& entry)
{
	if (entry.contains("speed"))
	{
		json speed_value = entry.at("speed");
		if (speed_value.is_number_float())
		{
			float speed_flt = (float)speed_value;

			if (fpclassify(speed_flt) == FP_ZERO || fpclassify(speed_flt) == FP_INFINITE || fpclassify(speed_flt) == FP_NAN)
			{
				throw std::runtime_error("invalid animation speed");
			}

			animation->speed = FloatToFixed(speed_flt);
		}
		else
		{
			animation->speed = ((INT32)speed_value) * FRACUNIT;
		}
	}

	if (entry.contains("direction"))
	{
		std::string direction = entry.value("direction", "forwards");

		if (direction == "forwards")
		{
			animation->direction = ANIM_DIR_FORWARDS;
		}
		else if (direction == "reverse")
		{
			animation->direction = ANIM_DIR_REVERSE;
		}
		else if (direction == "oscillate")
		{
			animation->direction = ANIM_DIR_OSCILLATE;
		}
		else
		{
			throw std::runtime_error("invalid animation direction '" + direction + "'");
		}
	}

	if (entry.contains("frames"))
	{
		json& entry_frames = entry.at("frames");
		size_t num_entry_frames = entry_frames.size(), i = 0;

		if (num_entry_frames == 0)
			return;

		if (animation->frames)
		{
			Z_Free(animation->frames);
			animation->frames = nullptr;
			animation->num_frames = 0;
		}

		if (num_entry_frames > animation->num_frames)
		{
			animation->frames = static_cast<animation_frame_s *>(
				Z_Realloc(animation->frames, num_entry_frames * sizeof(animation_frame_s), PU_STATIC, nullptr)
			);

			for (size_t j = animation->num_frames; j < num_entry_frames; j++)
			{
				P_InitAnimationFrame(&animation->frames[j]);
			}

			animation->num_frames = num_entry_frames;
		}

		for (json& frame_entry_obj : entry_frames)
		{
			parse_anim_frame(&animation->frames[i], frame_entry_obj);
			i++;
		}

		if (entry.contains("loop_index"))
		{
			int loop_index = entry.at("loop_index");

			if (loop_index < 0 || loop_index >= (signed)num_entry_frames)
			{
				if (loop_index != -1)
				{
					CONS_Alert(CONS_WARNING, "invalid animation loop index\n");
				}

				loop_index = UINT16_MAX;
			}

			animation->loop_index = (UINT16)loop_index;
		}
	}
}

static void parse_anim_list(std::string name, json& entry)
{
	animation_list_s *animation = find_animation_by_name(name.c_str());

	if (animation == nullptr)
	{
		animation = create_animation(name.c_str());

		P_AddAnimation(animation);
	}

	for (json& anim_list_entry : entry)
	{
		std::string subanimation_name = anim_list_entry.at("name");
		animation_s *entry = find_or_create_subanimation(animation, subanimation_name.c_str());
		parse_anim_entry(entry, anim_list_entry);
	}

	CONS_Printf("Added animation '%s'\n", animation->name);
}

static void read_animation_from_file(const char *lump, size_t lump_len)
{
	json anim_array = json::parse(lump, lump + lump_len);
	if (anim_array.is_array() == false || anim_array.size() == 0)
		return;

	try
	{
		for (json& anim_entry_obj : anim_array)
		{
			if (anim_entry_obj.is_object() == false)
				throw std::runtime_error("subanimation was not an object");

			for (auto& it : anim_entry_obj.items())
			{
				std::string subanimation_name = it.key();
				json& anim_entry_obj = it.value();
				parse_anim_list(subanimation_name, anim_entry_obj);
			}
		}
	}
	catch (const std::exception& ex)
	{
		CONS_Alert(CONS_ERROR, "Couldn't parse animation: %s\n", ex.what());
	}
}
