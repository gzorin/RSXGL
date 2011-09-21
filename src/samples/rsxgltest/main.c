/*
 * rsxgltest - host code
 *
 * I promise this won't become a rewrite of GLUT. In fact, I plan to switch to SDL soon.
 */

#include <EGL/egl.h>
#define GL3_PROTOTYPES
#include <GL3/gl3.h>
#include <GL3/rsxgl.h>
#include <GL3/rsxgl3ext.h>

#include <net/net.h>
#include <sysutil/sysutil.h>
#include <io/pad.h>

#include <stdio.h>
#include <unistd.h>
#include <malloc.h>
#include <string.h>
#include <errno.h>
#include <math.h>

#include <time.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <stdarg.h>

#include <rsx/commands.h>

// Test program fills these in:
extern const char * rsxgltest_name;

extern void rsxgltest_init(int, const char **);
extern int rsxgltest_draw();
extern void rsxgltest_pad(const padData *);
extern void rsxgltest_exit();

// Test program might want to use these:
int rsxgltest_width = 0, rsxgltest_height = 0;
float rsxgltest_elapsed_time = 0;

// Configure these (especially the IP) to your own setup.
// Use netcat to receive the results on your PC:
// TCP: nc -l -p 4000
// UDP: nc -u -l -p 4000
// For some versions of netcat the -p option may need to be removed.
//
#define TESTIP		"192.168.1.7"
//#define TESTIP          "192.168.1.115"
#define TESTPORT	9000

int sock = 0;

void tcp_init()
{
	printf("Beginning TCP socket test...\n");

	sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock < 0) {
		printf("Unable to create a socket: %d\n", errno);
		return;
	}
	printf("Socket created: %d\n", sock);

	struct sockaddr_in server;
	memset(&server, 0, sizeof(server));
	server.sin_len = sizeof(server);
	server.sin_family = AF_INET;
	inet_pton(AF_INET, TESTIP, &server.sin_addr);
	server.sin_port = htons(TESTPORT);

	int ret = connect(sock, (struct sockaddr*)&server, sizeof(server));
	if (ret) {
		printf("Unable to connect to server: %d\n", errno);
		return;
	}
}

void tcp_exit()
{
  int ret = shutdown(sock, SHUT_RDWR);
  if (ret < 0)
    printf("Unable to shutdown socket: %d\n", errno);
  else
    printf("Socket shutdown successfully!\n");
  
  ret = close(sock);
  if (ret < 0)
    printf("Unable to close socket: %d\n", ret);
  else
    printf("TCP test successful!\n");
}

void tcp_printf(const char * fmt,...)
{
  static char buffer[2048];

  va_list ap;
  va_start(ap,fmt);
  vsnprintf(buffer,2048,fmt,ap);
  va_end(ap);

  write(sock,buffer,strlen(buffer));
}

void tcp_puts(GLsizei n,const GLchar * s)
{
  write(sock,s,n);
}

void tcp_log(const char * fmt,...)
{
  tcp_printf("[%s]: ",rsxgltest_name);
  
  static char buffer[2048];

  va_list ap;
  va_start(ap,fmt);
  vsnprintf(buffer,2048,fmt,ap);
  va_end(ap);

  write(sock,buffer,strlen(buffer));
}

void report_glerror(const char * label)
{
  GLenum e = glGetError();
  if(e != GL_NO_ERROR) {
    if(label != 0) {
      tcp_printf("%s: %x\n",label,e);
    }
    else {
      tcp_printf("%x\n",e);
    }
  }
}

void
report_shader_info(GLuint shader)
{
  GLint type = 0, delete_status = 0, compile_status = 0;

  if(glIsShader(shader)) {
    glGetShaderiv(shader,GL_SHADER_TYPE,&type);
    glGetShaderiv(shader,GL_DELETE_STATUS,&delete_status);
    glGetShaderiv(shader,GL_COMPILE_STATUS,&compile_status);
    
    tcp_printf("shader: %u type: %x compile_status: %i delete_status: %i\n",shader,type,compile_status,delete_status);

    GLint nInfo = 0;
    glGetShaderiv(shader,GL_INFO_LOG_LENGTH,&nInfo);
    if(nInfo > 0) {
      tcp_printf("\tinfo length: %u\n",nInfo);
      char szInfo[nInfo + 1];
      glGetShaderInfoLog(shader,nInfo + 1,0,szInfo);
      tcp_printf("\tinfo: %s\n",szInfo);
    }

  }
  else {
    tcp_printf("%u is not a shader\n",shader);
  }
}

void
report_program_info(GLuint program)
{
  if(glIsProgram(program)) {
    GLint delete_status = 0, link_status = 0, validate_status = 0;

    glGetProgramiv(program,GL_DELETE_STATUS,&delete_status);
    glGetProgramiv(program,GL_LINK_STATUS,&link_status);
    glGetProgramiv(program,GL_VALIDATE_STATUS,&validate_status);
    
    tcp_printf("program: %u link_status: %i validate_status: %i delete_status: %i\n",program,link_status,validate_status,delete_status);

    GLint num_attached = 0;
    glGetProgramiv(program,GL_ATTACHED_SHADERS,&num_attached);
    tcp_printf("\tattached shaders: %u\n",num_attached);
    if(num_attached > 0) {
      GLuint attached[2] = { 0,0 };
      glGetAttachedShaders(program,2,0,attached);
      tcp_printf("\t");
      for(size_t i = 0;i < 2;++i) {
	if(attached[i] > 0) {
	  tcp_printf("%u ",attached[i]);
	}
      }
      tcp_printf("\n");
    }

    GLint nInfo = 0;
    glGetProgramiv(program,GL_INFO_LOG_LENGTH,&nInfo);
    if(nInfo > 0) {
      tcp_printf("\tinfo length: %u\n",nInfo);
      char szInfo[nInfo + 1];
      glGetProgramInfoLog(program,nInfo + 1,0,szInfo);
      tcp_printf("\tinfo: %s\n",szInfo);
    }
  }
  else {
    tcp_printf("%u is not a program\n",program);
  }
}

void
summarize_program(const char * label,GLuint program)
{
  tcp_printf("summary of program %s:\n",label);

  // Report on attributes:
  {
    GLint num_attribs = 0, attrib_name_length = 0;
    glGetProgramiv(program,GL_ACTIVE_ATTRIBUTES,&num_attribs);
    glGetProgramiv(program,GL_ACTIVE_ATTRIBUTE_MAX_LENGTH,&attrib_name_length);
    tcp_printf("%u attribs, name max length: %u\n",num_attribs,attrib_name_length);
    char szName[attrib_name_length + 1];

    for(size_t i = 0;i < num_attribs;++i) {
      GLint size = 0;
      GLenum type = 0;
      GLint location = 0;
      glGetActiveAttrib(program,i,attrib_name_length + 1,0,&size,&type,szName);
      location = glGetAttribLocation(program,szName);
      tcp_printf("\t%u: %s %u %u %u\n",i,szName,(unsigned int)location,(unsigned int)size,(unsigned int)type);
    }
  }

// Report on uniforms:
  {
    GLint num_uniforms = 0, uniform_name_length = 0;
    glGetProgramiv(program,GL_ACTIVE_UNIFORMS,&num_uniforms);
    glGetProgramiv(program,GL_ACTIVE_UNIFORM_MAX_LENGTH,&uniform_name_length);
    tcp_printf("%u uniforms, name max length: %u\n",num_uniforms,uniform_name_length);
    char szName[uniform_name_length + 1];

    for(size_t i = 0;i < num_uniforms;++i) {
      GLint size = 0;
      GLenum type = 0;
      GLint location = 0;
      glGetActiveUniform(program,i,uniform_name_length + 1,0,&size,&type,szName);
      location = glGetUniformLocation(program,szName);
      tcp_printf("\t%u: %s %u %u %x\n",i,szName,(unsigned int)location,(unsigned int)size,(unsigned int)type);
    }
  }
}

// quit:
int running = 1;

static void
eventHandle(u64 status, u64 param, void * userdata) {
  (void)param;
  (void)userdata;
  if(status == SYSUTIL_EXIT_GAME){
    //printf("Quit app requested\n");
    //exit(0);
    running = 0;
  }else{
    //printf("Unhandled event: %08llX\n", (unsigned long long int)status);
  }
}

void
appCleanup()
{
  sysUtilUnregisterCallback(SYSUTIL_EVENT_SLOT0);
  tcp_exit();
  netDeinitialize();
  printf("Exiting for real.\n");
}

/* Convenience macros for operations on timevals.
   NOTE: `timercmp' does not work for >= or <=.  */
#define	timerisset(tvp)		((tvp)->tv_sec || (tvp)->tv_usec)
#define	timerclear(tvp)		((tvp)->tv_sec = (tvp)->tv_usec = 0)
#define	timercmp(a, b, CMP) 						      \
  (((a)->tv_sec == (b)->tv_sec) ? 					      \
   ((a)->tv_usec CMP (b)->tv_usec) : 					      \
   ((a)->tv_sec CMP (b)->tv_sec))
#define	timeradd(a, b, result)						      \
  do {									      \
    (result)->tv_sec = (a)->tv_sec + (b)->tv_sec;			      \
    (result)->tv_usec = (a)->tv_usec + (b)->tv_usec;			      \
    if ((result)->tv_usec >= 1000000)					      \
      {									      \
	++(result)->tv_sec;						      \
	(result)->tv_usec -= 1000000;					      \
      }									      \
  } while (0)
#define	timersub(a, b, result)						      \
  do {									      \
    (result)->tv_sec = (a)->tv_sec - (b)->tv_sec;			      \
    (result)->tv_usec = (a)->tv_usec - (b)->tv_usec;			      \
    if ((result)->tv_usec < 0) {					      \
      --(result)->tv_sec;						      \
      (result)->tv_usec += 1000000;					      \
    }									      \
  } while (0)

int
main(int argc, const char ** argv)
{
  netInitialize();
  tcp_init();
  tcp_printf("%s\n",rsxgltest_name);

  glInitDebug(1024*16,tcp_puts);

  ioPadInit(1);
  padInfo padinfo;
  padData paddata;

  EGLDisplay dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);

  if(dpy != EGL_NO_DISPLAY) {
    // convert to a timeval structure:
    const float ft = 1.0f / 60.0f;
    float ft_integral, ft_fractional;
    ft_fractional = modff(ft,&ft_integral);
    struct timeval frame_time = { 0,0 };
    frame_time.tv_sec = (int)ft_integral;
    frame_time.tv_usec = (int)(ft_fractional * 1.0e6);
    
    EGLint version0 = 0,version1 = 0;
    EGLBoolean result;
    result = eglInitialize(dpy,&version0,&version1);
    
    if(result) {
      tcp_printf("eglInitialize version: %i %i:%i\n",version0,version1,(int)result);
      
      EGLint attribs[] = {
	EGL_RED_SIZE,8,
	EGL_BLUE_SIZE,8,
	EGL_GREEN_SIZE,8,
	EGL_ALPHA_SIZE,8,

	EGL_DEPTH_SIZE,16,
	EGL_NONE
      };
      EGLConfig config;
      EGLint nconfig = 0;
      result = eglChooseConfig(dpy,attribs,&config,1,&nconfig);
      tcp_printf("eglChooseConfig:%i %u configs\n",(int)result,nconfig);
      if(nconfig > 0) {
	EGLSurface surface = eglCreateWindowSurface(dpy,config,0,0);
	
	if(surface != EGL_NO_SURFACE) {
	  eglQuerySurface(dpy,surface,EGL_WIDTH,&rsxgltest_width);
	  eglQuerySurface(dpy,surface,EGL_HEIGHT,&rsxgltest_height);

	  tcp_printf("eglCreateWindowSurface: %ix%i\n",rsxgltest_width,rsxgltest_height);
	  
	  EGLContext ctx = eglCreateContext(dpy,config,0,0);
	  tcp_printf("eglCreateContext: %lu\n",(unsigned long)ctx);
	  
	  if(ctx != EGL_NO_CONTEXT) {
	    atexit(appCleanup);
	    sysUtilRegisterCallback(SYSUTIL_EVENT_SLOT0, eventHandle, NULL);
	    
	    struct timeval start_time, current_time;
	    struct timeval timeout_time = {
	      .tv_sec = 6,
	      .tv_usec = 0
	    };

	    // Initialize:
	    result = eglMakeCurrent(dpy,surface,surface,ctx);
	    rsxgltest_init(argc,argv);

	    gettimeofday(&start_time,0);
   
	    while(running) {
	      gettimeofday(&current_time,0);

	      struct timeval elapsed_time;
	      timersub(&current_time,&start_time,&elapsed_time);
	      rsxgltest_elapsed_time = ((float)(elapsed_time.tv_sec)) + ((float)(elapsed_time.tv_usec) / 1.0e6f);

	      ioPadGetInfo(&padinfo);
	      for(size_t i = 0;i < MAX_PADS;++i) {
		if(padinfo.status[i]) {
		  ioPadGetData(i,&paddata);
		  rsxgltest_pad(&paddata);
		  break;
		}
	      }
	      
	      result = eglMakeCurrent(dpy,surface,surface,ctx);
	      
	      result = rsxgltest_draw();
	      if(!result) break;
	      
	      result = eglSwapBuffers(dpy,surface);

	      EGLint e = eglGetError();
	      if(!result) {
		tcp_printf("Swap sync timed-out: %x\n",e);
		break;
	      }
	      else {
		struct timeval t, elapsed_time;
		gettimeofday(&t,0);
		timersub(&t,&current_time,&elapsed_time);
		
		if(timercmp(&elapsed_time,&frame_time,<)) {
		  struct timeval sleep_time;
		  timersub(&frame_time,&elapsed_time,&sleep_time);
		  usleep((sleep_time.tv_sec * 1e6) + sleep_time.tv_usec);
		}
		
		sysUtilCheckCallback();
	      }
	    }
	    
	    rsxgltest_exit();

	    result = eglDestroyContext(dpy,ctx);
	    tcp_printf("eglDestroyContext:%i\n",(int)result);
	  }
	  else {
	    tcp_printf("eglCreateContext failed: %x\n",eglGetError());
	  }
	}
	else {
	  tcp_printf("eglCreateWindowSurface failed: %x\n",eglGetError());
	}
      }
      
      result = eglTerminate(dpy);
      tcp_printf("eglTerminate:%i\n",(int)result);

      exit(0);
    }
    else {
      tcp_printf("eglInitialize failed: %x\n",eglGetError());
    }
  }
  else {
    tcp_printf("eglGetDisplay failed: %x\n",eglGetError());
  }

  appCleanup();
    
  return 0;
}
