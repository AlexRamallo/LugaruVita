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

#include "Utils/Input.hpp"
#include "SDL2.h"
#include <assert.h>

#include "Thirdparty/microprofile/microprofile.h"

bool keyDown[SDL_NUM_SCANCODES + 6];
bool keyPressed[SDL_NUM_SCANCODES + 6];


static SDL_GameController *getController(){
	static SDL_GameController *_controller = nullptr;
	if(_controller == nullptr){
		_controller = SDL_GameControllerOpen(0);
		if(_controller == NULL){
			LOG("getController failed: %s", SDL_GetError());
			assert(0 && "Failed to get a controller from SDL");
		}
	}
	return _controller;
}

void Input::Tick()
{
	SDL_PumpEvents();
#ifndef PLATFORM_VITA
	int numkeys;
	const Uint8* keyState = SDL_GetKeyboardState(&numkeys);
	for (int i = 0; i < numkeys; i++) {
		keyPressed[i] = !keyDown[i] && keyState[i];
		keyDown[i] = keyState[i];
	}
#endif
	SDL_GameController *ctl = getController();

	Uint8 mb = SDL_GetMouseState(NULL, NULL);
	for (int i = 1; i < 6; i++) {
		keyPressed[SDL_NUM_SCANCODES + i] = !keyDown[SDL_NUM_SCANCODES + i] && (mb & SDL_BUTTON(i));
		keyDown[SDL_NUM_SCANCODES + i] = (mb & SDL_BUTTON(i));
	}

	#define handle_key(k, s){\
		int st = SDL_GameControllerGetButton(ctl, s);\
		keyPressed[k] = !keyDown[k] && st;\
		keyDown[k] = st;\
	}

	handle_key(Game::leftkey, SDL_CONTROLLER_BUTTON_DPAD_LEFT);
	handle_key(Game::rightkey, SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
	handle_key(Game::forwardkey, SDL_CONTROLLER_BUTTON_DPAD_UP);
	handle_key(Game::backkey, SDL_CONTROLLER_BUTTON_DPAD_DOWN);

	handle_key(Game::jumpkey, SDL_CONTROLLER_BUTTON_A);
	handle_key(Game::crouchkey, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);
	handle_key(Game::drawkey, SDL_CONTROLLER_BUTTON_Y);
	handle_key(Game::attackkey, SDL_CONTROLLER_BUTTON_X);
	handle_key(Game::throwkey, SDL_CONTROLLER_BUTTON_B);
	
	handle_key(Game::startkey, SDL_CONTROLLER_BUTTON_START);
	handle_key(Game::selectkey, SDL_CONTROLLER_BUTTON_BACK);

	if(Game::analog_lh > 0.25){
		keyDown[Game::leftkey] = false;
		keyDown[Game::rightkey] = true;
	}else if(Game::analog_lh < -0.25){
		keyDown[Game::leftkey] = true;
		keyDown[Game::rightkey] = false;
	}

	if(Game::analog_lv > 0.25){
		keyDown[Game::forwardkey] = false;
		keyDown[Game::backkey] = true;
	}else if(Game::analog_lv < -0.25){
		keyDown[Game::forwardkey] = true;
		keyDown[Game::backkey] = false;
	}

    #if MICROPROFILE_ENABLED
    static int mpst = 0;

    if(SDL_GameControllerGetButton(ctl, SDL_CONTROLLER_BUTTON_LEFTSHOULDER)
    	&& SDL_GameControllerGetButton(ctl, SDL_CONTROLLER_BUTTON_Y)){
    	if(mpst == 0){
    		mpst = 1;
    	}
    }else{
    	mpst = 0;
    }

    if(mpst == 1){
    	mpst = 2;
        const char *dest = "ux0:data/microprofile_dump.html";
        MicroProfileDumpFile(dest, MicroProfileDumpTypeHtml, 100);
        SDL_Delay(500);
    }
    #endif
}

bool Input::isKeyDown(int k)
{
	if (k >= SDL_NUM_SCANCODES + 6) {
		return false;
	}
	return keyDown[k];
}

bool Input::isKeyPressed(int k)
{
	if (k >= SDL_NUM_SCANCODES + 6) {
		return false;
	}
	return keyPressed[k];
}

const char* Input::keyToChar(unsigned short i)
{
	if (i < SDL_NUM_SCANCODES) {
		return SDL_GetKeyName(SDL_GetKeyFromScancode(SDL_Scancode(i)));
	} else if (i == MOUSEBUTTON_LEFT) {
		return "mouse left button";
	} else if (i == MOUSEBUTTON_RIGHT) {
		return "mouse right button";
	} else if (i == MOUSEBUTTON_MIDDLE) {
		return "mouse middle button";
	} else if (i == MOUSEBUTTON_X1) {
		return "mouse button 4";
	} else if (i == MOUSEBUTTON_X2) {
		return "mouse button 5";
	} else {
		return "unknown";
	}
}

bool Input::MouseClicked()
{
	return isKeyPressed(MOUSEBUTTON_LEFT);
}
