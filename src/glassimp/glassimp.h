//-*-C-*-
// RSXGL - Graphics library for the PS3 GPU.
//
// Copyright (c) 2011 Alexander Betts (alex.betts@gmail.com)
//
// glassimp.h - Functions to facilitate loading assimp meshes into OpenGL vertex arrays.

#ifndef glassimp_H
#define glassimp_H

#define GL3_PROTOTYPES
#include <GL3/gl3.h>

#if defined(__cplusplus)
extern "C" {
#endif

enum {
  GLASSIMP_VERTEX_ARRAY = 0,
  GLASSIMP_NORMAL_ARRAY = 1,
  GLASSIMP_TEXTURE_COORD_ARRAY = 2,
  GLASSIMP_COLOR_ARRAY = 3
};

struct aiMesh;

GLuint glassimpTrianglesCount(struct aiMesh const * mesh);
GLuint glassimpTrianglesLoadArray(struct aiMesh const * mesh,const GLenum attrib,const GLuint index,const GLint size,const GLenum type,const GLsizei stride,GLvoid * pointer);
GLuint glassimpTrianglesLoadArrayElements(struct aiMesh const * mesh,const GLenum type,GLvoid * pointer);
GLuint glassimpTrianglesCopyArray(struct aiMesh const * mesh,const GLenum attrib,const GLuint index,const GLint size,const GLenum type,const GLsizei stride,GLvoid * pointer);
GLuint glassimpTrianglesFormat(struct aiMesh const * mesh,const GLboolean,const GLsizei n,GLenum const * attribs,GLuint const * indices,GLint const * sizes,GLenum * types,GLvoid * pointer,GLint * sizes_out,GLsizei * strides_out,GLvoid ** pointers_out);
GLuint glassimpTrianglesVertexSize(struct aiMesh const * mesh,const GLsizei n,GLenum const * attribs,GLuint const * indices,GLint const * sizes,GLenum * types);
GLvoid glassimpTrianglesLoadArrays(struct aiMesh const * mesh,const GLboolean,const GLsizei n,GLenum const * attribs,GLuint const * indices,GLint const * sizes,GLenum * types,GLvoid * pointer);
GLvoid glassimpTrianglesSetPointers(struct aiMesh const * mesh,const GLint * locations,const GLboolean,const GLsizei n,GLenum const * attribs,GLuint const * indices,GLint const * sizes,GLenum * types,GLvoid * pointer);

#if defined(__cplusplus)
}
#endif

#endif
