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

#ifndef _SPRITE_HPP_
#define _SPRITE_HPP_

#include "Environment/Lights.hpp"
#include "Environment/Terrain.hpp"
#include "Graphic/Texture.hpp"
#include "Graphic/gamegl.hpp"
#include "Math/Frustum.hpp"
#include "Math/XYZ.hpp"
#include "Objects/Object.hpp"
#include "Utils/ImageIO.hpp"
#include "Utils/WorkerThread.hpp"
#include "Thirdparty/vitagl/math_utils.h"

#include <vector>

enum
{
    cloudsprite = 0,
    bloodsprite,
    flamesprite,
    smoketype,
    weaponflamesprite,
    cloudimpactsprite,
    snowsprite,
    weaponshinesprite,
    bloodflamesprite,
    breathsprite,
    splintersprite,
    spritenumber
};

class Sprite
{
private:
    XYZ oldposition;
    XYZ position;
    XYZ velocity;
    float size;
    float initialsize;
    int type;
    int special;
    float color[3];
    float opacity;
    float rotation;
    float alivetime;
    float speed;
    float rotatespeed;
    bool alive;

    static float checkdelay;

    float calcDistanceMult(Sprite *spr);
public:
    static void AllocSprites(int count);

    static void submitAnimationJob(std::vector<WorkerThread::JobHandle> &out);

    static void doAnimate(int start_idx, int end_idx);

    static void DeleteSprite(int which);
    static void MakeSprite(int atype, XYZ where, XYZ avelocity, float red, float green, float blue, float asize, float aopacity);
    static void Draw();
    static void deleteSprites();
    static void setLastSpriteSpecial(int s);
    static void setLastSpriteSpeed(int s);
    static void setLastSpriteAlivetime(float al);

    static Texture cloudtexture;
    static Texture bloodtexture;
    static Texture flametexture;
    static Texture smoketexture;

    static Texture cloudimpacttexture;
    static Texture snowflaketexture;
    static Texture shinetexture;
    static Texture bloodflametexture;

    static Texture splintertexture;

    static Texture leaftexture;
    static Texture toothtexture;

    Sprite();
    ~Sprite(){};
};

#endif
