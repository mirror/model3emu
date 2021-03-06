/**
 ** Supermodel
 ** A Sega Model 3 Arcade Emulator.
 ** Copyright 2011 Bart Trzynadlowski, Nik Henson
 **
 ** This file is part of Supermodel.
 **
 ** Supermodel is free software: you can redistribute it and/or modify it under
 ** the terms of the GNU General Public License as published by the Free 
 ** Software Foundation, either version 3 of the License, or (at your option)
 ** any later version.
 **
 ** Supermodel is distributed in the hope that it will be useful, but WITHOUT
 ** ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 ** FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 ** more details.
 **
 ** You should have received a copy of the GNU General Public License along
 ** with Supermodel.  If not, see <http://www.gnu.org/licenses/>.
 **/

/*
 * Games.h
 * 
 * Header file containing Model 3 game and ROM file information.
 */

#ifndef INCLUDED_GAMES_H
#define INCLUDED_GAMES_H


#include "ROMLoad.h"	// ROMInfo structure


/******************************************************************************
 Definitions
******************************************************************************/

// Input flags
#define GAME_INPUT_COMMON			0x00001	// game has common controls
#define GAME_INPUT_VEHICLE			0x00002	// game has vehicle controls
#define GAME_INPUT_JOYSTICK1		0x00004	// game has joystick 1 
#define GAME_INPUT_JOYSTICK2		0x00008	// game has joystick 2
#define GAME_INPUT_FIGHTING			0x00010	// game has fighting game controls
#define GAME_INPUT_VR4				0x00020	// game has four VR view buttons
#define GAME_INPUT_VIEWCHANGE		0x00040 // game has single view change button
#define GAME_INPUT_SHIFT4			0x00080	// game has 4-speed shifter
#define GAME_INPUT_SHIFTUPDOWN      0x00100 // game has up/down shifter
#define GAME_INPUT_HANDBRAKE		0x00200	// game has handbrake
#define GAME_INPUT_HARLEY           0x00400 // game has Harley Davidson controls
#define GAME_INPUT_GUN1				0x00800	// game has light gun 1
#define GAME_INPUT_GUN2				0x01000	// game has light gun 2
#define GAME_INPUT_ANALOG_JOYSTICK	0x02000	// game has analog joystick
#define GAME_INPUT_TWIN_JOYSTICKS	0x04000	// game has twin joysticks
#define GAME_INPUT_SOCCER	  		0x08000	// game has soccer controls
#define GAME_INPUT_SPIKEOUT			0x10000	// game has Spikeout buttons
#define GAME_INPUT_ANALOG_GUN1		0x20000	// game has analog gun 1
#define GAME_INPUT_ANALOG_GUN2		0x40000	// game has analog gun 2
#define GAME_INPUT_SKI				0x80000 // game has ski controls
#define GAME_INPUT_ALL				0xFFFFF


/******************************************************************************
 Data Structures
******************************************************************************/

/*
 * GameInfo:
 *
 * Describes a Model 3 game. List is terminated when title == NULL.
 */
struct GameInfo
{
	// Game information
	const char		id[10];			// 9-character game identifier (also serves as zip archive file name)
	const char		*parent;		// parent game identifier
	const char		*title;			// complete game title
	const char		*mfgName;		// name of manufacturer
	unsigned		year;			// year released (in decimal)
	int				step;			// Model 3 hardware stepping: 0x10 = 1.0, 0x15 = 1.5, 0x20 = 2.0, 0x21 = 2.1
	unsigned		cromSize;		// size of fixed CROM (up to 8 MB)
	bool			mirrorLow64MB;	// mirror low 64 MB of banked CROM space to upper 64 MB
	unsigned		vromSize;		// size of video ROMs (32 or 64 MB; if 32 MB, will have to be mirrored)
	unsigned		sampleSize;		// size of sample ROMS (8 or 16 MB; if 8 MB, will have to be mirrored)
	unsigned		inputFlags;		// game input types
	int				mpegBoard;		// MPEG music board type: 0 = none, 1 = DSB1 (Z80), 2 = DSB2 (68K).
	bool			driveBoard;		// drive board (supported if true)

	// ROM files
	struct ROMInfo	ROM[48];
};


/******************************************************************************
 Model 3 Game List

 All games supported by Supermodel. All ROMs are loaded according to their 
 native endianness. That is, the PowerPC ROMs are loaded just as a real 
 PowerPC would see them. The emulator may reorder the bytes on its own for 
 performance reasons (but the ROMs are not specified that way here).
******************************************************************************/

extern const struct GameInfo g_Model3GameList[];


#endif	// INCLUDED_GAMES_H