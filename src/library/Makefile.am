CC = @PPU_CC@
CXX = @PPU_CXX@
AR = @PPU_AR@
RANLIB = @PPU_RANLIB@

CFLAGS = -O1 @PPU_CFLAGS@
CXXFLAGS = -O1 @PPU_CXXFLAGS@
CPPFLAGS = @PPU_CPPFLAGS@

lib_LIBRARIES = libEGL.a libGL.a
include_HEADERS = GL3/gl3ext.h GL3/rsxgl3ext.h GL3/rsxgl_compatibility.h

dlmalloc_CPPFLAGS = -DMSPACES -DONLY_MSPACES -DHAVE_MMAP=0 -Dmalloc_getpagesize=4096

libEGL_a_SOURCES = egl.c mem.c malloc.c
libEGL_a_CFLAGS = -std=gnu99 -fgnu89-inline
libEGL_a_CPPFLAGS = -D__RSX__ -I\$(top_srcdir)/include ${CPPFLAGS} -Wall $(dlmalloc_CPPFLAGS) $(PS3DEV_PPU_CPPFLAGS) $(PSL1GHT_PPU_CPPFLAGS)

libGL_a_DEPENDENCIES = ../cgcomp/libnv40.a
libGL_a_SOURCES = rsxgl_context.cc gl_fifo.c					\
	error.cc get.cc state.cc enable.cc arena.cc buffer.cc clear.cc draw.cc	\
	sync.cc									\
	program.cc attribs.cc uniforms.cc textures.cc framebuffer.cc		\
	ringbuffer_migrate.cc dumb_migrate.cc debug.c
libGL_a_CPPFLAGS = -D__RSX__ -I\$(top_srcdir)/include $(PS3DEV_PPU_CPPFLAGS) $(PSL1GHT_PPU_CPPFLAGS) -I$(top_srcdir)/src/cgcomp/include
libGL_a_CFLAGS = -std=gnu99 -fgnu89-inline
libGL_a_CXXFLAGS = -I$(top_srcdir)/extsrc/boost -std=c++0x -fno-rtti
libGL_a_LIBADD = ../cgcomp/libnv40.a @PPU_CXXLIB@
