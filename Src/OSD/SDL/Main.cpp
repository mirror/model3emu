/**
 ** Supermodel
 ** A Sega Model 3 Arcade Emulator.
 ** Copyright 2011-2012 Bart Trzynadlowski, Nik Henson
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
 * Main.cpp
 * 
 * Main program driver for the SDL port.
 *
 * To Do Before Next Release
 * -------------------------
 * - Add UI keys for balance setting? 
 * - 5.1 audio support?
 *
 * Compile-Time Options
 * --------------------
 * - SUPERMODEL_WIN32: Define this if compiling on Windows.
 * - SUPERMODEL_OSX: Define this if compiling on Mac OS X.
 * - SUPERMODEL_DEBUGGER: Enable the debugger.
 * - DEBUG: Debug mode (use with caution, produces large logs of game behavior)
 */
 
#include <new>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdarg>
#include "Pkgs/glew.h"
#ifdef SUPERMODEL_OSX
#include <SDL/SDL.h>
#else
#include <SDL.h>
#endif

#include "Supermodel.h"
#include "SDLInputSystem.h"
#ifdef SUPERMODEL_WIN32
#include "DirectInputSystem.h"
#include "WinOutputs.h"
#endif


/******************************************************************************
 Error and Debug Logging
******************************************************************************/

// Log file names
#define DEBUG_LOG_FILE	"debug.log"
#define ERROR_LOG_FILE	"error.log"

// Logger object is used to redirect log messages appropriately
static CLogger *s_Logger = NULL;

CLogger *GetLogger()
{
	return s_Logger;
}

void SetLogger(CLogger *Logger)
{
	s_Logger = Logger;
}

void DebugLog(const char *fmt, ...)
{
	if (s_Logger == NULL)
		return;
	va_list vl;
	va_start(vl, fmt);
	s_Logger->DebugLog(fmt, vl);
	va_end(vl);
}

void InfoLog(const char *fmt, ...)
{
	if (s_Logger == NULL)
		return;
	va_list vl;
	va_start(vl, fmt);
	s_Logger->InfoLog(fmt, vl);
	va_end(vl);
}

bool ErrorLog(const char *fmt, ...)
{
	if (s_Logger == NULL)
		return FAIL;
	va_list vl;
	va_start(vl, fmt);
	s_Logger->ErrorLog(fmt, vl);
	va_end(vl);
	return FAIL;
}


/******************************************************************************
 Display Management
******************************************************************************/

/*
 * Position and size of rectangular region within OpenGL display to render to.
 * Unlike the g_Config object, these end up containing the actual resolution 
 * (and computed offsets within the viewport) that will be rendered based on
 * what was obtained from SDL.
 */
unsigned	xOffset, yOffset;		// offset of renderer output within OpenGL viewport
unsigned 	xRes, yRes;				// renderer output resolution (can be smaller than GL viewport)
unsigned	totalXRes, totalYRes;	// total resolution (the whole GL viewport)

static bool SetGLGeometry(unsigned *xOffsetPtr, unsigned *yOffsetPtr, unsigned *xResPtr, unsigned *yResPtr, unsigned *totalXResPtr, unsigned *totalYResPtr, bool keepAspectRatio)
{
	const SDL_VideoInfo	*VideoInfo;
	float				 model3Ratio, ratio;
	float				 xRes, yRes;
	
	// What resolution did we actually get?
  	VideoInfo = SDL_GetVideoInfo();
  	*totalXResPtr = VideoInfo->current_w;
  	*totalYResPtr = VideoInfo->current_h;
  	
  	// If required, fix the aspect ratio of the resolution that the user passed to match Model 3 ratio
  	xRes = (float) *xResPtr;
  	yRes = (float) *yResPtr;
  	if (keepAspectRatio)
	{
		model3Ratio = 496.0f/384.0f;
  		ratio = xRes/yRes;
  		if (yRes < (xRes/model3Ratio))
  			xRes = yRes*model3Ratio;
  		if (xRes < (yRes*model3Ratio))
			yRes = xRes/model3Ratio;
	}
		
	// Center the visible area 
  	*xOffsetPtr = (*xResPtr - (unsigned) xRes)/2;
  	*yOffsetPtr = (*yResPtr - (unsigned) yRes)/2;
  	
  	// If the desired resolution is smaller than what we got, re-center again
  	if (*xResPtr < VideoInfo->current_w)
  		*xOffsetPtr += (VideoInfo->current_w - *xResPtr)/2;
  	if (*yResPtr < VideoInfo->current_h)
  		*yOffsetPtr += (VideoInfo->current_h - *yResPtr)/2;
  	
  	// OpenGL initialization
 	glViewport(0,0,*xResPtr,*yResPtr);
 	glClearColor(0.0,0.0,0.0,0.0);
 	glClearDepth(1.0);
 	glDepthFunc(GL_LESS);
 	glEnable(GL_DEPTH_TEST);
 	glShadeModel(GL_SMOOTH);
 	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
 	glDisable(GL_CULL_FACE);
 	glMatrixMode(GL_PROJECTION);
 	glLoadIdentity();
 	gluPerspective(90.0,(GLfloat)xRes/(GLfloat)yRes,0.1,1e5);
 	glMatrixMode(GL_MODELVIEW);
 	
 	// Clear both buffers to ensure a black border
 	for (int i = 0; i < 2; i++)
 	{
 		glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
 		SDL_GL_SwapBuffers();
 	}
 	
 	// Write back resolution parameters
 	*xResPtr = (unsigned) xRes;
 	*yResPtr = (unsigned) yRes;
 	
 	// Scissor box (to clip visible area)
 	if (!g_Config.wideScreen)
 	{
 		if (VideoInfo->current_w > *xResPtr || VideoInfo->current_h > *yResPtr)
 		{
	 		glEnable(GL_SCISSOR_TEST);
 			glScissor(*xOffsetPtr, *yOffsetPtr, *xResPtr, *yResPtr);
 		}
 	}
	return OKAY;
}

/*
 * CreateGLScreen():
 *
 * Creates an OpenGL display surface of the requested size. xOffset and yOffset
 * are used to return a display surface offset (for OpenGL viewport commands)
 * because the actual drawing area may need to be adjusted to preserve the 
 * Model 3 aspect ratio. The new resolution will be passed back as well -- both
 * the adjusted viewable area resolution and the total resolution.
 *
 * NOTE: keepAspectRatio should always be true. It has not yet been tested with
 * the wide screen hack.
 */
static bool CreateGLScreen(const char *caption, unsigned *xOffsetPtr, unsigned *yOffsetPtr, unsigned *xResPtr, unsigned *yResPtr, unsigned *totalXResPtr, unsigned *totalYResPtr, bool keepAspectRatio, bool fullScreen)
{
	GLenum err;
	
	// Initialize video subsystem
	if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0)
    	return ErrorLog("Unable to initialize SDL video subsystem: %s\n", SDL_GetError());
    
    // Important GL attributes
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE,5);	// need at least RGB555 for Model 3 textures
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE,5);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE,5);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE,16);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,1);

  	// Set vsync
	SDL_GL_SetAttribute(SDL_GL_SWAP_CONTROL, (g_Config.vsync ? 1 : 0));

	// Set video mode
  	if (SDL_SetVideoMode(*xResPtr,*yResPtr,0,SDL_OPENGL|(fullScreen?SDL_FULLSCREEN|SDL_HWSURFACE:0)) == NULL)
  	{
    	ErrorLog("Unable to create an OpenGL display: %s\n", SDL_GetError());
    	return FAIL;
  	}
  	
  	// Create window caption
  	SDL_WM_SetCaption(caption,NULL);
  	
  	// Initialize GLEW, allowing us to use features beyond OpenGL 1.2
	err = glewInit();
	if (GLEW_OK != err)
	{
		ErrorLog("OpenGL initialization failed: %s\n", glewGetErrorString(err));
		return FAIL;
	}
  	
	return SetGLGeometry(xOffsetPtr, yOffsetPtr, xResPtr, yResPtr, totalXResPtr, totalYResPtr, keepAspectRatio);
}

static bool ResizeGLScreen(unsigned *xOffsetPtr, unsigned *yOffsetPtr, unsigned *xResPtr, unsigned *yResPtr, unsigned *totalXResPtr, unsigned *totalYResPtr, bool keepAspectRatio, bool fullScreen)
{
	const SDL_VideoInfo	*VideoInfo;
	GLenum				err;
	float				model3Ratio, ratio;
	float				xRes, yRes;
	
	// Set video mode
  	if (SDL_SetVideoMode(*xResPtr,*yResPtr,0,SDL_OPENGL|(fullScreen?SDL_FULLSCREEN|SDL_HWSURFACE:0)) == NULL)
  	{
    	ErrorLog("Unable to create an OpenGL display: %s\n", SDL_GetError());
    	return FAIL;
  	}
  	
  	return SetGLGeometry(xOffsetPtr, yOffsetPtr, xResPtr, yResPtr, totalXResPtr, totalYResPtr, keepAspectRatio);
}

/*
 * PrintGLInfo():
 *
 * Queries and prints OpenGL information. A full list of extensions can
 * optionally be printed.
 */
static void PrintGLInfo(bool createScreen, bool infoLog, bool printExtensions)
{
	const GLubyte	*str;
	char			*strLocal;
	GLint			value;
	unsigned		xOffset, yOffset, xRes=496, yRes=384, totalXRes, totalYRes;
	
	if (createScreen)
	{
		if (OKAY != CreateGLScreen("Supermodel - Querying OpenGL Information...",&xOffset,&yOffset,&xRes,&yRes,&totalXRes,&totalYRes,false,false))
		{
			ErrorLog("Unable to query OpenGL.\n");
			return;
		}
	}
	
	if (infoLog)	InfoLog("OpenGL information:");
	else			puts("OpenGL information:\n");

	str = glGetString(GL_VENDOR);
	if (infoLog)	InfoLog("                   Vendor: %s", str);
	else			printf("                   Vendor: %s\n", str);
	
	str = glGetString(GL_RENDERER);
	if (infoLog)	InfoLog("                 Renderer: %s", str);
	else			printf("                 Renderer: %s\n", str);
	
	str = glGetString(GL_VERSION);
	if (infoLog)	InfoLog("                  Version: %s", str);
	else			printf("                  Version: %s\n", str);
	
	str = glGetString(GL_SHADING_LANGUAGE_VERSION);
	if (infoLog)	InfoLog(" Shading Language Version: %s", str);
	else			printf(" Shading Language Version: %s\n", str);
	
	glGetIntegerv(GL_MAX_ELEMENTS_VERTICES, &value);
	if (infoLog) 	InfoLog("Maximum Vertex Array Size: %d vertices", value);
	else			printf("Maximum Vertex Array Size: %d vertices\n", value);
	
	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &value);
	if (infoLog)	InfoLog("     Maximum Texture Size: %d texels", value);
	else			printf("     Maximum Texture Size: %d texels\n", value);
	
	glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &value);
	if (infoLog)	InfoLog("Maximum Vertex Attributes: %d", value);
	else			printf("Maximum Vertex Attributes: %d\n", value);
	
	glGetIntegerv(GL_MAX_VERTEX_UNIFORM_COMPONENTS, &value);
	if (infoLog)	InfoLog("  Maximum Vertex Uniforms: %d", value);
	else			printf("  Maximum Vertex Uniforms: %d\n", value);
	
	glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &value);
	if (infoLog)	InfoLog("Maximum Texture Img Units: %d", value);
	else			printf("Maximum Texture Img Units: %d\n", value);

	if (printExtensions)
	{
		str = glGetString(GL_EXTENSIONS);
		strLocal = (char *) malloc((strlen((char *) str)+1)*sizeof(char));
		if (NULL == strLocal)
		{
			if (infoLog)	InfoLog("     Supported Extensions: %s", str);
			else			printf("     Supported Extensions: %s\n", str);
		}
		else
		{
			strcpy(strLocal, (char *) str);
			if (infoLog) 	InfoLog("     Supported Extensions: %s", (strLocal = strtok(strLocal, " \t\n")));
			else			printf("     Supported Extensions: %s\n", (strLocal = strtok(strLocal, " \t\n")));
			while ((strLocal = strtok(NULL, " \t\n")) != NULL)
			{
				if (infoLog) 	InfoLog("                           %s", strLocal);
				else			printf("                           %s\n", strLocal);
			}
		}
	}
	
	if (infoLog)	InfoLog("");
	else			printf("\n");
}

/******************************************************************************
 Configuration
 
 Configuration file management and input settings.
******************************************************************************/

#define CONFIG_FILE_PATH	"Config/Supermodel.ini"
#define CONFIG_FILE_COMMENT	";\n" \
							"; Supermodel Configuration File\n" \
							";\n"

// Create and configure inputs
static bool ConfigureInputs(CInputs *Inputs, bool configure)
{
	// Open and parse configuration file
	CINIFile INI;
	INI.Open(CONFIG_FILE_PATH);	// doesn't matter if it exists or not, will get overwritten
	INI.SetDefaultSectionName("Global");
	INI.Parse();
	
	Inputs->ReadFromINIFile(&INI, "Global");
		
	// If the user wants to configure the inputs, do that now
	if (configure)
	{
		// Open an SDL window 
		unsigned xOffset, yOffset, xRes=496, yRes=384;
		if (OKAY != CreateGLScreen("Supermodel - Configuring Inputs...",&xOffset,&yOffset,&xRes,&yRes,&totalXRes,&totalYRes,false,false))
			return (bool) ErrorLog("Unable to start SDL to configure inputs.\n");
		
		// Configure the inputs
		if (Inputs->ConfigureInputs(NULL, xOffset, yOffset, xRes, yRes))
		{
			// Write input configuration and input system settings to config file
			Inputs->WriteToINIFile(&INI, "Global");
		
			if (OKAY != INI.Write(CONFIG_FILE_COMMENT))
				ErrorLog("Unable to save configuration to '%s'.", CONFIG_FILE_PATH);
			else
				printf("Configuration successfully saved to '%s'.\n", CONFIG_FILE_PATH);
		}
		else
			puts("Configuration aborted...");
		puts("");
	}
	
	INI.Close();
	return OKAY;
}

// Apply configuration settings from configuration file (does NOT read input settings; see ConfigureInputs())
static void ApplySettings(CINIFile *INI, const char *section)
{
	unsigned	x;
	int			y;
	string		String;
	
	// Model 3
	INI->Get(section, "MultiThreaded", g_Config.multiThreaded);
	INI->Get(section, "GPUMultiThreaded", g_Config.gpuMultiThreaded);
	if (OKAY == INI->Get(section, "PowerPCFrequency", x))
		g_Config.SetPowerPCFrequency(x);
	
	// 3D renderer
	INI->Get(section, "VertexShader", g_Config.vertexShaderFile);
	INI->Get(section, "FragmentShader", g_Config.fragmentShaderFile);
	
	// SCSP and DSB
	if (OKAY == INI->Get(section, "SoundVolume", x))
		g_Config.SetSoundVolume(x);
	if (OKAY == INI->Get(section, "MusicVolume", x))
		g_Config.SetMusicVolume(x);
	if (OKAY == INI->Get(section, "Balance", y))
		g_Config.SetSCSPBalance(y);
	INI->Get(section, "EmulateSound", g_Config.emulateSound);
	INI->Get(section, "EmulateDSB", g_Config.emulateDSB);

	// Drive board
#ifdef SUPERMODEL_WIN32
	INI->Get(section, "ForceFeedback", g_Config.forceFeedback);
#endif // SUPERMODEL_WIN32
	
	// OSD
	INI->Get(section, "XResolution", g_Config.xRes);
	INI->Get(section, "YResolution", g_Config.yRes);
	INI->Get(section, "FullScreen", g_Config.fullScreen);
	INI->Get(section, "WideScreen", g_Config.wideScreen);
	INI->Get(section, "MultiTexture", g_Config.multiTexture);
	INI->Get(section, "VSync", g_Config.vsync);
	INI->Get(section, "Throttle", g_Config.throttle);
	INI->Get(section, "ShowFrameRate", g_Config.showFPS);
	INI->Get(section, "Crosshairs", g_Config.crosshairs);
	INI->Get(section, "FlipStereo", g_Config.flipStereo);

#ifdef SUPERMODEL_WIN32
	// DirectInput ForceFeedback
	INI->Get(section, "DirectInputConstForceMax", g_Config.dInputConstForceLeftMax);
	INI->Get(section, "DirectInputConstForceMax", g_Config.dInputConstForceRightMax);
	INI->Get(section, "DirectInputConstForceLeftMax", g_Config.dInputConstForceLeftMax);
	INI->Get(section, "DirectInputConstForceRightMax", g_Config.dInputConstForceRightMax);
	INI->Get(section, "DirectInputSelfCenterMax", g_Config.dInputSelfCenterMax);
	INI->Get(section, "DirectInputFrictionMax", g_Config.dInputFrictionMax);
	INI->Get(section, "DirectInputVibrateMax", g_Config.dInputVibrateMax);

	// XInput ForceFeedback
	INI->Get(section, "XInputConstForceThreshold", g_Config.xInputConstForceThreshold);
	INI->Get(section, "XInputConstForceMax", g_Config.xInputConstForceMax);
	INI->Get(section, "XInputVibrateMax", g_Config.xInputVibrateMax);
#endif // SUPERMODEL_WIN32
}

// Read settings (from a specific section) from the config file
static void ReadConfigFile(const char *section)
{
	CINIFile	INI;	
	
	INI.Open(CONFIG_FILE_PATH);
	INI.SetDefaultSectionName("Global");	// required to read settings not associated with a specific section
	INI.Parse();
	ApplySettings(&INI, section);
	INI.Close();
}

// Log the configuration to info log
static void LogConfig(void)
{
	InfoLog("Program settings:");
	
	// COSDConfig
	InfoLog("\tXResolution                   = %d", g_Config.xRes);
	InfoLog("\tYResolution                   = %d", g_Config.yRes);
	InfoLog("\tFullScreen                    = %d", g_Config.fullScreen);
	InfoLog("\tWideScreen                    = %d", g_Config.wideScreen);
	InfoLog("\tVSync                         = %d", g_Config.vsync);
	InfoLog("\tMultiTexture                  = %d", g_Config.multiTexture);
	InfoLog("\tThrottle                      = %d", g_Config.throttle);
	InfoLog("\tShowFrameRate                 = %d", g_Config.showFPS);
	InfoLog("\tCrosshairs                    = %d", g_Config.crosshairs);
#ifdef SUPERMODEL_DEBUGGER
	InfoLog("\tDisableDebugger               = %d", g_Config.disableDebugger);
#endif
	InfoLog("\tInputSystem                   = %s", g_Config.GetInputSystem());
	InfoLog("\tOutputs                       = %s", g_Config.GetOutputs());
	InfoLog("\tFlipStereo                    = %d", g_Config.flipStereo);
	
#ifdef SUPERMODEL_WIN32
	// DirectInput ForceFeedback
	InfoLog("\tDirectInputConstForceLeftMax  = %u", g_Config.dInputConstForceLeftMax);
	InfoLog("\tDirectInputConstForceRightMax = %u", g_Config.dInputConstForceRightMax);
	InfoLog("\tDirectInputSelfCenterMax      = %u", g_Config.dInputSelfCenterMax);
	InfoLog("\tDirectInputFrictionMax        = %u", g_Config.dInputFrictionMax);
	InfoLog("\tDirectInputVibrateMax         = %u", g_Config.dInputVibrateMax);

	// XInput ForceFeedback
	InfoLog("\tXInputConstForceThreshold     = %u", g_Config.xInputConstForceThreshold);
	InfoLog("\tXInputConstForceMax           = %u", g_Config.xInputConstForceMax);
	InfoLog("\tXInputVibrateMax              = %u", g_Config.xInputVibrateMax);
#endif // SUPERMODEL_WIN32

	// CModel3Config
	InfoLog("\tMultiThreaded                 = %d", g_Config.multiThreaded);
	InfoLog("\tGPUMultiThreaded              = %d", g_Config.gpuMultiThreaded);
	InfoLog("\tPowerPCFrequency              = %d", g_Config.GetPowerPCFrequency());
	
	// CSoundBoardConfig
	InfoLog("\tEmulateSound                  = %d", g_Config.emulateSound);
	InfoLog("\tBalance                       = %d", g_Config.GetSCSPBalance());
	
	// CDSBConfig
	InfoLog("\tEmulateDSB                    = %d", g_Config.emulateDSB);
	InfoLog("\tSoundVolume                   = %d", g_Config.GetSoundVolume());
	InfoLog("\tMusicVolume                   = %d", g_Config.GetMusicVolume());
	
	// CDriveBoardConfig
#ifdef SUPERMODEL_WIN32
	InfoLog("\tForceFeedback                 = %d", g_Config.forceFeedback);
#endif

	// CRender3DConfig
	InfoLog("\tVertexShader                  = %s", g_Config.vertexShaderFile.c_str());
	InfoLog("\tFragmentShader                = %s", g_Config.fragmentShaderFile.c_str());

	InfoLog("");
}


/******************************************************************************
 Save States and NVRAM
 
 Save states and NVRAM use the same basic format. When anything changes that
 breaks compatibility with previous versions of Supermodel, the save state
 and NVRAM version numbers must be incremented as needed.
 
 Header block name: "Supermodel Save State" or "Supermodel NVRAM State"
 Data: Save state file version (4-byte integer), ROM set ID (up to 9 bytes, 
 including terminating \0).
 
 Different subsystems output their own blocks.
******************************************************************************/

#define STATE_FILE_VERSION	1	// save state file version
#define NVRAM_FILE_VERSION	0	// NVRAM file version

static unsigned	saveSlot = 0;	// save state slot #

static void SaveState(CModel3 *Model3)
{
	CBlockFile	SaveState;
	char		filePath[24];
	int			fileVersion = STATE_FILE_VERSION;
	
	sprintf(filePath, "Saves/%s.st%d", Model3->GetGameInfo()->id, saveSlot);
	if (OKAY != SaveState.Create(filePath, "Supermodel Save State", "Supermodel Version " SUPERMODEL_VERSION))
	{
		ErrorLog("Unable to save state to '%s'.", filePath);
		return;
	}
	
	// Write file format version and ROM set ID to header block 
	SaveState.Write(&fileVersion, sizeof(fileVersion));
	SaveState.Write(Model3->GetGameInfo()->id, strlen(Model3->GetGameInfo()->id)+1);
	
	// Save state
	Model3->SaveState(&SaveState);
	SaveState.Close();
	printf("Saved state to '%s'.\n", filePath);
	DebugLog("Saved state to '%s'.\n", filePath);
}

static void LoadState(CModel3 *Model3)
{
	CBlockFile	SaveState;
	char		filePath[24];
	int			fileVersion;
	
	// Generate file path
	sprintf(filePath, "Saves/%s.st%d", Model3->GetGameInfo()->id, saveSlot);
	
	// Open and check to make sure format is correct
	if (OKAY != SaveState.Load(filePath))
	{
		ErrorLog("Unable to load state from '%s'.", filePath);
		return;
	}
	
	if (OKAY != SaveState.FindBlock("Supermodel Save State"))
	{
		ErrorLog("'%s' does not appear to be a valid save state file.", filePath);
		return;
	}
	
	SaveState.Read(&fileVersion, sizeof(fileVersion));
	if (fileVersion != STATE_FILE_VERSION)
	{
		ErrorLog("'%s' is incompatible with this version of Supermodel.", filePath);
		return;
	}
	
	// Load
	Model3->LoadState(&SaveState);
	SaveState.Close();
	printf("Loaded state from '%s'.\n", filePath);
	DebugLog("Loaded state from '%s'.\n", filePath);
}

static void SaveNVRAM(CModel3 *Model3)
{
	CBlockFile	NVRAM;
	char		filePath[24];
	int			fileVersion = NVRAM_FILE_VERSION;
	
	sprintf(filePath, "NVRAM/%s.nv", Model3->GetGameInfo()->id);
	if (OKAY != NVRAM.Create(filePath, "Supermodel NVRAM State", "Supermodel Version " SUPERMODEL_VERSION))
	{
		ErrorLog("Unable to save NVRAM to '%s'. Make sure directory exists!", filePath);
		return;
	}
	
	// Write file format version and ROM set ID to header block 
	NVRAM.Write(&fileVersion, sizeof(fileVersion));
	NVRAM.Write(Model3->GetGameInfo()->id, strlen(Model3->GetGameInfo()->id)+1);
	
	// Save NVRAM
	Model3->SaveNVRAM(&NVRAM);
	NVRAM.Close();
	DebugLog("Saved NVRAM to '%s'.\n", filePath);
}

static void LoadNVRAM(CModel3 *Model3)
{
	CBlockFile	NVRAM;
	char		filePath[24];
	int			fileVersion;
	
	// Generate file path
	sprintf(filePath, "NVRAM/%s.nv", Model3->GetGameInfo()->id);
	
	// Open and check to make sure format is correct
	if (OKAY != NVRAM.Load(filePath))
	{
		//ErrorLog("Unable to restore NVRAM from '%s'.", filePath);
		return;
	}
	
	if (OKAY != NVRAM.FindBlock("Supermodel NVRAM State"))
	{
		ErrorLog("'%s' does not appear to be a valid NVRAM file.", filePath);
		return;
	}
	
	NVRAM.Read(&fileVersion, sizeof(fileVersion));
	if (fileVersion != NVRAM_FILE_VERSION)
	{
		ErrorLog("'%s' is incompatible with this version of Supermodel.", filePath);
		return;
	}
	
	// Load
	Model3->LoadNVRAM(&NVRAM);
	NVRAM.Close();
	DebugLog("Loaded NVRAM from '%s'.\n", filePath);
}


/******************************************************************************
 UI Rendering
 
 Currently, only does crosshairs for light gun games.
******************************************************************************/

static void GunToViewCoords(float *x, float *y)
{
	*x = (*x-150.0f)/(651.0f-150.0f);	// Scale [150,651] -> [0.0,1.0]
	*y = (*y-80.0f)/(465.0f-80.0f);		// Scale [80,465] -> [0.0,1.0]
}

static void DrawCrosshair(float x, float y, float r, float g, float b)
{
	float	base = 0.01f, height = 0.02f;	// geometric parameters of each triangle
	float	dist = 0.004f;					// distance of triangle tip from center
	float	a = (float)xRes/(float)yRes;	// aspect ratio (to square the crosshair)
	
	glColor3f(r, g, b);
	glVertex2f(x, y+dist);	// bottom triangle
	glVertex2f(x+base/2.0f, y+(dist+height)*a);
	glVertex2f(x-base/2.0f, y+(dist+height)*a);	
	glVertex2f(x, y-dist);	// top triangle
	glVertex2f(x-base/2.0f, y-(dist+height)*a);
	glVertex2f(x+base/2.0f, y-(dist+height)*a);
	glVertex2f(x-dist, y);	// left triangle
	glVertex2f(x-dist-height, y+(base/2.0f)*a);
	glVertex2f(x-dist-height, y-(base/2.0f)*a);
	glVertex2f(x+dist, y);	// right triangle
	glVertex2f(x+dist+height, y-(base/2.0f)*a);
	glVertex2f(x+dist+height, y+(base/2.0f)*a);
}

static void PrintGLError(GLenum error)
{
	switch (error)
	{
	case GL_INVALID_ENUM:		printf("invalid enum\n"); break;
	case GL_INVALID_VALUE:		printf("invalid value\n"); break;
	case GL_INVALID_OPERATION:	printf("invalid operation\n"); break;
	case GL_STACK_OVERFLOW:		printf("stack overflow\n"); break;
	case GL_STACK_UNDERFLOW:	printf("stack underflow\n"); break;
	case GL_OUT_OF_MEMORY:		printf("out of memory\n"); break;
	case GL_TABLE_TOO_LARGE:	printf("table too large\n"); break;
	case GL_NO_ERROR:			break;
	default:					printf("unknown error\n"); break;
	}
}

static void UpdateCrosshairs(CInputs *Inputs, unsigned crosshairs)
{
	float	x[2], y[2];
	
	crosshairs &= 3;
	if (!crosshairs)
		return;
		
	// Set up the viewport and orthogonal projection
	glUseProgram(0);		// no shaders
	glViewport(xOffset, yOffset, xRes, yRes);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
    gluOrtho2D(0.0, 1.0, 1.0, 0.0);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glDisable(GL_TEXTURE_2D);	// no texture mapping
	glDisable(GL_BLEND);		// no blending
	glDisable(GL_DEPTH_TEST);	// no Z-buffering needed	
	glDisable(GL_LIGHTING);
	
	// Convert gun coordinates to viewspace coordinates
	x[0] = (float) Inputs->gunX[0]->value;
	y[0] = (float) Inputs->gunY[0]->value;
	x[1] = (float) Inputs->gunX[1]->value;
	y[1] = (float) Inputs->gunY[1]->value;
	GunToViewCoords(&x[0], &y[0]);
	GunToViewCoords(&x[1], &y[1]);
	
	// Draw visible crosshairs	
	glBegin(GL_TRIANGLES);
	if ((crosshairs & 1) && !Inputs->trigger[0]->offscreenValue)	// Player 1
		DrawCrosshair(x[0], y[0], 1.0f, 0.0f, 0.0f);
	if ((crosshairs & 2) && !Inputs->trigger[1]->offscreenValue)	// Player 2
		DrawCrosshair(x[1], y[1], 0.0f, 1.0f, 0.0f);
	glEnd();
	
	//PrintGLError(glGetError());
}

	
/******************************************************************************
 Video Callbacks
******************************************************************************/

static CInputs *videoInputs = NULL;

bool BeginFrameVideo()
{
	return true;
}

void EndFrameVideo()
{
	// Show crosshairs for light gun games
	if (videoInputs)
		UpdateCrosshairs(videoInputs, g_Config.crosshairs);

	// Swap the buffers
	SDL_GL_SwapBuffers();
}


/******************************************************************************
 Main Program Loop
******************************************************************************/

#ifdef SUPERMODEL_DEBUGGER
int Supermodel(const char *zipFile, CModel3 *Model3, CInputs *Inputs, COutputs *Outputs, Debugger::CDebugger *Debugger, CINIFile *CmdLine)
{
	CLogger *oldLogger;
#else
int Supermodel(const char *zipFile, CInputs *Inputs, COutputs *Outputs, CINIFile *CmdLine)
{				  
	CModel3			*Model3 = new CModel3();
#endif // SUPERMODEL_DEBUGGER
	char			baseTitleStr[128], titleStr[128];
	CRender2D		*Render2D = new CRender2D();
	CRender3D		*Render3D = new CRender3D();
	unsigned		prevFPSTicks, currentFPSTicks, currentTicks, targetTicks, startTicks;
	unsigned		fpsFramesElapsed, framesElapsed;
	bool			gameHasLightguns = false;
	bool			quit = false;
	bool            paused = false;
	bool            dumpTimings = false;

	// Initialize and load ROMs
	if (OKAY != Model3->Init())
		return 1;
	if (OKAY != Model3->LoadROMSet(g_Model3GameList, zipFile))
		return 1;
		
	// Apply game-specific settings and then, lastly, command line settings
	ReadConfigFile(Model3->GetGameInfo()->id);
	ApplySettings(CmdLine, "Global");
		
	// Load NVRAM
	LoadNVRAM(Model3);
		
	// Start up SDL and open a GL window
  	totalXRes = xRes = g_Config.xRes;
  	totalYRes = yRes = g_Config.yRes;
  	sprintf(baseTitleStr, "Supermodel - %s", Model3->GetGameInfo()->title);
	if (OKAY != CreateGLScreen(baseTitleStr,&xOffset,&yOffset,&xRes,&yRes,&totalXRes,&totalYRes,true,g_Config.fullScreen))
		return 1;
		
	// Info log GL information and user options
	PrintGLInfo(false, true, false);
	LogConfig();
	
	// Initialize audio system
	if (OKAY != OpenAudio())
		return 1;

	// Hide mouse if fullscreen, enable crosshairs for gun games
	Inputs->GetInputSystem()->SetMouseVisibility(!g_Config.fullScreen);
	gameHasLightguns = !!(Model3->GetGameInfo()->inputFlags & (GAME_INPUT_GUN1|GAME_INPUT_GUN2));
	if (gameHasLightguns)
		videoInputs = Inputs;
	else
		videoInputs = NULL;

	// Attach the inputs to the emulator
	Model3->AttachInputs(Inputs);

	// Attach the outputs to the emulator
	if (Outputs != NULL)
		Model3->AttachOutputs(Outputs);
	
	// Initialize the renderer
	if (OKAY != Render2D->Init(xOffset, yOffset, xRes, yRes, totalXRes, totalYRes))
		goto QuitError;
	if (OKAY != Render3D->Init(xOffset, yOffset, xRes, yRes, totalXRes, totalYRes))
		goto QuitError;
	Model3->AttachRenderers(Render2D,Render3D);

	// Reset emulator
	Model3->Reset();
	
#ifdef SUPERMODEL_DEBUGGER
	// If debugger was supplied, set it as logger and attach it to system
	oldLogger = GetLogger();
	if (Debugger != NULL)
	{
		SetLogger(Debugger);
		Debugger->Attach();
	}
#endif // SUPERMODEL_DEBUGGER

	// Emulate!
	fpsFramesElapsed = 0;
	framesElapsed = 0;
	prevFPSTicks = SDL_GetTicks();
	startTicks = prevFPSTicks;
	while (!quit)
	{
		// Check if paused
		if (!paused)
		{
			// If not, run one frame
			Model3->RunFrame();
		}
		
		// Poll the inputs
		if (!Inputs->Poll(Model3->GetGameInfo(), xOffset, yOffset, xRes, yRes))
			quit = true;
		
#ifdef SUPERMODEL_DEBUGGER
		bool processUI = true;
		if (Debugger != NULL)
		{
			Debugger->Poll();

			// Check if debugger requests exit or pause
			if (Debugger->CheckExit())
			{
				quit = true;
				processUI = false;
			}
			else if (Debugger->CheckPause())	
			{
				paused = true;
				processUI = false;
			}
		}
		if (processUI)
		{
#endif // SUPERMODEL_DEBUGGER

		// Check UI controls
		if (Inputs->uiExit->Pressed())
 		{
			// Quit emulator
 			quit = true; 			
		}
		else if (Inputs->uiReset->Pressed())
 		{
			if (!paused)
			{
				Model3->PauseThreads();
				SetAudioEnabled(false);
			}

			// Reset emulator
			Model3->Reset();
			
#ifdef SUPERMODEL_DEBUGGER
			// If debugger was supplied, reset it too
			if (Debugger != NULL)
				Debugger->Reset();
#endif // SUPERMODEL_DEBUGGER

			if (!paused)
			{
				Model3->ResumeThreads();
				SetAudioEnabled(true);
			}

			puts("Model 3 reset.");
		}
		else if (Inputs->uiPause->Pressed())
		{
			// Toggle emulator paused flag
			paused = !paused;

			if (paused)
			{
				Model3->PauseThreads();
				SetAudioEnabled(false);
				sprintf(titleStr, "%s (Paused)", baseTitleStr);
				SDL_WM_SetCaption(titleStr,NULL);
			}
			else
			{
				Model3->ResumeThreads();
				SetAudioEnabled(true);
				SDL_WM_SetCaption(baseTitleStr,NULL);
			}
			
			// Send paused value as output
			if (Outputs != NULL)
				Outputs->SetValue(OutputPause, paused);
		}
		else if (Inputs->uiFullScreen->Pressed())
		{
			// Toggle emulator fullscreen
			g_Config.fullScreen = !g_Config.fullScreen;

			// Delete renderers and recreate them afterwards since GL context will most likely be lost when switching from/to fullscreen
			delete Render2D;
			delete Render3D;
			Render2D = NULL;
			Render3D = NULL;
			
			// Resize screen
			totalXRes = xRes = g_Config.xRes;
  			totalYRes = yRes = g_Config.yRes;
			if (OKAY != ResizeGLScreen(&xOffset,&yOffset,&xRes,&yRes,&totalXRes,&totalYRes,true,g_Config.fullScreen))
				goto QuitError;

			// Recreate renderers and attach to the emulator
			Render2D = new CRender2D();
			Render3D = new CRender3D();
			if (OKAY != Render2D->Init(xOffset, yOffset, xRes, yRes, totalXRes, totalYRes))
				goto QuitError;
			if (OKAY != Render3D->Init(xOffset, yOffset, xRes, yRes, totalXRes, totalYRes))
				goto QuitError;
			Model3->AttachRenderers(Render2D,Render3D);
		
			Inputs->GetInputSystem()->SetMouseVisibility(!g_Config.fullScreen);	
		}
		else if (Inputs->uiSaveState->Pressed())
		{
			if (!paused)
			{
				Model3->PauseThreads();
				SetAudioEnabled(false);
			}

			// Save game state
 			SaveState(Model3);

			if (!paused)
			{
				Model3->ResumeThreads();
				SetAudioEnabled(true);
			}
		}
		else if (Inputs->uiChangeSlot->Pressed())
		{
			// Change save slot
 			++saveSlot;
 			saveSlot %= 10;	// clamp to [0,9]
 			printf("Save slot: %d\n", saveSlot);
		}
		else if (Inputs->uiLoadState->Pressed())
		{
			if (!paused)
			{
				Model3->PauseThreads();
				SetAudioEnabled(false);
			}

			// Load game state
			LoadState(Model3);
						
#ifdef SUPERMODEL_DEBUGGER
			// If debugger was supplied, reset it after loading state
			if (Debugger != NULL)
				Debugger->Reset();
#endif // SUPERMODEL_DEBUGGER

			if (!paused)
			{
				Model3->ResumeThreads();
				SetAudioEnabled(true);
			}
		}
		else if (Inputs->uiMusicVolUp->Pressed())
		{
			// Increase music volume by 10%
			if (Model3->GetGameInfo()->mpegBoard)
			{
				int vol = (int) g_Config.GetMusicVolume() + 10;
				if (vol > 200)
					vol = 200;
				g_Config.SetMusicVolume(vol);
				printf("Music volume: %d%%", vol);
				if (200 == vol)
					puts(" (maximum)");
				else
					printf("\n");
			}
			else
				puts("This game does not have an MPEG music board.");
		}
		else if (Inputs->uiMusicVolDown->Pressed())
		{
			// Decrease music volume by 10%
			if (Model3->GetGameInfo()->mpegBoard)
			{
				int vol = (int) g_Config.GetMusicVolume() - 10;
				if (vol < 0)
					vol = 0;
				g_Config.SetMusicVolume(vol);
				printf("Music volume: %d%%", vol);
				if (0 == vol)
					puts(" (muted)");
				else
					printf("\n");
			}	
			else
				puts("This game does not have an MPEG music board.");
		}
		else if (Inputs->uiSoundVolUp->Pressed())
		{
			// Increase sound volume by 10%
			int vol = (int) g_Config.GetSoundVolume() + 10;
			if (vol > 200)
				vol = 200;
			g_Config.SetSoundVolume(vol);
			printf("Sound volume: %d%%", vol);
			if (200 == vol)
				puts(" (maximum)");
			else
				printf("\n");
		}
		else if (Inputs->uiSoundVolDown->Pressed())
		{
			// Decrease sound volume by 10%
			int vol = (int) g_Config.GetSoundVolume() - 10;
			if (vol < 0)
				vol = 0;
			g_Config.SetSoundVolume(vol);
			printf("Sound volume: %d%%", vol);
			if (0 == vol)
				puts(" (muted)");
			else
				printf("\n");
		}
		else if (Inputs->uiDumpInpState->Pressed())
		{
			// Dump input states
			Inputs->DumpState(Model3->GetGameInfo());
		}
		else if (Inputs->uiDumpTimings->Pressed())
		{
			dumpTimings = !dumpTimings;
		}
		else if (Inputs->uiSelectCrosshairs->Pressed() && gameHasLightguns)
 		{
			g_Config.crosshairs++;
			switch (g_Config.crosshairs&3)
			{
			case 0:	puts("Crosshairs disabled."); 				break;
			case 3:	puts("Crosshairs enabled.");				break;
			case 1:	puts("Showing Player 1 crosshair only.");	break;
			case 2: puts("Showing Player 2 crosshair only.");	break;
			}
 		}
		else if (Inputs->uiClearNVRAM->Pressed())
 		{
			// Clear NVRAM
 			Model3->ClearNVRAM();
 			puts("NVRAM cleared.");
 		}
		else if (Inputs->uiToggleFrLimit->Pressed())
 		{
			// Toggle frame limiting
 			g_Config.throttle = !g_Config.throttle;
 			printf("Frame limiting: %s\n", g_Config.throttle?"On":"Off");
 		}
#ifdef SUPERMODEL_DEBUGGER
			else if (Debugger != NULL && Inputs->uiEnterDebugger->Pressed())
			{
				// Break execution and enter debugger
				Debugger->ForceBreak(true);
			}
		}
#endif // SUPERMODEL_DEBUGGER
 		
 		// FPS and frame rate
 		currentFPSTicks = SDL_GetTicks();
 		currentTicks = currentFPSTicks;
 		
 		// FPS
 		if (g_Config.showFPS)
 		{
 			++fpsFramesElapsed;
			if((currentFPSTicks-prevFPSTicks) >= 1000)	// update FPS every 1 second (each tick is 1 ms)
			{
				sprintf(titleStr, "%s - %1.1f FPS%s", baseTitleStr, (float)fpsFramesElapsed/((float)(currentFPSTicks-prevFPSTicks)/1000.0f), paused ? " (Paused)" : "");
				SDL_WM_SetCaption(titleStr,NULL);
				prevFPSTicks = currentFPSTicks;			// reset tick count
				fpsFramesElapsed = 0;					// reset frame count
			}
		}
		
		// Frame limiting/paused
		if (paused || g_Config.throttle)
		{
			++framesElapsed;
			targetTicks = startTicks + (unsigned) ((float)framesElapsed * 1000.0f/60.0f);
			if (currentTicks <= targetTicks)	// add a delay until we reach the next (target) frame time
				SDL_Delay(targetTicks-currentTicks);
			else								// begin a new frame
			{
				framesElapsed = 0;
				startTicks = currentTicks;
			}
		}

		if (dumpTimings && !paused)
			Model3->DumpTimings();
	}

	// Make sure all threads are paused before shutting down
	Model3->PauseThreads();		
	
#ifdef SUPERMODEL_DEBUGGER
	// If debugger was supplied, detach it from system and restore old logger
	if (Debugger != NULL)
	{
		Debugger->Detach();
		SetLogger(oldLogger);
	}
#endif // SUPERMODEL_DEBUGGER
	
	// Save NVRAM
	SaveNVRAM(Model3);
	
	// Close audio
	CloseAudio();

	// Shut down
#ifndef SUPERMODEL_DEBUGGER
	delete Model3;
#endif // SUPERMODEL_DEBUGGER
	delete Render2D;
	delete Render3D;
	
	// Dump PowerPC registers
#ifdef DEBUG
	for (int i = 0; i < 32; i += 4)
        printf("R%d=%08X\tR%d=%08X\tR%d=%08X\tR%d=%08X\n",
        i + 0, ppc_get_gpr(i + 0),
        i + 1, ppc_get_gpr(i + 1),
        i + 2, ppc_get_gpr(i + 2),
        i + 3, ppc_get_gpr(i + 3));
	printf("PC =%08X\n", ppc_get_pc());
  	printf("LR =%08X\n", ppc_get_lr());
	/*
	printf("DBAT0U=%08X\tIBAT0U=%08X\n", ppc_read_spr(SPR603E_DBAT0U), ppc_read_spr(SPR603E_IBAT0U));
	printf("DBAT0L=%08X\tIBAT0L=%08X\n", ppc_read_spr(SPR603E_DBAT0L), ppc_read_spr(SPR603E_IBAT0L));
	printf("DBAT1U=%08X\tIBAT1U=%08X\n", ppc_read_spr(SPR603E_DBAT1U), ppc_read_spr(SPR603E_IBAT1U));
	printf("DBAT1L=%08X\tIBAT1L=%08X\n", ppc_read_spr(SPR603E_DBAT1L), ppc_read_spr(SPR603E_IBAT1L));
	printf("DBAT2U=%08X\tIBAT2U=%08X\n", ppc_read_spr(SPR603E_DBAT2U), ppc_read_spr(SPR603E_IBAT2U));
	printf("DBAT2L=%08X\tIBAT2L=%08X\n", ppc_read_spr(SPR603E_DBAT2L), ppc_read_spr(SPR603E_IBAT2L));
	printf("DBAT3U=%08X\tIBAT3U=%08X\n", ppc_read_spr(SPR603E_DBAT3U), ppc_read_spr(SPR603E_IBAT3U));
	printf("DBAT3L=%08X\tIBAT3L=%08X\n", ppc_read_spr(SPR603E_DBAT3L), ppc_read_spr(SPR603E_IBAT3L));
	for (int i = 0; i < 10; i++)
		printf("SR%d =%08X\n", i, ppc_read_sr(i));
	for (int i = 10; i < 16; i++)
		printf("SR%d=%08X\n", i, ppc_read_sr(i));
	printf("SDR1=%08X\n", ppc_read_spr(SPR603E_SDR1));
	*/
#endif
	
	return 0;

	// Quit with an error
QuitError:
#ifndef SUPERMODEL_DEBUGGER
	delete Model3;
#endif // SUPERMODEL_DEBUGGER
	delete Render2D;
	delete Render3D;
	return 1;
}


/******************************************************************************
 CROM Disassembler
******************************************************************************/

// Disassemble instructions from CROM
static int DisassembleCROM(const char *zipFile, UINT32 addr, unsigned n)
{
	const struct GameInfo	*Game;
	UINT8					*crom;
	struct ROMMap Map[] =
	{
		{ "CROM", 	NULL },
		{ "CROMxx",	NULL },
		{ NULL, 	NULL }
	};
	char	mnem[16], oprs[48];
	UINT32	op;
	
	// Do we have a valid CROM address?
	if (addr < 0xFF800000)
		return ErrorLog("Valid CROM address range is FF800000-FFFFFFFF.");
		
	// Allocate memory and set ROM region
	crom = new(std::nothrow) UINT8[0x8800000];
	if (NULL == crom)
		return ErrorLog("Insufficient memory to load CROM (need %d MB).", (0x8800000/8));
	Map[0].ptr = crom;
	Map[1].ptr = &crom[0x800000];
	
	// Load ROM set
	Game = LoadROMSetFromZIPFile(Map, g_Model3GameList, zipFile, false);
	if (NULL == Game)
		return ErrorLog("Failed to load ROM set.");	
		
	// Mirror CROM if necessary
	if (Game->cromSize < 0x800000)	// high part of fixed CROM region contains CROM0
		CopyRegion(crom, 0, 0x800000-0x200000, &crom[0x800000], 0x800000);
		
	// Disassemble!
	addr -= 0xFF800000;
	while ((n > 0) && ((addr+4) <= 0x800000))
	{
		op = (crom[addr+0]<<24) | (crom[addr+1]<<16) | (crom[addr+2]<<8) | crom[addr+3];
        
        printf("%08X: ", addr+0xFF800000);
        if (DisassemblePowerPC(op, addr+0xFF800000, mnem, oprs, 1))
        {
            if (mnem[0] != '\0')    // invalid form
                printf("%08X %s*\t%s\n", op, mnem, oprs);
            else
                printf("%08X ?\n", op);
        }
        else
            printf("%08X %s\t%s\n", op, mnem, oprs);
		
		addr += 4;
		--n;
	}
	
	delete [] crom;
	return OKAY;
}


/******************************************************************************
 Entry Point and Command Line Procesing
******************************************************************************/

// Print Supermodel title and copyright information
static void Title(void)
{
	puts("Supermodel: A Sega Model 3 Arcade Emulator (Version "SUPERMODEL_VERSION")");
	puts("Copyright (C) 2011-2012 by Bart Trzynadlowski and Nik Henson\n");
}

// Print usage information
static void Help(void)
{
	puts("Usage: Supermodel <romset> [options]");
	puts("ROM set must be a valid ZIP file containing a single game.");
	puts("");
	puts("General Options:");
	puts("    -?, -h                 Print this help text");
	puts("    -print-games           List supported games and quit");
	puts("");
	puts("Core Options:");
	printf("    -ppc-frequency=<freq>  PowerPC frequency in MHz [Default: %d]\n", g_Config.GetPowerPCFrequency());
	puts("    -no-threads            Disable multi-threading entirely");
	puts("    -gpu-multi-threaded    Run graphics rendering in separate thread [Default]");
	puts("    -no-gpu-thread         Run graphics rendering in main thread");
	puts("");
	puts("Video Options:");
	puts("    -res=<x>,<y>           Resolution [Default: 496,384]");
	puts("    -window                Windowed mode [Default]");
	puts("    -fullscreen            Full screen mode");
	puts("    -wide-screen           Expand 3D field of view to screen width");
	puts("    -multi-texture         Use 8 texture maps for decoding");
	puts("    -no-multi-texture      Decode to a single texture map [Default]");
	puts("    -no-throttle           Disable 60 Hz frame rate lock");
	puts("    -show-fps              Display frame rate in window title bar");
	puts("    -crosshairs=<n>        Crosshairs configuration for gun games:");
	puts("                            0=none [Default], 1=P1 only, 2=P2 only, 3=P1 & P2");
	puts("    -vert-shader=<file>    Load 3D vertex shader from external file");
	puts("    -frag-shader=<file>    Load 3D fragment shader from external file");
	puts("    -print-gl-info         Print OpenGL driver information and quit");
	puts("");
	puts("Audio Options:");
	puts("    -sound-volume=<vol>    Volume of sound effects in % [Default: 100]");
	puts("    -music-volume=<vol>    Volume of MPEG music in % [Default: 100]");
	puts("    -balance=<bal>         Relative front/rear balance in % [Default: 0]");
	puts("    -flip-stereo           Swap left and right audio channels");
	puts("    -no-sound              Disable sound board emulation (sound effects)");
	puts("    -no-dsb                Disable Digital Sound Board (MPEG music)");
	puts("");
	puts("Input Options:");
#ifdef SUPERMODEL_WIN32
	puts("    -force-feedback        Enable force feedback (DirectInput, XInput)");
#endif
	puts("    -config-inputs         Configure keyboards, mice, and game controllers");
#ifdef SUPERMODEL_WIN32
	printf("    -input-system=<s>      Input system [Default: %s]\n", g_Config.GetInputSystem());
	printf("    -outputs=<s>           Outputs [Default: %s]\n", g_Config.GetOutputs());
#endif
	puts("    -print-inputs          Prints current input configuration");
	puts("");
#ifdef SUPERMODEL_DEBUGGER
	puts("Debug Options:");
	puts("    -disable-debugger	     Completely disable debugger functionality");
	puts("    -enter-debugger        Enter debugger at start of emulation");
	puts("    -dis=<addr>[,n]        Disassemble PowerPC code from CROM");
	puts("");
#endif // SUPERMODEL_DEBUGGER
}

// Print game list
static void PrintGameList(void)
{
	int	i, j;
	
	puts("Supported games:");
	puts("");
	puts("    ROM Set         Title");
	puts("    -------         -----");
	for (i = 0; g_Model3GameList[i].title != NULL; i++)
	{
		printf("    %s", g_Model3GameList[i].id);
		for (j = strlen(g_Model3GameList[i].id); j < 9; j++)	// pad for alignment (no game ID is more than 9 letters)
			printf(" ");
		printf("       %s\n", g_Model3GameList[i].title);
	}
}

/*
 * main(argc, argv):
 *
 * Program entry point.
 */
int main(int argc, char **argv)
{
	CINIFile	CmdLine;	// not associated with any files, holds command line options
	int			i, ret;
	int			fileIdx=0;
	bool		cmdPrintInputs=false, cmdConfigInputs=false, cmdDis=false;
#ifdef SUPERMODEL_DEBUGGER
	int			cmdEnterDebugger=false;
#endif // SUPERMODEL_DEBUGGER
	char		*inputSystem = NULL;	// use default input system
	char        *outputs = NULL;
	unsigned	n;
	int			m;
	UINT32		addr;

	Title();
	if (argc <= 1)
	{
		Help();
		return 0;
	}

	// Create default logger
	CFileLogger Logger(DEBUG_LOG_FILE, ERROR_LOG_FILE);
	Logger.ClearLogs();
	SetLogger(&Logger);
	
	// Log the command line used to start Supermodel
	InfoLog("Started as:");
	for (i = 0; i < argc; i++)
		InfoLog("\targv[%d] = %s", i, argv[i]);
	InfoLog("");
	
	// Read global settings from INI file
	ReadConfigFile("Global");

	/*
	 * Parse command line. 
	 *
	 * Settings are stored in CmdLine so that they can be applied later, after
	 * game-specific settings are read from the configuration file (which
	 * requires the ROM set to be identified and therefore is done later).
	 *
	 * Some commands are processed here directly.
	 */
	CmdLine.SetDefaultSectionName("Global");	// command line settings are global-level
	for (i = 1; i < argc; i++)
	{
		if (!strcmp(argv[i],"-h") || !strcmp(argv[i],"-?"))
		{
			Help();
			return 0;
		}
		else if (!strcmp(argv[i],"-print-games"))
		{
			PrintGameList();
			return 0;
		}
		else if (!strncmp(argv[i],"-ppc-frequency",14))
		{
			int	f;
			ret = sscanf(&argv[i][14],"=%d",&f);
			if (ret != 1)
				ErrorLog("'-ppc-frequency' requires a frequency.");
			else
				CmdLine.Set("Global", "PowerPCFrequency", f);
		}
		else if (!strcmp(argv[i],"-no-threads"))
		{
			n = 0;
			CmdLine.Set("Global", "MultiThreaded", n);
		}
		else if (!strcmp(argv[i],"-gpu-multi-threaded"))
		{
			n = 1;
			CmdLine.Set("Global", "GPUMultiThreaded", n);
		}
		else if (!strcmp(argv[i],"-no-gpu-thread"))
		{
			n = 0;
			CmdLine.Set("Global", "GPUMultiThreaded", n);
		}
#ifdef SUPERMODEL_DEBUGGER
		else if (!strcmp(argv[i],"-disable-debugger"))
			g_Config.disableDebugger = true;
		else if (!strcmp(argv[i],"-enter-debugger"))
			cmdEnterDebugger = true;
#endif // SUPERMODEL_DEBUGGER
		else if (!strncmp(argv[i],"-sound-volume",13))
		{
			ret = sscanf(&argv[i][13],"=%d",&n);
			if (ret != 1)
				ErrorLog("'-sound-volume' requires a volume setting.");
			else
				CmdLine.Set("Global", "SoundVolume", n);
		}
		else if (!strncmp(argv[i],"-music-volume",13))
		{
			ret = sscanf(&argv[i][13],"=%d",&n);
			if (ret != 1)
				ErrorLog("'-music-volume' requires a volume setting.");
			else
				CmdLine.Set("Global", "MusicVolume", n);
		}
		else if (!strncmp(argv[i],"-balance",8))
		{
			ret = sscanf(&argv[i][8],"=%d",&m);
			if (ret != 1)
				ErrorLog("'-balance' requires a front/rear balance setting.");
			else
				CmdLine.Set("Global", "Balance", m);
		}
		else if (!strcmp(argv[i], "-flip-stereo"))
		{
			n = 1;
			CmdLine.Set("Global", "FlipStereo", n);
		}
		else if (!strcmp(argv[i], "-no-sound"))
		{
			n = 0;
			CmdLine.Set("Global", "EmulateSound", n);
		}
		else if (!strcmp(argv[i], "-no-dsb"))
		{
			n = 0;
			CmdLine.Set("Global", "EmulateDSB", n);
		}
#ifdef SUPERMODEL_WIN32
		else if (!strcmp(argv[i], "-force-feedback"))
		{
			n = 1;
			CmdLine.Set("Global", "ForceFeedback", n);
		}
#endif
		else if (!strncmp(argv[i],"-res",4))
		{
			unsigned	x, y;
			
			ret = sscanf(&argv[i][4],"=%d,%d",&x,&y);
			if (ret != 2)
			{
				ret = sscanf(&argv[i][4],"=%dx%d",&x,&y);
				if (ret != 2)
					ErrorLog("'-res' requires both a width and a height.");
			}
			if (ret == 2)
			{
				CmdLine.Set("Global", "XResolution", x);
				CmdLine.Set("Global", "YResolution", y);
			}
		}
		else if (!strcmp(argv[i],"-window"))
		{
			n = 0;
			CmdLine.Set("Global", "FullScreen", n);
		}
		else if (!strcmp(argv[i],"-fullscreen"))
		{
			n = 1;
			CmdLine.Set("Global", "FullScreen", n);
		}
		else if (!strcmp(argv[i],"-wide-screen"))
		{
			n = 1;
			CmdLine.Set("Global", "WideScreen", n);
		}
		else if (!strcmp(argv[i],"-multi-texture"))
		{
			n = 1;
			CmdLine.Set("Global", "MultiTexture", n);
		}
		else if (!strcmp(argv[i],"-no-multi-texture"))
		{
			n = 0;
			CmdLine.Set("Global", "MultiTexture", n);
		}
		else if (!strcmp(argv[i],"-vsync"))
		{
			n = 1;
			CmdLine.Set("Global", "VSync", n);
		}
		else if (!strcmp(argv[i],"-no-vsync"))
		{
			n = 0;
			CmdLine.Set("Global", "VSync", n);
		}
		else if (!strcmp(argv[i],"-no-throttle"))
		{
			n = 0;
			CmdLine.Set("Global", "Throttle", n);
		}
		else if (!strcmp(argv[i],"-show-fps"))
		{
			n = 1;
			CmdLine.Set("Global", "ShowFrameRate", n);
		}
		else if (!strncmp(argv[i],"-crosshairs",11))
		{
			unsigned	x;
			
			ret = sscanf(&argv[i][11],"=%d",&x);
			if (ret != 1 || x > 3)
				ErrorLog("'-crosshairs' requires a number 0-3");
			else
				CmdLine.Set("Global", "Crosshairs", x);
		}
		else if (!strncmp(argv[i],"-vert-shader",12))
		{
			if (argv[i][12] == '\0')
				ErrorLog("'-vert-shader' requires a file path.");
			else if (argv[i][12] != '=')
				ErrorLog("Ignoring unrecognized option: %s", argv[i]);
			else if (argv[i][13] == '\0')
				ErrorLog("'-vert-shader' requires a file path.");
			else
				CmdLine.Set("Global", "VertexShader", &argv[i][13]);
		}
		else if (!strncmp(argv[i],"-frag-shader",12))
		{
			if (argv[i][12] == '\0')
				ErrorLog("'-frag-shader' requires a file path.");
			else if (argv[i][12] != '=')
				ErrorLog("Ignoring unrecognized option: %s", argv[i]);
			else if (argv[i][13] == '\0')
				ErrorLog("'-frag-shader' requires a file path.");
			else
				CmdLine.Set("Global", "FragmentShader", &argv[i][13]);
		}
#ifdef SUPERMODEL_WIN32
		else if (!strncmp(argv[i],"-input-system", 13))	// this setting is not written to the config file!
		{
			if (argv[i][13] == '\0')
				ErrorLog("'-input-system' requires an input system name.");
			else if (argv[i][13] != '=')
				ErrorLog("Ignoring unrecognized option: %s", argv[i]);
			else if (argv[i][14] == '\0')
				ErrorLog("'-input-system' requires an input system name.");
			else
				inputSystem = &argv[i][14];
		}
		else if (!strncmp(argv[i],"-outputs", 8)) // this setting is not written to the config file!
		{
			if (argv[i][8] == '\0')
				ErrorLog("'-outputs' requires an outputs name.");
			else if (argv[i][8] != '=')
				ErrorLog("Ignoring unrecognized option: %s", argv[i]);
			else if (argv[i][9] == '\0')
				ErrorLog("'-outputs' requires an outputs name.");
			else
				outputs = &argv[i][9];
		}
#endif	// SUPERMODEL_WIN32
		else if (!strcmp(argv[i],"-print-inputs"))
			cmdPrintInputs = true;
		else if (!strcmp(argv[i],"-config-inputs"))
			cmdConfigInputs = true;
		else if (!strncmp(argv[i],"-dis",4))
		{
			ret = sscanf(&argv[i][4],"=%X,%X",&addr,&n);
			if (ret == 1)
			{
				n = 16;
				cmdDis = true;
			}
			else if (ret == 2)
				cmdDis = true;
			else
				ErrorLog("'-dis' requires address and, optionally, number of instructions.");
		}
		else if (!strcmp(argv[i],"-print-gl-info"))
		{
			PrintGLInfo(true, false, false);
			return 0;
		}
		else if (argv[i][0] == '-')
			ErrorLog("Ignoring unrecognized option: %s", argv[i]);
		else
		{
			if (fileIdx)		// already specified a file
 				ErrorLog("Multiple files specified. Using '%s', ignoring '%s'.", argv[fileIdx], argv[i]);
 			else
 				fileIdx = i;
 		}
	}
	
	// Initialize SDL (individual subsystems get initialized later)
	if (SDL_Init(0) != 0)
	{
		ErrorLog("Unable to initialize SDL: %s\n", SDL_GetError());
		return 1;
	}
	
	// Create input system (default is SDL) and debugger
	CInputSystem *InputSystem = NULL;
	CInputs *Inputs = NULL;
	COutputs *Outputs = NULL;
	int exitCode = 0;
#ifdef SUPERMODEL_DEBUGGER
	CModel3 *Model3 = NULL;
	Debugger::CSupermodelDebugger *Debugger = NULL;
#endif // SUPERMODEL_DEBUGGER

	// Create input system
	g_Config.SetInputSystem(inputSystem);
	if (stricmp(g_Config.GetInputSystem(), "sdl") == 0)
		InputSystem = new CSDLInputSystem();
#ifdef SUPERMODEL_WIN32
	else if (stricmp(g_Config.GetInputSystem(), "dinput") == 0)
		InputSystem = new CDirectInputSystem(false, false);
	else if (stricmp(g_Config.GetInputSystem(), "xinput") == 0)
		InputSystem = new CDirectInputSystem(false, true);
	else if (stricmp(g_Config.GetInputSystem(), "rawinput") == 0)
		InputSystem = new CDirectInputSystem(true, false);
#endif // SUPERMODEL_WIN32
	else
	{
		ErrorLog("Unknown input system: %s\n", g_Config.GetInputSystem());
		exitCode = 1;
		goto Exit;
	}

	// Create inputs from input system (configuring them if required)
	Inputs = new CInputs(InputSystem);
	if (!Inputs->Initialize())
	{
		ErrorLog("Unable to initalize inputs.\n");
		exitCode = 1;
		goto Exit;
	}
	
	if (ConfigureInputs(Inputs, cmdConfigInputs))
	{
		exitCode = 1;
		goto Exit;
	}

	if (cmdPrintInputs)
	{
		Inputs->PrintInputs(NULL);
		InputSystem->PrintSettings();
	}

	// Create outputs	
#ifdef SUPERMODEL_WIN32
	g_Config.SetOutputs(outputs);
	if (stricmp(g_Config.GetOutputs(), "none") == 0)
		Outputs = NULL;
	else if (stricmp(g_Config.GetOutputs(), "win") == 0)
		Outputs = new CWinOutputs();
	else
	{
		ErrorLog("Unknown outputs: %s\n", g_Config.GetOutputs());
		exitCode = 1;
		goto Exit;
	}
#endif // SUPERMODEL_WIN32

	// Initialize outputs
	if (Outputs != NULL && !Outputs->Initialize())
	{
		ErrorLog("Unable to initialize outputs.\n");
		exitCode = 1;
		goto Exit;
	}
	
	// From this point onwards, a ROM set is needed
	if (fileIdx == 0)
	{
		ErrorLog("No ROM set specified.");
		exitCode = 1;
		goto Exit;
	}
	
	if (cmdDis)
	{
		if (OKAY != DisassembleCROM(argv[fileIdx], addr, n))
			exitCode = 1;
		goto Exit;
	}

#ifdef SUPERMODEL_DEBUGGER
	// Create Model3
	Model3 = new CModel3();
	// Create Supermodel debugger unless debugging is disabled
	if (!g_Config.disableDebugger)
	{
		Debugger = new Debugger::CSupermodelDebugger(Model3, Inputs, &Logger);
		// If -enter-debugger option was set force debugger to break straightaway
		if (cmdEnterDebugger)
			Debugger->ForceBreak(true);
	}
	// Fire up Supermodel with debugger
	exitCode = Supermodel(argv[fileIdx],Model3,Inputs,Outputs,Debugger,&CmdLine);
	if (Debugger != NULL)
		delete Debugger;
	delete Model3;
#else
	// Fire up Supermodel
	exitCode = Supermodel(argv[fileIdx],Inputs,Outputs,&CmdLine);
#endif // SUPERMODEL_DEBUGGER

Exit:
	if (Inputs != NULL)
		delete Inputs;
	if (InputSystem != NULL)
		delete InputSystem;
	if (Outputs != NULL)
		delete Outputs;
	SDL_Quit();
	
	if (exitCode)
		InfoLog("Program terminated due to an error.");
	else
		InfoLog("Program terminated normally.");
		
	return exitCode;
}
