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
#include "m_fixed.h"
#include "p_pspr.h"
#include "w_wad.h"
#include "z_zone.h"

using nlohmann::json;

static std::vector<struct animation_list_s*> animation_defs;

static struct animation_list_s *find_animation_by_name(const char *name)
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

static struct animation_list_s *get_animation_by_id(UINT16 id)
{
	if (id == 0 || id > animation_defs.size())
	{
		return nullptr;
	}

	return animation_defs[id - 1];
}

static UINT16 get_animation_entry_id(struct animation_list_s *animation, const char *name)
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
static void P_UpdateAnimationFrame(mobj_t *mobj, struct animation_frame_s *frame)
{
	mobj->frame &= ~FF_FRAMEMASK;
	mobj->frame |= (frame->frame_num & FF_FRAMEMASK) | (frame->frame_flags & ~FF_FRAMEMASK);
	mobj->anim_frame_duration = frame->duration * FRACUNIT;
}

static void P_SetupMobjAnimation(mobj_t *mobj, struct animation_s *entry)
{
	if (mobj->anim_direction == ANIM_DIR_OSCILLATE && mobj->anim_speed_mul < 0)
		mobj->anim_speed_mul = -mobj->anim_speed_mul;

	mobj->anim_timer = 0;
	mobj->anim_direction = entry->direction;

	if (entry->num_frames == 0)
	{
		return;
	}

	if (mobj->anim_frame == UINT16_MAX)
	{
		if (mobj->anim_direction == ANIM_DIR_REVERSE)
			mobj->anim_frame = entry->num_frames - 1;
		else
			mobj->anim_frame = 0;
	}
	else
		mobj->anim_frame %= entry->num_frames;

	P_UpdateAnimationFrame(mobj, &entry->frames[mobj->anim_frame]);
}

boolean P_SetMobjAnimation(mobj_t *mobj, UINT16 animation_id, UINT16 entry_id, UINT16 start_frame)
{
	struct animation_list_s *animation = get_animation_by_id(animation_id);
	if (animation == nullptr)
	{
		CONS_Alert(CONS_ERROR, "P_SetMobjAnimation: Invalid animation ID %d\n", animation_id);
		return false;
	}

	if (animation->count == 0)
	{
		return false;
	}

	if (entry_id >= animation->count)
	{
		entry_id = 0;

		CONS_Alert(CONS_ERROR, "P_SetMobjAnimation: Invalid animation entry %d for animation ID %d\n", entry_id, animation_id);
	}

	mobj->animation = animation_id;
	mobj->anim_entry = entry_id;
	mobj->anim_frame = start_frame;

	P_SetupMobjAnimation(mobj, animation->animations[mobj->anim_entry]);

	return true;
}

boolean P_SetNamedMobjAnimation(mobj_t *mobj, const char *animation_name, const char *entry_name, UINT16 start_frame)
{
	UINT16 id = get_animation_id(animation_name);
	if (id == 0)
	{
		CONS_Alert(CONS_ERROR, "P_SetNamedMobjAnimation: Unknown animation '%s'\n", animation_name);
		return false;
	}

	struct animation_list_s *animation = animation_defs[id];
	if (animation->count == 0)
	{
		return false;
	}

	UINT16 entry_id = get_animation_entry_id(animation, entry_name);
	if (entry_id == UINT16_MAX)
	{
		entry_id = 0;

		CONS_Alert(CONS_ERROR, "P_SetNamedMobjAnimation: No animation entry named '%s' in animation '%s'\n", entry_name, animation_name);
	}

	mobj->animation = id;
	mobj->anim_entry = entry_id;
	mobj->anim_frame = start_frame;

	P_SetupMobjAnimation(mobj, animation->animations[mobj->anim_entry]);

	return true;
}

void P_UpdateAnimation(mobj_t *mobj)
{
	if (mobj->animation == 0 || mobj->animation > animation_defs.size())
	{
		return;
	}

	struct animation_list_s *animation = animation_defs[mobj->animation - 1];
	if (mobj->anim_entry >= animation->count)
	{
		return;
	}

	struct animation_s *entry = animation->animations[mobj->anim_entry];
	if (mobj->anim_frame >= entry->num_frames)
	{
		return;
	}

	if (entry->speed == 0)
	{
		return;
	}

	unsigned i = 0;

	fixed_t anim_speed = FixedMul(entry->speed, mobj->anim_speed_mul);
	if (anim_speed == 0)
		return;

	mobj->anim_timer += anim_speed;

	// Oscillating animation
	if (mobj->anim_direction == ANIM_DIR_OSCILLATE)
	{
		while (mobj->anim_timer < 0 || mobj->anim_timer > mobj->anim_frame_duration)
		{
			UINT16 frame = mobj->anim_frame;

			if (anim_speed > 0)
			{
				mobj->anim_timer -= mobj->anim_frame_duration;

				frame++;

				if (frame >= entry->num_frames)
				{
					if (entry->loop_index != 0 && entry->loop_index < entry->num_frames)
						frame = entry->loop_index;
					else
						frame = entry->num_frames - 1;

					mobj->anim_speed_mul = -mobj->anim_speed_mul;
					anim_speed = -anim_speed;
				}
			}
			else if (anim_speed < 0)
			{
				mobj->anim_timer += mobj->anim_frame_duration;

				if (frame == 0 || (entry->loop_index != 0 && (signed)frame - 1 < (signed)entry->loop_index))
				{
					if (entry->loop_index != 0 && entry->loop_index < entry->num_frames)
						frame = entry->loop_index;
					else
						frame = 0;

					mobj->anim_speed_mul = -mobj->anim_speed_mul;
					anim_speed = -anim_speed;
				}
				else
					frame--;
			}

			mobj->anim_frame = frame;

			P_UpdateAnimationFrame(mobj, &entry->frames[mobj->anim_frame]);

			if (++i > TICRATE)
				break;
		}

		return;
	}

	if (mobj->anim_direction == ANIM_DIR_REVERSE)
		anim_speed = -anim_speed;

	// Animate forwards
	if (anim_speed > 0)
	{
		while (mobj->anim_timer > mobj->anim_frame_duration)
		{
			mobj->anim_timer -= mobj->anim_frame_duration;
			mobj->anim_frame++;

			if (mobj->anim_frame >= entry->num_frames)
			{
				if (entry->loop_index < entry->num_frames)
					mobj->anim_frame = entry->loop_index;
				else
					mobj->anim_frame = entry->num_frames - 1;
			}

			P_UpdateAnimationFrame(mobj, &entry->frames[mobj->anim_frame]);

			if (++i > TICRATE)
				break;
		}
	}
	// Animate backwards
	else if (anim_speed < 0)
	{
		while (mobj->anim_timer < 0)
		{
			mobj->anim_timer += mobj->anim_frame_duration;

			if (mobj->anim_frame == 0)
			{
				if (entry->loop_index < entry->num_frames)
					mobj->anim_frame = entry->loop_index;
				else
					mobj->anim_frame = entry->num_frames - 1;
			}
			else
				mobj->anim_frame--;

			P_UpdateAnimationFrame(mobj, &entry->frames[mobj->anim_frame]);

			if (++i > TICRATE)
				break;
		}
	}
}

static struct animation_s *find_animation_entry(struct animation_list_s *list, const char *name)
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

struct animation_list_s *P_GetNamedAnimation(const char *animation_name)
{
	return find_animation_by_name(animation_name);
}

struct animation_s *P_GetNamedEntryInAnimation(struct animation_list_s *animation, const char *entry_name)
{
	if (animation)
	{
		return find_animation_entry(animation, entry_name);
	}

	return NULL;
}

UINT16 P_GetNamedAnimationID(const char *animation_name)
{
	return get_animation_id(animation_name);
}

UINT16 P_GetNamedEntryIDInAnimation(UINT16 animation_id, const char *entry_name)
{
	struct animation_list_s *animation = get_animation_by_id(animation_id);
	if (animation != nullptr)
	{
		for (size_t i = 0; i < animation->count; i++)
		{
			if (!strcmp(animation->animations[i]->name, entry_name))
				return i;
		}
	}

	return UINT16_MAX;
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

void P_InitAnimations(void)
{
	for (UINT16 i = 0; i < numwadfiles; i++)
		P_LoadAnimations(i);
}

static struct animation_list_s *find_or_create_animation(const char *name)
{
	struct animation_list_s *animation = find_animation_by_name(name);
	if (animation != nullptr)
		return animation;

	animation = static_cast<struct animation_list_s *>(
		Z_Calloc(sizeof(struct animation_list_s), PU_STATIC, NULL)
	);

	animation->name = Z_StrDup(name);

	animation_defs.push_back(animation);

	return animation;
}

static struct animation_s *find_or_create_animation_entry(struct animation_list_s *list, const char *name)
{
	for (size_t i = 0; i < list->count; i++)
	{
		if (!strcmp(list->animations[i]->name, name))
			return list->animations[i];
	}

	list->count++;
	list->animations = static_cast<struct animation_s **>(
		Z_Realloc(list->animations, list->count * sizeof(struct animation_s **), PU_STATIC, NULL)
	);

	struct animation_s *entry = static_cast<struct animation_s *>(
		Z_Calloc(sizeof(struct animation_s), PU_STATIC, NULL)
	);
	list->animations[list->count - 1] = entry;

	entry->name = Z_StrDup(name);

	return entry;
}

static void parse_anim_frame(struct animation_frame_s *frame, json& entry)
{
	UINT8 frame_num = 0;
	tic_t duration = 1;
	boolean mirrored = false;

	if (entry.is_object() == true)
	{
		frame_num = entry.value("frame", 0);
		duration = entry.value("duration", 1);
		mirrored = entry.value("mirrored", false);
	}
	else
	{
		frame_num = (UINT8)entry;
	}

	frame->frame_num = frame_num;
	frame->frame_flags = 0;
	frame->duration = duration;

	if (mirrored == true)
	{
		frame->frame_flags |= FF_HORIZONTALFLIP;
	}
}

static void parse_anim_entry(struct animation_s *animation, json& entry)
{
	fixed_t speed = FRACUNIT;
	unsigned loop_index = entry.value("loop_index", 0);
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
	animation->loop_index = 0;
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
		animation->frames = static_cast<struct animation_frame_s *>(
			Z_Calloc(num_entry_frames * sizeof(struct animation_frame_s), PU_STATIC, NULL)
		);

		for (json& frame_entry_obj : entry_frames)
		{
			parse_anim_frame(&animation->frames[i], frame_entry_obj);
			i++;
		}

		if (loop_index >= num_entry_frames)
		{
			loop_index = num_entry_frames - 1;
			CONS_Alert(CONS_WARNING, "invalid animation loop index\n");
		}
	}
	else
	{
		throw std::runtime_error("no frames in animation entry");
	}

	animation->loop_index = loop_index;
}

static void parse_anim_list(std::string name, json& entry)
{
	struct animation_list_s *animation = find_or_create_animation(name.c_str());

	for (json& anim_list_entry : entry)
	{
		std::string entry_name = anim_list_entry.at("name");
		struct animation_s *entry = find_or_create_animation_entry(animation, entry_name.c_str());
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
				throw std::runtime_error("animation entry was not an object");

			for (auto& it : anim_entry_obj.items())
			{
				std::string anim_entry_name = it.key();
				json& anim_entry_obj = it.value();
				parse_anim_list(anim_entry_name, anim_entry_obj);
			}
		}
	}
	catch (const std::exception& ex)
	{
		CONS_Alert(CONS_ERROR, "Couldn't parse animation: %s\n", ex.what());
	}
}
