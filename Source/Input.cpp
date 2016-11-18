/*
Copyright (C) 2003, 2010 - Wolfire Games
Copyright (C) 2010 - Côme <MCMic> BERNIGAUD

This file is part of Lugaru.

Lugaru is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

/**> HEADER FILES <**/
#include "Input.h"

extern bool keyboardfrozen;

bool keyDown[SDL_NUM_SCANCODES + 6];
bool keyPressed[SDL_NUM_SCANCODES + 6];

void Input::Tick()
{
    SDL_PumpEvents();
    int numkeys;
    const Uint8 *keyState = SDL_GetKeyboardState(&numkeys);
    for (int i = 0; i < numkeys; i++) {
        keyPressed[i] = !keyDown[i] && keyState[i];
        keyDown[i] = keyState[i];
    }
    Uint8 mb = SDL_GetMouseState(NULL, NULL);
    for (int i = 1; i < 6; i++) {
        keyPressed[SDL_NUM_SCANCODES + i] = !keyDown[SDL_NUM_SCANCODES + i] && (mb & SDL_BUTTON(i));
        keyDown[SDL_NUM_SCANCODES + i] = (mb & SDL_BUTTON(i));
    }
}

bool Input::isKeyDown(int k)
{
    if (keyboardfrozen || k >= SDL_NUM_SCANCODES + 6) // really useful? check that.
        return false;
    return keyDown[k];
}

bool Input::isKeyPressed(int k)
{
    if (keyboardfrozen || k >= SDL_NUM_SCANCODES + 6)
        return false;
    return keyPressed[k];
}

const char* Input::keyToChar(unsigned short i)
{
    if (i < SDL_NUM_SCANCODES)
        return SDL_GetScancodeName(SDL_Scancode(i));
    else if (i == MOUSEBUTTON1)
        return "mouse1";
    else if (i == MOUSEBUTTON2)
        return "mouse2";
    else if (i == MOUSEBUTTON3)
        return "mouse3";
    else
        return "unknown";
}

unsigned short Input::CharToKey(const char* which)
{
    for (unsigned short i = 0; i < SDL_NUM_SCANCODES; i++) {
        if (!strcasecmp(which, SDL_GetScancodeName(SDL_Scancode(i))))
            return i;
    }
    if (!strcasecmp(which, "mouse1")) {
        return MOUSEBUTTON1;
    }
    if (!strcasecmp(which, "mouse2")) {
        return MOUSEBUTTON2;
    }
    if (!strcasecmp(which, "mouse3")) {
        return MOUSEBUTTON3;
    }
    return SDL_NUM_SCANCODES;
}

Boolean Input::MouseClicked()
{
    return isKeyPressed(SDL_NUM_SCANCODES + SDL_BUTTON_LEFT);
}
