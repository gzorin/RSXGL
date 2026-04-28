AUTOMAKE_OPTIONS = subdir-objects

CC = @ppu_CC@
CXX = @ppu_CXX@
AR = @ppu_AR@
RANLIB = @ppu_RANLIB@

CFLAGS = -O1 @ppu_CFLAGS@ -D_GNU_SOURCE -D__POSIX_VISIBLE=999999
CXXFLAGS = -O1 @ppu_CXXFLAGS@ -D_GNU_SOURCE -D__POSIX_VISIBLE=999999
CPPFLAGS = @ppu_CPPFLAGS@ -D_GNU_SOURCE -D__POSIX_VISIBLE=999999

LIBDRM_LOCATION = @LIBDRM_LOCATION@
MESA_LOCATION = @MESA_LOCATION@
nouveau_LOCATION = $(MESA_LOCATION)/src/gallium/drivers/nouveau

noinst_LIBRARIES = libnouveau.a

libnouveau_a_SOURCES = $(nouveau_LOCATION)/nouveau_screen.c \
	$(nouveau_LOCATION)/nouveau_fence.c \
	$(nouveau_LOCATION)/nouveau_mm.c \
	$(nouveau_LOCATION)/nouveau_buffer.c \
	$(nouveau_LOCATION)/nouveau_video.c
libnouveau_a_CPPFLAGS = \
	-I$(LIBDRM_LOCATION) -I$(LIBDRM_LOCATION)/include -I$(LIBDRM_LOCATION)/include/drm -I$(LIBDRM_LOCATION)/nouveau \
	-I$(MESA_LOCATION)/src/gallium/include -I$(MESA_LOCATION)/src/gallium/drivers -I$(MESA_LOCATION)/src/gallium/auxiliary -I$(top_srcdir)/src \
	-I$(top_srcdir)/src/mesa/src/rsx/include
libnouveau_a_LIBADD = $(top_builddir)/src/drm/libdrm_nouveau.a
