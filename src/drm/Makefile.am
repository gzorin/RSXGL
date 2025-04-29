AUTOMAKE_OPTIONS = subdir-objects

CC = @ppu_CC@
CXX = @ppu_CXX@
AR = @ppu_AR@
RANLIB = @ppu_RANLIB@

CFLAGS = -O1 @ppu_CFLAGS@
CXXFLAGS = -O1 @ppu_CXXFLAGS@
CPPFLAGS = @ppu_CPPFLAGS@

LIBDRM_LOCATION = @LIBDRM_LOCATION@

noinst_LIBRARIES = libdrm_nouveau.a

libdrm_nouveau_a_SOURCES = $(LIBDRM_LOCATION)/nouveau/nouveau_device.c	\
	$(LIBDRM_LOCATION)/nouveau/nouveau_channel.c			\
	$(LIBDRM_LOCATION)/nouveau/nouveau_pushbuf.c			\
	$(LIBDRM_LOCATION)/nouveau/nouveau_grobj.c			\
	$(LIBDRM_LOCATION)/nouveau/nouveau_notifier.c			\
	$(LIBDRM_LOCATION)/nouveau/nouveau_bo.c				\
	$(LIBDRM_LOCATION)/nouveau/nouveau_resource.c			\
	$(LIBDRM_LOCATION)/nouveau/nouveau_private.h			\
	$(LIBDRM_LOCATION)/nouveau/nouveau_reloc.c \
	xf86drm_stubs.c
libdrm_nouveau_a_CPPFLAGS = -I$(LIBDRM_LOCATION) -I$(LIBDRM_LOCATION)/include/drm $(PSL1GHT_CPPFLAGS)
