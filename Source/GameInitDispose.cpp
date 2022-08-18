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

#include "Game.hpp"

#include "Animation/Animation.hpp"
#include "Audio/openal_wrapper.hpp"
#include "Graphic/Texture.hpp"
#include "Menu/Menu.hpp"
#include "Utils/Folders.hpp"
#include "Utils/FileCache.hpp"
#include "Utils/WorkerThread.hpp"

#include <vector>
#include <tuple>
#include <sstream>

#include <pthread.h>
#include "Thirdparty/microprofile/microprofile.h"

#define LOG_MUTEX true

extern float screenwidth, screenheight;
extern float viewdistance;
extern XYZ viewer;
extern float fadestart;
extern float texscale;
extern float gravity;
extern Light light;
extern Terrain terrain;
extern int kTextureSize;
extern float texdetail;
extern float realtexdetail;
extern float volume;
extern int detail;
extern bool cellophane;
extern bool ismotionblur;
extern bool trilinear;
extern bool musictoggle;
extern int environment;
extern bool ambientsound;
extern float multiplier;
extern int netdatanew;
extern float mapinfo;
extern bool stillloading;
extern int mainmenu;
extern bool visibleloading;
extern float flashamount, flashr, flashg, flashb;
extern int flashdelay;
extern int whichjointstartarray[26];
extern int whichjointendarray[26];
extern float slomospeed;
extern bool gamestarted;

extern float accountcampaignhighscore[10];
extern float accountcampaignfasttime[10];
extern float accountcampaignscore[10];
extern float accountcampaigntime[10];

extern int accountcampaignchoicesmade[10];
extern int accountcampaignchoices[10][5000];

static bool log_enabled = false;

void vLOG(const std::string &fmt, va_list args);

void LOG_TOGGLE(char set){
    log_enabled = set;
}


#if LOG_MUTEX
static pthread_mutex_t mtxLog;
static bool did_init_log = false;
#endif

//#define LOG_BUF false

#if LOG_BUF
static const int log_buf_size = 1000;
static std::vector<std::string> log_buf(log_buf_size);
static int log_head = 0;

void LOG_FLUSH(){
    std::stringstream ss;
    std::string last_msg;
    int repeat_ct = 0;
    for(int i = 0; i < log_buf_size; i++){
        std::string &msg = log_buf[(log_head + i) % log_buf_size];
        
        if(msg != std::string()){
            if(msg == last_msg){
                repeat_ct++;
            }else if(repeat_ct > 0){
                ss << "[x" << repeat_ct+1 << "] " << msg;
                repeat_ct = 0;
                last_msg = std::string();
            }else{
                last_msg = msg;
                ss << msg;
            }
        }
    }

    std::cout << ss.str();
}
#else
void LOG_FLUSH(){}
#endif

void LOG(const std::string &fmt, ...)
{  
#ifndef NDEBUG

#if LOG_MUTEX
    if(!did_init_log){
        did_init_log = true;
        ASSERT(!pthread_mutex_init(&mtxLog, NULL));
    }
#endif

#if !LOG_BUF
    if(!log_enabled) return;
#endif

#if LOG_MUTEX
    ASSERT(!pthread_mutex_lock(&mtxLog));
#endif

    //stolen from https://en.cppreference.com/w/cpp/utility/variadic    
    va_list args;
    va_start(args, fmt);
    bool is_fmt = false;

    std::stringstream ss;

    for(char c: fmt){
        if(is_fmt){
            is_fmt = false;
            if (c == 'd') {
                int i = va_arg(args, int);
                ss << i;
            } else if (c == 'c') {
                // note automatic conversion to integral type
                int c = va_arg(args, int);
                ss << static_cast<char>(c);
            } else if (c == 'f') {
                double d = va_arg(args, double);
                ss << d;
            } else if (c == 'p') {
                void* d = va_arg(args, void*);
                ss << d;
            } else if(c == 's') {
                char *s = va_arg(args, char*);
                ss << s;
            } else if(c == '%') {
                ss << '%';
            } else{
                ss << '%' << c;
            }
        }else{
            if(c == '%'){
                is_fmt = true;
            }else{
                ss << c;
            }
        }
    }
    ss << std::endl;
    va_end(args);

    #if LOG_BUF
    log_buf[log_head] = ss.str();
    log_head = (log_head + 1) % log_buf.size();
    #else
    std::cout << ss.str();
    #endif

#if LOG_MUTEX
    ASSERT(!pthread_mutex_unlock(&mtxLog));
#endif

#endif
}

void Dispose()
{
    LOGFUNC;

    if (Game::endgame == 2) {
        Account::active().endGame();
        Game::endgame = 0;
    }

    Account::saveFile(Folders::getUserSavePath());

    //textures.clear();

    LOG("Shutting down sound system...");

    OPENAL_StopSound(OPENAL_ALL);

    for (int i = 0; i < sounds_count; ++i) {
        OPENAL_Sample_Free(samp[i]);
    }

    OPENAL_Close();
}

void Game::newGame()
{
    text = new Text();
    textmono = new Text();
    skybox = new SkyBox();

    if(!terrain.allocate()){
        LOG("Failed to allocate space for terrain");
    }
}

void Game::deleteGame()
{
    delete skybox;
    delete text;
    delete textmono;

    glDeleteTextures(1, &screentexture);
    glDeleteTextures(1, &screentexture2);

    Dispose();
}

void LoadSave(const std::string& fileName, GLubyte* array)
{
    LOGFUNC;
    LOG_TOGGLE(true);
    LOG(std::string("Loading (S)...") + fileName);

    //Load Image
    float temptexdetail = texdetail;
    texdetail = 1;

    //Load Image
    ImageRec texture;
    if (!load_image(Folders::getResourcePath(fileName).c_str(), texture)) {
        LOG("\tFailed to load %s", fileName.c_str());
        texdetail = temptexdetail;
        return;
    }
    texdetail = temptexdetail;

    int bytesPerPixel = texture.getBitsPerPixel() / 8;

    int tempnum = 0;
    for (int i = 0; i < (int)(texture.getHeight() * texture.getWidth() * bytesPerPixel); i++) {
        if ((i + 1) % 4 || bytesPerPixel == 3) {
            array[tempnum] = texture.data[i];
            tempnum++;
        }
    }
    LOG_TOGGLE(false);
}

//***************> ResizeGLScene() <******/
GLvoid Game::ReSizeGLScene(float fov, float pnear)
{
    if (screenheight == 0) {
        screenheight = 1;
    }

    glViewport(0, 0, screenwidth, screenheight);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    gluPerspective(fov, (GLfloat)screenwidth / (GLfloat)screenheight, pnear, viewdistance);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

extern pthread_t main_thread;

void Game::LoadingScreen()
{
    if (!visibleloading) {
        return;
    }

    if(pthread_self() != main_thread){
        return;
    }

    static float loadprogress;
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
    if (multiplier > 10) {
        multiplier = 10;
    }
    if (multiplier > .05) {
        frametime = currTime; // reset for next time interval

        glLoadIdentity();
        //Clear to black
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        loadtime += multiplier * 4;

        loadprogress = loadtime;
        if (loadprogress > 100) {
            loadprogress = 100;
        }

        //Background

        glEnable(GL_TEXTURE_2D);
        loadscreentexture.bind();
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glDisable(GL_LIGHTING);
        glDepthMask(0);
        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glLoadIdentity();
        glOrtho(0, screenwidth, 0, screenheight, -100, 100);
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glLoadIdentity();
        glTranslatef(screenwidth / 2, screenheight / 2, 0);
        glScalef((float)screenwidth / 2, (float)screenheight / 2, 1);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDisable(GL_BLEND);
        glColor4f(loadprogress / 100, loadprogress / 100, loadprogress / 100, 1);
        glPushMatrix();
        glBegin(GL_QUADS);
        glTexCoord2f(.1 - loadprogress / 100, 0 + loadprogress / 100 + .3);
        glVertex3f(-1, -1, 0.0f);
        glTexCoord2f(.1 - loadprogress / 100, 0 + loadprogress / 100 + .3);
        glVertex3f(1, -1, 0.0f);
        glTexCoord2f(.1 - loadprogress / 100, 1 + loadprogress / 100 + .3);
        glVertex3f(1, 1, 0.0f);
        glTexCoord2f(.1 - loadprogress / 100, 1 + loadprogress / 100 + .3);
        glVertex3f(-1, 1, 0.0f);
        glEnd();
        glPopMatrix();
        glEnable(GL_BLEND);
        glPushMatrix();
        glBegin(GL_QUADS);
        glTexCoord2f(.4 + loadprogress / 100, 0 + loadprogress / 100);
        glVertex3f(-1, -1, 0.0f);
        glTexCoord2f(.4 + loadprogress / 100, 0 + loadprogress / 100);
        glVertex3f(1, -1, 0.0f);
        glTexCoord2f(.4 + loadprogress / 100, 1 + loadprogress / 100);
        glVertex3f(1, 1, 0.0f);
        glTexCoord2f(.4 + loadprogress / 100, 1 + loadprogress / 100);
        glVertex3f(-1, 1, 0.0f);
        glEnd();
        glPopMatrix();
        glDisable(GL_TEXTURE_2D);
        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
        glMatrixMode(GL_MODELVIEW);
        glPopMatrix();
        glDisable(GL_BLEND);
        glDepthMask(1);

        glEnable(GL_TEXTURE_2D);
        loadscreentexture.bind();
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glDisable(GL_LIGHTING);
        glDepthMask(0);
        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glLoadIdentity();
        glOrtho(0, screenwidth, 0, screenheight, -100, 100);
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glLoadIdentity();
        glTranslatef(screenwidth / 2, screenheight / 2, 0);
        glScalef((float)screenwidth / 2 * (1.5 - (loadprogress) / 200), (float)screenheight / 2 * (1.5 - (loadprogress) / 200), 1);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        glEnable(GL_BLEND);
        glColor4f(loadprogress / 100, loadprogress / 100, loadprogress / 100, 1);
        glPushMatrix();
        glBegin(GL_QUADS);
        glTexCoord2f(0 + .5, 0 + .5);
        glVertex3f(-1, -1, 0.0f);
        glTexCoord2f(1 + .5, 0 + .5);
        glVertex3f(1, -1, 0.0f);
        glTexCoord2f(1 + .5, 1 + .5);
        glVertex3f(1, 1, 0.0f);
        glTexCoord2f(0 + .5, 1 + .5);
        glVertex3f(-1, 1, 0.0f);
        glEnd();
        glPopMatrix();
        glDisable(GL_TEXTURE_2D);
        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
        glMatrixMode(GL_MODELVIEW);
        glPopMatrix();
        glDisable(GL_BLEND);
        glDepthMask(1);

        glEnable(GL_TEXTURE_2D);
        loadscreentexture.bind();
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glDisable(GL_LIGHTING);
        glDepthMask(0);
        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glLoadIdentity();
        glOrtho(0, screenwidth, 0, screenheight, -100, 100);
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glLoadIdentity();
        glTranslatef(screenwidth / 2, screenheight / 2, 0);
        glScalef((float)screenwidth / 2 * (100 + loadprogress) / 100, (float)screenheight / 2 * (100 + loadprogress) / 100, 1);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        glEnable(GL_BLEND);
        glColor4f(loadprogress / 100, loadprogress / 100, loadprogress / 100, .4);
        glPushMatrix();
        glBegin(GL_QUADS);
        glTexCoord2f(0 + .2, 0 + .8);
        glVertex3f(-1, -1, 0.0f);
        glTexCoord2f(1 + .2, 0 + .8);
        glVertex3f(1, -1, 0.0f);
        glTexCoord2f(1 + .2, 1 + .8);
        glVertex3f(1, 1, 0.0f);
        glTexCoord2f(0 + .2, 1 + .8);
        glVertex3f(-1, 1, 0.0f);
        glEnd();
        glPopMatrix();
        glDisable(GL_TEXTURE_2D);
        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
        glMatrixMode(GL_MODELVIEW);
        glPopMatrix();
        glDisable(GL_BLEND);
        glDepthMask(1);

        //Text

        if (flashamount > 0) {
            if (flashamount > 1) {
                flashamount = 1;
            }
            if (flashdelay <= 0) {
                flashamount -= multiplier;
            }
            flashdelay--;
            if (flashamount < 0) {
                flashamount = 0;
            }
            glDisable(GL_DEPTH_TEST);
            glDisable(GL_CULL_FACE);
            glDisable(GL_LIGHTING);
            glDisable(GL_TEXTURE_2D);
            glDepthMask(0);
            glMatrixMode(GL_PROJECTION);
            glPushMatrix();
            glLoadIdentity();
            glOrtho(0, screenwidth, 0, screenheight, -100, 100);
            glMatrixMode(GL_MODELVIEW);
            glPushMatrix();
            glLoadIdentity();
            glScalef(screenwidth, screenheight, 1);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glEnable(GL_BLEND);
            glColor4f(flashr, flashg, flashb, flashamount);
            glBegin(GL_QUADS);
            glVertex3f(0, 0, 0.0f);
            glVertex3f(256, 0, 0.0f);
            glVertex3f(256, 256, 0.0f);
            glVertex3f(0, 256, 0.0f);
            glEnd();
            glMatrixMode(GL_PROJECTION);
            glPopMatrix();
            glMatrixMode(GL_MODELVIEW);
            glPopMatrix();
            glEnable(GL_DEPTH_TEST);
            glEnable(GL_CULL_FACE);
            glDisable(GL_BLEND);
            glDepthMask(1);
        }

        swap_gl_buffers();
    }
}

void FadeLoadingScreen(float howmuch)
{
    static float loadprogress;

    glLoadIdentity();
    //Clear to black
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    loadprogress = howmuch;

    //Background

    glDisable(GL_TEXTURE_2D);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_LIGHTING);
    glDepthMask(0);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, screenwidth, 0, screenheight, -100, 100);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glTranslatef(screenwidth / 2, screenheight / 2, 0);

    glScalef((float)screenwidth / 2, (float)screenheight / 2, 1);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_BLEND);
    glColor4f(loadprogress / 100, 0, 0, 1);
    glPushMatrix();
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0);
    glVertex3f(-1, -1, 0.0f);
    glTexCoord2f(1, 0);
    glVertex3f(1, -1, 0.0f);
    glTexCoord2f(1, 1);
    glVertex3f(1, 1, 0.0f);
    glTexCoord2f(0, 1);
    glVertex3f(-1, 1, 0.0f);
    glEnd();
    glPopMatrix();
    glDisable(GL_TEXTURE_2D);
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glDisable(GL_BLEND);
    glDepthMask(1);

    LOG("\tE");

    //Text
    swap_gl_buffers();

    LOG("\tF");
}

void Game::InitGame()
{
    LOG_TOGGLE(false);
    std::cout << "InitGame A\n";

    LOGFUNC;

    numchallengelevels = 14;

    Account::loadFile(Folders::getUserLoadPath());

    std::cout << "InitGame B\n";

    //need to implement instancing to up this number w/o tanking perf
    Sprite::AllocSprites(200);


    whichjointstartarray[0] = righthip;
    whichjointendarray[0] = rightfoot;

    whichjointstartarray[1] = righthip;
    whichjointendarray[1] = rightankle;

    whichjointstartarray[2] = righthip;
    whichjointendarray[2] = rightknee;

    whichjointstartarray[3] = rightknee;
    whichjointendarray[3] = rightankle;

    whichjointstartarray[4] = rightankle;
    whichjointendarray[4] = rightfoot;

    whichjointstartarray[5] = lefthip;
    whichjointendarray[5] = leftfoot;

    whichjointstartarray[6] = lefthip;
    whichjointendarray[6] = leftankle;

    whichjointstartarray[7] = lefthip;
    whichjointendarray[7] = leftknee;

    whichjointstartarray[8] = leftknee;
    whichjointendarray[8] = leftankle;

    whichjointstartarray[9] = leftankle;
    whichjointendarray[9] = leftfoot;

    whichjointstartarray[10] = abdomen;
    whichjointendarray[10] = rightshoulder;

    whichjointstartarray[11] = abdomen;
    whichjointendarray[11] = rightelbow;

    whichjointstartarray[12] = abdomen;
    whichjointendarray[12] = rightwrist;

    whichjointstartarray[13] = abdomen;
    whichjointendarray[13] = righthand;

    whichjointstartarray[14] = rightshoulder;
    whichjointendarray[14] = rightelbow;

    whichjointstartarray[15] = rightelbow;
    whichjointendarray[15] = rightwrist;

    whichjointstartarray[16] = rightwrist;
    whichjointendarray[16] = righthand;

    whichjointstartarray[17] = abdomen;
    whichjointendarray[17] = leftshoulder;

    whichjointstartarray[18] = abdomen;
    whichjointendarray[18] = leftelbow;

    whichjointstartarray[19] = abdomen;
    whichjointendarray[19] = leftwrist;

    whichjointstartarray[20] = abdomen;
    whichjointendarray[20] = lefthand;

    whichjointstartarray[21] = leftshoulder;
    whichjointendarray[21] = leftelbow;

    whichjointstartarray[22] = leftelbow;
    whichjointendarray[22] = leftwrist;

    whichjointstartarray[23] = leftwrist;
    whichjointendarray[23] = lefthand;

    whichjointstartarray[24] = abdomen;
    whichjointendarray[24] = neck;

    whichjointstartarray[25] = neck;
    whichjointendarray[25] = head;

    std::cout << "InitGame C\n";

    LOG("FadeLoadingScreen...");

    FadeLoadingScreen(0);

    std::cout << "InitGame D\n";

    stillloading = 1;

    int temptexdetail = texdetail;
    texdetail = 1;
    text->LoadFontTexture("Textures/Font.png");
    
    std::cout << "InitGame E\n";

    text->BuildFont();

    std::cout << "InitGame F\n";

    textmono->LoadFontTexture("Textures/FontMono.png");
    textmono->BuildFont();
    texdetail = temptexdetail;

    std::cout << "InitGame G\n";    

    FadeLoadingScreen(10);

    std::cout << "InitGame H\n";    

    if (detail == 2) {
        texdetail = 1;
    }
    if (detail == 1) {
        texdetail = 2;
    }
    if (detail == 0) {
        texdetail = 4;
    }

    LOG("Initializing sound system...");

    OPENAL_Init(44100, 32, 0);

    OPENAL_SetSFXMasterVolume((int)(volume * 255));
    loadAllSounds();

    if (musictoggle) {
        emit_stream_np(stream_menutheme);
    }

    cursortexture.load("Textures/Cursor.png", 0);
    LOG("Loaded cursor!");

    Mapcircletexture.load("Textures/MapCircle.png", 0);
    Mapboxtexture.load("Textures/MapBox.png", 0);
    Maparrowtexture.load("Textures/MapArrow.png", 0);

    temptexdetail = texdetail;
    if (texdetail > 2) {
        texdetail = 2;
    }
    Mainmenuitems[0].load("Textures/Lugaru.png", 0);
    Mainmenuitems[1].load("Textures/NewGame.png", 0);
    Mainmenuitems[2].load("Textures/Options.png", 0);
    Mainmenuitems[3].load("Textures/Quit.png", 0);
    Mainmenuitems[4].load("Textures/Eyelid.png", 0);
    Mainmenuitems[5].load("Textures/Resume.png", 0);
    Mainmenuitems[6].load("Textures/EndGame.png", 0);

    LOG("Loaded all menu textures");

    texdetail = temptexdetail;

    FadeLoadingScreen(95);

    gameon = 0;
    mainmenu = 1;

    stillloading = 0;
    firstLoadDone = false;

    newdetail = detail;
    newscreenwidth = screenwidth;
    newscreenheight = screenheight;

    LOG("Menu::Load()");

    Menu::Load();

    LOG("Loading all animations...");

    Animation::loadAll();

    LOG("Loading PersonType...");

    PersonType::Load();

    LOG("Creating Player...");
    Person *player = new Person();
    int patchx = player->whichpatchx;
    int patchz = player->whichpatchz;
    LOG("Player(INIT) patch X: %d\tZ: %d", patchx, patchz);
    Person::players.emplace_back(player);
 
    LOG("InitGame Done!!");
}

void Game::LoadScreenTexture()
{
    //VITAGL: TODO
    //glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    if (!Game::screentexture) {
        glGenTextures(1, &Game::screentexture);
    }
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, Game::screentexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 0, 0, kTextureSize, kTextureSize, 0);
}

//TODO: move LoadStuff() closer to GameTick.cpp to get rid of various vars shared in Game.hpp
/* Loads models and textures which only needs to be loaded once */
void Game::LoadStuff()
{
    LOG_TOGGLE(true);
    MICROPROFILE_SCOPEI("Game", "LoadStuff", 0xffff3456);
    FileCache::InitCache();
    Model::initModelCache();
    LOG("Game::LoadStuff()");

    std::vector<std::tuple<WorkerThread::JobHandle, Texture*>> loadTexJobs;

    float temptexdetail;
    float viewdistdetail;
    float megascale = 1;

    LOGFUNC;

    loadtime = 0;

    stillloading = 1;

    visibleloading = false; //don't use loadscreentexture yet
    loadscreentexture.load("Textures/Fire.jpg", 1);
    visibleloading = true;

    temptexdetail = texdetail;
    texdetail = 1;
    text->LoadFontTexture("Textures/Font.png");
    text->BuildFont();
    textmono->LoadFontTexture("Textures/FontMono.png");
    textmono->BuildFont();
    texdetail = temptexdetail;

    viewdistdetail = 2;
    viewdistance = 50 * megascale * viewdistdetail;

    if (detail == 2) {
        texdetail = 1;
        kTextureSize = 1024;
    } else if (detail == 1) {
        texdetail = 2;
        kTextureSize = 512;
    } else {
        texdetail = 4;
        kTextureSize = 256;
    }

    realtexdetail = texdetail;

    Weapon::Load();

    /*
    terrain.shadowtexture.load("Textures/Shadow.png", 0);
    terrain.bloodtexture.load("Textures/Blood.png", 0);
    terrain.breaktexture.load("Textures/Break.png", 0);
    terrain.bloodtexture2.load("Textures/Blood.png", 0);

    terrain.footprinttexture.load("Textures/Footprint.png", 0);
    terrain.bodyprinttexture.load("Textures/Bodyprint.png", 0);
    hawktexture.load("Textures/Hawk.png", 0);

    Sprite::cloudtexture.load("Textures/Cloud.png", 1);
    Sprite::cloudimpacttexture.load("Textures/CloudImpact.png", 1);
    Sprite::bloodtexture.load("Textures/BloodParticle.png", 1);
    Sprite::snowflaketexture.load("Textures/SnowFlake.png", 1);
    Sprite::flametexture.load("Textures/Flame.png", 1);
    Sprite::bloodflametexture.load("Textures/BloodFlame.png", 1);
    Sprite::smoketexture.load("Textures/Smoke.png", 1);
    Sprite::shinetexture.load("Textures/Shine.png", 1);
    Sprite::splintertexture.load("Textures/Splinter.png", 1);
    Sprite::leaftexture.load("Textures/Leaf.png", 1);
    Sprite::toothtexture.load("Textures/Tooth.png", 1);
    */

    loadTexJobs.emplace_back(terrain.shadowtexture.submitLoadJob("Textures/Shadow.png", 0), &terrain.shadowtexture);
    loadTexJobs.emplace_back(terrain.bloodtexture.submitLoadJob("Textures/Blood.png", 0), &terrain.bloodtexture);
    loadTexJobs.emplace_back(terrain.breaktexture.submitLoadJob("Textures/Break.png", 0), &terrain.breaktexture);
    loadTexJobs.emplace_back(terrain.bloodtexture2.submitLoadJob("Textures/Blood.png", 0), &terrain.bloodtexture2);
    loadTexJobs.emplace_back(terrain.footprinttexture.submitLoadJob("Textures/Footprint.png", 0), &terrain.footprinttexture);
    loadTexJobs.emplace_back(terrain.bodyprinttexture.submitLoadJob("Textures/Bodyprint.png", 0), &terrain.bodyprinttexture);
    loadTexJobs.emplace_back(hawktexture.submitLoadJob("Textures/Hawk.png", 0), &hawktexture);
    loadTexJobs.emplace_back(Sprite::cloudtexture.submitLoadJob("Textures/Cloud.png", 1), &Sprite::cloudtexture);
    loadTexJobs.emplace_back(Sprite::cloudimpacttexture.submitLoadJob("Textures/CloudImpact.png", 1), &Sprite::cloudimpacttexture);
    loadTexJobs.emplace_back(Sprite::bloodtexture.submitLoadJob("Textures/BloodParticle.png", 1), &Sprite::bloodtexture);
    loadTexJobs.emplace_back(Sprite::snowflaketexture.submitLoadJob("Textures/SnowFlake.png", 1), &Sprite::snowflaketexture);
    loadTexJobs.emplace_back(Sprite::flametexture.submitLoadJob("Textures/Flame.png", 1), &Sprite::flametexture);
    loadTexJobs.emplace_back(Sprite::bloodflametexture.submitLoadJob("Textures/BloodFlame.png", 1), &Sprite::bloodflametexture);
    loadTexJobs.emplace_back(Sprite::smoketexture.submitLoadJob("Textures/Smoke.png", 1), &Sprite::smoketexture);
    loadTexJobs.emplace_back(Sprite::shinetexture.submitLoadJob("Textures/Shine.png", 1), &Sprite::shinetexture);
    loadTexJobs.emplace_back(Sprite::splintertexture.submitLoadJob("Textures/Splinter.png", 1), &Sprite::splintertexture);
    loadTexJobs.emplace_back(Sprite::leaftexture.submitLoadJob("Textures/Leaf.png", 1), &Sprite::leaftexture);
    loadTexJobs.emplace_back(Sprite::toothtexture.submitLoadJob("Textures/Tooth.png", 1), &Sprite::toothtexture);

    yaw = 0;
    pitch = 0;
    ReSizeGLScene(90, .01);

    viewer = 0;

    //Set up distant light
    light.color[0] = .95;
    light.color[1] = .95;
    light.color[2] = 1;
    light.ambient[0] = .2;
    light.ambient[1] = .2;
    light.ambient[2] = .24;
    light.location.x = 1;
    light.location.y = 1;
    light.location.z = -.2;
    Normalise(&light.location);

    LoadingScreen();

    SetUpLighting();

    fadestart = .6;
    gravity = -10;

    texscale = .2 / megascale / viewdistdetail;
    terrain.scale = 3 * megascale * viewdistdetail;

    viewer.x = terrain.size / 2 * terrain.scale;
    viewer.z = terrain.size / 2 * terrain.scale;

    hawk.load("Models/Hawk.solid");
    hawk.Scale(.03, .03, .03);
    hawk.Rotate(90, 1, 1);
    hawk.CalculateNormals(0);
    hawk.ScaleNormals(-1, -1, -1);
    hawkcoords.x = terrain.size / 2 * terrain.scale - 5 - 7;
    hawkcoords.z = terrain.size / 2 * terrain.scale - 5 - 7;
    hawkcoords.y = terrain.getHeight(hawkcoords.x, hawkcoords.z) + 25;

    eye.load("Models/Eye.solid");
    eye.Scale(.03, .03, .03);
    eye.CalculateNormals(0);

    cornea.load("Models/Cornea.solid");
    cornea.Scale(.03, .03, .03);
    cornea.CalculateNormals(0);

    iris.load("Models/Iris.solid");
    iris.Scale(.03, .03, .03);
    iris.CalculateNormals(0);

    LoadSave("Textures/WolfBloodFur.png", &PersonType::types[wolftype].bloodText[0]);
    LoadSave("Textures/BloodFur.png", &PersonType::types[rabbittype].bloodText[0]);

    oldenvironment = -4;

    gameon = 1;
    mainmenu = 0;

    //Fix knife stab, too lazy to do it manually
    XYZ moveamount;
    moveamount = 0;
    moveamount.z = 2;
    // FIXME - Why this uses skeleton.joints.size() and not Animation::numjoints? (are they equal?)
    // It seems skeleton.joints.size() is 0 at this point, so this is useless.
    for (unsigned i = 0; i < Person::players[0]->skeleton.joints.size(); i++) {
        for (unsigned j = 0; j < Animation::animations[knifesneakattackanim].frames.size(); j++) {
            Animation::animations[knifesneakattackanim].frames[j].joints[i].position += moveamount;
        }
    }

    LoadingScreen();

    for (unsigned i = 0; i < Person::players[0]->skeleton.joints.size(); i++) {
        for (unsigned j = 0; j < Animation::animations[knifesneakattackedanim].frames.size(); j++) {
            Animation::animations[knifesneakattackedanim].frames[j].joints[i].position += moveamount;
        }
    }

    LoadingScreen();

    for (unsigned i = 0; i < Person::players[0]->skeleton.joints.size(); i++) {
        Animation::animations[dead1anim].frames[1].joints[i].position = Animation::animations[dead1anim].frames[0].joints[i].position;
        Animation::animations[dead2anim].frames[1].joints[i].position = Animation::animations[dead2anim].frames[0].joints[i].position;
        Animation::animations[dead3anim].frames[1].joints[i].position = Animation::animations[dead3anim].frames[0].joints[i].position;
        Animation::animations[dead4anim].frames[1].joints[i].position = Animation::animations[dead4anim].frames[0].joints[i].position;
    }
    Animation::animations[dead1anim].frames[0].speed = 0.001;
    Animation::animations[dead2anim].frames[0].speed = 0.001;
    Animation::animations[dead3anim].frames[0].speed = 0.001;
    Animation::animations[dead4anim].frames[0].speed = 0.001;

    Animation::animations[dead1anim].frames[1].speed = 0.001;
    Animation::animations[dead2anim].frames[1].speed = 0.001;
    Animation::animations[dead3anim].frames[1].speed = 0.001;
    Animation::animations[dead4anim].frames[1].speed = 0.001;

    for (unsigned i = 0; i < Person::players[0]->skeleton.joints.size(); i++) {
        for (unsigned j = 0; j < Animation::animations[swordsneakattackanim].frames.size(); j++) {
            Animation::animations[swordsneakattackanim].frames[j].joints[i].position += moveamount;
        }
    }
    LoadingScreen();
    for (unsigned j = 0; j < Animation::animations[swordsneakattackanim].frames.size(); j++) {
        Animation::animations[swordsneakattackanim].frames[j].weapontarget += moveamount;
    }

    LoadingScreen();

    for (unsigned i = 0; i < Person::players[0]->skeleton.joints.size(); i++) {
        for (unsigned j = 0; j < Animation::animations[swordsneakattackedanim].frames.size(); j++) {
            Animation::animations[swordsneakattackedanim].frames[j].joints[i].position += moveamount;
        }
    }

    LoadingScreen();

    if (!screentexture) {
        LoadScreenTexture();
    }

    if (targetlevel != 7) {
        emit_sound_at(fireendsound);
    }

    stillloading = 0;
    loading = 0;
    changedelay = 1;

    visibleloading = false;
    firstLoadDone = true;


    for(auto &it: loadTexJobs){
        WorkerThread::join(std::get<0>(it), true);
        std::get<1>(it)->upload();
    }

    Model::clearModelCache();
    FileCache::ClearCache();
}
