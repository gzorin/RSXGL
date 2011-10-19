/*
 * glassview - render models loaded with the assimp asset importer.
 */

#define GL3_PROTOTYPES
#include <GL3/gl3.h>

#include "rsxgltest.h"
#include "math3d.h"
#include "sine_wave.h"

#include <stddef.h>
#include "glassview_vpo.h"
#include "glassview_fpo.h"

#include "diffuse_vpo.h"
#include "diffuse_fpo.h"

#include <io/pad.h>

#include <math.h>
#include <Eigen/Geometry>

#include <assimp.hpp>
#include <aiScene.h>
#include <aiPostProcess.h>

#include "glassimp.h"

#include "teapot_obj.h"
#include "teddy_obj.h"
#include "crab_obj.h"
#include "yamato_obj.h"

const char * rsxgltest_name = "glassview";

#if defined(assert)
#undef assert
#endif

#define assert(__e) ((__e) ? (void)0 : tcp_printf("assertion \"%s\" failed: file \"%s\", line %d%s%s\n", \
						  (__e), __FILE__, __LINE__, \
						  __PRETTY_FUNCTION__ ? ", function: " : "", __PRETTY_FUNCTION__ ? __PRETTY_FUNCTION__ : ""))

struct sine_wave_t rgb_waves[3] = {
  { 0.5f,
    0.5f,
    1.0f
  },
  {
    0.5f,
    0.5f,
    1.5f
  },
  {
    0.5f,
    0.5f,
    2.5f
  }
};

GLuint shaders[2] = { 0,0 };

GLuint program = 0;

GLint ProjMatrix_location = -1, TransMatrix_location = -1, NormalMatrix_location = -1,
  vertex_location = -1, normal_location = -1, uv_location = -1,
  light_location = -1;

const GLfloat model_pitch_rate = 10.0, model_yaw_rate = 10.0, model_scale_rate = 0.05, model_scale_min = 0.1,model_scale_max = 5.0;
GLfloat model_pitch = 0, model_yaw = 0.0, model_scale = 1.0;

const GLfloat light[4] = { 10.0f, 10.0f, 10.0f, 1.0f };

// degrees per second:
#define DTOR(X) ((X)*0.01745329f)
#define RTOD(d) ((d)*57.295788f)

Eigen::Projective3f ProjMatrix(perspective(DTOR(54.3 / 2.0),1920.0 / 1080.0,0.1,1000.0));

Transform3f ViewTransform;

struct asset_model_spec {
  uint8_t const * data;
  const size_t size;

  float scale;
};

struct asset_model {
  GLuint vbo, ibo, vao;
  GLuint ntris;
  float scale;

  asset_model()
    : vbo(0), ibo(0), vao(0), ntris(0), scale(1.0f) {
  }
};

const size_t nmodels = 4;
size_t imodel = 0;

asset_model_spec model_specs[nmodels] = {
  { teapot_obj, teapot_obj_size, 1.0 },
  { teddy_obj, teddy_obj_size, 1.0 },
  { crab_obj, crab_obj_size, 1.0 },
  { yamato_obj, yamato_obj_size, 1.0 }
};

asset_model models[nmodels];

asset_model
asset_to_gl(asset_model_spec const & model_spec)
{
  asset_model result;
 
  Assimp::Importer importer;
  const aiScene * scene = importer.ReadFileFromMemory((char const *)model_spec.data,model_spec.size,aiProcess_PreTransformVertices | aiProcess_Triangulate,"obj");

  if(scene != 0 && scene -> mNumMeshes > 0) {
    aiMesh const * mesh = scene -> mMeshes[0];
    if(mesh != 0 && mesh -> mNumFaces > 0 && mesh -> mNumVertices > 0) {

      const GLuint nTriangles = glassimpTrianglesCount(mesh);
      GLenum attribs[] = {
	GLASSIMP_VERTEX_ARRAY, GLASSIMP_NORMAL_ARRAY, GLASSIMP_TEXTURE_COORD_ARRAY
      };
      GLuint indices[] = {
	0, 0, 0
      };
      GLint sizes[] = {
	3, 3, 2
      };
      GLenum types[] = {
	GL_FLOAT, GL_FLOAT, GL_FLOAT
      };
      GLint sizes_out[3];
      GLsizei strides_out[3];
      GLvoid * pointers_out[3];
      GLuint stride = glassimpTrianglesFormat(mesh,GL_TRUE,3,attribs,indices,sizes,types,0,sizes_out,strides_out,pointers_out);

      glGenBuffers(1,&result.vbo);
      glBindBuffer(GL_ARRAY_BUFFER,result.vbo);
      glBufferData(GL_ARRAY_BUFFER,stride * nTriangles * 3,0,GL_STATIC_DRAW);
      void * pvbo = glMapBuffer(GL_ARRAY_BUFFER,GL_WRITE_ONLY);
      glassimpTrianglesLoadArrays(mesh,GL_TRUE,3,attribs,indices,sizes,types,pvbo);
      glUnmapBuffer(GL_ARRAY_BUFFER);

      GLint locations[3] = {
	vertex_location, normal_location, uv_location
      };

      glGenVertexArrays(1,&result.vao);
      glBindVertexArray(result.vao);
      glassimpTrianglesSetPointers(mesh,locations,GL_TRUE,3,attribs,indices,sizes,types,0);
      
      glBindBuffer(GL_ARRAY_BUFFER,0);
      glBindVertexArray(0);

      result.ntris = nTriangles;

      return result;
    }
  }

  tcp_printf("Failed to read scene");
  return result;
}

extern "C"
void
rsxgltest_pad(unsigned int,const padData * paddata)
{
  static unsigned int square = ~0, circle = ~0;

  if(paddata -> BTN_SQUARE != square) {
    square = paddata -> BTN_SQUARE;
    
    if(square) {
      imodel = (imodel - 1) % nmodels;
    }
  }
  else if(paddata -> BTN_CIRCLE != circle) {
    circle = paddata -> BTN_CIRCLE;
    
    if(circle) {
      imodel = (imodel + 1) % nmodels;
    }
  }

#if 0
  tcp_printf("%u %u %u %u\n",
	     (unsigned int)paddata -> ANA_L_H,
	     (unsigned int)paddata -> ANA_L_V,
	     (unsigned int)paddata -> ANA_R_H,
	     (unsigned int)paddata -> ANA_R_V);
#endif

  // abs of values below this get ignored:
  const float threshold = 0.05;

  const float
    left_stick_h = ((float)paddata -> ANA_L_H - 127.0f) / 127.0f,
    left_stick_v = ((float)paddata -> ANA_L_V - 127.0f) / 127.0f,
    right_stick_h = ((float)paddata -> ANA_R_H - 127.0f) / 127.0f,
    right_stick_v = ((float)paddata -> ANA_R_V - 127.0f) / 127.0f;

  if(fabs(left_stick_h) > threshold) {
    model_yaw += left_stick_h * model_yaw_rate;
  }

  if(fabs(left_stick_v) > threshold) {
    model_pitch += left_stick_v * model_pitch_rate;
  }

  if(fabs(right_stick_v) > threshold) {
    model_scale += -right_stick_v * model_scale_rate;
    if(model_scale < model_scale_min) {
      model_scale = model_scale_min;
    }
    else if(model_scale > model_scale_max) {
      model_scale = model_scale_max;
    }
  }
}

extern "C"
void
rsxgltest_init(int argc,const char ** argv)
{
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LEQUAL);

  // Set up us the program:
  shaders[0] = glCreateShader(GL_VERTEX_SHADER);
  shaders[1] = glCreateShader(GL_FRAGMENT_SHADER);

  program = glCreateProgram();

  glAttachShader(program,shaders[0]);
  glAttachShader(program,shaders[1]);

  // Supply shader binaries:
  glShaderBinary(1,shaders,0,diffuse_vpo,diffuse_vpo_size);
  glShaderBinary(1,shaders + 1,0,diffuse_fpo,diffuse_fpo_size);

  // Link the program for real:
  glLinkProgram(program);
  glValidateProgram(program);
  
  summarize_program("glassview",program);

  vertex_location = glGetAttribLocation(program,"vertex");
  normal_location = glGetAttribLocation(program,"normal");
  uv_location = glGetAttribLocation(program,"uv");

  ProjMatrix_location = glGetUniformLocation(program,"ProjMatrix");
  TransMatrix_location = glGetUniformLocation(program,"TransMatrix");
  NormalMatrix_location = glGetUniformLocation(program,"NormalMatrix");

  light_location = glGetUniformLocation(program,"light");

  glUseProgram(program);

  glUniformMatrix4fv(ProjMatrix_location,1,GL_FALSE,ProjMatrix.data());
  glUniform4fv(light_location,1,light);

  // Load the models:
  for(size_t i = 0;i < nmodels;++i) {
    models[i] = asset_to_gl(model_specs[i]);
  }

#if 0
  // Draw wireframe models for now:
  glPolygonMode(GL_FRONT_AND_BACK,GL_LINE);
  glLineWidth(1.0);
#endif

  // The hell with that, draw a diffuse-shaded object, dammit:
  glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);

  // Viewing matrix:
  ViewTransform = 
    Transform3f::Identity() *
    Eigen::Translation3f(7.806,7.419,11.525) *
    (
     Eigen::AngleAxisf(DTOR(0),Eigen::Vector3f::UnitZ()) *
     Eigen::AngleAxisf(DTOR(31.8),Eigen::Vector3f::UnitY()) *
     Eigen::AngleAxisf(DTOR(-22.538),Eigen::Vector3f::UnitX())
     );
  
}

// Courtesy of Matt Hall:
static
Eigen::Quaternionf quaternion_from_euler(const float pitch,const float yaw,const float roll)
{
  float ys=sinf(yaw/2.0f), yc=cosf(yaw/2.0f);
  float ps=sinf(pitch/2.0f), pc=cosf(pitch/2.0f);
  float rs=sinf(roll/2.0f), rc=cosf(roll/2.0f);

  return Eigen::Quaternionf(rs*ps*ys+rc*pc*yc,
			    rc*ps*yc+rs*pc*ys,
			    rc*pc*ys-rs*ps*yc,
			    rs*pc*yc-rc*ps*ys);
}

extern "C"
int
rsxgltest_draw()
{
  Transform3f ViewTransformInv = ViewTransform.inverse();

  float rgb[3] = {
    compute_sine_wave(rgb_waves,rsxgltest_elapsed_time),
    compute_sine_wave(rgb_waves + 1,rsxgltest_elapsed_time),
    compute_sine_wave(rgb_waves + 2,rsxgltest_elapsed_time)
  };

  glClearColor(rgb[0] * 0.1,rgb[1] * 0.1,rgb[2] * 0.1,1.0);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  if(models[imodel].ntris > 0 &&
     models[imodel].vbo != 0 &&
     /*models[imodel].ibo != 0 &&*/
     models[imodel].vao != 0) {
    glBindVertexArray(models[imodel].vao);

    /*
      ( Eigen::AngleAxisf(DTOR(model_yaw),Eigen::Vector3f::UnitY()) *
      Eigen::AngleAxisf(DTOR(model_pitch),Eigen::Vector3f::UnitX()) )
    */

    Transform3f transform =
      Transform3f::Identity() *
      quaternion_from_euler(DTOR(model_pitch),DTOR(model_yaw),0.0f) *
      Eigen::UniformScaling< float >(models[imodel].scale * model_scale);

    Transform3f modelview = ViewTransformInv * transform;

    Eigen::Affine3f normal = Eigen::Affine3f::Identity();
    normal.linear() = modelview.linear().inverse().transpose();

    glUniformMatrix4fv(TransMatrix_location,1,GL_FALSE,modelview.data());
    glUniformMatrix4fv(NormalMatrix_location,1,GL_FALSE,normal.data());

    glDrawArrays(GL_TRIANGLES,0,models[imodel].ntris * 3);

#if 0
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,models[imodel].ibo);
    glDrawElements(GL_TRIANGLES,models[imodel].ntris * 3,GL_UNSIGNED_INT,0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,0);
#endif

    glFlush();

    glBindVertexArray(0);
  }

  return 1;
}

extern "C"
void
rsxgltest_exit()
{
  glDeleteShader(shaders[0]);
  glDeleteShader(shaders[1]);
  glDeleteProgram(program);
}
