#include "mem.h"
#include "nv40.h"

#include "egl_types.h"
#include "rsxgl_config.h"
#include "rsxgl_limits.h"

#include "util/u_format.h"
#include "nouveau/nouveau_winsys.h"

#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <assert.h>

#include <EGL/egl.h>
#include "GL3/rsxgl.h"

#include <sysutil/video.h>
#include <rsx/gcm_sys.h>

//
#if !defined(NDEBUG)

#include "rsxgl_assert.h"

#if defined(assert)
#undef assert
#endif

#define assert rsxgl_assert

#endif

static EGLint rsxegl_error = EGL_SUCCESS;
static int rsxegl_initialized = 0;

EGLAPI EGLint EGLAPIENTRY
eglGetError(void)
{
  EGLint e = rsxegl_error;
  rsxegl_error = EGL_SUCCESS;
  return e;
}

static inline void
rsxeglSetError(EGLint e)
{
  if(rsxegl_error == EGL_SUCCESS && e != EGL_SUCCESS) {
    rsxegl_error = e;
  }
}

#define RSXEGL_NOERROR(RETURN)			\
  {rsxeglSetError(EGL_SUCCESS);			\
    return (RETURN);}

#define RSXEGL_ERROR(ERROR,RETURN)		\
  {rsxeglSetError((ERROR));			\
    return (RETURN);}

#define RSXEGL_NOERROR_				\
  {rsxeglSetError(EGL_SUCCESS);			\
    return;}

#define RSXEGL_ERROR_(ERROR)			\
  {rsxeglSetError((ERROR));			\
    return;}

static struct rsxegl_display_t {
  videoState state;
  videoResolution resolution;
  
} rsxegl_display;

EGLAPI EGLDisplay EGLAPIENTRY
eglGetDisplay(EGLNativeDisplayType display_id)
{
  // Requesting something other than the default (only) display:
  if(display_id != EGL_DEFAULT_DISPLAY) {
    RSXEGL_ERROR(EGL_BAD_PARAMETER,EGL_NO_DISPLAY);
  }
  
  // Get the video state:
  if(videoGetState(0, 0, &rsxegl_display.state) != 0) {
    RSXEGL_ERROR(EGL_BAD_ALLOC,EGL_NO_DISPLAY);
  }
  
  if(rsxegl_display.state.state != 0) {
    RSXEGL_ERROR(EGL_BAD_ALLOC,EGL_NO_DISPLAY);
  }
  
  if(videoGetResolution(rsxegl_display.state.displayMode.resolution, &rsxegl_display.resolution) != 0) {
    RSXEGL_ERROR(EGL_BAD_ALLOC,EGL_NO_DISPLAY);
  }
  
  RSXEGL_NOERROR(&rsxegl_display);
}

// Macro that will check EGLDisplay values that are passed in:
#define RSXEGL_CHECK_DISPLAY(DPY,RETURN)	\
  if((DPY) != &rsxegl_display) {		\
    RSXEGL_ERROR(EGL_BAD_DISPLAY,(RETURN));	\
  }

// RSX-specific initialization parameters: size of the shared memory buffer, and length of the command buffer:

// Also read by mem.c:
struct rsxgl_init_parameters_t rsxgl_init_parameters = {
  .gcm_buffer_size = RSXGL_CONFIG_default_gcm_buffer_size,
  .command_buffer_length = RSXGL_CONFIG_default_command_buffer_length,
  .max_swap_wait_iterations = 100000,
  .swap_wait_interval = RSXGL_SYNC_SLEEP_INTERVAL,
  .rsx_mspace_offset = 0,
  .rsx_mspace_size = 0
};

static void * rsx_shared_memory = 0;

EGLAPI void EGLAPIENTRY
rsxglConfigure(struct rsxgl_init_parameters_t const * parameters)
{
  // RSX shared memory buffer must be 1MB at least:
  if(parameters -> gcm_buffer_size < 1024*1024) {
    RSXEGL_ERROR_(EGL_BAD_PARAMETER);
  }

  const uint32_t _rsx_command_buffer_size = (parameters -> command_buffer_length) * sizeof(uint32_t);

  // Command buffer size must fit in the overall buffer:
  if(_rsx_command_buffer_size > parameters -> gcm_buffer_size) {
    RSXEGL_ERROR_(EGL_BAD_PARAMETER);
  }

  rsxgl_init_parameters = *parameters;
}

gcmContextData * rsx_gcm_context = 0;

static struct pipe_screen * rsx_screen = 0;

EGLAPI EGLBoolean EGLAPIENTRY
eglInitialize(EGLDisplay dpy,EGLint * major,EGLint * minor)
{
  RSXEGL_CHECK_DISPLAY(dpy,EGL_FALSE);

  // Have we done this before?
  if(rsxegl_initialized == 0) {
    // Allocate memory that will be shared with the RSX:
    rsx_shared_memory = memalign(1024*1024,rsxgl_init_parameters.gcm_buffer_size);
    if(rsx_shared_memory == 0) {
      RSXEGL_ERROR(EGL_BAD_ALLOC,EGL_FALSE);
    }

    // Initialize RSX:
    gcmContextData * _rsx_gcm_context ATTRIBUTE_PRXPTR;
    int r = gcmInitBody(&_rsx_gcm_context,rsxgl_init_parameters.command_buffer_length * sizeof(uint32_t),rsxgl_init_parameters.gcm_buffer_size,rsx_shared_memory);
    if(r != 0) {
      RSXEGL_ERROR(EGL_BAD_ALLOC,EGL_FALSE);
    }
    rsx_gcm_context = _rsx_gcm_context;

    gcmSetFlipMode(GCM_FLIP_VSYNC);
    gcmResetFlipStatus();

    //
    rsx_screen = nvfx_screen_create(0);

    rsxegl_initialized = 1;
  }

  if(major != 0) *major = 1;
  if(minor != 0) *minor = 4;

  RSXEGL_NOERROR(EGL_TRUE);
}

#define RSXEGL_CHECK_INITIALIZED(RETURN)	\
  if(rsxegl_initialized == 0) {			\
    RSXEGL_ERROR(EGL_NOT_INITIALIZED,(RETURN));	\
  }

void
rsx_flush()
{
  gcmControlRegister *control = gcmGetControlRegister();
  __asm __volatile__("sync"); // Sync, to make sure the command was written;
  uint32_t offset;
  gcmAddressToOffset(rsx_gcm_context->current, &offset);
  control->put = offset;
}

EGLAPI EGLBoolean EGLAPIENTRY
eglTerminate(EGLDisplay dpy)
{
  RSXEGL_CHECK_DISPLAY(dpy,EGL_FALSE);
  RSXEGL_CHECK_INITIALIZED(EGL_FALSE);

  // There seems to be a gcmTerminate... but not sure what its arguments are:
  // gcmTerminate();
  // Probably should free this too:
  // free(rsx_shared_memory);

  rsxegl_initialized = 0;

  RSXEGL_NOERROR(EGL_TRUE);
}

EGLAPI const char * EGLAPIENTRY
eglQueryString(EGLDisplay dpy,EGLint name)
{
  RSXEGL_CHECK_DISPLAY(dpy,NULL);
  RSXEGL_CHECK_INITIALIZED(NULL);

  switch(name) {
  case EGL_CLIENT_APIS:
    return "OpenGL ES";
    break;
  case EGL_EXTENSIONS:
    RSXEGL_NOERROR("");
    break;
  case EGL_VENDOR:
    RSXEGL_NOERROR("Blackbird");
    break;
  case EGL_VERSION:
    RSXEGL_NOERROR("1.4");
    break;
  default:
    RSXEGL_ERROR(EGL_BAD_PARAMETER,"");
  };
}

struct rsxegl_config_t rsxegl_configs[] = {
  {
    .egl_config_id = 0,
    .egl_buffer_size = 32,
    .egl_red_size = 8,
    .egl_green_size = 8,
    .egl_blue_size = 8,
    .egl_alpha_size = 8,
    .egl_depth_size = 16,
    .egl_stencil_size = 0,

    .color_pixel_size = 4,
    .depth_pixel_size = 2,

    .video_format = VIDEO_BUFFER_FORMAT_XRGB,
    .color_pformat = PIPE_FORMAT_R8G8B8A8_UNORM,
    .depth_pformat = PIPE_FORMAT_Z16_UNORM
  },

  {
    .egl_config_id = 1,
    .egl_buffer_size = 32,
    .egl_red_size = 8,
    .egl_green_size = 8,
    .egl_blue_size = 8,
    .egl_alpha_size = 8,
    .egl_depth_size = 24,
    .egl_stencil_size = 8,

    .color_pixel_size = 4,
    .depth_pixel_size = 4,

    .video_format = VIDEO_BUFFER_FORMAT_XRGB,
    .color_pformat = PIPE_FORMAT_R8G8B8A8_UNORM,
    .depth_pformat = PIPE_FORMAT_S8_UINT_Z24_UNORM
  },

  {
    .egl_config_id = 2,
    .egl_buffer_size = 24,
    .egl_red_size = 8,
    .egl_green_size = 8,
    .egl_blue_size = 8,
    .egl_alpha_size = 0,
    .egl_depth_size = 16,
    .egl_stencil_size = 0,

    .color_pixel_size = 4,
    .depth_pixel_size = 2,

    .video_format = VIDEO_BUFFER_FORMAT_XRGB,
    .color_pformat = PIPE_FORMAT_R8G8B8_UNORM,
    .depth_pformat = PIPE_FORMAT_Z16_UNORM
  },

  {
    .egl_config_id = 3,
    .egl_buffer_size = 24,
    .egl_red_size = 8,
    .egl_green_size = 8,
    .egl_blue_size = 8,
    .egl_alpha_size = 0,
    .egl_depth_size = 24,
    .egl_stencil_size = 8,

    .color_pixel_size = 4,
    .depth_pixel_size = 4,

    .video_format = VIDEO_BUFFER_FORMAT_XRGB,
    .color_pformat = PIPE_FORMAT_R8G8B8_UNORM,
    .depth_pformat = PIPE_FORMAT_S8_UINT_Z24_UNORM
  },


};

static const unsigned int rsxegl_num_configs = sizeof(rsxegl_configs) / sizeof(struct rsxegl_config_t);

EGLAPI EGLBoolean EGLAPIENTRY
eglGetConfigs(EGLDisplay dpy,EGLConfig * configs,EGLint config_size,EGLint * num_configs)
{
  RSXEGL_CHECK_DISPLAY(dpy,EGL_FALSE);
  RSXEGL_CHECK_INITIALIZED(EGL_FALSE);

  if(num_configs == 0) {
    RSXEGL_ERROR(EGL_BAD_PARAMETER,EGL_FALSE);
  }

  if(configs == 0) {
    *num_configs = rsxegl_num_configs;
  }
  else {
    *num_configs = (config_size < rsxegl_num_configs ? config_size : rsxegl_num_configs);
    EGLConfig * pconfig = configs;
    for(unsigned int i = 0,n = *num_configs;i < n;++i) {
      *pconfig = rsxegl_configs + i;
      ++pconfig;
    }
  }

  RSXEGL_NOERROR(EGL_TRUE);
}

EGLAPI EGLBoolean EGLAPIENTRY
eglGetConfigAttrib(EGLDisplay dpy,EGLConfig _config,EGLint attribute,EGLint *value)
{
  assert(_config != 0);
  assert(value != 0);

  struct rsxegl_config_t * config = (struct rsxegl_config_t *)_config;

  switch(attribute) {
  case EGL_CONFIG_ID:
    *value = config -> egl_config_id;
    break;
  case EGL_BUFFER_SIZE:
    *value = config -> egl_buffer_size;
    break;
  case EGL_RED_SIZE:
    *value = config -> egl_red_size;
    break;
  case EGL_GREEN_SIZE:
    *value = config -> egl_green_size;
    break;
  case EGL_BLUE_SIZE:
    *value = config -> egl_blue_size;
    break;
  case EGL_ALPHA_SIZE:
    *value = config -> egl_alpha_size;
    break;
  case EGL_DEPTH_SIZE:
    *value = config -> egl_depth_size;
    break;
  case EGL_STENCIL_SIZE:
    *value = config -> egl_stencil_size;
    break;

  case EGL_LUMINANCE_SIZE:
    *value = 0;
    break;
  case EGL_ALPHA_MASK_SIZE:
    *value = 0;
    break;

  case EGL_SAMPLE_BUFFERS:
    *value = 0;
    break;
  case EGL_SAMPLES:
    *value = 0;
    break;

  case EGL_SURFACE_TYPE:
    *value = EGL_WINDOW_BIT;
    break;
  case EGL_RENDERABLE_TYPE:
  case EGL_CONFORMANT:
    *value = EGL_OPENGL_ES2_BIT;
    break;

  case EGL_BIND_TO_TEXTURE_RGB:
  case EGL_BIND_TO_TEXTURE_RGBA:
    *value = EGL_FALSE;
    break;

  case EGL_COLOR_BUFFER_TYPE:
    *value = EGL_RGB_BUFFER;
    break;

  case EGL_CONFIG_CAVEAT:
    *value = EGL_NONE;
    break;

  case EGL_LEVEL:
    *value = 0;
    break;

  case EGL_TRANSPARENT_TYPE:
    *value = EGL_NONE;
    break;
  case EGL_TRANSPARENT_RED_VALUE:
  case EGL_TRANSPARENT_GREEN_VALUE:
  case EGL_TRANSPARENT_BLUE_VALUE:
    *value = 0;
    break;

  case EGL_MAX_PBUFFER_WIDTH:
  case EGL_MAX_PBUFFER_HEIGHT:
  case EGL_MAX_PBUFFER_PIXELS:
    *value = 0;
    break;

  default:
    RSXEGL_ERROR(EGL_BAD_PARAMETER,EGL_FALSE);
    break;
  };

  RSXEGL_NOERROR(EGL_TRUE);
}

EGLAPI EGLBoolean EGLAPIENTRY
eglChooseConfig(EGLDisplay dpy,const EGLint * attrib_list,EGLConfig * configs,EGLint config_size,EGLint * num_config)
{
  if(attrib_list == 0 || *attrib_list == EGL_NONE) {
    RSXEGL_NOERROR(eglGetConfigs(dpy,configs,config_size,num_config));
  }

  struct rsxegl_config_t desired_config = {
    .egl_config_id = EGL_DONT_CARE,
    .egl_buffer_size = 0,
    .egl_red_size = 0,
    .egl_blue_size = 0,
    .egl_green_size = 0,
    .egl_alpha_size = 0,
    .egl_depth_size = 0,
    .egl_stencil_size = 0
  };

  const EGLint * pattrib = attrib_list;
  while(*pattrib != EGL_NONE) {
    switch(*pattrib++) {
    case EGL_CONFIG_ID:
      desired_config.egl_config_id = *pattrib++;
      break;

    case EGL_BUFFER_SIZE:
      desired_config.egl_buffer_size = *pattrib++;
      break;

    case EGL_RED_SIZE:
      desired_config.egl_red_size = *pattrib++;
      break;
    case EGL_GREEN_SIZE:
      desired_config.egl_green_size = *pattrib++;
      break;
    case EGL_BLUE_SIZE:
      desired_config.egl_blue_size = *pattrib++;
      break;
    case EGL_ALPHA_SIZE:
      desired_config.egl_alpha_size = *pattrib++;
      break;

    case EGL_DEPTH_SIZE:
      desired_config.egl_depth_size = *pattrib++;
      break;

    case EGL_STENCIL_SIZE:
      desired_config.egl_stencil_size = *pattrib++;
      break;

    default:
      pattrib++;
      break;
    };
  };

  EGLConfig * pconfig = configs;
  EGLint i = 0, n = (config_size < rsxegl_num_configs ? config_size : rsxegl_num_configs);

  EGLint j = 0;
  while(i < n && j < rsxegl_num_configs) {
    struct rsxegl_config_t * config = rsxegl_configs + j;

    if(config -> egl_red_size >= desired_config.egl_red_size &&
       config -> egl_green_size >= desired_config.egl_green_size &&
       config -> egl_blue_size >= desired_config.egl_blue_size &&
       config -> egl_alpha_size >= desired_config.egl_alpha_size &&
       config -> egl_buffer_size >= desired_config.egl_buffer_size &&
       config -> egl_depth_size >= desired_config.egl_depth_size &&
       config -> egl_stencil_size >= desired_config.egl_stencil_size &&
       (desired_config.egl_config_id == EGL_DONT_CARE || desired_config.egl_config_id == config -> egl_config_id)) {
      pconfig[i++] = config;
    }

    ++j;
  }

  *num_config = i;

  RSXEGL_NOERROR(EGL_TRUE);
}

static inline uint32_t
align64(const uint32_t n)
{
  const uint32_t tmp = n & 63;
  return tmp ? (n + 64 - tmp) : n;
}

EGLAPI EGLSurface EGLAPIENTRY
eglCreateWindowSurface(EGLDisplay _dpy,EGLConfig _config,EGLNativeWindowType win,const EGLint * attrib_list)
{
  RSXEGL_CHECK_DISPLAY(_dpy,EGL_NO_SURFACE);
  RSXEGL_CHECK_INITIALIZED(EGL_NO_SURFACE);

  struct rsxegl_display_t * dpy = (struct rsxegl_display_t *)_dpy;
  struct rsxegl_config_t * config = (struct rsxegl_config_t *)_config;
  
  // Configure the buffer format to xRGB
  videoConfiguration vconfig;
  memset(&vconfig, 0, sizeof(videoConfiguration));
  vconfig.resolution = dpy -> state.displayMode.resolution;
  vconfig.format = config -> video_format;
  vconfig.pitch = util_format_get_stride(config -> color_pformat,dpy -> resolution.width); //config -> color_pixel_size * dpy -> resolution.width;
  vconfig.aspect = VIDEO_ASPECT_AUTO;
  
  if(videoConfigure(0, &vconfig, NULL, 0)) {
    RSXEGL_ERROR(EGL_BAD_ALLOC,EGL_NO_SURFACE);
  }
  
  if(videoGetState(0, 0, &dpy -> state)) {
    RSXEGL_ERROR(EGL_BAD_ALLOC,EGL_NO_SURFACE);
  }

  //
  struct rsxegl_surface_t * surface = (struct rsxegl_surface_t *)malloc(sizeof(struct rsxegl_surface_t));

  surface -> config = config;

  surface -> double_buffered = EGL_BACK_BUFFER;
  surface -> buffer = 0;

  surface -> color_pformat = config -> color_pformat;
  surface -> depth_pformat = config -> depth_pformat;
  
  // Fill-in resolution, etc:
  surface -> width = dpy -> resolution.width;
  surface -> height = dpy -> resolution.height;
  surface -> x = 0;
  surface -> y = 0;
  
  // Allocate buffers:
  surface -> color_pitch = align64(util_format_get_stride(config -> color_pformat,surface -> width));
  surface -> depth_pitch = align64(util_format_get_stride(config -> depth_pformat,surface -> width));
  
  surface -> color_pixel_size = config -> color_pixel_size;
  surface -> depth_pixel_size = config -> depth_pixel_size;
  
  uint32_t
    color_buffer_size = util_format_get_2d_size(config -> color_pformat,surface -> color_pitch,surface -> height),
    depth_buffer_size = util_format_get_2d_size(config -> depth_pformat,surface -> depth_pitch,surface -> height);

  void * buffers[] = {
    rsxgl_rsx_memalign(64,color_buffer_size),
    rsxgl_rsx_memalign(64,color_buffer_size),
    rsxgl_rsx_memalign(64,depth_buffer_size)
  };
  
  if(buffers[0] == 0) {
    RSXEGL_ERROR(EGL_BAD_ALLOC,EGL_NO_SURFACE);
  }
  if(buffers[1] == 0) {
    RSXEGL_ERROR(EGL_BAD_ALLOC,EGL_NO_SURFACE);
  }
  if(buffers[2] == 0) {
    RSXEGL_ERROR(EGL_BAD_ALLOC,EGL_NO_SURFACE);
  }

  surface -> color_address[0] = buffers[0];
  surface -> color_address[1] = buffers[1];
  surface -> depth_address = buffers[2];
  
  uint32_t offsets[] = { 0,0,0 };
  
  if(gcmAddressToOffset(buffers[0],offsets + 0) != 0) {
    RSXEGL_ERROR(EGL_BAD_ALLOC,EGL_NO_SURFACE);
  }
  if(gcmAddressToOffset(buffers[1],offsets + 1) != 0) {
    RSXEGL_ERROR(EGL_BAD_ALLOC,EGL_NO_SURFACE);
  }
  if(gcmAddressToOffset(buffers[2],offsets + 2) != 0) {
    RSXEGL_ERROR(EGL_BAD_ALLOC,EGL_NO_SURFACE);
  }
  
  surface -> color_buffer[0].offset = offsets[0];
  surface -> color_buffer[0].location = 0;
  
  surface -> color_buffer[1].offset = offsets[1];
  surface -> color_buffer[1].location = 0;
  
  surface -> depth_buffer.offset = offsets[2];
  surface -> depth_buffer.location = 0;
  
  if(gcmSetDisplayBuffer(0, surface -> color_buffer[0].offset, surface -> color_pitch, surface -> width, surface -> height) != 0) {
    RSXEGL_ERROR(EGL_BAD_ALLOC,EGL_NO_SURFACE);
  }
  if(gcmSetDisplayBuffer(1, surface -> color_buffer[1].offset, surface -> color_pitch, surface -> width, surface -> height) != 0) {
    RSXEGL_ERROR(EGL_BAD_ALLOC,EGL_NO_SURFACE);
  }
  
  gcmResetFlipStatus();
  
  assert(rsx_gcm_context != 0);
  int r = gcmSetFlip(rsx_gcm_context,1);
  assert(r == 0);
  rsx_flush(rsx_gcm_context);
  gcmSetWaitFlip(rsx_gcm_context); // Prevent the RSX from continuing until the flip has finished.
  
  RSXEGL_NOERROR(surface);
}

#define RSXEGL_CHECK_SURFACE(SURFACE,RETURN)	\
  if((SURFACE) == 0) {				\
    RSXEGL_ERROR(EGL_BAD_SURFACE,(RETURN));	\
  }

EGLAPI EGLBoolean EGLAPIENTRY
eglDestroySurface(EGLDisplay dpy,EGLSurface surface)
{
  RSXEGL_CHECK_DISPLAY(dpy,EGL_FALSE);
  RSXEGL_CHECK_INITIALIZED(EGL_FALSE);

  if(surface != 0) {
    // TODO - delete the buffers:
    RSXEGL_NOERROR(EGL_TRUE);
  }
  else {
    RSXEGL_ERROR(EGL_BAD_SURFACE,EGL_FALSE);
  }
}

EGLAPI EGLBoolean EGLAPIENTRY
eglQuerySurface(EGLDisplay dpy, EGLSurface _surface,
		EGLint attribute, EGLint *value)
{
  RSXEGL_CHECK_DISPLAY(dpy,EGL_FALSE);
  RSXEGL_CHECK_INITIALIZED(EGL_FALSE);

  if(_surface != 0) {
    struct rsxegl_surface_t * surface = (struct rsxegl_surface_t *)_surface;

    // TODO - finish these:
    switch(attribute) {
    case EGL_CONFIG_ID:
      break;
    case EGL_LARGEST_PBUFFER:
      break;
    case EGL_WIDTH:
      *value = surface -> width;
      break;
    case EGL_HEIGHT:
      *value = surface -> height;
      break;
    case EGL_HORIZONTAL_RESOLUTION:
    case EGL_VERTICAL_RESOLUTION:
    case EGL_PIXEL_ASPECT_RATIO:
      *value = EGL_UNKNOWN;
      break;
    case EGL_RENDER_BUFFER:
      *value = surface -> double_buffered;
      break;
    case EGL_MULTISAMPLE_RESOLVE:
      *value = EGL_MULTISAMPLE_RESOLVE_DEFAULT;
      break;
    case EGL_TEXTURE_FORMAT:
    case EGL_TEXTURE_TARGET:
    case EGL_MIPMAP_TEXTURE:
    case EGL_MIPMAP_LEVEL:
      break;
    default:
      RSXEGL_ERROR(EGL_BAD_PARAMETER,EGL_FALSE);
    }

    RSXEGL_NOERROR(EGL_TRUE);
  }
  else {
    RSXEGL_ERROR(EGL_BAD_SURFACE,EGL_FALSE);
  }
}

static const EGLenum rsxegl_api = EGL_OPENGL_API;

EGLAPI EGLBoolean EGLAPIENTRY
eglBindAPI(EGLenum api)
{
  if(api != EGL_OPENGL_API) {
    RSXEGL_ERROR(EGL_BAD_PARAMETER,EGL_FALSE);
  }
  else {
    RSXEGL_NOERROR(EGL_TRUE);
  }
}

EGLAPI EGLBoolean EGLAPIENTRY
eglQueryAPI()
{
  RSXEGL_NOERROR(rsxegl_api);
}

struct rsxgl_object_context_t;

extern struct rsxegl_context_t * rsxgl_context_create(const struct rsxegl_config_t *,gcmContextData *,struct pipe_screen *,struct rsxgl_object_context_t *);
extern struct rsxgl_object_context_t * rsxgl_object_context_create();

static struct rsxegl_context_t * current_rsxgl_ctx = 0;

EGLAPI EGLContext EGLAPIENTRY
eglCreateContext(EGLDisplay dpy,EGLConfig config,EGLContext share_context,const EGLint * attrib_list)
{
  RSXEGL_CHECK_DISPLAY(dpy,EGL_NO_CONTEXT);
  RSXEGL_CHECK_INITIALIZED(EGL_NO_CONTEXT);

  struct rsxegl_context_t * ctx = 0;

  switch(rsxegl_api) {
  case EGL_OPENGL_API:
    ctx = rsxgl_context_create(config,rsx_gcm_context,rsx_screen,rsxgl_object_context_create());
    assert(ctx -> callback != 0);
    RSXEGL_NOERROR(ctx);
  default:
    RSXEGL_NOERROR(EGL_NO_CONTEXT);
  };
}

EGLAPI EGLBoolean EGLAPIENTRY
eglDestroyContext(EGLDisplay dpy,EGLContext _ctx)
{
  RSXEGL_CHECK_DISPLAY(dpy,EGL_FALSE);
  RSXEGL_CHECK_INITIALIZED(EGL_FALSE);

  if(rsxegl_api == EGL_OPENGL_API) {
    struct rsxegl_context_t * ctx = (struct rsxegl_context_t *)_ctx;

    if(ctx == 0) {
      RSXEGL_ERROR(EGL_BAD_CONTEXT,EGL_FALSE);
    }
    else {
      (*ctx -> callback)(ctx,RSXEGL_DESTROY_CONTEXT);
    }

    RSXEGL_NOERROR(EGL_TRUE);
  }
  else {
    RSXEGL_NOERROR(EGL_FALSE);
  }
}

EGLAPI EGLBoolean EGLAPIENTRY
eglMakeCurrent(EGLDisplay dpy,EGLSurface draw,EGLSurface read,EGLContext _ctx)
{
  RSXEGL_CHECK_DISPLAY(dpy,EGL_FALSE);
  RSXEGL_CHECK_INITIALIZED(EGL_FALSE);

  if(rsxegl_api == EGL_OPENGL_API) {
    if(current_rsxgl_ctx != 0) {
      current_rsxgl_ctx -> draw = 0;
      current_rsxgl_ctx -> read = 0;

      current_rsxgl_ctx = 0;
    }

    struct rsxegl_context_t * ctx = (struct rsxegl_context_t *)_ctx;

    if(ctx == 0) {
      RSXEGL_ERROR(EGL_BAD_CONTEXT,EGL_FALSE);
    }
    else {
      RSXEGL_CHECK_SURFACE(draw,EGL_FALSE);
      RSXEGL_CHECK_SURFACE(read,EGL_FALSE);
    }

    current_rsxgl_ctx = ctx;

    current_rsxgl_ctx -> draw = draw;
    current_rsxgl_ctx -> read = read;

    (*current_rsxgl_ctx -> callback)(current_rsxgl_ctx,RSXEGL_MAKE_CONTEXT_CURRENT);

    RSXEGL_NOERROR(EGL_TRUE);
  }
  else {
    RSXEGL_NOERROR(EGL_FALSE);
  }
}

EGLAPI EGLContext EGLAPIENTRY
eglGetCurrentContext()
{
  if(rsxegl_api == EGL_OPENGL_API) {
    if(current_rsxgl_ctx != 0) {
      RSXEGL_NOERROR(current_rsxgl_ctx);
    }
    else {
      RSXEGL_NOERROR(EGL_NO_CONTEXT);
    }
  }
  else {
    RSXEGL_NOERROR(EGL_NO_CONTEXT);
  }
}

EGLAPI EGLSurface EGLAPIENTRY
eglGetCurrentSurface(EGLint readdraw)
{
  if(rsxegl_api == EGL_OPENGL_API) {
    if(current_rsxgl_ctx != 0) {
      if(readdraw == EGL_READ) {
	return (EGLSurface)current_rsxgl_ctx -> read;
      }
      else if(readdraw == EGL_DRAW) {
	return (EGLSurface)current_rsxgl_ctx -> draw;
      }
      else {
	RSXEGL_ERROR(EGL_BAD_PARAMETER,EGL_NO_SURFACE);
      }
    }
    else {
      RSXEGL_ERROR(EGL_BAD_CONTEXT,EGL_NO_SURFACE);
    }
  }
  else {
    RSXEGL_NOERROR(EGL_NO_SURFACE);
  }
}

EGLAPI EGLSurface EGLAPIENTRY
eglGetCurrentDisplay()
{
  if(rsxegl_api == EGL_OPENGL_API) {
    if(current_rsxgl_ctx != 0) {
      if(current_rsxgl_ctx -> valid) {
	RSXEGL_NOERROR(&rsxegl_display);
      }
      else {
	RSXEGL_NOERROR(EGL_NO_DISPLAY);
      }
    }
    else {
      RSXEGL_ERROR(EGL_BAD_CONTEXT,EGL_NO_DISPLAY);
    }
  }
  else {
    RSXEGL_NOERROR(EGL_NO_DISPLAY);
  }
}

EGLAPI EGLBoolean EGLAPIENTRY
eglQueryContext(EGLDisplay dpy,EGLContext _ctx,EGLint attribute,EGLint * value)
{
  RSXEGL_CHECK_DISPLAY(dpy,EGL_FALSE);
  RSXEGL_CHECK_INITIALIZED(EGL_FALSE);

  if(rsxegl_api == EGL_OPENGL_API) {
    struct rsxegl_context_t * ctx = (struct rsxegl_context_t *)_ctx;
    //RSXEGL_CHECK_CONTEXT(ctx,EGL_FALSE);

    switch(attribute) {
    case EGL_CONFIG_ID:
      *value = ((const struct rsxegl_config_t *)ctx -> config) -> egl_config_id;
      break;
    case EGL_CONTEXT_CLIENT_TYPE:
      *value = ctx -> api;
      break;
    case EGL_CONTEXT_CLIENT_VERSION:
      // Not sure what this should be... 2?
      *value = 2;
      break;
    case EGL_RENDER_BUFFER:
      if(ctx -> draw == 0) {
	*value = EGL_NONE;
      }
      else {
	*value = ((const struct rsxegl_surface_t *)ctx -> draw) -> double_buffered;
      }
      break;
    default:
      RSXEGL_ERROR(EGL_BAD_ATTRIBUTE,EGL_FALSE);
      break;
    };
    
    RSXEGL_NOERROR(EGL_TRUE);
  }
  else {
    RSXEGL_NOERROR(EGL_FALSE);
  }
}

EGLAPI EGLBoolean EGLAPIENTRY
eglWaitClient()
{
  if(rsxegl_api == EGL_OPENGL_API) {
    return EGL_TRUE;
  }
  else {
    return EGL_FALSE;
  }
}

EGLAPI EGLBoolean EGLAPIENTRY
eglWaitNative(EGLint engine)
{
  if(engine == EGL_CORE_NATIVE_ENGINE) {
    return EGL_TRUE;
  }
  else {
    RSXEGL_ERROR(EGL_BAD_PARAMETER,EGL_FALSE);
  }
}

EGLAPI EGLBoolean eglSwapBuffers(EGLDisplay dpy,EGLSurface _surface)
{
  RSXEGL_CHECK_DISPLAY(dpy,EGL_FALSE);
  RSXEGL_CHECK_INITIALIZED(EGL_FALSE);

  struct rsxegl_surface_t * surface = (struct rsxegl_surface_t *)_surface;

  RSXEGL_CHECK_SURFACE(surface,EGL_FALSE);

  if(surface -> double_buffered == EGL_BACK_BUFFER) {
    assert(rsx_gcm_context != 0);
    int r = gcmSetFlip(rsx_gcm_context, surface -> buffer);
    assert(r == 0);

    // flush the command buffer:
    rsx_flush(rsx_gcm_context);
    gcmSetWaitFlip(rsx_gcm_context); // Prevent the RSX from continuing until the flip has finished.
    surface -> buffer = !surface -> buffer;
    (*current_rsxgl_ctx -> callback)(current_rsxgl_ctx,RSXEGL_POST_CPU_SWAP);

    // wait for the GPU to finish:
    uint32_t iter = 0;
    while(((rsxgl_init_parameters.max_swap_wait_iterations == 0) || (iter < rsxgl_init_parameters.max_swap_wait_iterations)) &&
	  gcmGetFlipStatus()) {
      usleep(rsxgl_init_parameters.swap_wait_interval);
      ++iter;
    }
    (*current_rsxgl_ctx -> callback)(current_rsxgl_ctx,RSXEGL_POST_GPU_SWAP);

    RSXEGL_NOERROR(((rsxgl_init_parameters.max_swap_wait_iterations == 0) || (iter < rsxgl_init_parameters.max_swap_wait_iterations)) ? EGL_TRUE : EGL_FALSE);
  }
  else {
    RSXEGL_NOERROR(EGL_FALSE);
  }
}
