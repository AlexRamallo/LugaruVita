#include <vitaGL.h>

#define LOG_ERRORS 2
#define HAVE_DEBUG_INTERFACE 1


//https://docs.huihoo.com/mesa3d/intro.html
#define GL_FOG_COORDINATE_SOURCE GL_FOG_COORD_SRC
#define GL_FOG_COORDINATE GL_FOG_COORD
#define GL_CURRENT_FOG_COORDINATE GL_CURRENT_FOG_COORD
#define GL_FOG_COORDINATE_ARRAY_TYPE GL_FOG_COORD_ARRAY_TYPE
#define GL_FOG_COORDINATE_ARRAY_STRIDE GL_FOG_COORD_ARRAY_STRIDE
#define GL_FOG_COORDINATE_ARRAY_POINTER GL_FOG_COORD_ARRAY_POINTER
#define GL_FOG_COORDINATE_ARRAY GL_FOG_COORD_ARRAY
#define GL_SOURCE0_RGB GL_SRC0_RGB
#define GL_SOURCE1_RGB GL_SRC1_RGB
#define GL_SOURCE2_RGB GL_SRC2_RGB
#define GL_SOURCE0_ALPHA GL_SRC0_ALPHA
#define GL_SOURCE1_ALPHA GL_SRC1_ALPHA
#define GL_SOURCE2_ALPHA GL_SRC2_ALPHA

void glTranslated( GLdouble x, GLdouble y, GLdouble z );
void glCallLists( GLsizei n, GLenum type, const GLvoid *lists );
void glListBase( GLuint base );

void glReadBuffer( GLenum mode );
void glDrawBuffer( GLenum mode );
void glPixelTransferi( GLenum pname, GLint param );

void glCopyTexImage2D( GLenum target, GLint level,
                                        GLenum internalformat,
                                        GLint x, GLint y,
                                        GLsizei width, GLsizei height,
                                        GLint border );

void glCopyTexSubImage2D( GLenum target, GLint level,
                                           GLint xoffset, GLint yoffset,
                                           GLint x, GLint y,
                                           GLsizei width, GLsizei height );

void vglDrawArrays(GLenum mode, GLint first, GLsizei count);