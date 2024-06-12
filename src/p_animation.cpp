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
#include "w_wad.h"
#include "z_zone.h"

using nlohmann::json;

static std::vector<struct animation_list_s*> animation_defs;

static void P_ReadAnimationFromFile(const char *lump, size_t lump_len);

static void P_LoadAnimationsFromRange(UINT16 wadnum, UINT16 start, UINT16 end)
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

		P_ReadAnimationFromFile(lump_text, lump_len);

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
			P_LoadAnimationsFromRange(wadnum, start, end);
		}
	}

	start = W_CheckNumForFolderStartPK3("LongSprites/", wadnum, 0);
	if (start != INT16_MAX)
	{
		end = W_CheckNumForFolderEndPK3("LongSprites/", wadnum, start);
		if (end != INT16_MAX)
		{
			P_LoadAnimationsFromRange(wadnum, start, end);
		}
	}
}

void P_InitAnimations(void)
{
	for (UINT16 i = 0; i < numwadfiles; i++)
		P_LoadAnimations(i);
}

static struct animation_list_s *P_FindAnimationList(const char *name)
{
	for (size_t i = 0; i < animation_defs.size(); i++)
	{
		if (!strcmp(animation_defs[i]->name, name))
			return animation_defs[i];
	}

	return nullptr;
}

static struct animation_s *P_FindAnimationEntry(struct animation_list_s *list, const char *name)
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

struct animation_s *P_GetAnimationEntry(UINT16 list_id, const char *entry_name)
{
	if (list_id < animation_defs.size())
	{
		struct animation_list_s *animation = animation_defs[list_id];

		return P_FindAnimationEntry(animation, entry_name);
	}

	return nullptr;
}

struct animation_s *P_GetAnimationEntryByID(UINT16 list_id, UINT16 entry_id)
{
	if (list_id < animation_defs.size())
	{
		struct animation_list_s *animation = animation_defs[list_id];

		if (entry_id < animation->count)
			return animation->animations[entry_id];
	}

	return nullptr;
}

struct animation_s *P_GetNamedAnimationEntry(const char *list_name, const char *entry_name)
{
	struct animation_list_s *animation = P_FindAnimationList(list_name);

	return P_FindAnimationEntry(animation, entry_name);
}

static struct animation_list_s *P_FindOrCreateAnimation(const char *name)
{
	struct animation_list_s *animation = P_FindAnimationList(name);
	if (animation != nullptr)
		return animation;

	animation = static_cast<struct animation_list_s *>(
		Z_Calloc(sizeof(struct animation_list_s), PU_STATIC, NULL)
	);

	animation->name = Z_StrDup(name);

	animation_defs.push_back(animation);

	return animation;
}

static struct animation_s *P_FindOrCreateAnimationEntry(struct animation_list_s *list, const char *name)
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
	UINT8 num = entry.value("frame", 0);
	tic_t duration = entry.value("duration", 1);
	boolean mirrored = entry.value("mirrored", false);

	frame->num = num;
	frame->duration = duration;
	frame->mirrored = mirrored;
}

static void parse_anim_entry(struct animation_s *animation, json& entry)
{
	tic_t speed = entry.value("speed", 1);
	unsigned loop_index = entry.value("loop_index", 0);
	std::string direction = entry.value("direction", "forward");
	animation_direction_e direction_type;
	json& entry_frames = entry.at("frames");
	size_t num_entry_frames = entry_frames.size(), i = 0;

	if (speed < 1)
	{
		throw std::runtime_error("invalid animation speed");
	}

	if (direction == "forward")
	{
		direction_type = ANIM_DIR_FORWARD;
	}
	else if (direction == "backward")
	{
		direction_type = ANIM_DIR_BACKWARD;
	}
	else if (direction == "ping-pong")
	{
		direction_type = ANIM_DIR_PINGPONG;
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
	}

	if (num_entry_frames)
	{
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
	struct animation_list_s *animation = P_FindOrCreateAnimation(name.c_str());

	for (json& anim_list_entry : entry)
	{
		std::string entry_name = anim_list_entry.at("name");
		struct animation_s *entry = P_FindOrCreateAnimationEntry(animation, entry_name.c_str());
		parse_anim_entry(entry, anim_list_entry);
	}

	CONS_Printf("Added animation '%s'\n", animation->name);
}

void P_ReadAnimationFromFile(const char *lump, size_t lump_len)
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
