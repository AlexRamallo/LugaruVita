/*
Copyright (C) 2003, 2010 - Wolfire Games
Copyright (C) 2010-2017 - Lugaru contributors (see AUTHORS file)

This file is part of Lugaru.

Lugaru is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

Lugaru is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Lugaru.  If not, see <http://www.gnu.org/licenses/>.
*/
#define MICROPROFILE_IMPL 1
#include "Thirdparty/microprofile/microprofile.h"

#include "Utils/WorkerThread.hpp"

#if PACK_ASSETS
    #include <physfs.h>

    #define PHYFSPP_IMPL
    #include "Thirdparty/physfs-hpp.h"
#endif

#include "Game.hpp"

#include "Audio/openal_wrapper.hpp"
#include "Graphic/gamegl.hpp"
#include "Platform/Platform.hpp"
#include "User/Settings.hpp"
#include "Menu/Menu.hpp"
#include "Version.hpp"

#include <fstream>
#include <iostream>
#include <thread>
#include <ostream>
#include <math.h>
#include <set>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <assert.h>

#include <signal.h>

extern "C" {
    #include <math_neon.h>
}

using namespace Game;

#ifdef WIN32
#include <shellapi.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

#if PLATFORM_VITA
#include <psp2/io/stat.h>

extern "C" {
    unsigned int _newlib_heap_size_user = 128 * 1024 * 1024;

    //increase if flickering occurs
    #define VGL_VERTEX_POOL_SIZE 16 * 1024 * 1024
}

#include <vitaGL.h>

#include "Utils/PVRTexLoader.hpp"

PVRTexLoader *pvr_loader = nullptr;
#endif

extern bool did_just_load;
extern float gravity;
extern float multiplier;
extern float realmultiplier;
extern int slomo;
extern bool cellophane;
extern float texdetail;

extern bool freeze;
extern bool stillloading;
extern int mainmenu;

extern float slomospeed;
extern float slomofreq;

extern int difficulty;

extern SDL_Window* sdlwindow;

using namespace std;

set<pair<int, int>> resolutions;

// statics/globals (internal only) ------------------------------------------

// Menu defs

int kContextWidth;
int kContextHeight;

//-----------------------------------------------------------------------------------------------------------------------

// OpenGL Drawing

void initGL()
{
    glClear(GL_COLOR_BUFFER_BIT);
    swap_gl_buffers();
    glMatrixMode(GL_PROJECTION);

    // clear all states
    glDisable(GL_ALPHA_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_FOG);
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);

    /*
    VITAGL: TODO
    glDisable(GL_LOGIC_OP);
    glDisable(GL_TEXTURE_1D);
    glPixelTransferi(GL_MAP_COLOR, GL_FALSE);
    glPixelTransferi(GL_RED_SCALE, 1);
    glPixelTransferi(GL_RED_BIAS, 0);
    glPixelTransferi(GL_GREEN_SCALE, 1);
    glPixelTransferi(GL_GREEN_BIAS, 0);
    glPixelTransferi(GL_BLUE_SCALE, 1);
    glPixelTransferi(GL_BLUE_BIAS, 0);
    glPixelTransferi(GL_ALPHA_SCALE, 1);
    glPixelTransferi(GL_ALPHA_BIAS, 0);
    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
    glEnable(GL_DITHER);
    */

    // set initial rendering states
    glShadeModel(GL_SMOOTH);
    glClearDepth(1.0f);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
    glCullFace(GL_FRONT);
    glEnable(GL_CULL_FACE);
    glEnable(GL_LIGHTING);
    glEnable(GL_COLOR_MATERIAL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glAlphaFunc(GL_GREATER, 0.5f);

    if (CanInitStereo(stereomode)) {
        InitStereo(stereomode);
    } else {
        fprintf(stderr, "Failed to initialize stereo, disabling.\n");
        stereomode = stereoNone;
    }
}

void toggleFullscreen()
{
    fullscreen = !fullscreen;
    Uint32 flags = SDL_GetWindowFlags(sdlwindow);
    if (flags & SDL_WINDOW_FULLSCREEN) {
        flags &= ~SDL_WINDOW_FULLSCREEN;
    } else {
        flags |= SDL_WINDOW_FULLSCREEN;
    }
    SDL_SetWindowFullscreen(sdlwindow, flags);
}

static int rstick_deadzone = 8000;
static int lstick_deadzone = 8000;
static float rstick_x = 0;
static float rstick_y = 0;
static float lstick_x = 0;
static float lstick_y = 0;

static float deltah_vel = 0.0f;
static float deltav_vel = 0.0f;

void update_analog_sticks(){
    deltah_vel += rstick_x;
    deltav_vel += rstick_y;

    deltah += deltah_vel * 350 * multiplier;
    deltav += deltav_vel * 250 * multiplier;

    deltah_vel *= 0.6f;
    deltav_vel *= 0.6f;

    if(fabsf(deltah_vel) < 0.15) deltah_vel = 0.0f;
    if(fabsf(deltav_vel) < 0.15) deltav_vel = 0.0f;

    analog_lh = lstick_x;
    analog_lv = lstick_y;
}

SDL_bool sdlEventProc(const SDL_Event& e)
{
    switch (e.type) {
        case SDL_QUIT:
            return SDL_FALSE;

        case SDL_WINDOWEVENT:
            if (e.window.event == SDL_WINDOWEVENT_CLOSE) {
                return SDL_FALSE;
            }
            break;

        case SDL_MOUSEMOTION:
            deltah += e.motion.xrel;
            deltav += e.motion.yrel;
            break;

        case SDL_CONTROLLERAXISMOTION:
            switch(e.caxis.axis){
                default: break;
                case SDL_CONTROLLER_AXIS_RIGHTX:
                    rstick_x = (float) (abs(e.caxis.value) < rstick_deadzone ? 0 : e.caxis.value) / 32767.0f;
                    break;
                case SDL_CONTROLLER_AXIS_RIGHTY:
                    rstick_y = (float) (abs(e.caxis.value) < rstick_deadzone ? 0 : e.caxis.value) / 32767.0f;
                    break;
                case SDL_CONTROLLER_AXIS_LEFTX:
                    lstick_x = (float) (abs(e.caxis.value) < lstick_deadzone ? 0 : e.caxis.value) / 32767.0f;
                    break;
                case SDL_CONTROLLER_AXIS_LEFTY:
                    lstick_y = (float) (abs(e.caxis.value) < lstick_deadzone ? 0 : e.caxis.value) / 32767.0f;
                    break;
            }
            break;

        case SDL_KEYDOWN:
            if ((e.key.keysym.scancode == SDL_SCANCODE_G) &&
                (e.key.keysym.mod & KMOD_CTRL)) {
                SDL_bool mode = SDL_TRUE;
                if ((SDL_GetWindowFlags(sdlwindow) & SDL_WINDOW_FULLSCREEN) == 0) {
                    mode = (SDL_GetWindowGrab(sdlwindow) ? SDL_FALSE : SDL_TRUE);
                }
                SDL_SetWindowGrab(sdlwindow, mode);
                SDL_SetRelativeMouseMode(mode);
            } else if ((e.key.keysym.scancode == SDL_SCANCODE_RETURN) && (e.key.keysym.mod & KMOD_ALT)) {
                toggleFullscreen();
            }
            break;
    }

    return SDL_TRUE;
}

// --------------------------------------------------------------------------

static Point gMidPoint;

bool SetUp()
{
    MICROPROFILE_SCOPEI("main", "SetUp", 0xffff3456);

    LOGFUNC;

    cellophane = 0;
    texdetail = 4;
    slomospeed = 0.25;
    slomofreq = 8012;

    DefaultSettings();

    if (!SDL_WasInit(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER)) {
        if (SDL_Init(SDL_INIT_VIDEO) == -1) {
            fprintf(stderr, "SDL_Init() failed: %s\n", SDL_GetError());
            return false;
        }
        if (SDL_Init(SDL_INIT_GAMECONTROLLER) == -1) {
            fprintf(stderr, "SDL_Init() failed: %s\n", SDL_GetError());
            return false;
        }

        #if MICROPROFILE_ENABLED
        //TODO: use SDL_net to implement missing networking stuff?
        #endif
    }

    SDL_GameController *controller = NULL;
    for (int i = 0; i < SDL_NumJoysticks(); ++i) {
        if (SDL_IsGameController(i)) {
            controller = SDL_GameControllerOpen(i);
            if (controller) {
                break;
            } else {
                fprintf(stderr, "Could not open gamecontroller %d: %s\n", i, SDL_GetError());
            }
        }
    }

    if (!LoadSettings()) {
        fprintf(stderr, "Failed to load config, creating default\n");
        SaveSettings();
    }

    /*
    if (SDL_GL_LoadLibrary(NULL) == -1) {
        fprintf(stderr, "SDL_GL_LoadLibrary() failed: %s\n", SDL_GetError());
        SDL_Quit();
        return false;
    }
    */

    for (int displayIdx = 0; displayIdx < SDL_GetNumVideoDisplays(); ++displayIdx) {
        for (int i = 0; i < SDL_GetNumDisplayModes(displayIdx); ++i) {
            SDL_DisplayMode mode;
            if (SDL_GetDisplayMode(displayIdx, i, &mode) == -1) {
                continue;
            }
            if ((mode.w < 640) || (mode.h < 480)) {
                continue; // sane lower limit.
            }
            pair<int, int> resolution(mode.w, mode.h);
            resolutions.insert(resolution);
        }
    }

    if (resolutions.empty()) {
        const std::string error = "No suitable video resolutions found.";
        cerr << error << endl;
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Lugaru init failed!", error.c_str(), NULL);
        SDL_Quit();
        return false;
    }

    if (commandLineOptions[SHOWRESOLUTIONS]) {
        printf("Available resolutions:\n");
        for (auto resolution = resolutions.begin(); resolution != resolutions.end(); resolution++) {
            printf("  %d x %d\n", (int)resolution->first, (int)resolution->second);
        }
    }

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 1);

    Uint32 sdlflags = SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN;
    if (commandLineOptions[FULLSCREEN]) {
        fullscreen = commandLineOptions[FULLSCREEN].last()->type();
    }
    if (fullscreen) {
        sdlflags |= SDL_WINDOW_FULLSCREEN;
    }
    if (!commandLineOptions[NOMOUSEGRAB].last()->type()) {
        sdlflags |= SDL_WINDOW_INPUT_GRABBED;
    }

    sdlwindow = SDL_CreateWindow("Lugaru", SDL_WINDOWPOS_CENTERED_DISPLAY(0), SDL_WINDOWPOS_CENTERED_DISPLAY(0),
                                 kContextWidth, kContextHeight, sdlflags);

    if (!sdlwindow) {
        fprintf(stderr, "SDL_CreateWindow() failed: %s\n", SDL_GetError());
        fprintf(stderr, "forcing 640x480...\n");
        kContextWidth = 640;
        kContextHeight = 480;
        sdlwindow = SDL_CreateWindow("Lugaru", SDL_WINDOWPOS_CENTERED_DISPLAY(0), SDL_WINDOWPOS_CENTERED_DISPLAY(0),
                                     kContextWidth, kContextHeight, sdlflags);
        if (!sdlwindow) {
            fprintf(stderr, "SDL_CreateWindow() failed: %s\n", SDL_GetError());
            fprintf(stderr, "forcing 640x480 windowed mode...\n");
            sdlflags &= ~SDL_WINDOW_FULLSCREEN;
            sdlwindow = SDL_CreateWindow("Lugaru", SDL_WINDOWPOS_CENTERED_DISPLAY(0), SDL_WINDOWPOS_CENTERED_DISPLAY(0),
                                         kContextWidth, kContextHeight, sdlflags);

            if (!sdlwindow) {
                fprintf(stderr, "SDL_CreateWindow() failed: %s\n", SDL_GetError());
                return false;
            }
        }
    }

    SDL_GLContext glctx = SDL_GL_CreateContext(sdlwindow);
    if (!glctx) {
        fprintf(stderr, "SDL_GL_CreateContext() failed: %s\n", SDL_GetError());
        SDL_Quit();
        return false;
    }

    SDL_GL_MakeCurrent(sdlwindow, glctx); 

    /*
    int dblbuf = 0;
    if ((SDL_GL_GetAttribute(SDL_GL_DOUBLEBUFFER, &dblbuf) == -1) || (!dblbuf)) {
        fprintf(stderr, "Failed to get a double-buffered context.\n");
        SDL_Quit();
        return false;
    }

    if (SDL_GL_SetSwapInterval(-1) == -1) { // try swap_tear first.
        SDL_GL_SetSwapInterval(1);
    }
    */

    SDL_ShowCursor(0);
    if (!commandLineOptions[NOMOUSEGRAB].last()->type()) {
        SDL_SetRelativeMouseMode(SDL_TRUE);
    }

    initGL();

    GLint width = kContextWidth;
    GLint height = kContextHeight;
    gMidPoint.h = width / 2;
    gMidPoint.v = height / 2;
    screenwidth = width;
    screenheight = height;

    newdetail = detail;
    newscreenwidth = screenwidth;
    newscreenheight = screenheight;


    /* If saved resolution is not in the list, add it to the list (so that it’s selectable in the options) */
    pair<int, int> startresolution(width, height);
    if (resolutions.find(startresolution) == resolutions.end()) {
        resolutions.insert(startresolution);
    }

    InitGame();

    return true;
}

static void DoMouse()
{

    if (mainmenu || ((abs(deltah) < 10 * realmultiplier * 1000) && (abs(deltav) < 10 * realmultiplier * 1000))) {
        deltah *= usermousesensitivity;
        deltav *= usermousesensitivity;
        mousecoordh += deltah;
        mousecoordv += deltav;
        if (mousecoordh < 0) {
            mousecoordh = 0;
        } else if (mousecoordh >= kContextWidth) {
            mousecoordh = kContextWidth - 1;
        }
        if (mousecoordv < 0) {
            mousecoordv = 0;
        } else if (mousecoordv >= kContextHeight) {
            mousecoordv = kContextHeight - 1;
        }
    }
}

void DoFrameRate(int update)
{
    static long frames = 0;

    static AbsoluteTime time = { 0, 0 };
    static AbsoluteTime frametime = { 0, 0 };
    AbsoluteTime currTime = UpTime();
    double deltaTime = (float)AbsoluteDeltaToDuration(currTime, frametime);

    if (0 > deltaTime) { // if negative microseconds
        deltaTime /= -1000000.0;
    } else { // else milliseconds
        deltaTime /= 1000.0;
    }

    multiplier = deltaTime;
    if (multiplier < .001) {
        multiplier = .001;
    }
    if (multiplier > 0.03) {
        multiplier = 0.03;
    }
    if (update) {
        frametime = currTime; // reset for next time interval
    }

    deltaTime = (float)AbsoluteDeltaToDuration(currTime, time);

    if (0 > deltaTime) { // if negative microseconds
        deltaTime /= -1000000.0;
    } else { // else milliseconds
        deltaTime /= 1000.0;
    }
    frames++;
    if (0.001 <= deltaTime) { // has update interval passed
        if (update) {
            time = currTime; // reset for next time interval
            frames = 0;
        }
    }
}

void DoUpdate()
{
    static float sps = 200;
    static int count;
    static float oldmult;

    DoFrameRate(1);
    //if (multiplier > .1) {
    //    multiplier = .1;
    //}

    if(did_just_load){
        did_just_load = false;
        multiplier = 0.001f;
        oldmult = 0.001f;
    }

    fps = 1 / multiplier;

    count = multiplier * sps;
    if (count < 2) {
        count = 2;
    }
    
    count = 1;//Big speed up (gameplay consequences?)

    realmultiplier = multiplier;
    multiplier *= gamespeed;
    if (difficulty == 1) {
        multiplier *= .9;
    }
    if (difficulty == 0) {
        multiplier *= .8;
    }

    if (loading == 4) {
        multiplier *= .00001;
    }
    if (slomo && !mainmenu) {
        multiplier *= slomospeed;
    }

    oldmult = multiplier;
    multiplier /= (float)count;

    DoMouse();

    TickOnce();

    for (int i = 0; i < count; i++) {
        Tick();
    }

    multiplier = oldmult;

    TickOnceAfter();
    /* - Debug code to test how many channels were active on average per frame
        static long frames = 0;

        static AbsoluteTime start = {0,0};
        AbsoluteTime currTime = UpTime ();
        static int num_channels = 0;

        num_channels += OPENAL_GetChannelsPlaying();
        double deltaTime = (float) AbsoluteDeltaToDuration (currTime, start);

        if (0 > deltaTime)  // if negative microseconds
            deltaTime /= -1000000.0;
        else                // else milliseconds
            deltaTime /= 1000.0;

        ++frames;

        if (deltaTime >= 1)
        {
            start = currTime;
            float avg_channels = (float)num_channels / (float)frames;

            ofstream opstream("log.txt",ios::app);
            opstream << "Average frame count: ";
            opstream << frames;
            opstream << " frames - ";
            opstream << avg_channels;
            opstream << " per frame.\n";
            opstream.close();

            frames = 0;
            num_channels = 0;
        }
    */
    if (stereomode == stereoNone) {
        DrawGLScene(stereoCenter);
    } else {
        DrawGLScene(stereoLeft);
        DrawGLScene(stereoRight);
    }

    MicroProfileFlip();
}

// --------------------------------------------------------------------------

void CleanUp(void)
{
    LOGFUNC;

    #if PLATFORM_VITA
    vglEnd();
    #endif

    delete[] commandLineOptionsBuffer;

    SDL_Quit();
}

// --------------------------------------------------------------------------

static bool IsFocused()
{
    return ((SDL_GetWindowFlags(sdlwindow) & SDL_WINDOW_INPUT_FOCUS) != 0);
}

#ifndef WIN32
// (code lifted from physfs: http://icculus.org/physfs/ ... zlib license.)
static char* findBinaryInPath(const char* bin, char* envr)
{
    size_t alloc_size = 0;
    char* exe = NULL;
    char* start = envr;
    char* ptr;

    do {
        size_t size;
        ptr = strchr(start, ':'); /* find next $PATH separator. */
        if (ptr) {
            *ptr = '\0';
        }
        size = strlen(start) + strlen(bin) + 2;
        if (size > alloc_size) {
            char* x = (char*)realloc(exe, size);
            if (x == NULL) {
                if (exe != NULL) {
                    free(exe);
                }
                return (NULL);
            } /* if */

            alloc_size = size;
            exe = x;
        } /* if */

        /* build full binary path... */
        strcpy(exe, start);
        if ((exe[0] == '\0') || (exe[strlen(exe) - 1] != '/')) {
            strcat(exe, "/");
        }
        strcat(exe, bin);

        if (access(exe, X_OK) == 0) { /* Exists as executable? We're done. */
            strcpy(exe, start);       /* i'm lazy. piss off. */
            return (exe);
        } /* if */

        start = ptr + 1; /* start points to beginning of next element. */
    } while (ptr != NULL);

    if (exe != NULL) {
        free(exe);
    }

    return (NULL); /* doesn't exist in path. */
} /* findBinaryInPath */

char* calcBaseDir(const char* argv0)
{
    /* If there isn't a path on argv0, then look through the $PATH for it. */
    char* retval;
    char* envr;

    if (strchr(argv0, '/')) {
        retval = strdup(argv0);
        if (retval) {
            *((char*)strrchr(retval, '/')) = '\0';
        }
        return (retval);
    }

    envr = getenv("PATH");
    if (!envr) {
        return NULL;
    }
    envr = strdup(envr);
    if (!envr) {
        return NULL;
    }
    retval = findBinaryInPath(argv0, envr);
    free(envr);
    return (retval);
}

static inline void chdirToAppPath(const char* argv0)
{
    char* dir = calcBaseDir(argv0);
    if (dir) {
#if (defined(__APPLE__) && defined(__MACH__))
        // Chop off /Contents/MacOS if it's at the end of the string, so we
        //  land in the base of the app bundle.
        const size_t len = strlen(dir);
        const char* bundledirs = "/Contents/MacOS";
        const size_t bundledirslen = strlen(bundledirs);
        if (len > bundledirslen) {
            char* ptr = (dir + len) - bundledirslen;
            if (strcasecmp(ptr, bundledirs) == 0)
                *ptr = '\0';
        }
#endif
        errno = 0;
        if (chdir(dir) != 0) {
            printf("Error changing dir to '%s' (%s).\n", dir, strerror(errno));
        }
        free(dir);
    }
}
#endif

const option::Descriptor usage[] =
    {
      { UNKNOWN, 0, "", "", option::Arg::None, "USAGE: lugaru [options]\n\n"
                                               "Options:" },
      { VERSION, 0, "v", "version", option::Arg::None, " -v, --version     Print version and exit." },
      { HELP, 0, "h", "help", option::Arg::None, " -h, --help        Print usage and exit." },
      { FULLSCREEN, 1, "f", "fullscreen", option::Arg::None, " -f, --fullscreen  Start the game in fullscreen mode." },
      { FULLSCREEN, 0, "w", "windowed", option::Arg::None, " -w, --windowed    Start the game in windowed mode (default)." },
      { NOMOUSEGRAB, 1, "", "nomousegrab", option::Arg::None, " --nomousegrab     Disable mousegrab." },
      { NOMOUSEGRAB, 0, "", "mousegrab", option::Arg::None, " --mousegrab       Enable mousegrab (default)." },
      { SOUND, 1, "", "nosound", option::Arg::None, " --nosound         Disable sound." },
      { OPENALINFO, 0, "", "openal-info", option::Arg::None, " --openal-info     Print info about OpenAL at launch." },
      { SHOWRESOLUTIONS, 0, "", "showresolutions", option::Arg::None, " --showresolutions List the resolutions found by SDL at launch." },
      { DEVTOOLS, 0, "d", "devtools", option::Arg::None, " -d, --devtools    Enable dev tools: console, level editor and debug info." },
      { CMD, 0, "c", "command", option::Arg::Optional, " -c, --command    Run this command at game start. May be used to load a map." },
      { 0, 0, 0, 0, 0, 0 }
    };

option::Option commandLineOptions[commandLineOptionsNumber];
option::Option* commandLineOptionsBuffer;

int lugaru_main(int argc, char** argv)
{
    #if PLATFORM_VITA
        MicroProfileSetForceEnable(true);
        MicroProfileSetEnableAllGroups(true);
        MicroProfileSetForceMetaCounters(true);
        pvr_loader = new PVRTexLoader();
    #endif

    argc -= (argc > 0);
    argv += (argc > 0); // skip program name argv[0] if present
    option::Stats stats(true, usage, argc, argv);
    if (commandLineOptionsNumber != stats.options_max) {
        std::cerr << "Found incorrect command line option number" << std::endl;
        return 1;
    }
    commandLineOptionsBuffer = new option::Option[stats.buffer_max];
    option::Parser parse(true, usage, argc, argv, commandLineOptions, commandLineOptionsBuffer);

    if (parse.error()) {
        delete[] commandLineOptionsBuffer;
        return 1;
    }

    // Always start by printing the version and info to the stdout
    std::cout << "--------------------------------------------------------------------------\n"
              << "Lugaru HD: The Rabbit's Foot, by Wolfire Games and the OSS Lugaru project.\n\n"
              << "Licensed under the GPL 2.0+ and CC-BY-SA 3.0 and 4.0 licenses.\n"
              << "More information, updates and bug reports at http://osslugaru.gitlab.io\n"
              << std::endl;

    std::cout << "Version " + VERSION_STRING + " -- " + VERSION_BUILD_TYPE + " build\n"
              << "--------------------------------------------------------------------------\n"
              << std::endl;

    if (commandLineOptions[VERSION]) {
        // That was enough, quit.
        delete[] commandLineOptionsBuffer;
        return 0;
    }

    if (commandLineOptions[HELP]) {
        option::printUsage(std::cout, usage);
        delete[] commandLineOptionsBuffer;
        return 0;
    }

    if (option::Option* opt = commandLineOptions[UNKNOWN]) {
        std::cerr << "Unknown option: " << opt->name << "\n";
        option::printUsage(std::cerr, usage);
        delete[] commandLineOptionsBuffer;
        return 1;
    }

// !!! FIXME: we could use a Win32 API for this.  --ryan.
#ifndef WIN32
    chdirToAppPath(argv[0]);
#endif

    LOGFUNC;

    {
        newGame();

        if (!SetUp()) {
            delete[] commandLineOptionsBuffer;
            return 42;
        }

        if (commandLineOptions[DEVTOOLS]) {
            devtools = true;
        }

        bool gameDone = false;
        bool gameFocused = true;

        srand(time(nullptr));

        if (commandLineOptions[CMD].count() > 0) {
            devtools = true;
            Menu::startChallengeLevel(1);
            for (option::Option* opt = commandLineOptions[CMD]; opt; opt = opt->next()) {
                if (opt->arg && (strlen(opt->arg) > 0)) {
                    cmd_dispatch(opt->arg);
                }
            }
        }

        LOG_TOGGLE(true);
        bool res = WorkerThread::init();
        ASSERT(res && "Failed to init WorkerThread system");
        WorkerThread::spawnWorkers(2);
        /*
        auto j1 = WorkerThread::submitJob(WorkerThread::WRK_TEST, (int)2, (int)12, (int)17);
        auto j2 = WorkerThread::submitJob(WorkerThread::WRK_TEST, (int)69, (int)420, (int)101);
        
        int waitit = 0;
        while(!WorkerThread::tryJoin(j2)){
            waitit++;
        }
        LOG("j2 is done! iterations: %d", waitit);

        WorkerThread::join(j1);
        LOG("j1 is done!");
        //*/

        LOG_TOGGLE(false);

        while (!gameDone && !tryquit) {
            if (IsFocused()) {
                gameFocused = true;

                // check windows messages

                deltah = 0;
                deltav = 0;
                SDL_Event e;
                if (!waiting) {
                    // message pump
                    while (SDL_PollEvent(&e)) {
                        if (!sdlEventProc(e)) {
                            gameDone = true;
                            break;
                        }
                    }
                }

                update_analog_sticks();

                // game
                DoUpdate();
            } else {
                if (gameFocused) {
                    // allow game chance to pause
                    gameFocused = false;
                    DoUpdate();
                }

                // game is not in focus, give CPU time to other apps by waiting for messages instead of 'peeking'
                SDL_WaitEvent(0);
            }
        }

        deleteGame();
    }

    CleanUp();
    MicroProfileShutdown();

    return 0;
}

pthread_t main_thread;

int main(int argc, char** argv){
    main_thread = pthread_self();
    vglSetVertexPoolSize(VGL_VERTEX_POOL_SIZE);

    MicroProfileOnThreadCreate("MainThread");

    #ifndef NDEBUG
    //*
    std::ofstream ofs{"ux0:data/lugaru_runlog.txt"}; 
    std::cout.rdbuf(ofs.rdbuf());
    std::cerr.rdbuf(ofs.rdbuf());
    std::cout.setf( std::ios_base::unitbuf );

    freopen("ux0:data/lugaru_runlog.txt", "a", stdout);
    freopen("ux0:data/lugaru_runlog.txt", "a", stderr);
    //*/
    #endif

    #if PACK_ASSETS
    if(!PHYSFS_init(nullptr)){
        std::cout << "Failed to initialize PhysFS\n";
        return -1;
    }

    /*if(!PHYSFS_mount("app0:Data.zip", "", 0)){
        std::cout << "Failed to mount asset pack (app0:Data.zip)\n";
        return -1;
    }*/

    const char *mounts[4][2] = {
        {"app0:Data.zip", ""},
        {"ux0:data/lugaru/user", "/user"},
        {"ux0:data/lugaru/home", "/home"},
        {"ux0:data/lugaru/config", "/config"}
    };

    std::string writedir = "ux0:data/lugaru";
    if(!PHYSFS_setWriteDir(writedir.c_str())){
        LOG("Failed to open write directory at: %s. Error: %s", writedir.c_str(), PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode()));
    }

    for(int i = 0; i < 4; i++){
        const char *src = mounts[i][0];
        const char *dst = mounts[i][1];

        if(!PHYSFS_mount(src, dst, 0)){
            auto ec = PHYSFS_getLastErrorCode();
            if(ec == PHYSFS_ERR_NOT_FOUND){
                if (sceIoMkdir(src, 0777) == -1) {
                    std::cout
                        << "Failed to create directory: "
                        << src
                        << " (" << errno << ")\n";
                    return -1;
                }else{
                    std::cout << "Created missing directory: " << src << "\n";
                    i--;
                    continue; 
                }
            }

            std::cout
                << "Failed to mount "
                << "\"" << src << "\""
                << " to "
                << "\"" << dst << "\""
                << "\n\tError: "
                << PHYSFS_getErrorByCode(ec)
                << "\n";
            return -1;
        }
    }

    #endif

    int ret = lugaru_main(argc, argv);
    std::cout << "Main returned with code: " << ret << std::endl;

    return ret;
}