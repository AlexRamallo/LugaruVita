#include <GL/gl.h>
#include <stdio.h>
#include <iostream>
#include <assert.h>

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
	for(GLsizei i = 0; i < n; i++){
		switch(type){
			default:
				std::cout << "glCallLists currently only supports GL_BYTE for type parameter" << std::endl;
				assert(0);
				return;
			case GL_BYTE:
				int8_t dlist_it = ((int8_t*)lists)[i];
				GLuint dlist = dlist_base + (GLuint) dlist_it;
				//VITAGL: TODO
				//it seems VitaGL currently ignores the dlist parameter, need to fix
				glCallList(dlist);
				break;
		}
	}
}

void glTranslated( GLdouble x, GLdouble y, GLdouble z ){
	glTranslatex((float) x, (float) y, (float) z);
}
