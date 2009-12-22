/* Even simpler for many drivers than trivial/tri -- pass-through
 * vertex shader and vertex data in a VBO.
 */

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <GL/glew.h>
#include <GL/glut.h>


struct {
   GLfloat pos[4];
   GLfloat color[4];
} verts[] =  
{
   { {  -0.9, -0.9, 0.0, 1.0 },
     {.8,0,0, 1}, 
   },

   { {  0.9,  -0.9, 0.0, 1.0 },
     { 0, .9, 0, 1 },
   },

   { { 0,  0.9, 0.0, 1.0 },
     {0,0,.7, 1}, 
   },
};

GLuint arrayObj;

static void Init( void )
{
   GLint errno;
   GLuint prognum;
   
   static const char *prog1 =
      "!!ARBvp1.0\n"
      "MOV  result.color, vertex.color;\n"
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

   glGenBuffersARB(1, &arrayObj);
   glBindBufferARB(GL_ARRAY_BUFFER_ARB, arrayObj);
   glBufferDataARB(GL_ARRAY_BUFFER_ARB, sizeof(verts), verts, GL_STATIC_DRAW_ARB);

   glVertexPointer( 4, GL_FLOAT, sizeof(verts[0]), 0 );
   glColorPointer( 4, GL_FLOAT, sizeof(verts[0]), (void *)(4*sizeof(float)) );
}



static void Display( void )
{
   glClearColor(0.3, 0.3, 0.3, 1);
   glClear( GL_COLOR_BUFFER_BIT );

   glEnable(GL_VERTEX_PROGRAM_NV);
   glDrawArrays( GL_TRIANGLES, 0, 3 );

   glutSwapBuffers();
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
   glutInitDisplayMode( GLUT_RGB | GLUT_DOUBLE );
   glutCreateWindow(argv[0]);
   glewInit();
   glutReshapeFunc( Reshape );
   glutKeyboardFunc( Key );
   glutDisplayFunc( Display );
   Init();
   glutMainLoop();
   return 0;
}
