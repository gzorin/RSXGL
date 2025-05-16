CC = @ppu_CC@
CXX = @ppu_CXX@
AR = @ppu_AR@
RANLIB = @ppu_RANLIB@

MKLIB = @top_builddir@/src/mesa/mklib-rsx

CFLAGS = -O3 @ppu_CFLAGS@
CXXFLAGS = -O3 @ppu_CXXFLAGS@
CPPFLAGS = @ppu_CPPFLAGS@
PSL1GHT_CPPFLAGS = @PSL1GHT_CPPFLAGS@
PSL1GHT_LDFLAGS = @PSL1GHT_LDFLAGS@

ppu_lib_LIBRARIES = libEGL.a libGL.a
nobase_ppu_include_HEADERS = GL3/rsxgl.h GL3/gl3ext.h GL3/rsxgl3ext.h GL3/rsxgl_compatibility.h

dlmalloc_CPPFLAGS = -DMSPACES -DONLY_MSPACES -DHAVE_MMAP=0 -Dmalloc_getpagesize=4096

MESA_LOCATION = @MESA_LOCATION@
MESA_CPPFLAGS = -I$(MESA_LOCATION)/src \
	-I$(MESA_LOCATION)/src/mesa \
	-I$(MESA_LOCATION)/src/mapi \
	-I$(MESA_LOCATION)/include \
	-I$(MESA_LOCATION)/src/gallium/include \
	-I$(MESA_LOCATION)/src/gallium/auxiliary \
	-I$(MESA_LOCATION)/src/gallium/drivers \
	-DPIPE_OS_UNIX

LIBDRM_LOCATION = @LIBDRM_LOCATION@
LIBDRM_CPPFLAGS = -I$(LIBDRM_LOCATION) -I$(LIBDRM_LOCATION)/include -I$(LIBDRM_LOCATION)/include/drm -I$(LIBDRM_LOCATION)/nouveau

libEGL_a_SOURCES = egl.c mem.c malloc.c dl.c
libEGL_a_CFLAGS = -std=gnu99 -fgnu89-inline
libEGL_a_CPPFLAGS = -D__RSX__ -I$(top_srcdir)/src -I\$(top_srcdir)/include -Wall $(dlmalloc_CPPFLAGS) $(PSL1GHT_CPPFLAGS) \
	$(MESA_CPPFLAGS) $(LIBDRM_CPPFLAGS) -I$(MESA_LOCATION)/src/gallium/drivers/nvfx
libEGL_a_DEPENDENCIES = $(top_builddir)/src/nvfx/libnvfx.a \
	$(top_builddir)/src/nouveau/libnouveau.a \
	$(top_builddir)/src/drm/libdrm_nouveau.a \
	$(top_builddir)/extsrc/mesa/src/gallium/auxiliary/libgallium.a
libEGL.a: $(libEGL_a_OBJECTS) $(libEGL_a_DEPENDENCIES)
	@. $(MKLIB) -static -arch PS3 -o EGL \
	$(libEGL_a_OBJECTS) \
	$(top_builddir)/src/nvfx/libnvfx.a \
	$(top_builddir)/src/nouveau/libnouveau.a \
	$(top_builddir)/src/drm/libdrm_nouveau.a \
	$(top_builddir)/extsrc/mesa/src/gallium/auxiliary/libgallium.a

libGL_a_SOURCES = rsxgl_context.cc rsxgl_object_context.cc gl_fifo.c					\
	error.cc get.cc state.cc enable.cc arena.cc buffer.cc clear.cc draw.cc	\
	sync.cc query.cc							\
	compiler_context.cc compiler_translate.c program.cc attribs.cc uniforms.cc textures.cc framebuffer.cc		\
	ringbuffer_migrate.cc dumb_migrate.cc texture_migrate.cc debug.c \
	pixel_store.cc st_format.c
libGL_a_CPPFLAGS = -Wall -D__RSX__ -I$(top_srcdir)/src -I\$(top_srcdir)/include $(PSL1GHT_CPPFLAGS) \
	$(MESA_CPPFLAGS) $(LIBDRM_CPPFLAGS)
libGL_a_CFLAGS = -std=gnu99 -fgnu89-inline
libGL_a_CXXFLAGS = -I$(top_srcdir)/extsrc/boost -std=c++11
libGL_a_DEPENDENCIES = libEGL.a \
	$(top_builddir)/extsrc/mesa/src/mesa/libmesa.a \
	$(top_builddir)/extsrc/mesa/src/mesa/libmesagallium.a \
	$(top_builddir)/extsrc/mesa/src/mapi/glapi/libglapi.a \
	$(top_builddir)/extsrc/mesa/src/glsl/libglsl.a
libGL.a: $(libGL_a_OBJECTS) $(libGL_a_DEPENDENCIES)
	@. $(MKLIB) -static -arch PS3 -cplusplus -o GL \
	$(libGL_a_OBJECTS) \
	$(top_builddir)/extsrc/mesa/src/mesa/libmesa.a \
	$(top_builddir)/extsrc/mesa/src/mesa/libmesagallium.a \
	$(top_builddir)/extsrc/mesa/src/gallium/auxiliary/libgallium.a \
	$(top_builddir)/extsrc/mesa/src/mapi/glapi/libglapi.a \
	$(top_builddir)/extsrc/mesa/src/glsl/libglsl.a