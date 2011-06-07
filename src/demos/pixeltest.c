
/* Pixeltest for lines.
 * Hui Qi Tay 
 *
 * Modified from
 * glReadPixels and glCopyPixels test by Brian Paul.
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <GL/glew.h> /* for GL_UNSIGNED_SHORT_5_6_5 */
#include "glut_wrap.h"


static int ImgWidth, ImgHeight;
static int WinWidth, WinHeight;

static int APosX, APosY;         /* simple drawpixels */
static int BPosX, BPosY;         /* read/draw pixels */
int MouseButton, MouseY, MouseX; /* mouse control */
float x0offset = 0.0f;           /* for line translation */
float y0offset = 0.0f; 
float x1offset = 0.0f;
float y1offset = 0.0f;
float width = 1.0f;

struct Line{
   float x0;
   float y0;
   float x1;
   float y1;
};

#define STEP 16                 /* subpixel resolution 1/STEP */
#define SIZE 128                /* of non-zoomed drawing region */
#define ZOOM 32                 /* scale factor for zooming */

struct Line line;
static GLboolean DrawFront = GL_FALSE;
GLushort TempImage[SIZE][SIZE]; /* original 128 by 128 pixel image */
GLushort myImage[SIZE*ZOOM][SIZE*ZOOM]; /* zoom by a factor of 32 */

static GLenum ReadFormat = GL_RGB;
static GLenum ReadType = GL_UNSIGNED_SHORT_5_6_5;

static void
Reset( void )
{
   APosX = 8;     APosY = 24;
   MouseX = 8;    MouseY = 24;
   BPosX = APosX + ImgWidth + 40;   BPosY = 24;
}


static void
PrintString(const char *s)
{
   while (*s) {
      glutBitmapCharacter(GLUT_BITMAP_8_BY_13, (int) *s);
      s++;
   }
}

static void
drawzoomline(int x0, int y0, int x1, int y1, GLushort color)
{
   /* Use the Bresenham Line Algorithm here. */
   GLboolean steep = (abs(y1 - y0) > abs(x1 - x0));
      
   int deltax;
   int deltay;
   float error;
   float deltaerr;
   int ystep;
   int x, y;

   if (steep) {
      int temp0 = x0;
      int temp1 = x1;
      x0 = y0;
      y0 = temp0;
      x1 = y1;
      y1 = temp1;
   }
   if (x0 > x1) {
      int tem0 = x0;
      int tem1 = y0;
      x0 = x1;
      x1 = tem0;
      y0 = y1;
      y1 = tem1;
   }
      
   deltax = x1 - x0;
   deltay = abs(y1 - y0);
   error = 0.0;
   deltaerr = (float)deltay / (float)deltax;
   y = y0;

   if (y0 < y1)
      ystep = 1; 
   else
      ystep = -1;
  
   for (x = x0; x < x1; x++) {
      if (y>0 && x>0) {
         if (!steep) {
            myImage[y][x] = color;
            myImage[y+1][x] = color;
         }
         else {
            myImage[x][y] = color;
            myImage[x+1][y] = color;
         }
      }
      error = error + deltaerr;
      if (error >= 0.5) {
         y = y + ystep;
         error = error - 1.0;
      }
   }
} 

static void
drawline(float xf0, float yf0, float xf1, float yf1, GLushort color)
{
   /* map line endpoints on to relevant pixel grids */
   int x0 = xf0 * ZOOM; 
   int y0 = yf0 * ZOOM;
   int x1 = xf1 * ZOOM;
   int y1 = yf1 * ZOOM;

   drawzoomline(x0, y0, x1, y1, color);
}

static void
plot(int x, int y, GLushort color)
{
   int i, j;

   for (i=0; i<ZOOM; i++) {
      for (j=0; j<ZOOM; j++) {
         if (i == 0 || j == 0) {
            /* draws the gridlines, use top-left rule */
            myImage[x*ZOOM+i][y*ZOOM+j] = 0;
         }
         else if ((i >= 13 && i <= 19 && j == 16 ) || (j >= 13 && j <= 19 && i == 16) ){
            /* draws the pixel centre */
            myImage[x*ZOOM+i][y*ZOOM+j] = 0;
         }
	 else if ( (i + j == 15) || fabsf(i - j) == 16 || (i + j == 47) ){
	    /* draws the pixel diamond, to test diamond exit rule */
            myImage[x*ZOOM+i][y*ZOOM+j] = 0;
         }
         else
            myImage[x*ZOOM+i][y*ZOOM+j]= color;
      }   
   }
}


static void
drawMagnifiedView(void)
{
   float halfwidth = width/2;
   int i, j;

   /* Sets up pixel grid for each pixel
    */      
   for (i=0; i<128; i++){
      for (j=0; j<128; j++){
         plot(i, j, TempImage[i][j]);
      }
   }
      
   /* Draws the actual line on zoomed version */
   drawline(line.x0+x0offset+APosX-MouseX, line.y0+y0offset+APosY-MouseY, 
            line.x1+x1offset+APosX-MouseX, line.y1+y1offset+APosY-MouseY, 0);
      
   /* Draws bounding line area */ 
   if (fabsf(line.x0 + x0offset - line.x1 - x1offset) >= 
       fabsf(line.y0 + y0offset - line.y1 - y1offset)) {
      /* X-MAJOR line */
      drawline(line.x0+x0offset+APosX-MouseX, line.y0+y0offset+APosY-MouseY+halfwidth, 
               line.x1+x1offset+APosX-MouseX, line.y1+y1offset+APosY-MouseY+halfwidth, 0xffff);
      drawline(line.x0+x0offset+APosX-MouseX, line.y0+y0offset+APosY-MouseY-halfwidth, 
               line.x1+x1offset+APosX-MouseX, line.y1+y1offset+APosY-MouseY-halfwidth, 0xffff);
      drawline(line.x0+x0offset+APosX-MouseX, line.y0+y0offset+APosY-MouseY+halfwidth, 
               line.x0+x0offset+APosX-MouseX, line.y0+y0offset+APosY-MouseY-halfwidth, 0xffff);
      drawline(line.x1+x1offset+APosX-MouseX, line.y1+y1offset+APosY-MouseY+halfwidth, 
               line.x1+x1offset+APosX-MouseX, line.y1+y1offset+APosY-MouseY-halfwidth, 0xffff);
   }
   else {
      /* Y-MAJOR line */
      drawline(line.x0+x0offset+APosX-MouseX+halfwidth, line.y0+y0offset+APosY-MouseY, 
               line.x1+x1offset+APosX-MouseX+halfwidth, line.y1+y1offset+APosY-MouseY, 0xffff);
      drawline(line.x0+x0offset+APosX-MouseX-halfwidth, line.y0+y0offset+APosY-MouseY, 
               line.x1+x1offset+APosX-MouseX-halfwidth, line.y1+y1offset+APosY-MouseY, 0xffff);
      drawline(line.x0+x0offset+APosX-MouseX+halfwidth, line.y0+y0offset+APosY-MouseY, 
               line.x0+x0offset+APosX-MouseX-halfwidth, line.y0+y0offset+APosY-MouseY, 0xffff);
      drawline(line.x1+x1offset+APosX-MouseX+halfwidth, line.y1+y1offset+APosY-MouseY, 
               line.x1+x1offset+APosX-MouseX-halfwidth, line.y1+y1offset+APosY-MouseY, 0xffff);
   }
}

static void
Display( void )
{
   float z = 0;

   glClearColor(.3, .3, .3, 1);
   glClear( GL_COLOR_BUFFER_BIT );
   
   /* draw original image */

   glViewport(APosX, APosY, ImgWidth, ImgHeight);
   glMatrixMode( GL_PROJECTION );
   glLoadIdentity();
   /* transformation to match the pixel array */
   glOrtho(0, ImgWidth, 0, ImgHeight, -1.0, 1.0);
   glDisable(GL_CULL_FACE);

   /* Blue background
    */
   glBegin(GL_POLYGON);
   glColor3f(.5,.5,1); 
   glVertex3f(0, 0, z);
   glVertex3f(0, 150, z);
   glVertex3f(150, 150, z);
   glVertex3f(150, 0, z);
   glEnd();

   /* Original line
    */
   glLineWidth(width);
   glBegin(GL_LINES);
   glColor3f(.8,0,0);
   glVertex3f(line.x0+x0offset, line.y0+y0offset, z);
   glColor3f(0,.9,0); 
   glVertex3f(line.x1+x1offset, line.y1+y1offset, z);
   glEnd();


   glColor3f(1,1,1);
   glViewport( 0, 0, WinWidth, WinHeight );
   glMatrixMode( GL_PROJECTION );
   glLoadIdentity();
   glOrtho( 0.0, WinWidth, 0.0, WinHeight, -1.0, 1.0 );
   
   /* might try alignment=4 here for testing */
   glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
   glPixelStorei(GL_PACK_ALIGNMENT, 1);

   /* Legend */
   glLineWidth(1.0);
   glBegin(GL_LINE_STRIP);
   glVertex3f(590, 800, 0);
   glVertex3f(590, 670, 0);
   glVertex3f(990, 670, 0);
   glVertex3f(990, 800, 0);   
   glVertex3f(590, 800, 0);   
   glEnd();

   glRasterPos2i(600, 780);
   PrintString("Key:");
   glRasterPos2i(600, 760);   
   PrintString(" Change linewidth: v/b");
   glRasterPos2i(600, 740);   
   PrintString(" Move whole line: UP/DOWN/LEFT/RIGHT arrow keys");
   glRasterPos2i(600, 720);   
   PrintString(" Move endpoint: i/k/j/l");
   glRasterPos2i(600, 700);   
   PrintString(" Move startpoint: w/s/a/d");
   glRasterPos2i(600, 680);
   PrintString(" Change view: drag/click mouse");

   glRasterPos2i(8, ImgHeight+30);   
   PrintString("Zoom Pixel Test");

   glRasterPos2i(APosX, 5);
   PrintString("Original");

   /* do readpixels, drawpixels */
   glRasterPos2i(BPosX, 5);
   PrintString("Read/DrawPixels");

   /* clear the temporary image to white (helpful for debugging */
   memset(TempImage, 255, sizeof TempImage);

   /* Read pixels from the color buffer */
   glReadPixels(MouseX, MouseY, ImgWidth, ImgHeight,
                ReadFormat, ReadType, TempImage);
   
   glRasterPos2i(BPosX, BPosY);
   glDisable(GL_DITHER);
   drawMagnifiedView();
 
   /* Write pixels to the frame buffer */
   glDrawPixels(ImgWidth*5, ImgHeight*5, ReadFormat, ReadType, myImage);

   if (!DrawFront)
      glutSwapBuffers();
   else
      glFinish();
}


static void
Reshape( int width, int height )
{
   WinWidth = width;
   WinHeight = height;

   glViewport( 0, 0, width, height );
   glMatrixMode( GL_PROJECTION );
   glLoadIdentity();
   glOrtho( 0.0, width, 0.0, height, -1.0, 1.0 );
   glMatrixMode( GL_MODELVIEW );
   glLoadIdentity();
}


static void 
Key( unsigned char key, int x, int y)
{
   switch (key) {
   case 'w':
      y0offset += 1.0/(float)STEP;
      break;
   case 's':
      y0offset -= 1.0/(float)STEP;
      break;
   case 'd':
      x0offset += 1.0/(float)STEP;
      break;
   case 'a':
      x0offset -= 1.0/(float)STEP;
      break;
   case 'i':
      y1offset += 1.0/(float)STEP;
      break;
   case 'k':
      y1offset -= 1.0/(float)STEP;
      break;
   case 'l':
      x1offset += 1.0/(float)STEP;
      break;
   case 'j':
      x1offset -= 1.0/(float)STEP;
      break;
   case 'b':
      width += 1.0/(float) STEP;
      printf("width = %f\n", width);
      break;
   case 'v':
      width -= 1.0/(float) STEP;
      printf("width = %f\n", width);
      break;
   case 27:
      exit(1);
      break;
   default:
      return;
   }
   glutPostRedisplay();
} 

static void 
SpecialKey( int k, int x, int y)
{
   switch (k) {
   case GLUT_KEY_UP:
      y0offset += 1.0/(float)STEP;
      y1offset += 1.0/(float)STEP;
      break;
   case GLUT_KEY_DOWN:
      y0offset -= 1.0/(float)STEP;
      y1offset -= 1.0/(float)STEP;
      break;
   case GLUT_KEY_RIGHT:
      x0offset += 1.0/(float)STEP;
      x1offset += 1.0/(float)STEP;
      break;
   case GLUT_KEY_LEFT:
      x0offset -= 1.0/(float)STEP;
      x1offset -= 1.0/(float)STEP;
      break;
   default:
      return;
   }
   glutPostRedisplay();
} 
 
static void 
Mouse(int button, int state, int x, int y)
{
   /* if left mouse button is pressed and dragged
    * then change screen view
    */
   if (state == GLUT_DOWN &&
       button == GLUT_LEFT_BUTTON &&
       x < 160 &&
       y > WinHeight-180)
   {
	    MouseX = x;
            MouseY = WinHeight - y;
   }
   glutPostRedisplay();
}

static void 
processMouseMotion(int x, int y)
{
    MouseX = x;
    MouseY = WinHeight - y;
    glutPostRedisplay();
}

static void
Init(void)
{
   printf("GL_VERSION = %s\n", (char *) glGetString(GL_VERSION));
   printf("GL_RENDERER = %s\n", (char *) glGetString(GL_RENDERER));

   ImgWidth=128;
   ImgHeight=128;
   x0offset = 0;
   y0offset = 0;
   x1offset = 0;
   y1offset = 0;

   line.x0 = 2;
   line.y0 = 2;
   line.x1 = 15;
   line.y1 = 15; 
   glutSetCursor(GLUT_CURSOR_CROSSHAIR);

   glPixelStorei(GL_UNPACK_ROW_LENGTH, ImgWidth*ZOOM);
   glPixelStorei(GL_PACK_ROW_LENGTH, ImgWidth);

   Reset();
}


int
main( int argc, char *argv[] )
{
   glutInitWindowSize( 1000, 800 );
   glutInit( &argc, argv );
  
   glutInitDisplayMode( GLUT_RGB | GLUT_DOUBLE );
   glutCreateWindow(argv[0]);
   Init();
   glutReshapeFunc( Reshape );
   glutKeyboardFunc (Key);
   glutSpecialFunc(SpecialKey);
   glutDisplayFunc( Display );

   glutMouseFunc(Mouse);
   glutMotionFunc(processMouseMotion);

   glutMainLoop();
   return 0;
}
