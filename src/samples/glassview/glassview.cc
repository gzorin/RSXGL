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

#include "teapot_obj.h"
#include "teddy_obj.h"
#include "crab_obj.h"

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

const size_t nmodels = 3;
size_t imodel = 0;

asset_model_spec model_specs[nmodels] = {
  { teapot_obj, teapot_obj_size, 1.0 },
  { teddy_obj, teddy_obj_size, 1.0 },
  { crab_obj, crab_obj_size, 1.0 }
};

asset_model models[nmodels];

struct triangulated_aiMesh_to_vertex_array {
  GLvoid * positions, * normals, * uvs, * colors;
  GLsizei position_size, normal_size, uv_size, color_size;
  GLenum position_type, normal_type, uv_type, color_type;
  GLsizei position_stride, normal_stride, uv_stride, color_stride;

  struct output {
    GLsizei nPositions, nNormals, nUVs, nColors;
    GLvoid * positions, * normals, * uvs, * colors;

    output()
      : nPositions(0), nNormals(0), nUVs(0), nColors(0),
	positions(0), normals(0), uvs(0), colors(0) {
    }
  };

  triangulated_aiMesh_to_vertex_array()
    : positions(0), normals(0), uvs(0), colors(0), position_size(0), normal_size(0), uv_size(0), color_size(0), position_type(0), normal_type(0), uv_type(0), color_type(0), position_stride(0), normal_stride(0), uv_stride(0), color_stride(0) {
  }

  unsigned int num_triangles(aiMesh const * mesh) const {
    if(!mesh -> HasFaces()) {
      return 0;
    }
    else {
      unsigned int nLessThanThree = 0, nThree = 0, nQuad = 0, nNSided = 0;
      unsigned int nTriangles = 0;
      aiFace const * face = mesh -> mFaces;
      for(unsigned int i = 0,n = mesh -> mNumFaces;i < n;++i,++face) {
	switch(face -> mNumIndices) {
	case 0:
	case 1:
	case 2:
	  ++nLessThanThree;
	  break;
	case 3:
	  ++nThree;
	  ++nTriangles;
	  break;
	case 4:
	  ++nQuad;
	  nTriangles += 2;
	  break;
	default:
	  ++nNSided;
	  break;
	}
      }

      return nTriangles;
    }
  }

  output fill(aiMesh const * mesh,const bool bWantNormals,const bool bWantUVs,const bool bWantColors) {
    output result;

    if(!mesh -> HasFaces() || !mesh -> HasPositions()) return result;

    // Not doing any type conversion now:
    if(position_type != GL_FLOAT) return result;
    if(positions == 0) return result;
    if(position_size == 0) return result;

    const bool bNormals = bWantNormals && (normal_type == GL_FLOAT && normals != 0 && normal_size > 0 && mesh -> HasNormals()),
      bUVs = bWantUVs && (uv_type == GL_FLOAT && uvs != 0 && uv_size > 0 && mesh -> HasTextureCoords(0)),
      bColors = bWantColors && (color_type == GL_FLOAT && colors != 0 && color_size > 0 && mesh -> HasVertexColors(0));

    uint8_t * position = (uint8_t *)positions;
    uint8_t * normal = (uint8_t *)normals;
    uint8_t * uv = (uint8_t *)uvs;
    uint8_t * color = (uint8_t *)colors;

    struct copy_vector {
      void operator()(uint8_t * _array,aiVector3D const & v,const GLsizei size,const float w) const {
	GLfloat * array = (GLfloat *)_array;

	if(size > 0) array[0] = v.x;
	if(size > 1) array[1] = v.y;
	if(size > 2) array[2] = v.z;
	if(size > 3) array[3] = w;
      }

      void operator()(uint8_t * _array,aiColor4D const & c,const GLsizei size) const {
	GLfloat * array = (GLfloat *)_array;

	if(size > 0) array[0] = c.r;
	if(size > 1) array[1] = c.g;
	if(size > 2) array[2] = c.b;
	if(size > 3) array[3] = c.a;
      }
    };

    aiFace const * face = mesh -> mFaces;
    copy_vector op;

    for(unsigned int i = 0,n = mesh -> mNumFaces;i < n;++i,++face) {
      unsigned int const * indices = face -> mIndices;
      for(unsigned int j = 0,m = face -> mNumIndices;j < m;++j,++indices) {

	const unsigned int index = *indices;
	{
	  op(position,mesh -> mVertices[index],position_size,1.0f);
	  position += position_stride;
	  ++result.nPositions;
	}

	if(bNormals) {
	  op(normal,mesh -> mNormals[index],normal_size,0.0f);
	  normal += normal_stride;
	  ++result.nNormals;
	}

	if(bUVs) {
	  op(uv,mesh -> mTextureCoords[0][index],(uv_size < mesh -> mNumUVComponents[0]) ? uv_size : mesh -> mNumUVComponents[0],0.0f);
	  uv += uv_stride;
	  ++result.nUVs;
	}

	if(bColors) {
	  op(color,mesh -> mColors[0][index],color_size);
	  color += color_stride;
	  ++result.nColors;
	}
      }
    }

    result.positions = position;
    result.normals = normal;
    result.uvs = uv;
    result.colors = color;

    return result;
  }

};

asset_model
asset_to_gl(asset_model_spec const & model_spec)
{
  asset_model result;
 
  Assimp::Importer importer;
  const aiScene * scene = importer.ReadFileFromMemory((char const *)model_spec.data,model_spec.size,aiProcess_PreTransformVertices | aiProcess_Triangulate,"obj");

  if(scene != 0 && scene -> mNumMeshes > 0) {
    aiMesh const * mesh = scene -> mMeshes[0];
    if(mesh != 0 && mesh -> mNumFaces > 0 && mesh -> mNumVertices > 0) {
      result.ntris = mesh -> mNumFaces;
      const GLsizei nverts = result.ntris * 3;

      triangulated_aiMesh_to_vertex_array op;

      op.position_size = 3;
      op.position_type = GL_FLOAT;

      op.normal_size = (mesh -> HasNormals()) ? 3 : 0;
      op.normal_type = GL_FLOAT;

      op.uv_size = (mesh -> HasTextureCoords(0)) ? 2 : 0;
      op.uv_type = GL_FLOAT;

      op.color_size = (mesh -> HasVertexColors(0)) ? 4 : 0;
      op.color_type = GL_FLOAT;

      const GLsizei position_offset = 0;
      const GLsizei normal_offset = position_offset + op.position_size * sizeof(GLfloat);
      const GLsizei uv_offset = normal_offset + op.normal_size * sizeof(GLfloat);
      const GLsizei color_offset = uv_offset + op.uv_size * sizeof(GLfloat);
      const GLuint vertex_size = color_offset + op.color_size * sizeof(GLfloat);

      op.position_stride = vertex_size;
      op.normal_stride = vertex_size;
      op.uv_stride = vertex_size;
      op.color_stride = vertex_size;

      const GLuint ntris = op.num_triangles(mesh);

      glGenBuffers(1,&result.vbo);
      glBindBuffer(GL_ARRAY_BUFFER,result.vbo);

      glBufferData(GL_ARRAY_BUFFER,vertex_size * nverts,0,GL_STATIC_DRAW);
      uint8_t * buffer = (uint8_t *)glMapBuffer(GL_ARRAY_BUFFER,GL_WRITE_ONLY);

      op.positions = buffer + position_offset;
      op.normals = buffer + normal_offset;
      op.uvs = buffer + uv_offset;
      op.colors = buffer + color_offset;

      const triangulated_aiMesh_to_vertex_array::output op_out = op.fill(mesh,true,false,false);

      glUnmapBuffer(GL_ARRAY_BUFFER);

      //
      glGenBuffers(1,&result.ibo);
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,result.ibo);
      glBufferData(GL_ELEMENT_ARRAY_BUFFER,sizeof(uint32_t) * nverts,0,GL_STATIC_DRAW);

      uint32_t * indices = (uint32_t *)glMapBuffer(GL_ELEMENT_ARRAY_BUFFER,GL_WRITE_ONLY);
      
      for(uint32_t i = 0;i < nverts;++i,++indices) {
	*indices = i;
      }

      glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,0);


      //
      glGenVertexArrays(1,&result.vao);
      glBindVertexArray(result.vao);
      glEnableVertexAttribArray(vertex_location);
      glVertexAttribPointer(vertex_location,3,GL_FLOAT,GL_FALSE,op.position_stride,(const GLfloat *)position_offset);

      if(normal_location > 0 && op.normal_size > 0) {
	glEnableVertexAttribArray(normal_location);
	glVertexAttribPointer(normal_location,3,GL_FLOAT,GL_FALSE,op.normal_stride,(const GLfloat *)normal_offset);
      }
      if(uv_location > 0 && op.uv_size > 0) {
	glEnableVertexAttribArray(uv_location);
	glVertexAttribPointer(uv_location,2,GL_FLOAT,GL_FALSE,op.uv_stride,(const GLfloat *)uv_offset);
      }
      
      glBindBuffer(GL_ARRAY_BUFFER,0);
      glBindVertexArray(0);
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
     models[imodel].ibo != 0 &&
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
