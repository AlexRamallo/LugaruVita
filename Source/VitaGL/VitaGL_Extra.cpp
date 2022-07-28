#include <GL/gl.h>
#include <stdio.h>
#include <iostream>
#include "Utils/Log.h"
#include <assert.h>

#include "Thirdparty/microprofile/microprofile.h"

void glReadBuffer( GLenum mode ){
	//--
}
void glDrawBuffer( GLenum mode ){
	//--
}

void glPixelTransferi( GLenum pname, GLint param ){
	//--
}


void glCopyTexImage2D( GLenum target, GLint level,
                                        GLenum internalformat,
                                        GLint x, GLint y,
                                        GLsizei width, GLsizei height,
                                        GLint border ){
	//--
}

void glCopyTexSubImage2D( GLenum target, GLint level,
                                           GLint xoffset, GLint yoffset,
                                           GLint x, GLint y,
                                           GLsizei width, GLsizei height ){
	//--
}

static GLuint dlist_base = 0;
void glListBase( GLuint base ){
	dlist_base = base;
}

/*
GL_BYTE lists is treated as an array of signed bytes, each in the range -128 through 127. 
*/
void glCallLists( GLsizei n, GLenum type, const GLvoid *lists ){
	switch(type){
		default:
			LOG("glCallLists currently only supports GL_BYTE for type parameter");
			return;
		
		case GL_BYTE:
			for(int i = 0; i < n; i++){
				GLbyte dlist_it = ((GLbyte*)lists)[i];
				GLuint dlist = (GLuint) dlist_it;
				glCallList(dlist_base + dlist);
			}
			break;
	}
}

void glTranslated( GLdouble x, GLdouble y, GLdouble z ){
	glTranslatef((float) x, (float) y, (float) z);
}

void vglDrawArrays(GLenum mode, GLint first, GLsizei count) {
	MICROPROFILE_SCOPEI("vitaGL", "glDrawArrays", 0xff0000);
	glDrawArrays(mode, first, count);
}