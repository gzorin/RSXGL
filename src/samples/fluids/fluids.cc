/*
 * stable fluids
 */

#define GL3_PROTOTYPES
#include <GL3/gl3.h>
#include <GL3/gl3ext.h>

#include "rsxgltest.h"
#include "math3d.h"

#include <stddef.h>
#include "fluids_vpo.h"
#include "fluids_fpo.h"
#include "pointer_vpo.h"
#include "pointer_fpo.h"

#include "texture.h"
#include "pointer_png.h"

#include <io/pad.h>

#include <math.h>
#include <Eigen/Geometry>
#include <stdlib.h>
#include <stdio.h>

const char * rsxgltest_name = "fluids";

/* From demo.c: */
/* macros */

#define IX(i,j) ((i)+(N+2)*(j))

/* external definitions (from solver.c) */

extern "C" void dens_step ( int N, float * x, float * x0, float * u, float * v, float diff, float dt );
extern "C" void vel_step ( int N, float * u, float * v, float * u0, float * v0, float visc, float dt );

/* global variables */

static int N;
static float dt, diff, visc;
static float force, source;
static int dvel;

static float * u, * v, * u_prev, * v_prev;
static float * dens, * dens_prev;

static uint8_t * dens8 = 0;

static int win_x, win_y;
static int mouse_down[3] = { 0,0,0 };
static int omx, omy, mx, my;

/*
  ----------------------------------------------------------------------
   free/clear/allocate simulation data
  ----------------------------------------------------------------------
*/


static void free_data ( void )
{
	if ( u ) free ( u );
	if ( v ) free ( v );
	if ( u_prev ) free ( u_prev );
	if ( v_prev ) free ( v_prev );
	if ( dens ) free ( dens );
	if ( dens_prev ) free ( dens_prev );
}

static void clear_data ( void )
{
	int i, size=(N+2)*(N+2);

	for ( i=0 ; i<size ; i++ ) {
		u[i] = v[i] = u_prev[i] = v_prev[i] = dens[i] = dens_prev[i] = 0.0f;
	}
}

static int allocate_data ( void )
{
	int size = (N+2)*(N+2);

	u			= (float *) malloc ( size*sizeof(float) );
	v			= (float *) malloc ( size*sizeof(float) );
	u_prev		= (float *) malloc ( size*sizeof(float) );
	v_prev		= (float *) malloc ( size*sizeof(float) );
	dens		= (float *) malloc ( size*sizeof(float) );	
	dens_prev	= (float *) malloc ( size*sizeof(float) );

	dens8 = (uint8_t *) malloc ( size*sizeof(uint8_t) );

	if ( !u || !v || !u_prev || !v_prev || !dens || !dens_prev || !dens8 ) {
	  tcp_printf ( "cannot allocate data\n" );
		return ( 0 );
	}

	return ( 1 );
}

/*
  ----------------------------------------------------------------------
   relates mouse movements to forces sources
  ----------------------------------------------------------------------
*/

static void get_from_UI ( float * d, float * u, float * v )
{
	int i, j, size = (N+2)*(N+2);

	for ( i=0 ; i<size ; i++ ) {
		u[i] = v[i] = d[i] = 0.0f;
	}

	if ( !mouse_down[0] && !mouse_down[2] ) return;

	//i = (int)((       mx /(float)win_x)*N+1);
	i = (int)(((mx - ((win_x - win_y) / 2)) / (float)win_y)*N+1);
	j = (int)(((win_y-my)/(float)win_y)*N+1);

	if ( i<1 || i>N || j<1 || j>N ) return;

	if ( mouse_down[0] ) {
		u[IX(i,j)] = force * (mx-omx);
		v[IX(i,j)] = force * (omy-my);
	}

	if ( mouse_down[2] ) {
		d[IX(i,j)] = source;
	}

	omx = mx;
	omy = my;

	return;
}

/* OpenGL globals: */
GLuint programs[2] = { 0,0 };
GLuint geometry[2] = { 0,0 };
GLuint textures[2] = { 0,0 };
GLuint vaos[2] = { 0,0 };
int xy_location = -1;

extern "C"
void
rsxgltest_pad(unsigned int,const padData * paddata)
{
  static unsigned int cross = ~0, triangle = ~0;

  if(paddata -> BTN_CROSS != cross) {
    cross = paddata -> BTN_CROSS;
    mouse_down[2] = cross;
  }
  else if(paddata -> BTN_TRIANGLE != triangle) {
    triangle = paddata -> BTN_TRIANGLE;
    mouse_down[0] = triangle;
  }

  // abs of values below this get ignored:
  const float threshold = 0.05;

  const float
    left_stick_h = ((float)paddata -> ANA_L_H - 127.0f) / 127.0f,
    left_stick_v = ((float)paddata -> ANA_L_V - 127.0f) / 127.0f,
    right_stick_h = ((float)paddata -> ANA_R_H - 127.0f) / 127.0f,
    right_stick_v = ((float)paddata -> ANA_R_V - 127.0f) / 127.0f;

  const float
    dmx = 20, dmy = 20;

  if(fabs(left_stick_h) > threshold) {
    mx += dmx * left_stick_h;
    if(mx < 0) mx = 0;
    else if(mx > win_x) mx = win_x;
  }

  if(fabs(left_stick_v) > threshold) {
    my += dmy * left_stick_v;
    if(my < 0) my = 0;
    else if(my > win_y) my = win_y;
  }
}

extern "C"
void
rsxgltest_init(int argc,const char ** argv)
{
  tcp_printf("%s\n",__PRETTY_FUNCTION__);

  N = 80;
  dt = 0.1f;
  diff = 0.0f;
  visc = 0.0f;
  force = 5.0f;
  source = 100.0f;
  tcp_printf ( "Using defaults : N=%d dt=%g diff=%g visc=%g force = %g source=%g\n",
	    N, dt, diff, visc, force, source );

  dvel = 0;

  if ( !allocate_data () ) exit ( 1 );
  clear_data ();

  win_x = rsxgltest_width;
  win_y = rsxgltest_height;
  omx = mx = win_x / 2;
  omx = my = win_y / 2;

  glDisable(GL_DEPTH_TEST);
  glDepthMask(0);

  glGenBuffers(2,geometry);
  glGenTextures(2,textures);
  glGenVertexArrays(2,vaos);

  // Setup fluid:
  {
    GLuint shaders[2] = {
      glCreateShader(GL_VERTEX_SHADER),
      glCreateShader(GL_FRAGMENT_SHADER)
    };

    programs[0] = glCreateProgram();
  
    glAttachShader(programs[0],shaders[0]);
    glAttachShader(programs[0],shaders[1]);
    
    glShaderBinary(1,&shaders[0],0,fluids_vpo,fluids_vpo_size);
    glShaderBinary(1,&shaders[1],0,fluids_fpo,fluids_fpo_size);
    
    glLinkProgram(programs[0]);
    glValidateProgram(programs[0]);
  
    GLint
      vertex_location = glGetAttribLocation(programs[0],"position"),
      ProjMatrix_location = glGetUniformLocation(programs[0],"ProjMatrix"),
      texture_location = glGetUniformLocation(programs[0],"texture");

    tcp_printf("vertex_location: %i ProjMatrix_location: %i texture_location: %i\n",
	       vertex_location,ProjMatrix_location,texture_location);
    
    glUseProgram(programs[0]);
    
    const float tmp = (((float)rsxgltest_width / (float)rsxgltest_height) - 1.0) / 2.0;
    Eigen::Projective3f ProjMatrix = ortho(-tmp,1 + tmp,0,1,-1,1);
    glUniformMatrix4fv(ProjMatrix_location,1,GL_FALSE,ProjMatrix.data());
    
    glUniform1i(texture_location,0);

    // Geometry:
    const float _geometry[] = {
      0,0,
      1,0,
      1,1,
      
      1,1,
      0,1,
      0,0
    };
 
    glBindVertexArray(vaos[0]);
    glBindBuffer(GL_ARRAY_BUFFER,geometry[0]);
    glBufferData(GL_ARRAY_BUFFER,sizeof(float) * 2 * 6,_geometry,GL_STATIC_DRAW);
    glEnableVertexAttribArray(vertex_location);
    glVertexAttribPointer(vertex_location,2,GL_FLOAT,GL_FALSE,0,0);
    glBindBuffer(GL_ARRAY_BUFFER,0);
    
    // Textures
    glBindTexture(GL_TEXTURE_2D,textures[0]);
    glTexStorage2D(GL_TEXTURE_2D,1,GL_R8,N + 2,N + 2);
    
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
  }

  // Setup pointer:
  {
    GLuint shaders[2] = {
      glCreateShader(GL_VERTEX_SHADER),
      glCreateShader(GL_FRAGMENT_SHADER)
    };

    programs[1] = glCreateProgram();
  
    glAttachShader(programs[1],shaders[0]);
    glAttachShader(programs[1],shaders[1]);
    
    glShaderBinary(1,&shaders[0],0,pointer_vpo,pointer_vpo_size);
    glShaderBinary(1,&shaders[1],0,pointer_fpo,pointer_fpo_size);
    
    glLinkProgram(programs[1]);
    glValidateProgram(programs[1]);
  
    GLint
      vertex_location = glGetAttribLocation(programs[1],"position"),
      tc_location = glGetAttribLocation(programs[1],"tc"),
      ProjMatrix_location = glGetUniformLocation(programs[1],"ProjMatrix"),
      texture_location = glGetUniformLocation(programs[0],"texture");
    xy_location = glGetUniformLocation(programs[1],"xy");

    tcp_printf("vertex_location: %i tc_location: %i ProjMatrix_location: %i xy_location: %i texture_location: %i\n",
	       vertex_location,tc_location,ProjMatrix_location,xy_location,texture_location);
    
    glUseProgram(programs[1]);

    Eigen::Projective3f ProjMatrix = ortho(0,rsxgltest_width,rsxgltest_height,0,-1,1);
    glUniformMatrix4fv(ProjMatrix_location,1,GL_FALSE,ProjMatrix.data());

    glUniform1i(texture_location,0);

    // Geometry:
    const float _geometry[] = {
      0,0, 0,0,
      32,0, 1,0,
      32,32, 1,1,
      
      32,32, 1,1,
      0,32, 0,1,
      0,0, 0,0
    };
 
    glBindVertexArray(vaos[1]);
    glBindBuffer(GL_ARRAY_BUFFER,geometry[1]);
    glBufferData(GL_ARRAY_BUFFER,sizeof(float) * 4 * 6,_geometry,GL_STATIC_DRAW);
    glEnableVertexAttribArray(vertex_location);
    glVertexAttribPointer(vertex_location,2,GL_FLOAT,GL_FALSE,sizeof(float) * 4,0);
    glEnableVertexAttribArray(tc_location);
    glVertexAttribPointer(tc_location,2,GL_FLOAT,GL_FALSE,sizeof(float) * 4,(const GLvoid *)(sizeof(float) * 2));
    glBindBuffer(GL_ARRAY_BUFFER,0);

    Image pointer_image = loadPng(pointer_png);

    // Textures
    glBindTexture(GL_TEXTURE_2D,textures[1]);
    glTexStorage2D(GL_TEXTURE_2D,1,GL_RGBA,pointer_image.width,pointer_image.height);
    glTexSubImage2D(GL_TEXTURE_2D,0,0,0,pointer_image.width,pointer_image.height,GL_BGRA,GL_UNSIGNED_BYTE,pointer_image.data);
    
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
  }
}

extern "C"
int
rsxgltest_draw()
{
  // Update simulation:
  get_from_UI ( dens_prev, u_prev, v_prev );
  vel_step ( N, u, v, u_prev, v_prev, visc, rsxgltest_delta_time );
  dens_step ( N, dens, dens_prev, u, v, diff, rsxgltest_delta_time );
  
  // Draw:
  glClearColor(0,0,0,1);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  // Fluid:
  {
    glDisable(GL_BLEND);

    glUseProgram(programs[0]);

    for(int i = 0,n = (N+2)*(N+2);i < n;++i) {
      dens8[i] = (uint8_t)(dens[i] * 255.0f);
    }
    
    glBindTexture(GL_TEXTURE_2D,textures[0]);
    glTexSubImage2D(GL_TEXTURE_2D,0,0,0,N + 2,N + 2,GL_RED,GL_UNSIGNED_BYTE,dens8);

    glBindVertexArray(vaos[0]);
    glDrawArrays(GL_TRIANGLES,0,6);
  }

  // Pointer
  {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);

    glUseProgram(programs[1]);

    glUniform2f(xy_location,mx,my);

    glBindTexture(GL_TEXTURE_2D,textures[1]);

    glBindVertexArray(vaos[1]);
    glDrawArrays(GL_TRIANGLES,0,6);
  }

  return 1;
}

extern "C"
void
rsxgltest_exit()
{
  tcp_printf("%s\n",__PRETTY_FUNCTION__);
}
