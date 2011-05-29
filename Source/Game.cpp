#include "Game.h"
#include "openal_wrapper.h"
#include "SDL_thread.h"

extern int mainmenu;

int numdialogues;
int numdialogueboxes[max_dialogues];
int dialoguetype[max_dialogues];
int dialogueboxlocation[max_dialogues][max_dialoguelength];
float dialogueboxcolor[max_dialogues][max_dialoguelength][3];
int dialogueboxsound[max_dialogues][max_dialoguelength];
char dialoguetext[max_dialogues][max_dialoguelength][128];
char dialoguename[max_dialogues][max_dialoguelength][64];
XYZ dialoguecamera[max_dialogues][max_dialoguelength];
XYZ participantlocation[max_dialogues][10];
int participantfocus[max_dialogues][max_dialoguelength];
int participantaction[max_dialogues][max_dialoguelength];
float participantrotation[max_dialogues][10];
XYZ participantfacing[max_dialogues][max_dialoguelength][10];
float dialoguecamerarotation[max_dialogues][max_dialoguelength];
float dialoguecamerarotation2[max_dialogues][max_dialoguelength];
int indialogue;
int whichdialogue;
int directing;
float dialoguetime;
int dialoguegonethrough[20];

namespace Game{
    GLuint terraintexture;
    GLuint terraintexture2;
    GLuint terraintexture3;
    GLuint screentexture;
    GLuint screentexture2;
    GLuint logotexture;
    GLuint loadscreentexture;
    GLuint Maparrowtexture;
    GLuint Mapboxtexture;
    GLuint Mapcircletexture;
    GLuint cursortexture;
    GLuint Mainmenuitems[10];

    int selected;
    int keyselect;
    int indemo;

    bool won;

    bool entername;

    char menustring[100][256];
    char registrationname[256];
    float registrationnumber;

    int newdetail;
    int newscreenwidth;
    int newscreenheight;

    bool gameon;
    float deltah,deltav;
    int mousecoordh,mousecoordv;
    int oldmousecoordh,oldmousecoordv;
    float rotation,rotation2;
    SkyBox skybox;
    bool cameramode;
    int olddrawmode;
    int drawmode;
    bool firstload;
    bool oldbutton;

    float leveltime;
    float loadtime;

    Model hawk;
    XYZ hawkcoords;
    XYZ realhawkcoords;
    GLuint hawktexture;
    float hawkrotation;
    float hawkcalldelay;

    Model eye;
    Model iris;
    Model cornea;

    bool stealthloading;

    std::vector<CampaignLevel> campaignlevels;
    int whichchoice;
    int actuallevel;
    bool winhotspot;
    bool windialogue;

    bool minimap;

    int musictype,oldmusictype,oldoldmusictype;
    bool realthreat;

    Model rabbit;
    XYZ rabbitcoords;

    XYZ mapcenter;
    float mapradius;

    Text* text;
    float fps;

    XYZ cameraloc;
    float cameradist;

    int drawtoggle;

    bool editorenabled;
    int editortype;
    float editorsize;
    float editorrotation;
    float editorrotation2;

    float brightness;

    int quit;
    int tryquit;

    XYZ pathpoint[30];
    int numpathpoints;
    int numpathpointconnect[30];
    int pathpointconnect[30][30];
    int pathpointselected;

    int endgame;
    bool scoreadded;
    int numchallengelevels;

    bool console;
    int archiveselected;
    char consoletext[15][256];
    int consolechars[15];
    bool chatting;
    char displaytext[15][256];
    int displaychars[15];
    float displaytime[15];
    float displayblinkdelay;
    bool displayblink;
    int displayselected;
    bool consolekeydown;
    float consoleblinkdelay;
    bool consoleblink;
    int consoleselected;
    bool autocam;

    unsigned short crouchkey,jumpkey,forwardkey,chatkey,backkey,leftkey,rightkey,drawkey,throwkey,attackkey;
    unsigned short consolekey;
    bool oldattackkey;

    int loading;
    float talkdelay;
    
    int numboundaries;
    XYZ boundary[360];

    int whichlevel;
    int oldenvironment;
    int targetlevel;
    float changedelay;

    float musicvolume[4];
    float oldmusicvolume[4];
    int musicselected;
    int change;

    bool waiting;
    Account* accountactive;
}

void Game::newGame()
{
    text=NULL;
    text=new Text();

	terraintexture = 0;
	terraintexture2 = 0;
	terraintexture3 = 0;
	screentexture = 0;
	screentexture2 = 0;
	logotexture = 0;
	loadscreentexture = 0;
	Maparrowtexture = 0;
	Mapboxtexture = 0;
	Mapcircletexture = 0;
	cursortexture = 0;

	memset(Mainmenuitems, 0, sizeof(Mainmenuitems));

	selected = 0;
	keyselect = 0;
	indemo = 0;

	won = 0;

	entername = 0;

	memset(menustring, 0, sizeof(menustring));
	memset(registrationname, 0, sizeof(registrationname));
	registrationnumber = 0;

	newdetail = 0;
	newscreenwidth = 0;
	newscreenheight = 0;

	gameon = 0;
	deltah = 0,deltav = 0;
	mousecoordh = 0,mousecoordv = 0;
	oldmousecoordh = 0,oldmousecoordv = 0;
	rotation = 0,rotation2 = 0;

//	SkyBox skybox;

	cameramode = 0;
	olddrawmode = 0;
	drawmode = 0;
	firstload = 0;
	oldbutton = 0;

	leveltime = 0;
	loadtime = 0;

//	Model hawk;

//	XYZ hawkcoords;
//	XYZ realhawkcoords;

	hawktexture = 0;
	hawkrotation = 0;
	hawkcalldelay = 0;
/*
	Model eye;
	Model iris;
	Model cornea;
*/
	stealthloading = 0;

	whichchoice = 0;
	actuallevel = 0;
	winhotspot = false;
	windialogue = false;

	minimap = 0;

	musictype = 0,oldmusictype = 0,oldoldmusictype = 0;
	realthreat = 0;

//	Model rabbit;
//	XYZ rabbitcoords;

//	XYZ mapcenter;
	mapradius = 0;

//	Text text;
	fps = 0;

//	XYZ cameraloc;
	cameradist = 0;

	drawtoggle = 0;

	editorenabled = 0;
	editortype = 0;
	editorsize = 0;
	editorrotation = 0;
	editorrotation2 = 0;

	brightness = 0;

	quit = 0;
	tryquit = 0;

//	XYZ pathpoint[30];
	numpathpoints = 0;
	memset(numpathpointconnect, 0, sizeof(numpathpointconnect));
	memset(pathpointconnect, 0, sizeof(pathpointconnect));
	pathpointselected = 0;

	endgame = 0;
	scoreadded = 0;
	numchallengelevels = 0;

	console = false;
	archiveselected = 0;

	memset(consoletext, 0, sizeof(consoletext));
	memset(consolechars, 0, sizeof(consolechars));
	chatting = 0;
	memset(displaytext, 0, sizeof(displaytext));
	memset(displaychars, 0, sizeof(displaychars));
	memset(displaytime, 0, sizeof(displaytime));
	displayblinkdelay = 0;
	displayblink = 0;
	displayselected = 0;
	consolekeydown = 0;
	consoleblinkdelay = 0;
	consoleblink = 0;
	consoleselected = 0;
	autocam = 0;

	crouchkey = 0,jumpkey = 0,forwardkey = 0,chatkey = 0,backkey = 0,leftkey = 0,rightkey = 0,drawkey = 0,throwkey = 0,attackkey = 0;
	consolekey = 0;
	oldattackkey = 0;

	loading = 0;
	talkdelay = 0;

	numboundaries = 0;
//	XYZ boundary[360];

	whichlevel = 0;
	oldenvironment = 0;
	targetlevel = 0;
	changedelay = 0;

	memset(musicvolume, 0, sizeof(musicvolume));
	memset(oldmusicvolume, 0, sizeof(oldmusicvolume));
	musicselected = 0;
	change = 0;
	
//------------

	waiting = false;
	mainmenu = 0;
	
	accountactive = NULL;
}

void Game::deleteGame(){
    if(text)
        delete text;
    for(int i=0;i<10;i++){
        if(Mainmenuitems[i])glDeleteTextures( 1, &Mainmenuitems[i] );
    }
    glDeleteTextures( 1, &cursortexture );
    glDeleteTextures( 1, &Maparrowtexture );
    glDeleteTextures( 1, &Mapboxtexture );
    glDeleteTextures( 1, &Mapcircletexture );
    glDeleteTextures( 1, &terraintexture );
    glDeleteTextures( 1, &terraintexture2 );
    if(screentexture>0)glDeleteTextures( 1, &screentexture );
    if(screentexture2>0)glDeleteTextures( 1, &screentexture2 );
    glDeleteTextures( 1, &hawktexture );
    glDeleteTextures( 1, &logotexture );
    glDeleteTextures( 1, &loadscreentexture );

    Dispose();
}

void Game::fireSound(int sound) {
	emit_sound_at(sound);
}

void Game::inputText(char* str, int* charselected, int* nb_chars) {
	SDL_Event evenement;
	
	if(!waiting) {
		waiting=true;
		SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY,SDL_DEFAULT_REPEAT_INTERVAL);
		SDL_EnableUNICODE(true);
	}

	while(SDL_PollEvent(&evenement)) {
	
		switch(evenement.type) {
			case SDL_KEYDOWN:
				if(evenement.key.keysym.sym == SDLK_ESCAPE) {
					for(int i=0;i<255;i++)
						str[i]=0;
					*nb_chars=0;
					*charselected=0;
					waiting=false;
				} else if(evenement.key.keysym.sym==SDLK_BACKSPACE){
					if((*charselected)!=0) {
						for(int i=(*charselected)-1;i<255;i++)
							str[i]=str[i+1];
						str[255]=0;
						(*charselected)--;
						(*nb_chars)--;
					}
				} else if(evenement.key.keysym.sym==SDLK_DELETE){
					if((*charselected)<(*nb_chars)){
						for(int i=(*charselected);i<255;i++)
							str[i]=str[i+1];
						str[255]=0;
						(*nb_chars)--;
					}
				} else if(evenement.key.keysym.sym==SDLK_HOME){
					(*charselected)=0;
				} else if(evenement.key.keysym.sym==SDLK_END){
					(*charselected)=(*nb_chars);
				} else if(evenement.key.keysym.sym==SDLK_LEFT){
					if((*charselected)!=0)
						(*charselected)--;
				} else if(evenement.key.keysym.sym==SDLK_RIGHT){
					if((*charselected)<(*nb_chars))
						(*charselected)++;
				} else if(evenement.key.keysym.sym==SDLK_RETURN) {
					waiting=false;
				} else if(evenement.key.keysym.unicode>=32&&evenement.key.keysym.unicode<127&&(*nb_chars)<60){
					for(int i=255;i>=(*charselected)+1;i--)
						str[i]=str[i-1];
					str[*charselected]=evenement.key.keysym.unicode;
					(*charselected)++;
					(*nb_chars)++;
				}
			break;
		}
	}
	
	if(!waiting) {
		SDL_EnableKeyRepeat(0,0); // disable key repeat
		SDL_EnableUNICODE(false);
	}
}

void Game::setKeySelected() {
	waiting=true;
    printf("launch thread\n");
	SDL_Thread* thread = SDL_CreateThread(Game::setKeySelected_thread,NULL);
    if ( thread == NULL ) {
        fprintf(stderr, "Unable to create thread: %s\n", SDL_GetError());
		waiting=false;
        return;
    }
}

int Game::setKeySelected_thread(void* data) {
	int keycode=-1;
	SDL_Event evenement;
	while(keycode==-1) {
		SDL_WaitEvent(&evenement);
		switch(evenement.type) {
			case SDL_KEYDOWN:
				keycode = evenement.key.keysym.sym;
			break;
			case SDL_MOUSEBUTTONDOWN:
				keycode = SDLK_LAST+evenement.button.button;
			break;
			default:
			break;
		}
	}
	if(keycode != SDLK_ESCAPE) {
		fireSound();
		switch(keyselect) {
			case 0: forwardkey=keycode;
			break;
			case 1: backkey=keycode;
			break;
			case 2: leftkey=keycode;
			break;
			case 3: rightkey=keycode;
			break;
			case 4: crouchkey=keycode;
			break;
			case 5: jumpkey=keycode;
			break;
			case 6: drawkey=keycode;
			break;
			case 7: throwkey=keycode;
			break;
			case 8: attackkey=keycode;
			break;
			case 9: consolekey=keycode;
			break;
			default:
			break;
		}
	}
	keyselect=-1;
	waiting=false;
    return 0;
}

void Game::DrawGL() {
	if ( stereomode == stereoNone ) {
		DrawGLScene(stereoCenter);
	} else {
		DrawGLScene(stereoLeft);
		DrawGLScene(stereoRight);
	}
}
