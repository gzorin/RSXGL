/*
 * assview - render models loaded with the assimp asset importer.
 */

#define GL3_PROTOTYPES
#include <GL3/gl3.h>

#include "rsxgltest.h"
#include "math3d.h"
#include "sine_wave.h"

#include <stddef.h>
#include "assview_vpo.h"
#include "assview_fpo.h"

#include <io/pad.h>

#include <math.h>
#include <Eigen/Geometry>

#include <assimp.h>
#include <aiScene.h>

#include "cornell_box_obj_bin.h"
#include "cornell_box_mtl_bin.h"

#include "crab_obj_bin.h"

const char * rsxgltest_name = "assview";

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

struct sine_wave_t xyz_waves[3] = {
  { 0.5f,
    0.5f,
    1.0f / 4.0f
  },
  {
    0.5f,
    0.5f,
    1.5f / 4.0f
  },
  {
    0.5f,
    0.5f,
    2.5f / 4.0f
  }
};

GLuint shaders[2] = { 0,0 };

GLuint program = 0;

GLint ProjMatrix_location = -1, TransMatrix_location = -1,
  vertex_location = -1, normal_location = -1, uv_location = -1;

#define DTOR(X) ((X)*0.01745329f)
#define RTOD(d) ((d)*57.295788f)

Eigen::Projective3f ProjMatrix(perspective(DTOR(54.3),1920.0 / 1080.0,0.1,1000.0));

Eigen::Affine3f ViewMatrixInv = 
  Eigen::Affine3f(Eigen::Affine3f::Identity() * 
		  (
		   Eigen::Translation3f(1.779,2.221,4.034) *
		   (
		    Eigen::AngleAxisf(DTOR(0),Eigen::Vector3f::UnitZ()) *
		    Eigen::AngleAxisf(DTOR(23.8),Eigen::Vector3f::UnitY()) *
		    Eigen::AngleAxisf(DTOR(-26.738),Eigen::Vector3f::UnitX())
		    )
		   )
		  ).inverse();

struct asset_model_spec {
  uint8_t const * data;
  const size_t size;

  float scale;
};

struct asset_model {
  GLuint vbo, vao;
  GLuint ntris;
  float scale;

  asset_model()
    : vbo(0), vao(0), ntris(0), scale(1.0f) {
  }
};

const size_t nmodels = 1, imodel = 0;

asset_model_spec model_specs[nmodels] = {
  { cornell_box_obj_bin, cornell_box_obj_bin_size, 1.0 / 200.0 },
  
  // The OBJ reader has problems parsing this, and hangs the program:
  // { crab_obj_bin, crab_obj_bin_size, 0.5 }
};

asset_model models[nmodels];

asset_model
asset_to_gl(asset_model_spec const & model_spec)
{
  asset_model result;
  
  const aiScene * scene = aiImportFileFromMemory((char const *)model_spec.data,model_spec.size,0,"OBJ");
  if(scene != 0) {
    tcp_printf("read scene; it has %u meshes\n",(uint32_t)scene -> mNumMeshes);
  }
  else {
    tcp_printf("Failed to read scene");
  }

  return result;
}

#if 0
// Turn a obj_scene_data into an OpenGL vertex array object. This function performs no checking on the component indices
//  to determine if they're valid.
asset_model
asset_to_gl(asset_model_spec const & model_spec)
{
  asset_model result;

  obj_scene_data asset;
  int r = parse_inline_obj_scene(&asset,(char const *)model_spec.data,model_spec.size);
  if(!r) return result;

  // count number of triangles:
  uint32_t ntris = 0;
  
  for(size_t i = 0,n = asset.face_count;i < n;++i) {
    obj_face const * face = asset.face_list[i];

    ntris += (face -> vertex_count == 4) ? 2 : (face -> vertex_count == 3) ? 1 : 0;
  }

  tcp_printf("model has %u triangles\n",ntris);

  // size of a single vertex - vec3, vec3, vec2
  struct vertex {
    GLfloat position[3], normal[3], uv[2];

    vertex() {
      position[0] = 0.0;
      position[1] = 0.0;
      position[2] = 0.0;
      normal[0] = 0.0;
      normal[1] = 0.0;
      normal[2] = 0.0;
      uv[0] = 0.0;
      uv[1] = 0.0;
    }

    static
    vertex create(obj_scene_data const& object,
		  obj_face const * face,const size_t j) {
      vertex result;

      // vertex:
      const int vertex_index = face -> vertex_index[j];
      if(vertex_index >= 0) {
	obj_vector const * position = object.vertex_list[vertex_index];
	result.position[0] = position -> e[0];
	result.position[1] = position -> e[1];
	result.position[2] = position -> e[2];
      }
      
      // normal:
      const int normal_index = face -> normal_index[j];
      if(normal_index >= 0) {
	obj_vector const * normal = object.vertex_normal_list[normal_index];
	result.normal[0] = normal -> e[0];
	result.normal[1] = normal -> e[1];
	result.normal[2] = normal -> e[2];
      }
      
      // uv:
      const int texture_index = face -> texture_index[j];
      if(texture_index >= 0) {
	obj_vector const * uv = object.vertex_texture_list[texture_index];
	result.uv[0] = uv -> e[0];
	result.uv[1] = uv -> e[1];
      }

      return result;
    }
  };

  glGenBuffers(1,&result.vbo);

  glBindBuffer(GL_ARRAY_BUFFER,result.vbo);

  tcp_printf(" building buffer object: want %u bytes\n",(uint32_t)(sizeof(vertex) * ntris * 3));
  glBufferData(GL_ARRAY_BUFFER,sizeof(vertex) * ntris * 3,0,GL_STATIC_DRAW);
  
  vertex * _vbo = (vertex *)glMapBuffer(GL_ARRAY_BUFFER,GL_WRITE_ONLY);
  vertex * pvbo = _vbo;

  obj_face ** pface = asset.face_list;
  for(size_t i = 0,n = asset.face_count;i < n;++i,++pface) {
    obj_face const * face = *pface;

    if(face -> vertex_count == 3) {
      *pvbo++ = vertex::create(asset,face,0);
      *pvbo++ = vertex::create(asset,face,1);
      *pvbo++ = vertex::create(asset,face,2);
    }
    else if(face -> vertex_count == 4) {
      *pvbo++ = vertex::create(asset,face,0);
      *pvbo++ = vertex::create(asset,face,1);
      *pvbo++ = vertex::create(asset,face,2);

      *pvbo++ = vertex::create(asset,face,2);
      *pvbo++ = vertex::create(asset,face,3);
      *pvbo++ = vertex::create(asset,face,0);
    }
  }

  glUnmapBuffer(GL_ARRAY_BUFFER);

  glGenVertexArrays(1,&result.vao);

  glBindVertexArray(result.vao);
  glEnableVertexAttribArray(vertex_location);
  if(normal_location > 0) glEnableVertexAttribArray(normal_location);
  if(uv_location > 0) glEnableVertexAttribArray(uv_location);

  glVertexAttribPointer(vertex_location,3,GL_FLOAT,GL_FALSE,sizeof(vertex),0);
  if(normal_location > 0) glVertexAttribPointer(normal_location,3,GL_FLOAT,GL_FALSE,sizeof(vertex),(const GLfloat *)0 + 3);
  if(normal_location > 0) glVertexAttribPointer(uv_location,2,GL_FLOAT,GL_FALSE,sizeof(vertex),(const GLfloat *)0 + 6);

  glBindBuffer(GL_ARRAY_BUFFER,0);
  glBindVertexArray(0);

  result.ntris = ntris;

  result.scale = model_spec.scale;

  tcp_printf(" finished processing model");

  return result;
}
#endif

extern "C"
void
rsxgltest_pad(const padData * paddata)
{
  if(paddata -> BTN_UP) {
    tcp_printf("up\n");
  }
  else if(paddata -> BTN_DOWN) {
    tcp_printf("down\n");
  }
}

extern "C"
void
rsxgltest_init(int argc,const char ** argv)
{
  tcp_printf("%s\n",__PRETTY_FUNCTION__);

  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LEQUAL);

  // Set up us the program:
  shaders[0] = glCreateShader(GL_VERTEX_SHADER);
  shaders[1] = glCreateShader(GL_FRAGMENT_SHADER);

  program = glCreateProgram();

  glAttachShader(program,shaders[0]);
  glAttachShader(program,shaders[1]);

  // Supply shader binaries:
  glShaderBinary(1,shaders,0,assview_vpo,assview_vpo_size);
  glShaderBinary(1,shaders + 1,0,assview_fpo,assview_fpo_size);

  // Link the program for real:
  glLinkProgram(program);
  glValidateProgram(program);
  
  summarize_program("assview",program);

  vertex_location = glGetAttribLocation(program,"vertex");
  normal_location = glGetAttribLocation(program,"normal");
  uv_location = glGetAttribLocation(program,"uv");

  ProjMatrix_location = glGetUniformLocation(program,"ProjMatrix");
  TransMatrix_location = glGetUniformLocation(program,"TransMatrix");

  glUseProgram(program);

  glUniformMatrix4fv(ProjMatrix_location,1,GL_FALSE,ProjMatrix.data());

  // Load the models:
  for(size_t i = 0;i < nmodels;++i) {
    models[i] = asset_to_gl(model_specs[i]);
  }

  // Draw wireframe models for now:
  glPolygonMode(GL_FRONT_AND_BACK,GL_LINE);
  glLineWidth(3.0);
}

extern "C"
int
rsxgltest_draw()
{
  tcp_printf("%s\n",__PRETTY_FUNCTION__);

  float rgb[3] = {
    compute_sine_wave(rgb_waves,rsxgltest_elapsed_time),
    compute_sine_wave(rgb_waves + 1,rsxgltest_elapsed_time),
    compute_sine_wave(rgb_waves + 2,rsxgltest_elapsed_time)
  };

  glClearColor(rgb[0],rgb[1],rgb[2],1.0);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  float xyz[3] = {
    compute_sine_wave(xyz_waves,rsxgltest_elapsed_time),
    compute_sine_wave(xyz_waves + 1,rsxgltest_elapsed_time),
    compute_sine_wave(xyz_waves + 2,rsxgltest_elapsed_time)
  };

  Eigen::Affine3f rotmat = 
    Eigen::Affine3f::Identity() * 
    Eigen::AngleAxisf(DTOR(xyz[2]) * 360.0f,Eigen::Vector3f::UnitZ()) *
    Eigen::AngleAxisf(DTOR(xyz[1]) * 360.0f,Eigen::Vector3f::UnitY()) *
    Eigen::AngleAxisf(DTOR(xyz[0]) * 360.0f,Eigen::Vector3f::UnitX());

  glBindVertexArray(models[imodel].vao);

  if(models[imodel].ntris > 0 &&
     models[imodel].vbo != 0 &&
     models[imodel].vao != 0) {
    Eigen::Affine3f modelview = ViewMatrixInv * (Eigen::Affine3f::Identity() * rotmat * Eigen::UniformScaling< float >(models[imodel].scale));
    glUniformMatrix4fv(TransMatrix_location,1,GL_FALSE,modelview.data());

    glDrawArrays(GL_TRIANGLES,0,models[imodel].ntris * 3);
    glFlush();
  }

  glBindVertexArray(0);

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
