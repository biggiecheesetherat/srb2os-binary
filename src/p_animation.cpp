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

static UINT16 get_subanimation_id(animation_list_s *animation, const char *name)
{
	if (animation)
	{
		for (size_t i = 0; i < animation->count; i++)
		{
			if (!strcmp(animation->animations[i]->name, name))
				return i;
		}
	}

	return UINT16_MAX;
}

// Animation playback
static void P_UpdateMobjAnimationFrame(mobj_t *mobj, animation_frame_s *frame)
{
	mobj->frame &= ~FF_FRAMEMASK;
	mobj->frame |= (frame->frame_num & FF_FRAMEMASK) | (frame->frame_flags & ~FF_FRAMEMASK);
}

boolean P_SetupAnimator(animator_s *animator, UINT16 animation_id, UINT16 subanimation_id, UINT16 start_frame)
{
	animation_list_s *animation = get_animation_by_id(animation_id);
	animation_s *entry;

	if (animation == nullptr || animation->count == 0 || subanimation_id >= animation->count)
	{
		return false;
	}

	animator->animation = animation_id;
	animator->subanimation = subanimation_id;
	animator->timer = 0;

	entry = animation->animations[subanimation_id];

	if (animator->direction == ANIM_DIR_OSCILLATE && animator->speed_mul < 0)
		animator->speed_mul = -animator->speed_mul;

	animator->direction = entry->direction;

	if (entry->num_frames == 0)
	{
		animator->frame = 0;
		animator->frame_duration = 0;
		return false;
	}

	animator->frame = start_frame;

	if (animator->frame == UINT16_MAX)
	{
		if (animator->direction == ANIM_DIR_REVERSE)
			animator->frame = entry->num_frames - 1;
		else
			animator->frame = 0;
	}
	else
		animator->frame %= entry->num_frames;

	animator->frame_duration = entry->frames[animator->frame].duration * FRACUNIT;

	return true;
}

static boolean P_SetupMobjAnimation(mobj_t *mobj, UINT16 animation_id, UINT16 subanimation_id, UINT16 start_frame)
{
	if (P_SetupAnimator(&mobj->animator, animation_id, subanimation_id, start_frame))
	{
		animation_list_s *animation = animation_defs[animation_id];
		animation_s *entry = animation->animations[subanimation_id];
		P_UpdateMobjAnimationFrame(mobj, &entry->frames[mobj->animator.frame]);
		return true;
	}

	return false;
}

UINT32 P_GetAnimatorFrame(animator_s *animator)
{
	animation_frame_s *anim_frame;

	if (!animator || animator->animation == 0 || animator->animation > animation_defs.size())
	{
		return 0;
	}

	animation_list_s *animation = animation_defs[animator->animation - 1];
	if (animator->subanimation >= animation->count)
	{
		return 0;
	}

	animation_s *entry = animation->animations[animator->subanimation];
	if (animator->frame >= entry->num_frames)
	{
		return 0;
	}

	anim_frame = &entry->frames[animator->frame];

	return (anim_frame->frame_num & FF_FRAMEMASK) | (anim_frame->frame_flags & ~FF_FRAMEMASK);
}

boolean P_SetMobjAnimation(mobj_t *mobj, UINT16 animation_id, UINT16 subanimation_id, UINT16 start_frame)
{
	animation_list_s *animation = get_animation_by_id(animation_id);
	if (animation == nullptr)
	{
		CONS_Alert(CONS_ERROR, "P_SetMobjAnimation: Invalid animation ID %d\n", animation_id);
		return false;
	}

	if (animation->count == 0)
	{
		return false;
	}

	if (subanimation_id >= animation->count)
	{
		CONS_Alert(CONS_ERROR, "P_SetMobjAnimation: Invalid subanimation ID %d/%s for animation '%s'\n", subanimation_id, sizeu1(animation->count), P_GetAnimationNameByID(animation_id));

		subanimation_id = 0;
	}

	if (mobj->animator.animation == animation_id && mobj->animator.subanimation == subanimation_id)
		return true;

	P_SetupMobjAnimation(mobj, animation_id, subanimation_id, start_frame);

	return true;
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
	if (animation->count == 0)
	{
		return false;
	}

	UINT16 subanimation_id = get_subanimation_id(animation, subanimation_name);
	if (subanimation_id == UINT16_MAX)
	{
		subanimation_id = 0;

		CONS_Alert(CONS_ERROR, "P_SetNamedMobjAnimation: No subanimation named '%s' in animation '%s'\n", subanimation_name, animation_name);
	}

	if (mobj->animator.animation == animation_id && mobj->animator.subanimation == subanimation_id)
		return true;

	P_SetupMobjAnimation(mobj, animation_id, subanimation_id, start_frame);

	return true;
}

void P_DoAnimationPlayback(animator_s *animator, mobj_t *mobj, tic_t timedelta)
{
	if (animator->animation == 0 || animator->animation > animation_defs.size())
	{
		return;
	}

	animation_list_s *animation = animation_defs[animator->animation - 1];
	if (animator->subanimation >= animation->count)
	{
		return;
	}

	animation_s *entry = animation->animations[animator->subanimation];
	if (animator->frame >= entry->num_frames)
	{
		return;
	}

	unsigned i = 0;
	animation_frame_s *anim_frame;

	fixed_t anim_speed = FixedMul(FixedMul(entry->speed, animator->speed_mul), timedelta);
	if (anim_speed == 0)
		return;

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

			anim_frame = &entry->frames[animator->frame];

			if (mobj != nullptr)
			{
				P_UpdateMobjAnimationFrame(mobj, anim_frame);
			}

			animator->frame_duration = anim_frame->duration * FRACUNIT;

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
			animator->frame++;

			if (animator->frame >= entry->num_frames)
			{
				if (entry->loop_index == UINT16_MAX)
					animator->frame = 0;
				else if (entry->loop_index < entry->num_frames)
					animator->frame = entry->loop_index;
				else
					animator->frame = entry->num_frames - 1;
			}

			anim_frame = &entry->frames[animator->frame];

			if (mobj != nullptr)
			{
				P_UpdateMobjAnimationFrame(mobj, anim_frame);
			}

			animator->frame_duration = anim_frame->duration * FRACUNIT;

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

			if (animator->frame == 0)
			{
				if (entry->loop_index != UINT16_MAX && entry->loop_index < entry->num_frames)
					animator->frame = entry->loop_index;
				else
					animator->frame = entry->num_frames - 1;
			}
			else
				animator->frame--;

			anim_frame = &entry->frames[animator->frame];

			if (mobj != nullptr)
			{
				P_UpdateMobjAnimationFrame(mobj, anim_frame);
			}

			animator->frame_duration = anim_frame->duration * FRACUNIT;

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

	if (subanimation_id < animation->count)
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

	if (subanimation_id < animation->count)
	{
		return animation->animations[subanimation_id]->num_frames;
	}

	return 0;
}

static animation_list_s *find_or_create_animation(const char *name)
{
	animation_list_s *animation = find_animation_by_name(name);
	if (animation != nullptr)
		return animation;

	animation = static_cast<animation_list_s *>(
		Z_Calloc(sizeof(animation_list_s), PU_STATIC, NULL)
	);

	animation->name = Z_StrDup(name);

	animation_defs.push_back(animation);

	return animation;
}

static animation_s *find_or_create_subanimation(animation_list_s *list, const char *name, int index)
{
	for (size_t i = 0; i < list->count; i++)
	{
		if (!strcmp(list->animations[i]->name, name))
			return list->animations[i];
	}

	animation_s *subanimation = static_cast<animation_s *>(
		Z_Calloc(sizeof(animation_s), PU_STATIC, NULL)
	);
	subanimation->name = Z_StrDup(name);
	subanimation->speed = FRACUNIT;
	subanimation->loop_index = UINT16_MAX;
	subanimation->direction = ANIM_DIR_FORWARDS;

	list->animations = static_cast<animation_s **>(
		Z_Realloc(list->animations, (list->count + 1) * sizeof(animation_s **), PU_STATIC, NULL)
	);

	if (index < 0 || (unsigned)index >= list->count)
	{
		list->animations[list->count] = subanimation;
	}
	else
	{
		memmove(&list->animations[index + 1], &list->animations[index], list->count - index);
		list->animations[index] = subanimation;
	}

	list->count++;

	return subanimation;
}

animation_list_s *P_FindOrCreateAnimation(const char *animation_name)
{
	return find_or_create_animation(animation_name);
}

animation_s *P_FindOrCreateSubAnimation(animation_list_s *animation, const char *subanimation_name)
{
	return find_or_create_subanimation(animation, subanimation_name, -1);
}

animation_s *P_FindOrCreateSubAnimationAt(animation_list_s *animation, const char *subanimation_name, unsigned index)
{
	return find_or_create_subanimation(animation, subanimation_name, (int)index);
}

void P_InitAnimations(void)
{
	// TODO: Create a preset player animation that skins inherit from.
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
	UINT16 start = INT16_MAX, end = INT16_MAX;

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
}

static void parse_anim_frame(animation_frame_s *frame, json& entry)
{
	UINT8 frame_num = 0;
	tic_t duration = 1;
	boolean flip_x = false;
	boolean flip_y = false;

	if (entry.is_object() == true)
	{
		frame_num = entry.value("frame", 0);
		duration = entry.value("duration", 1);
		flip_x = entry.value("flip_x", false);
		flip_y = entry.value("flip_y", false);
	}
	else
	{
		frame_num = (UINT8)entry;
	}

	frame->frame_num = frame_num;
	frame->frame_flags = 0;
	frame->duration = duration;

	if (flip_x == true)
	{
		frame->frame_flags |= FF_HORIZONTALFLIP;
	}
	if (flip_y == true)
	{
		frame->frame_flags |= FF_VERTICALFLIP;
	}
}

static void parse_anim_entry(animation_s *animation, json& entry)
{
	fixed_t speed = FRACUNIT;
	std::string direction = entry.value("direction", "forwards");
	animation_direction_e direction_type;
	json& entry_frames = entry.at("frames");
	size_t num_entry_frames = entry_frames.size(), i = 0;

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

			speed = FloatToFixed(speed_flt);
		}
		else
		{
			speed = ((INT32)speed_value) * FRACUNIT;
		}
	}

	if (direction == "forwards")
	{
		direction_type = ANIM_DIR_FORWARDS;
	}
	else if (direction == "reverse")
	{
		direction_type = ANIM_DIR_REVERSE;
	}
	else if (direction == "oscillate")
	{
		direction_type = ANIM_DIR_OSCILLATE;
	}
	else
	{
		throw std::runtime_error("invalid animation direction '" + direction + "'");
	}

	animation->speed = speed;
	animation->direction = direction_type;

	if (animation->frames)
	{
		Z_Free(animation->frames);
		animation->frames = NULL;
		animation->num_frames = 0;
	}

	if (num_entry_frames)
	{
		animation->num_frames = num_entry_frames;
		animation->frames = static_cast<animation_frame_s *>(
			Z_Calloc(num_entry_frames * sizeof(animation_frame_s), PU_STATIC, NULL)
		);

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
	else
	{
		throw std::runtime_error("no frames in subanimation");
	}
}

static void parse_anim_list(std::string name, json& entry)
{
	animation_list_s *animation = find_or_create_animation(name.c_str());

	for (json& anim_list_entry : entry)
	{
		std::string subanimation_name = anim_list_entry.at("name");
		animation_s *entry = find_or_create_subanimation(animation, subanimation_name.c_str(), -1);
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
