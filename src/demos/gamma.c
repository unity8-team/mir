
/* Draw test patterns to help determine correct gamma value for a display.
   When the intensities of the inner squares nearly match the intensities
   of their frames (from some distance the borders should disappear) then
   you've found the right gamma value.

   You can set Mesa's gamma values (for red, green and blue) with the
   MESA_GAMMA environment variable.  But only on X windows!
   For example:
        setenv MESA_GAMMA 1.5 1.6 1.4
   Sets the red gamma value to 1.5, green to 1.6 and blue to 1.4.
   See the main README file for more information.

   For more info about gamma correction see:
   http://www.inforamp.net/~poynton/notes/colour_and_gamma/GammaFAQ.html

   This program is in the public domain

   Brian Paul  19 Oct 1995
   Kai Schuetz 05 Jun 1999 */

/* Conversion to GLUT by Mark J. Kilgard */


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "glut_wrap.h"

static void
Reshape(int width, int height)
{
  glViewport(0, 0, (GLint) width, (GLint) height);

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(-1.0, 1.0, -1.0, 1.0, -1.0, 1.0);
  glMatrixMode(GL_MODELVIEW);
  glShadeModel(GL_FLAT);
}

/* ARGSUSED1 */
static void
key_esc(unsigned char key, int x, int y)
{
  if(key == 27) exit(0);  /* Exit on Escape */
}

static GLubyte p25[] = {
  0xaa, 0xaa, 0xaa, 0xaa, 0x00, 0x00, 0x00, 0x00,
  0xaa, 0xaa, 0xaa, 0xaa, 0x00, 0x00, 0x00, 0x00,
  0xaa, 0xaa, 0xaa, 0xaa, 0x00, 0x00, 0x00, 0x00,
  0xaa, 0xaa, 0xaa, 0xaa, 0x00, 0x00, 0x00, 0x00,
  0xaa, 0xaa, 0xaa, 0xaa, 0x00, 0x00, 0x00, 0x00,
  0xaa, 0xaa, 0xaa, 0xaa, 0x00, 0x00, 0x00, 0x00,
  0xaa, 0xaa, 0xaa, 0xaa, 0x00, 0x00, 0x00, 0x00,
  0xaa, 0xaa, 0xaa, 0xaa, 0x00, 0x00, 0x00, 0x00,
  0xaa, 0xaa, 0xaa, 0xaa, 0x00, 0x00, 0x00, 0x00,
  0xaa, 0xaa, 0xaa, 0xaa, 0x00, 0x00, 0x00, 0x00,
  0xaa, 0xaa, 0xaa, 0xaa, 0x00, 0x00, 0x00, 0x00,
  0xaa, 0xaa, 0xaa, 0xaa, 0x00, 0x00, 0x00, 0x00,
  0xaa, 0xaa, 0xaa, 0xaa, 0x00, 0x00, 0x00, 0x00,
  0xaa, 0xaa, 0xaa, 0xaa, 0x00, 0x00, 0x00, 0x00,
  0xaa, 0xaa, 0xaa, 0xaa, 0x00, 0x00, 0x00, 0x00,
  0xaa, 0xaa, 0xaa, 0xaa, 0x00, 0x00, 0x00, 0x00,
};

static GLubyte p50[] = {
  0xaa, 0xaa, 0xaa, 0xaa, 0x55, 0x55, 0x55, 0x55,
  0xaa, 0xaa, 0xaa, 0xaa, 0x55, 0x55, 0x55, 0x55,
  0xaa, 0xaa, 0xaa, 0xaa, 0x55, 0x55, 0x55, 0x55,
  0xaa, 0xaa, 0xaa, 0xaa, 0x55, 0x55, 0x55, 0x55,
  0xaa, 0xaa, 0xaa, 0xaa, 0x55, 0x55, 0x55, 0x55,
  0xaa, 0xaa, 0xaa, 0xaa, 0x55, 0x55, 0x55, 0x55,
  0xaa, 0xaa, 0xaa, 0xaa, 0x55, 0x55, 0x55, 0x55,
  0xaa, 0xaa, 0xaa, 0xaa, 0x55, 0x55, 0x55, 0x55,
  0xaa, 0xaa, 0xaa, 0xaa, 0x55, 0x55, 0x55, 0x55,
  0xaa, 0xaa, 0xaa, 0xaa, 0x55, 0x55, 0x55, 0x55,
  0xaa, 0xaa, 0xaa, 0xaa, 0x55, 0x55, 0x55, 0x55,
  0xaa, 0xaa, 0xaa, 0xaa, 0x55, 0x55, 0x55, 0x55,
  0xaa, 0xaa, 0xaa, 0xaa, 0x55, 0x55, 0x55, 0x55,
  0xaa, 0xaa, 0xaa, 0xaa, 0x55, 0x55, 0x55, 0x55,
  0xaa, 0xaa, 0xaa, 0xaa, 0x55, 0x55, 0x55, 0x55,
  0xaa, 0xaa, 0xaa, 0xaa, 0x55, 0x55, 0x55, 0x55,
};

static GLubyte p75[] = {
  0xaa, 0xaa, 0xaa, 0xaa, 0xff, 0xff, 0xff, 0xff,
  0xaa, 0xaa, 0xaa, 0xaa, 0xff, 0xff, 0xff, 0xff,
  0xaa, 0xaa, 0xaa, 0xaa, 0xff, 0xff, 0xff, 0xff,
  0xaa, 0xaa, 0xaa, 0xaa, 0xff, 0xff, 0xff, 0xff,
  0xaa, 0xaa, 0xaa, 0xaa, 0xff, 0xff, 0xff, 0xff,
  0xaa, 0xaa, 0xaa, 0xaa, 0xff, 0xff, 0xff, 0xff,
  0xaa, 0xaa, 0xaa, 0xaa, 0xff, 0xff, 0xff, 0xff,
  0xaa, 0xaa, 0xaa, 0xaa, 0xff, 0xff, 0xff, 0xff,
  0xaa, 0xaa, 0xaa, 0xaa, 0xff, 0xff, 0xff, 0xff,
  0xaa, 0xaa, 0xaa, 0xaa, 0xff, 0xff, 0xff, 0xff,
  0xaa, 0xaa, 0xaa, 0xaa, 0xff, 0xff, 0xff, 0xff,
  0xaa, 0xaa, 0xaa, 0xaa, 0xff, 0xff, 0xff, 0xff,
  0xaa, 0xaa, 0xaa, 0xaa, 0xff, 0xff, 0xff, 0xff,
  0xaa, 0xaa, 0xaa, 0xaa, 0xff, 0xff, 0xff, 0xff,
  0xaa, 0xaa, 0xaa, 0xaa, 0xff, 0xff, 0xff, 0xff,
  0xaa, 0xaa, 0xaa, 0xaa, 0xff, 0xff, 0xff, 0xff,
};

static GLubyte *stippletab[4] = {NULL, p25, p50, p75};

static void
gamma_ramp(GLfloat yoffs, GLfloat r, GLfloat g, GLfloat b)
{
  GLint d;

  glColor3f(0.0, 0.0, 0.0);     /* solid black, no stipple */
  glRectf(-1.0, yoffs, -0.6, yoffs + 0.5);

  for(d = 1; d < 4; d++) {  /* increasing density from 25% to 75% */
    GLfloat xcoord = (-1.0 + d*0.4);
    GLfloat t = d * 0.25;

    glColor3f(r*t, g*t, b*t); /* draw outer rect */
    glRectf(xcoord, yoffs, xcoord+0.4, yoffs + 0.5);

    glColor3f(0.0, 0.0, 0.0);   /* "clear" inner rect */
    glRectf(xcoord + 0.1, yoffs + 0.125, xcoord + 0.3, yoffs + 0.375);

    glColor3f(r, g, b);         /* draw stippled inner rect */
    glEnable(GL_POLYGON_STIPPLE);
    glPolygonStipple(stippletab[d]);
    glRectf(xcoord + 0.1, yoffs + 0.125, xcoord + 0.3, yoffs + 0.375);
    glDisable(GL_POLYGON_STIPPLE);
  }
  glColor3f(r, g, b);           /* solid color, no stipple */
  glRectf(0.6, yoffs, 1.0, yoffs + 0.5);
}

static void
display(void)
{
  gamma_ramp( 0.5, 1.0, 1.0, 1.0); /* white ramp */
  gamma_ramp( 0.0, 1.0, 0.0, 0.0); /* red ramp */
  gamma_ramp(-0.5, 0.0, 1.0, 0.0); /* green ramp */
  gamma_ramp(-1.0, 0.0, 0.0, 1.0); /* blue ramp */
  glFlush();
}

int
main(int argc, char **argv)
{
  glutInitWindowSize(500, 400);
  glutInit(&argc, argv);
  glutInitDisplayMode(GLUT_RGB | GLUT_SINGLE);
  glutCreateWindow("gamma test patterns");
  glutReshapeFunc(Reshape);
  glutDisplayFunc(display);
  glutKeyboardFunc(key_esc);

  glutMainLoop();
  return 0;             /* ANSI C requires main to return int. */
}
