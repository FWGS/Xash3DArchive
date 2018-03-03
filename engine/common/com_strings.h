/*
com_strings.h - all paths to external resources that hardcoded into engine
Copyright (C) 2018 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#ifndef COM_STRINGS_H
#define COM_STRINGS_H

// default colored message headers
#define S_NOTE			"^2Note:^7 "
#define S_WARN			"^3Warning:^7 "
#define S_ERROR			"^1Error:^7 "
#define S_USAGE			"Usage: "

#define S_OPENGL_NOTE		"^2OpenGL Note:^7 "
#define S_OPENGL_WARN		"^3OpenGL Warning:^7 "
#define S_OPENGL_ERROR		"^3OpenGL Error:^7 "

// end game final default message
#define DEFAULT_ENDGAME_MESSAGE	"The End"

// path to the hash-pak that contain custom player decals
#define CUSTOM_RES_PATH		"custom.hpk"

// path to default playermodel in GoldSrc
#define DEFAULT_PLAYER_PATH_HALFLIFE	"models/player.mdl"

// path to default playermodel in Quake
#define DEFAULT_PLAYER_PATH_QUAKE	"progs/player.mdl"

// debug beams
#define DEFAULT_LASERBEAM_PATH	"sprites/laserbeam.spr"

#define CVAR_GLCONFIG_DESCRIPTION	"enable or disable %s"

#endif//COM_STRINGS_H