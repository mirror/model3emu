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
 * OSDConfig.h
 * 
 * Header file defining the COSDConfig class: OSD configuration settings,
 * inherited by CConfig.
 */

#ifndef INCLUDED_OSDCONFIG_H
#define INCLUDED_OSDCONFIG_H


#include <string>
#include "Supermodel.h"
using namespace std;


/*
 * COSDConfig:
 *
 * Settings used by COSDConfig.
 */
class COSDConfig
{
public:
	unsigned	xRes, yRes;		// X and Y resolution, in pixels
	bool 		fullScreen;		// Full screen mode (if true)
	bool		wideScreen;		// Wide screen hack
	bool        vsync;          // Enable v-sync
	bool 		throttle;		// 60 Hz frame limiting
	bool		showFPS;		// Show frame rate
	unsigned    crosshairs;     // For game guns: 0 = no crosshairs, 1 = player 1 only, 2 = player 2 only, 3 = both players
	bool		flipStereo;		// Flip stereo channels

#ifdef SUPERMODEL_DEBUGGER
	bool		disableDebugger;	// disables the debugger (not stored in the config. file)
#endif

#ifdef SUPERMODEL_WIN32
	unsigned	dInputConstForceLeftMax;
	unsigned	dInputConstForceRightMax;
	unsigned	dInputSelfCenterMax;
	unsigned	dInputFrictionMax;
	unsigned	dInputVibrateMax;
	unsigned	xInputConstForceThreshold;
	unsigned	xInputConstForceMax;
	unsigned	xInputVibrateMax;
#endif

	// Input system
	inline void SetInputSystem(const char *inpSysName)
	{
		if (inpSysName == NULL)
		{
#ifdef SUPERMODEL_WIN32	// default is DirectInput on Windows
			inputSystem = "dinput";
#else					// everyone else uses SDL
			inputSystem = "sdl";
#endif
			return;
		}
		
		if (stricmp(inpSysName,"sdl") 
#ifdef SUPERMODEL_WIN32
			&& stricmp(inpSysName,"dinput") && stricmp(inpSysName,"xinput") && stricmp(inpSysName,"rawinput")
#endif
			)
		{
#ifdef SUPERMODEL_WIN32
			ErrorLog("Unknown input system '%s', defaulting to DirectInput.", inpSysName);
			inputSystem = "dinput";
#else
			ErrorLog("Unknown input system '%s', defaulting to SDL.", inpSysName);
			inputSystem = "sdl";
#endif
			return;
		}
		
		inputSystem = inpSysName;
	}
	
	inline const char *GetInputSystem(void)
	{
		return inputSystem.c_str();
	}

	// Outputs
	inline void SetOutputs(const char *outputsName)
	{
		if (outputsName == NULL)
		{
			outputs = "none";
			return;
		}

		if (stricmp(outputsName, "none")
#ifdef SUPERMODEL_WIN32
			&& stricmp(outputsName, "win")
#endif
			)
		{
			ErrorLog("Unknown outputs '%s', defaulting to None.", outputsName);
			outputs = "none";
			return;
		}

		outputs = outputsName;
	}

	inline const char *GetOutputs(void)
	{
		return outputs.c_str();
	}
		
	// Defaults
	COSDConfig(void)
	{
		xRes = 496;
		yRes = 384;
		fullScreen = false;
		wideScreen = false;
		vsync = true;
		throttle = true;
		showFPS = false;
		crosshairs = 0;
		flipStereo = false;
#ifdef SUPERMODEL_DEBUGGER
		disableDebugger = false;
#endif
#ifdef SUPERMODEL_WIN32
		inputSystem = "dinput";
		dInputConstForceLeftMax = 100;
		dInputConstForceRightMax = 100;
		dInputSelfCenterMax = 100;
		dInputFrictionMax = 100;
		dInputVibrateMax = 100;
		xInputConstForceThreshold = 30;
		xInputConstForceMax = 100;
		xInputVibrateMax = 100;
#else
		inputSystem = "sdl";
#endif
		outputs = "none";
	}
	
private:
	string	inputSystem;
	string  outputs;
};


#endif	// INCLUDED_OSDCONFIG_H
