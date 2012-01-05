CC = @ppu_CC@
CXX = @ppu_CXX@
AR = @ppu_AR@
RANLIB = @ppu_RANLIB@

CFLAGS = -O1 @ppu_CFLAGS@
CXXFLAGS = -O1 @ppu_CXXFLAGS@
CPPFLAGS = @ppu_CPPFLAGS@
PSL1GHT_CPPFLAGS = @PSL1GHT_CPPFLAGS@
PSL1GHT_LDFLAGS = @PSL1GHT_CPPFLAGS@

ppu_lib_LIBRARIES = libEGL.a libGL.a
nobase_ppu_include_HEADERS = GL3/rsxgl.h GL3/gl3ext.h GL3/rsxgl3ext.h GL3/rsxgl_compatibility.h

dlmalloc_CPPFLAGS = -DMSPACES -DONLY_MSPACES -DHAVE_MMAP=0 -Dmalloc_getpagesize=4096

libEGL_a_SOURCES = egl.c mem.c malloc.c
libEGL_a_CFLAGS = -std=gnu99 -fgnu89-inline
libEGL_a_CPPFLAGS = -D__RSX__ -I\$(top_srcdir)/include -Wall $(dlmalloc_CPPFLAGS) $(PSL1GHT_CPPFLAGS)

libGL_a_SOURCES = rsxgl_context.cc rsxgl_object_context.cc gl_fifo.c					\
	error.cc get.cc state.cc enable.cc arena.cc buffer.cc clear.cc draw.cc	\
	sync.cc query.cc							\
	program.cc attribs.cc uniforms.cc textures.cc framebuffer.cc		\
	ringbuffer_migrate.cc dumb_migrate.cc debug.c \
	../cgcomp/source/nv40vertprog.c ../cgcomp/source/nv40fragprog.c
libGL_a_CPPFLAGS = -D__RSX__ -I\$(top_srcdir)/include $(PSL1GHT_CPPFLAGS) -I$(top_srcdir)/src/cgcomp/include
libGL_a_CFLAGS = -std=gnu99 -fgnu89-inline
libGL_a_CXXFLAGS = -I$(top_srcdir)/extsrc/boost -std=c++0x -fno-rtti
libGL_a_LIBADD = @PPU_CXXLIB@
