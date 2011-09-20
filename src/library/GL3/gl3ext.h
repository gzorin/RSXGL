#ifndef __gl3ext_h_
#define __gl3ext_h_

#ifdef __cplusplus
extern "C" {
#endif

//-*-C-*-
// RSXGL - Graphics library for the PS3 GPU.
//
// Copyright (c) 2011 Alexander Betts (alex.betts@gmail.com)
//
// gl3ext.h - OpenGL extensions implemented by RSXGL but not included in the draft gl3.h header.

#ifndef GL_ARB_texture_storage
#define GL_TEXTURE_IMMUTABLE_FORMAT       0x912F
#endif

#ifndef GL_ARB_texture_storage
#define GL_ARB_texture_storage 1
#ifdef GL3_PROTOTYPES
GLAPI void APIENTRY glTexStorage1D(GLenum target, GLsizei levels,GLenum internalformat,GLsizei width);
GLAPI void APIENTRY glTexStorage2D(GLenum target, GLsizei levels,GLenum internalformat,GLsizei width, GLsizei height);
GLAPI void APIENTRY glTexStorage3D(GLenum target, GLsizei levels,GLenum internalformat,GLsizei width, GLsizei height, GLsizei depth);
GLAPI void APIENTRY glTextureStorage1DEXT(GLuint texture, GLenum target, GLsizei levels,GLenum internalformat,GLsizei width);
GLAPI void APIENTRY glTextureStorage2DEXT(GLuint texture, GLenum target, GLsizei levels,GLenum internalformat,GLsizei width, GLsizei height);
GLAPI void APIENTRY glTextureStorage3DEXT(GLuint texture, GLenum target, GLsizei levels,GLenum internalformat,GLsizei width, GLsizei height, GLsizei depth);
#endif
#endif

#ifdef __cplusplus
}
#endif

#endif
