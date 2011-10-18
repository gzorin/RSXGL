#ifndef glassimp_H
#define glassimp_H

#include <GL3/gl3.h>

#include <aiMesh.h>

#if defined(__cplusplus)
extern "C" {
#endif

enum {
  GLASSIMP_VERTEX_ARRAY = 0,
  GLASSIMP_NORMAL_ARRAY = 1,
  GLASSIMP_TEXTURE_COORD_ARRAY = 2,
  GLASSIMP_COLOR_ARRAY = 3
};

GLuint glassimpTrianglesCount(struct aiMesh const * mesh);
GLuint glassimpTrianglesLoadArray(struct aiMesh const * mesh,GLenum attrib,GLuint index,GLint size,GLenum type,GLsizei stride,GLvoid * pointer);
GLuint glassimpTrianglesLoadArrayElements(struct aiMesh const * mesh,GLenum type,GLvoid * pointer);
GLuint glassimpTrianglesFormat(struct aiMesh const * mesh,GLboolean,GLsizei n,GLenum const * attribs,GLuint const * indices,GLint const * sizes,GLenum * types,GLvoid * pointer,GLint * sizes_out,GLsizei * strides,GLvoid ** pointers);

#if defined(__cplusplus)
}
#endif

#endif
