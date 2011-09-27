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

#include <assimp.hpp>
#include <aiScene.h>
#include <aiPostProcess.h>

#include "teapot_obj.h"

const char * rsxgltest_name = "assview";

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
  GLuint vbo, ibo, vao;
  GLuint ntris;
  float scale;

  asset_model()
    : vbo(0), ibo(0), vao(0), ntris(0), scale(1.0f) {
  }
};

const size_t nmodels = 1, imodel = 0;

asset_model_spec model_specs[nmodels] = {
  { teapot_obj, teapot_obj_size, 1.0 / 200.0 },
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

      tcp_printf("offsets: %u %u %u %u vertex_size: %u\n",
		 position_offset,normal_offset,uv_offset,color_offset,vertex_size);
      
      op.position_stride = vertex_size;
      op.normal_stride = vertex_size;
      op.uv_stride = vertex_size;
      op.color_stride = vertex_size;

      const GLuint ntris = op.num_triangles(mesh);

      glGenBuffers(1,&result.vbo);
      glBindBuffer(GL_ARRAY_BUFFER,result.vbo);

      tcp_printf("building buffer object: %i triangles, %i vertices, %u bytes\n",result.ntris,nverts,(GLuint)(vertex_size * nverts));
      glBufferData(GL_ARRAY_BUFFER,vertex_size * nverts,0,GL_STATIC_DRAW);
      {const GLenum e = glGetError();
      if(e != GL_NO_ERROR) {
	tcp_printf("error: %x\n",e);
      }}

      tcp_printf("\tabout to map\n");
      uint8_t * buffer = (uint8_t *)glMapBuffer(GL_ARRAY_BUFFER,GL_WRITE_ONLY);
      {const GLenum e = glGetError();
      if(e != GL_NO_ERROR) {
	tcp_printf("error: %x\n",e);
      }}
      tcp_printf("\t\tbuffer: %u\n",(uint32_t)((uint64_t)buffer));

      op.positions = buffer + position_offset;
      op.normals = buffer + normal_offset;
      op.uvs = buffer + uv_offset;
      op.colors = buffer + color_offset;

      tcp_printf("\tabout to fill\n");
      const triangulated_aiMesh_to_vertex_array::output op_out = op.fill(mesh,false,false,false);

      tcp_printf("filled-in: %u positions, %u normals, %u uvs, %u colors\n",
		 op_out.nPositions,op_out.nNormals,op_out.nUVs,op_out.nColors);

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

      tcp_printf("read scene; it has %u meshes\n",(uint32_t)scene -> mNumMeshes);
      return result;
    }
  }

  tcp_printf("Failed to read scene");
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
  glLineWidth(1.0);
}

extern "C"
int
rsxgltest_draw()
{
  float rgb[3] = {
    compute_sine_wave(rgb_waves,rsxgltest_elapsed_time),
    compute_sine_wave(rgb_waves + 1,rsxgltest_elapsed_time),
    compute_sine_wave(rgb_waves + 2,rsxgltest_elapsed_time)
  };

  glClearColor(rgb[0] * 0.1,rgb[1] * 0.1,rgb[2] * 0.1,1.0);
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

  if(models[imodel].ntris > 0 &&
     models[imodel].vbo != 0 &&
     models[imodel].ibo != 0 &&
     models[imodel].vao != 0) {
    glBindVertexArray(models[imodel].vao);

    Eigen::Affine3f modelview = ViewMatrixInv * (Eigen::Affine3f::Identity() * rotmat * Eigen::UniformScaling< float >(models[imodel].scale));
    glUniformMatrix4fv(TransMatrix_location,1,GL_FALSE,modelview.data());

#if 0
    size_t itri = 0;
    const size_t ntris = models[imodel].ntris;
    const size_t maxbatch = 256 / 3;

    while(itri < ntris) {
      const size_t nbatch = std::min(ntris - itri,maxbatch);
      glDrawArrays(GL_POINTS,itri * 3,nbatch * 3);
      itri += nbatch;
    }
#endif

#if 0
    glDrawArrays(GL_TRIANGLES,0,models[imodel].ntris * 3);
#endif

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,models[imodel].ibo);
    glDrawElements(GL_TRIANGLES,models[imodel].ntris * 3,GL_UNSIGNED_INT,0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,0);

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
