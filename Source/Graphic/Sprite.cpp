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

#include "Graphic/Sprite.hpp"

#include "Game.hpp"
#include "Thirdparty/microprofile/microprofile.h"
#include "Objects/Person.hpp"

extern XYZ viewer;
extern float viewdistance;
extern float fadestart;
extern int environment;
extern float texscale;
extern Light light;
extern float multiplier;
extern float gravity;
extern Terrain terrain;
extern int detail;
extern XYZ viewerfacing;
extern int bloodtoggle;
extern XYZ windvector;

// init statics
Texture Sprite::cloudtexture;
Texture Sprite::cloudimpacttexture;
Texture Sprite::bloodtexture;
Texture Sprite::flametexture;
Texture Sprite::bloodflametexture;
Texture Sprite::smoketexture;
Texture Sprite::snowflaketexture;
Texture Sprite::shinetexture;
Texture Sprite::splintertexture;
Texture Sprite::leaftexture;
Texture Sprite::toothtexture;

float Sprite::checkdelay = 0;

static std::vector<Sprite> sprites;
static int sprites_head = 0;
static int sprites_count = 0;
void Sprite::AllocSprites(int count){
	sprites.resize(count);
}
void Sprite::deleteSprites(){
	sprites_count = 0;
}

struct AnimateSprites: WorkerThread::Job {
	float mat[4][4];
	int start_idx, end_idx;
	AnimateSprites(float m[4][4], int start, int end):
		Job(),
		start_idx(start),
		end_idx(end)
	{
		memcpy(mat, m, sizeof(mat));
	}
	~AnimateSprites(){}
	void execute() override {
		Sprite::doAnimate(mat, start_idx, end_idx);
	}
};
static bool check;

void Sprite::submitAnimationJob(std::vector<WorkerThread::JobHandle> &out){
	checkdelay -= multiplier * 10;
	check = 0;
	if (checkdelay <= 0) {
		check = 1;
		checkdelay = 1;
	}

	int sprites_per_job = ceil((float)sprites_count / 3.0f);

	MICROPROFILE_COUNTER_SET("sprites_per_job", sprites_per_job);

	float mmodel[4][4];
	glMatrixMode(GL_MODELVIEW);
	glGetFloatv(GL_MODELVIEW_MATRIX, &mmodel[0][0]);

	for(int start = 0; start < sprites_count; start += sprites_per_job){
		int end = start + sprites_per_job;
		if(end >= sprites_count){
			end = sprites_count - 1;
		}
		WorkerThread::JobHandle hndl = WorkerThread::submitJob<AnimateSprites>(mmodel, start, end);
		out.push_back(hndl);
	}
}

void Sprite::doAnimate(float (&mmodel)[4][4], int start_idx, int end_idx){
	MICROPROFILE_SCOPEI("Sprite", "animate", 0x55ff55);
	
	float tempmult = multiplier;
	XYZ start, end, colpoint;
	XYZ tempviewer = viewer + viewerfacing * 6;

	//TODO: why reverse iteration?
	//for (int i = sprites.size() - 1; i >= 0; i--) {
	for (int i = start_idx; i <= end_idx; i++) {
		Sprite *sprite = &sprites[i];

		{MICROPROFILE_SCOPEI("Sprite", "anim", 0x5555ff);

		multiplier = tempmult;
		if (sprite->type != snowsprite) {
			sprite->position += sprite->velocity * multiplier;
			sprite->velocity += windvector * multiplier;
		}
		if (sprite->type == flamesprite || sprite->type == smoketype) {
			sprite->position += windvector * multiplier / 2;
		}
		if ((sprite->type == flamesprite || sprite->type == weaponflamesprite || sprite->type == weaponshinesprite || sprite->type == bloodflamesprite)) {
			multiplier *= sprite->speed * .7;
		}
		sprite->alivetime += multiplier;

		if (sprite->type == cloudsprite || sprite->type == cloudimpactsprite) {
			sprite->opacity -= multiplier / 2;
			sprite->size += multiplier / 2;
			sprite->velocity.y += gravity * multiplier * .25;
		}
		if (sprite->type == breathsprite) {
			sprite->opacity -= multiplier / 2;
			sprite->size += multiplier / 2;
			if (findLength(&sprite->velocity) <= multiplier) {
				sprite->velocity = 0;
			} else {
				XYZ slowdown;
				slowdown = sprite->velocity * -1;
				Normalise(&slowdown);
				slowdown *= multiplier;
				sprite->velocity += slowdown;
			}
		}
		if (sprite->type == snowsprite) {
			sprite->size -= multiplier / 120;
			sprite->rotation += multiplier * 360;
			sprite->position.y -= multiplier;
			sprite->position += windvector * multiplier;
			if (sprite->position.y < tempviewer.y - 6) {
				sprite->position.y += 12;
			}
			if (sprite->position.y > tempviewer.y + 6) {
				sprite->position.y -= 12;
			}
			if (sprite->position.z < tempviewer.z - 6) {
				sprite->position.z += 12;
			}
			if (sprite->position.z > tempviewer.z + 6) {
				sprite->position.z -= 12;
			}
			if (sprite->position.x < tempviewer.x - 6) {
				sprite->position.x += 12;
			}
			if (sprite->position.x > tempviewer.x + 6) {
				sprite->position.x -= 12;
			}
		}

		//TODO: move bloodsprite processing to main thread since it probably isn't thread safe
		//accesses Terrain::patchobjects, Person::players, Object::objects
		if (sprite->type == bloodsprite) {
			bool spritehit = 0;
			sprite->rotation += multiplier * 100;
			sprite->velocity.y += gravity * multiplier;
			if (check) {
				XYZ where, startpoint, endpoint, movepoint, footpoint;
				float rotationpoint;
				int whichtri;

				//TODO: check if Person::DoBloodBigWhere is thread safe
				for (unsigned j = 0; j < Person::players.size(); j++) {
					if (!spritehit && Person::players[j]->dead && sprite->alivetime > .1) {
						where = sprite->oldposition;
						where -= Person::players[j]->coords;
						if (!Person::players[j]->skeleton.free) {
							where = DoRotation(where, 0, -Person::players[j]->yaw, 0);
						}
						startpoint = where;
						where = sprite->position;
						where -= Person::players[j]->coords;
						if (!Person::players[j]->skeleton.free) {
							where = DoRotation(where, 0, -Person::players[j]->yaw, 0);
						}
						endpoint = where;

						movepoint = 0;
						rotationpoint = 0;
						whichtri = Person::players[j]->skeleton.drawmodel.LineCheck(&startpoint, &endpoint, &footpoint, &movepoint, &rotationpoint);
						if (whichtri != -1) {
							spritehit = 1;
							Person::players[j]->DoBloodBigWhere(0, 160, sprite->oldposition);
							DeleteSprite(i);
						}
					}
				}

				int whichpatchx = sprite->position.x / (terrain.size / subdivision * terrain.scale);
				int whichpatchz = sprite->position.z / (terrain.size / subdivision * terrain.scale);
				if (whichpatchx > 0 && whichpatchz > 0 && whichpatchx < subdivision && whichpatchz < subdivision) {
					if (!spritehit) {
						for (unsigned int j = 0; j < terrain.patchobjects[whichpatchx][whichpatchz].size(); j++) {
							int k = terrain.patchobjects[whichpatchx][whichpatchz][j];
							start = sprite->oldposition;
							end = sprite->position;
							if (!spritehit) {
								if (Object::objects[k]->model.LineCheck(&start, &end, &colpoint, &Object::objects[k]->position, &Object::objects[k]->yaw) != -1) {
									if (detail == 2 || (detail == 1 && abs(Random() % 4) == 0) || (detail == 0 && abs(Random() % 8) == 0)) {
										Object::objects[k]->model.MakeDecal(blooddecalfast, DoRotation(colpoint - Object::objects[k]->position, 0, -Object::objects[k]->yaw, 0), sprite->size * 1.6, .5, Random() % 360);
									}
									DeleteSprite(i);
									spritehit = 1;
								}
							}
						}
					}
				}
				if (!spritehit) {
					if (sprite->position.y < terrain.getHeight(sprite->position.x, sprite->position.z)) {
						terrain.MakeDecal(blooddecalfast, sprite->position, sprite->size * 1.6, .6, Random() % 360);
						DeleteSprite(i);
					}
				}
			}
		}else if (sprite->type == splintersprite) {
			sprite->rotation += sprite->rotatespeed * multiplier;
			sprite->opacity -= multiplier / 2;
			if (sprite->special == 0 || sprite->special == 2 || sprite->special == 3) {
				sprite->velocity.y += gravity * multiplier;
			}
			if (sprite->special == 1) {
				sprite->velocity.y += gravity * multiplier * .5;
			}
		}else if (sprite->type == flamesprite || sprite->type == weaponflamesprite || sprite->type == weaponshinesprite || sprite->type == bloodflamesprite) {
			sprite->rotation += multiplier * sprite->rotatespeed;
			sprite->opacity -= multiplier * 5 / 4;
			if (sprite->type != weaponshinesprite && sprite->type != bloodflamesprite) {
				if (sprite->opacity < .5 && sprite->opacity + multiplier * 5 / 4 >= .5 && (abs(Random() % 4) == 0 || (sprite->initialsize > 2 && Random() % 2 == 0))) {
					MakeSprite(smoketype, sprite->position, sprite->velocity, .9, .9, .6, sprite->size * 1.2, .4);
				}
			}
			if (sprite->alivetime > .14 && (sprite->type == flamesprite)) {
				sprite->velocity = 0;
				sprite->velocity.y = 1.5;
			}
		}else if (sprite->type == smoketype) {
			sprite->opacity -= multiplier / 3 / sprite->initialsize;
			sprite->color[0] -= multiplier;
			sprite->color[1] -= multiplier;
			sprite->color[2] -= multiplier;
			if (sprite->color[0] < .6) {
				sprite->color[0] = .6;
			}
			if (sprite->color[1] < .6) {
				sprite->color[1] = .6;
			}
			if (sprite->color[2] < .6) {
				sprite->color[2] = .6;
			}
			sprite->size += multiplier;
			sprite->velocity = 0;
			sprite->velocity.y = 1.5;
			sprite->rotation += multiplier * sprite->rotatespeed / 5;
		}
		if (sprite->opacity <= 0 || sprite->size <= 0) {
			DeleteSprite(i);
			continue;
		}

		}//MICROPROFILE

		{MICROPROFILE_SCOPEI("Sprite", "transform", 0x55ff55);
		//transformations
		//glMatrixMode(GL_MODELVIEW);
		//glPushMatrix();

		matrix4x4 mat;
		matrix4x4_transpose(mat, mmodel);

		//glTranslatef(sprite->position.x, sprite->position.y, sprite->position.z);
		matrix4x4_translate(mat, sprite->position.x, sprite->position.y, sprite->position.z);

		if ((sprite->type == flamesprite || sprite->type == weaponflamesprite || sprite->type == weaponshinesprite)) {
			XYZ difference = viewer - sprite->position;
			Normalise(&difference);
			float sz = sprite->size;
			/*glTranslatef(
				(difference.x * sz) / 4,
				(difference.y * sz) / 4,
				(difference.z * sz) / 4);*/
			matrix4x4_translate(mat,
				(difference.x * sz) / 4,
				(difference.y * sz) / 4,
				(difference.z * sz) / 4);
		}
		if (sprite->type == snowsprite) {
			//glRotatef(sprite->rotation * .2, 0, .3, 1);
			matrix4x4_rotate_z(mat, DEG_TO_RAD(sprite->rotation * .2));
			//glTranslatef(1, 0, 0);
			matrix4x4_translate(mat, 1, 0, 0);
		}
		//glGetFloatv(GL_MODELVIEW_MATRIX, M);
		sprite->rpoint.x = mat[0][3];
		sprite->rpoint.y = mat[1][3];
		sprite->rpoint.z = mat[2][3];

		}//MICROPROFILE

		if (check) {
			sprite->oldposition = sprite->position;
		}
	}
}

void Sprite::setLastSpriteSpecial(int s){
	int last = sprites_head - 1;
	if(last < 0){
		last = sprites_count - 1;
	}
	sprites[last].special = s;
}
void Sprite::setLastSpriteSpeed(int s){
	int last = sprites_head - 1;
	if(last < 0){
		last = sprites_count - 1;
	}
	sprites[last].speed = s;
}
void Sprite::setLastSpriteAlivetime(float al){
	int last = sprites_head - 1;
	if(last < 0){
		last = sprites_count - 1;
	}
	sprites[last].alivetime = al;
}

//Functions
void Sprite::Draw()
{
	MICROPROFILE_SCOPEI("Sprite", "Draw", 0x008fff);
	float M[16];
	XYZ point;
	float distancemult;
	int lasttype;
	int lastspecial;
	bool check;
	bool blend;
	float tempmult;
	XYZ difference;
	float lightcolor[3];
	float viewdistsquared = viewdistance * viewdistance;
	XYZ tempviewer;

	tempviewer = viewer + viewerfacing * 6;
	check = 0;

	lightcolor[0] = light.color[0] * .5 + light.ambient[0];
	lightcolor[1] = light.color[1] * .5 + light.ambient[1];
	lightcolor[2] = light.color[2] * .5 + light.ambient[2];

	lasttype = -1;
	lastspecial = -1;
	glEnable(GL_BLEND);
	glDisable(GL_LIGHTING);
	glDisable(GL_CULL_FACE);
	glEnable(GL_TEXTURE_2D);
	blend = 1;
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDepthMask(0);
	glAlphaFunc(GL_GREATER, 0.0001);

	MICROPROFILE_COUNTER_SET("Sprites", sprites_count);
	{MICROPROFILE_SCOPEI("Sprite", "render", 0x55ff55);

	for (unsigned i = 0; i < sprites_count; i++) {
		if(!sprites[i].alive){
			continue;
		}
		{MICROPROFILE_SCOPEI("Sprite", "draw", 0x55ff55);
		//draw
		//TODO: separate sprites by type to minimize binding calls

		if (lasttype != sprites[i].type) {
			switch (sprites[i].type) {
				case cloudsprite:
					cloudtexture.bind();
					if (!blend) {
						blend = 1;
						glAlphaFunc(GL_GREATER, 0.0001);
						glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
					}
					break;
				case breathsprite:
				case cloudimpactsprite:
					cloudimpacttexture.bind();
					if (!blend) {
						blend = 1;
						glAlphaFunc(GL_GREATER, 0.0001);
						glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
					}
					break;
				case smoketype:
					smoketexture.bind();
					if (!blend) {
						blend = 1;
						glAlphaFunc(GL_GREATER, 0.0001);
						glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
					}
					break;
				case bloodsprite:
					bloodtexture.bind();
					if (!blend) {
						blend = 1;
						glAlphaFunc(GL_GREATER, 0.0001);
						glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
					}
					break;
				case splintersprite:
					if (lastspecial != sprites[i].special) {
						if (sprites[i].special == 0) {
							splintertexture.bind();
						}
						if (sprites[i].special == 1) {
							leaftexture.bind();
						}
						if (sprites[i].special == 2) {
							snowflaketexture.bind();
						}
						if (sprites[i].special == 3) {
							toothtexture.bind();
						}
						if (!blend) {
							blend = 1;
							glAlphaFunc(GL_GREATER, 0.0001);
							glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
						}
					}
					break;
				case snowsprite:
					snowflaketexture.bind();
					if (!blend) {
						blend = 1;
						glAlphaFunc(GL_GREATER, 0.0001);
						glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
					}
					break;
				case weaponshinesprite:
					shinetexture.bind();
					if (blend) {
						blend = 0;
						glAlphaFunc(GL_GREATER, 0.001);
						glBlendFunc(GL_SRC_ALPHA, GL_ONE);
					}
					break;
				case flamesprite:
				case weaponflamesprite:
					flametexture.bind();
					if (blend || lasttype == bloodflamesprite) {
						blend = 0;
						glAlphaFunc(GL_GREATER, 0.3);
						glBlendFunc(GL_SRC_ALPHA, GL_ONE);
					}
					break;
				case bloodflamesprite:
					bloodflametexture.bind();
					if (blend) {
						blend = 0;
						glAlphaFunc(GL_GREATER, 0.3);
						glBlendFunc(GL_ONE, GL_ZERO);
					}
					break;
			}
		}

		if (sprites[i].type == snowsprite) {
			distancemult = (144 - (distsq(&tempviewer, &sprites[i].position) - (144 * fadestart)) * (1 / (1 - fadestart))) / 144;
		} else {
			distancemult = (viewdistsquared - (distsq(&viewer, &sprites[i].position) - (viewdistsquared * fadestart)) * (1 / (1 - fadestart))) / viewdistsquared;
		}
		if (sprites[i].type == flamesprite) {
			if (distancemult >= 1) {
				glColor4f(sprites[i].color[0], sprites[i].color[1], sprites[i].color[2], sprites[i].opacity);
			} else {
				glColor4f(sprites[i].color[0], sprites[i].color[1], sprites[i].color[2], sprites[i].opacity * distancemult);
			}
		} else {
			if (distancemult >= 1) {
				glColor4f(sprites[i].color[0] * lightcolor[0], sprites[i].color[1] * lightcolor[1], sprites[i].color[2] * lightcolor[2], sprites[i].opacity);
			} else {
				glColor4f(sprites[i].color[0] * lightcolor[0], sprites[i].color[1] * lightcolor[1], sprites[i].color[2] * lightcolor[2], sprites[i].opacity * distancemult);
			}
		}

		lasttype = sprites[i].type;
		lastspecial = sprites[i].special;

		glMatrixMode(GL_MODELVIEW);
		glPushMatrix();
		glLoadIdentity();

		glTranslatef(sprites[i].rpoint.x, sprites[i].rpoint.y, sprites[i].rpoint.z);
		glRotatef(sprites[i].rotation, 0, 0, 1);

		if ((sprites[i].type == flamesprite || sprites[i].type == weaponflamesprite || sprites[i].type == weaponshinesprite || sprites[i].type == bloodflamesprite)) {
			if (sprites[i].alivetime < .14) {
				glScalef(sprites[i].alivetime / .14, sprites[i].alivetime / .14, sprites[i].alivetime / .14);
			}
		}
		if (sprites[i].type == smoketype || sprites[i].type == snowsprite || sprites[i].type == weaponshinesprite || sprites[i].type == breathsprite) {
			if (sprites[i].alivetime < .3) {
				if (distancemult >= 1) {
					glColor4f(sprites[i].color[0] * lightcolor[0], sprites[i].color[1] * lightcolor[1], sprites[i].color[2] * lightcolor[2], sprites[i].opacity * sprites[i].alivetime / .3);
				}
				if (distancemult < 1) {
					glColor4f(sprites[i].color[0] * lightcolor[0], sprites[i].color[1] * lightcolor[1], sprites[i].color[2] * lightcolor[2], sprites[i].opacity * distancemult * sprites[i].alivetime / .3);
				}
			}
		}
		if (sprites[i].type == splintersprite && sprites[i].special > 0 && sprites[i].special != 3) {
			if (sprites[i].alivetime < .2) {
				if (distancemult >= 1) {
					glColor4f(sprites[i].color[0] * lightcolor[0], sprites[i].color[1] * lightcolor[1], sprites[i].color[2] * lightcolor[2], sprites[i].alivetime / .2);
				} else {
					glColor4f(sprites[i].color[0] * lightcolor[0], sprites[i].color[1] * lightcolor[1], sprites[i].color[2] * lightcolor[2], distancemult * sprites[i].alivetime / .2);
				}
			} else {
				if (distancemult >= 1) {
					glColor4f(sprites[i].color[0] * lightcolor[0], sprites[i].color[1] * lightcolor[1], sprites[i].color[2] * lightcolor[2], 1);
				} else {
					glColor4f(sprites[i].color[0] * lightcolor[0], sprites[i].color[1] * lightcolor[1], sprites[i].color[2] * lightcolor[2], distancemult);
				}
			}
		}
		if (sprites[i].type == splintersprite && (sprites[i].special == 0 || sprites[i].special == 3)) {
			if (distancemult >= 1) {
				glColor4f(sprites[i].color[0] * lightcolor[0], sprites[i].color[1] * lightcolor[1], sprites[i].color[2] * lightcolor[2], 1);
			} else {
				glColor4f(sprites[i].color[0] * lightcolor[0], sprites[i].color[1] * lightcolor[1], sprites[i].color[2] * lightcolor[2], distancemult);
			}
		}

		glBegin(GL_TRIANGLES);
		glTexCoord2f(1.0f, 1.0f);
		glVertex3f(.5 * sprites[i].size, .5 * sprites[i].size, 0.0f);
		glTexCoord2f(0.0f, 1.0f);
		glVertex3f(-.5 * sprites[i].size, .5 * sprites[i].size, 0.0f);
		glTexCoord2f(1.0f, 0.0f);
		glVertex3f(.5 * sprites[i].size, -.5 * sprites[i].size, 0.0f);
		glTexCoord2f(0.0f, 0.0f);
		glVertex3f(-.5 * sprites[i].size, -.5 * sprites[i].size, 0.0f);
		glTexCoord2f(1.0f, 0.0f);
		glVertex3f(.5 * sprites[i].size, -.5 * sprites[i].size, 0.0f);
		glTexCoord2f(0.0f, 1.0f);
		glVertex3f(-.5 * sprites[i].size, .5 * sprites[i].size, 0.0f);
		glEnd();
		glPopMatrix();
		}//MICROPROFILE
	}
	}//MICROPROFILE

	glAlphaFunc(GL_GREATER, 0.0001);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

}

void Sprite::DeleteSprite(int i)
{
	//MICROPROFILE_SCOPEI("Sprite", "DeleteSprite", 0x008fff);
	//sprites.erase(sprites.begin() + i);
	sprites[i].alive = false;
}

void Sprite::MakeSprite(int atype, XYZ where, XYZ avelocity, float red, float green, float blue, float asize, float aopacity)
{
	MICROPROFILE_SCOPEI("Sprite", "MakeSprite", 0x008fff);

	if(sprites_count < sprites.size()){
		sprites_count++;
	}

	if(++sprites_head >= sprites_count){
		sprites_head = 0;
	}
	
	Sprite *spr = &sprites[sprites_head];
	spr->alive = true;

	if ((atype != bloodsprite && atype != bloodflamesprite) || bloodtoggle) {
		spr->special = 0;
		spr->type = atype;
		spr->position = where;
		spr->oldposition = where;
		spr->velocity = avelocity;
		spr->alivetime = 0;
		spr->opacity = aopacity;
		spr->size = asize;
		spr->initialsize = asize;
		spr->color[0] = red;
		spr->color[1] = green;
		spr->color[2] = blue;
		spr->rotatespeed = abs(Random() % 720) - 360;
		spr->speed = float(abs(Random() % 100)) / 200 + 1.5;
	}
}

Sprite::Sprite()
{
	oldposition = 0;
	position = 0;
	velocity = 0;
	size = 0;
	initialsize = 0;
	type = 0;
	special = 0;
	memset(color, 0, sizeof(color));
	opacity = 0;
	rotation = 0;
	alivetime = 0;
	speed = 0;
	rotatespeed = 0;
}
