// RSXGL - Graphics library for the PS3 GPU.
//
// Copyright (c) 2011 Alexander Betts (alex.betts@gmail.com)
//
// glassimp.cc - Functions to facilitate loading assimp meshes into OpenGL vertex arrays.

#include "glassimp.h"

#include <aiMesh.h>

#include <stdint.h>
#include <algorithm>

extern "C" GLuint
glassimpTrianglesCount(struct aiMesh const * mesh)
{
  if(mesh -> HasFaces()) {
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
	// TODO - Triangulate quads elsewhere:
	//nTriangles += 2;
	break;
      default:
	++nNSided;
	break;
      }
    }
    
    return nTriangles;
  }
  else {
    return 0;
  }
}

struct copy_vector {
  void operator()(uint8_t * _array,aiVector3D const & v,const GLint size,const float w) const {
    GLfloat * array = (GLfloat *)_array;
    
    if(size > 0) array[0] = v.x;
    if(size > 1) array[1] = v.y;
    if(size > 2) array[2] = v.z;
    if(size > 3) array[3] = w;
  }
  
  void operator()(uint8_t * _array,aiColor4D const & c,const GLint size,const float) const {
    GLfloat * array = (GLfloat *)_array;
    
    if(size > 0) array[0] = c.r;
    if(size > 1) array[1] = c.g;
    if(size > 2) array[2] = c.b;
    if(size > 3) array[3] = c.a;
  }
};

template < typename _Type >
struct load_array {
  GLuint operator()(struct aiMesh const * mesh,const GLint size,const GLsizei stride,_Type const * vectors,GLvoid * pointer,const float w) const {
    uint8_t * array = (uint8_t *)pointer;
    
    GLuint nVertices = 0;
    copy_vector op;
  
    aiFace const * face = mesh -> mFaces;
    for(unsigned int i = 0,n = mesh -> mNumFaces;i < n;++i,++face) {
      if(face -> mNumIndices != 3) continue;
      
      unsigned int const * indices = face -> mIndices;
      for(unsigned int j = 0,m = face -> mNumIndices;j < m;++j,++indices) {
	const unsigned int index = *indices;
	op(array,vectors[index],size,w);
	array += stride;
	++nVertices;
      }
    }

    return nVertices;
  }
};

template < typename _Type >
struct load_elements {
  GLuint operator()(struct aiMesh const * mesh,GLvoid * pointer) const {
    _Type * array = (_Type *)pointer;
    
    GLuint nElements = 0;
    copy_vector op;
  
    aiFace const * face = mesh -> mFaces;
    for(unsigned int i = 0,n = mesh -> mNumFaces;i < n;++i,++face) {
      if(face -> mNumIndices != 3) continue;
      
      unsigned int const * indices = face -> mIndices;
      for(unsigned int j = 0,m = face -> mNumIndices;j < m;++j,++array,++indices) {
	*array = (_Type)*indices;
	++nElements;
      }
    }

    return nElements;
  }
};

extern "C" GLuint
glassimpTrianglesLoadArray(struct aiMesh const * mesh,const GLenum attrib,const GLuint index,const GLint size,const GLenum type,const GLsizei stride,GLvoid * pointer)
{
  if(size < 0 || size > 4) return 0;
  if(type != GL_FLOAT) return 0;

  if(attrib == GLASSIMP_VERTEX_ARRAY) {
    return load_array< aiVector3D >()(mesh,size,stride,mesh -> mVertices,pointer,1.0f);
  }
  else if(attrib == GLASSIMP_NORMAL_ARRAY && mesh -> HasNormals()) {
    return load_array< aiVector3D >()(mesh,size,stride,mesh -> mNormals,pointer,0.0f);
  }
  else if(attrib == GLASSIMP_TEXTURE_COORD_ARRAY && mesh -> HasTextureCoords(index)) {
    return load_array< aiVector3D >()(mesh,std::min(size,(GLint)mesh -> mNumUVComponents[index]),stride,mesh -> mTextureCoords[index],pointer,0.0f);
  }
  else if(attrib == GLASSIMP_COLOR_ARRAY && mesh -> HasVertexColors(index)) {
    return load_array< aiColor4D >()(mesh,size,stride,mesh -> mColors[index],pointer,0.0f);
  }
  else {
    return 0;
  }
}

extern "C" GLuint
glassimpTrianglesLoadArrayElements(struct aiMesh const * mesh,const GLenum type,GLvoid * pointer)
{
  if(type == GL_UNSIGNED_BYTE) {
    return load_elements< uint8_t >()(mesh,pointer);
  }
  else if(type == GL_UNSIGNED_SHORT) {
    return load_elements< uint16_t >()(mesh,pointer);
  }
  else if(type == GL_UNSIGNED_INT) {
    return load_elements< uint32_t >()(mesh,pointer);
  }
  else {
    return 0;
  }
}

template < typename _Type >
struct copy_array {
  GLuint operator()(struct aiMesh const * mesh,const GLint size,const GLsizei stride,_Type const * vectors,GLvoid * pointer,const float w) const {
    uint8_t * array = (uint8_t *)pointer;
    
    GLuint nVertices = 0;
    copy_vector op;

    for(unsigned int i = 0,n = mesh -> mNumVertices;i < n;++i) {
      op(array,vectors[i],size,w);
      array += stride;
      ++nVertices;
    }

    return nVertices;
  }
};

extern "C" GLuint
glassimpTrianglesCopyArray(struct aiMesh const * mesh,const GLenum attrib,const GLuint index,const GLint size,const GLenum type,const GLsizei stride,GLvoid * pointer)
{
  if(size < 0 || size > 4) return 0;
  if(type != GL_FLOAT) return 0;

  if(attrib == GLASSIMP_VERTEX_ARRAY) {
    return copy_array< aiVector3D >()(mesh,size,stride,mesh -> mVertices,pointer,1.0f);
  }
  else if(attrib == GLASSIMP_NORMAL_ARRAY && mesh -> HasNormals()) {
    return copy_array< aiVector3D >()(mesh,size,stride,mesh -> mNormals,pointer,0.0f);
  }
  else if(attrib == GLASSIMP_TEXTURE_COORD_ARRAY && mesh -> HasTextureCoords(index)) {
    return copy_array< aiVector3D >()(mesh,std::min(size,(GLint)mesh -> mNumUVComponents[index]),stride,mesh -> mTextureCoords[index],pointer,0.0f);
  }
  else if(attrib == GLASSIMP_COLOR_ARRAY && mesh -> HasVertexColors(index)) {
    return copy_array< aiColor4D >()(mesh,size,stride,mesh -> mColors[index],pointer,0.0f);
  }
  else {
    return 0;
  }
}

extern "C" GLuint
glassimpTrianglesFormat(struct aiMesh const * mesh,const GLboolean interleaved,const GLsizei n,GLenum const * attribs,GLuint const * indices,GLint const * sizes,GLenum * types,GLvoid * pointer,GLint * sizes_out,GLsizei * strides_out,GLvoid ** pointers_out)
{
  GLuint stride = 0;
  GLint * psizes_out = sizes_out;

  for(GLsizei i = 0;i < n;++i) {
    const GLenum attrib = *attribs;
    const GLenum type = *types;
    GLint size_out = 0;

    if(attrib == GLASSIMP_VERTEX_ARRAY && type == GL_FLOAT) {
      size_out = std::min(*sizes,(GLint)3);
    }
    else if(attrib == GLASSIMP_NORMAL_ARRAY && mesh -> HasNormals() && type == GL_FLOAT) {
      size_out = std::min(*sizes,(GLint)3);
    }
    else if(attrib == GLASSIMP_TEXTURE_COORD_ARRAY && mesh -> HasTextureCoords(*indices) && type == GL_FLOAT) {
      size_out = std::min(*sizes,(GLint)mesh -> mNumUVComponents[*indices]);
    }
    else if(attrib == GLASSIMP_COLOR_ARRAY && mesh -> HasVertexColors(*indices) && type == GL_FLOAT) {
      size_out = std::min(*sizes,(GLint)4);
    }
    else {
      size_out = 0;
    }

    stride += size_out;

    *psizes_out = size_out;
    ++psizes_out;

    ++attribs;
    ++indices;
    ++sizes;
    ++types;
  }

  uint8_t * p_pointer = (uint8_t *)pointer;
  if(interleaved) {
    for(GLsizei i = 0;i < n;++i) {
      *strides_out = stride * sizeof(float);
      ++strides_out;

      *pointers_out = p_pointer;
      ++pointers_out;

      p_pointer += sizeof(float) * sizes_out[i];
    }
  }
  else {
    const GLuint nVertices = glassimpTrianglesCount(mesh) * 3;
    for(GLsizei i = 0;i < n;++i) {
      *strides_out = 0;
      ++strides_out;

      *pointers_out = p_pointer;
      ++pointers_out;

      p_pointer += sizeof(float) * sizes_out[i] * nVertices;
    }
  }
  
  return stride * sizeof(float);
}

extern "C" GLuint
glassimpTrianglesVertexSize(struct aiMesh const * mesh,const GLsizei n,GLenum const * attribs,GLuint const * indices,GLint const * sizes,GLenum * types)
{
  GLuint stride = 0;

  for(GLsizei i = 0;i < n;++i) {
    const GLenum attrib = *attribs;
    const GLenum type = *types;
    GLint size_out = 0;

    if(attrib == GLASSIMP_VERTEX_ARRAY && type == GL_FLOAT) {
      size_out = std::min(*sizes,(GLint)3);
    }
    else if(attrib == GLASSIMP_NORMAL_ARRAY && mesh -> HasNormals() && type == GL_FLOAT) {
      size_out = std::min(*sizes,(GLint)3);
    }
    else if(attrib == GLASSIMP_TEXTURE_COORD_ARRAY && mesh -> HasTextureCoords(*indices) && type == GL_FLOAT) {
      size_out = std::min(*sizes,(GLint)mesh -> mNumUVComponents[*indices]);
    }
    else if(attrib == GLASSIMP_COLOR_ARRAY && mesh -> HasVertexColors(*indices) && type == GL_FLOAT) {
      size_out = std::min(*sizes,(GLint)4);
    }
    else {
      size_out = 0;
    }

    stride += size_out;

    ++attribs;
    ++indices;
    ++sizes;
    ++types;
  }

  return stride * sizeof(float);
}

extern "C" GLvoid
glassimpTrianglesLoadArrays(struct aiMesh const * mesh,const GLboolean interleaved,const GLsizei n,GLenum const * attribs,GLuint const * indices,GLint const * sizes,GLenum * types,GLvoid * pointer)
{
  GLint sizes_out[n];
  GLsizei strides_out[n];
  GLvoid * pointers_out[n];

  glassimpTrianglesFormat(mesh,interleaved,n,attribs,indices,sizes,types,pointer,sizes_out,strides_out,pointers_out);

  for(GLsizei i = 0;i < n;++i) {
    if(sizes_out[i] == 0) continue;

    glassimpTrianglesLoadArray(mesh,attribs[i],indices[i],sizes_out[i],types[i],strides_out[i],pointers_out[i]);
  }
}

extern "C" GLvoid
glassimpTrianglesSetPointers(struct aiMesh const * mesh,const GLint * locations,const GLboolean interleaved,const GLsizei n,GLenum const * attribs,GLuint const * indices,GLint const * sizes,GLenum * types,GLvoid * pointer)
{
  GLint sizes_out[n];
  GLsizei strides_out[n];
  GLvoid * pointers_out[n];

  glassimpTrianglesFormat(mesh,interleaved,n,attribs,indices,sizes,types,pointer,sizes_out,strides_out,pointers_out);

  GLint * psizes_out = sizes_out;
  GLsizei * pstrides_out = strides_out;
  GLvoid ** ppointers_out = pointers_out;
  for(GLsizei i = 0;i < n;++i,++locations,++psizes_out,++types,++pstrides_out,++ppointers_out) {
    if(*psizes_out == 0 || *locations < 0) {
      if(*locations >= 0) {
	glDisableVertexAttribArray(*locations);
      }
    }
    else {
      glEnableVertexAttribArray(*locations);
      glVertexAttribPointer(*locations,*psizes_out,*types,GL_FALSE,*pstrides_out,*ppointers_out);
    }
  }
}
