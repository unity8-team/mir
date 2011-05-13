/* Test rebasing */

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <GL/glew.h>
#include "glut_wrap.h"

#define NR_VERTS 20000
GLfloat verts[NR_VERTS][4];
GLushort indices[NR_VERTS*2];



static void Init( void )
{
   GLint errno;
   GLuint prognum;
   GLint i;

   static const char *prog1 =
      "!!ARBvp1.0\n"
      "MOV  result.color, {1.0}.xxxx;\n"
      "MOV  result.position, vertex.position;\n"
      "END\n";

   glGenProgramsARB(1, &prognum);
   glBindProgramARB(GL_VERTEX_PROGRAM_ARB, prognum);
   glProgramStringARB(GL_VERTEX_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB,
		      strlen(prog1), (const GLubyte *) prog1);

   assert(glIsProgramARB(prognum));
   errno = glGetError();
   printf("glGetError = %d\n", errno);
   if (errno != GL_NO_ERROR)
   {
      GLint errorpos;

      glGetIntegerv(GL_PROGRAM_ERROR_POSITION_ARB, &errorpos);
      printf("errorpos: %d\n", errorpos);
      printf("%s\n", (char *)glGetString(GL_PROGRAM_ERROR_STRING_ARB));
   }


   glEnableClientState( GL_VERTEX_ARRAY );
   glEnableClientState( GL_COLOR_ARRAY );
   glVertexPointer( 3, GL_FLOAT, sizeof(verts[0]), verts );


   for (i = 0; i < NR_VERTS; i++) {
      verts[i][0] = cos(i / (float)NR_VERTS * 2.0 * 3.141592);
      verts[i][1] = sin(i / (float)NR_VERTS * 2.0 * 3.141592);
      verts[i][2] = 0.0;
      verts[i][3] = 1.0;

      indices[2*i+0] = i;
      indices[2*i+1] = (i+1 == NR_VERTS) ? 0 : i+1;
   }
}



static void Display( void )
{
   glClearColor(0.3, 0.3, 0.3, 1);
   glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

   glEnable(GL_VERTEX_PROGRAM_NV);

   /* Should have min_index == 1, maybe force a rebase:
    */
   glDrawElements( GL_LINES, NR_VERTS*2, GL_UNSIGNED_SHORT, indices );

   glFlush(); 
}


static void Reshape( int width, int height )
{
   glViewport( 0, 0, width, height );
   glMatrixMode( GL_PROJECTION );
   glLoadIdentity();
   glOrtho(-1.0, 1.0, -1.0, 1.0, -0.5, 1000.0);
   glMatrixMode( GL_MODELVIEW );
   glLoadIdentity();
   /*glTranslatef( 0.0, 0.0, -15.0 );*/
}


static void Key( unsigned char key, int x, int y )
{
   (void) x;
   (void) y;
   switch (key) {
      case 27:
         exit(0);
         break;
   }
   glutPostRedisplay();
}




int main( int argc, char *argv[] )
{
   glutInit( &argc, argv );
   glutInitWindowPosition( 0, 0 );
   glutInitWindowSize( 250, 250 );
   glutInitDisplayMode( GLUT_RGB | GLUT_SINGLE | GLUT_DEPTH );
   glutCreateWindow(argv[0]);
   glewInit();
   glutReshapeFunc( Reshape );
   glutKeyboardFunc( Key );
   glutDisplayFunc( Display );
   Init();
   glutMainLoop();
   return 0;
}
