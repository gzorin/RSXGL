CC = @ppu_CC@
CXX = @ppu_CXX@
AR = @ppu_AR@
RANLIB = @ppu_RANLIB@

CFLAGS = -O1 @ppu_CFLAGS@
CXXFLAGS = -O1 @ppu_CXXFLAGS@
CPPFLAGS = @ppu_CPPFLAGS@

LIBDRM_LOCATION = @LIBDRM_LOCATION@
MESA_LOCATION = @MESA_LOCATION@
nouveau_LOCATION = $(MESA_LOCATION)/src/gallium/drivers/nouveau

noinst_LIBRARIES = libnvfx.a

libnvfx_a_SOURCES = nv04_2d.c \
	nvfx_buffer.c \
	nvfx_context.c \
	nvfx_clear.c \
	nvfx_draw.c \
	nvfx_fragprog.c \
	nvfx_fragtex.c \
	nv30_fragtex.c \
	nv40_fragtex.c \
	nvfx_miptree.c \
	nvfx_push.c \
	nvfx_query.c \
	nvfx_resource.c \
	nvfx_screen.c \
	nvfx_state.c \
	nvfx_state_emit.c \
	nvfx_state_fb.c \
	nvfx_surface.c \
	nvfx_transfer.c \
	nvfx_vbo.c \
	nvfx_vertprog.c
libnvfx_a_CPPFLAGS = \
	-I$(LIBDRM_LOCATION) -I$(LIBDRM_LOCATION)/include -I$(LIBDRM_LOCATION)/include/drm -I$(LIBDRM_LOCATION)/nouveau \
	-I$(MESA_LOCATION)/src/gallium/include -I$(MESA_LOCATION)/src/gallium/auxiliary -I$(top_srcdir)/src \
	-I$(top_srcdir)/src/mesa/src/rsx/include
libnvfx_a_CFLAGS = -std=c99
