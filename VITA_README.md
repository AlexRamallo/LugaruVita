# Missing OpenGL functions in vitaGL

* glReadBuffer
* glDrawBuffer

* glPixelStorei
* glPixelTransferi
* glCopyTexSubImage2D

* glCopyTexImage2D
	* used for motion blur

#Display lists

http://www.songho.ca/opengl/gl_displaylist.html
https://registry.khronos.org/OpenGL-Refpages/gl2.1/xhtml/glCallLists.xml

* glCallLists
	* glListBase

For this, need to implement display lists, aka command buffering. glVertex* style commands could
be stored in a VBO, but will need extra structure for state switches. Need to get a list of possible
display list commands, at least as used by the game.


# Misc

https://registry.khronos.org/OpenGL-Refpages/gl2.1/xhtml/glTranslate.xml

* glTranslated
