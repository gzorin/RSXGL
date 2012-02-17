MESA_LOCATION = @MESA_LOCATION@

TOP = $(MESA_LOCATION)
include $(MESA_LOCATION)/configs/current

MESA_CFLAGS = -Wall -Wmissing-prototypes -Wdeclaration-after-statement \
	-Wpointer-arith $(OPT_FLAGS) $(PIC_FLAGS) $(ARCH_FLAGS) \
	$(DEFINES) $(ASM_FLAGS) -std=c99 -ffast-math $(RSX_INCLUDES)

MESA_CXXFLAGS = -Wall -Wpointer-arith $(OPT_FLAGS) $(PIC_FLAGS) $(ARCH_FLAGS) \
	$(DEFINES) $(RSX_INCLUDES)

CC = @ppu_CC@
CXX = @ppu_CXX@
AR = @ppu_AR@
RANLIB = @ppu_RANLIB@

CFLAGS = @ppu_CFLAGS@
CXXFLAGS = @ppu_CXXFLAGS@
CPPFLAGS = @ppu_CPPFLAGS@

noinst_LIBRARIES = libnvfxc.a

libnvfxc_a_SOURCES = nvfxcc.cc nvfxc.c dlfcn.c
libnvfxc_a_CPPFLAGS = -I$(MESA_LOCATION)/src/glsl -I$(MESA_LOCATION)/src/mesa -I$(MESA_LOCATION)/src/mapi -I$(MESA_LOCATION)/include -I$(MESA_LOCATION)/src/gallium/include -I$(MESA_LOCATION)/src/gallium/auxiliary \
	-I$(MESA_LOCATION)/src/gallium/drivers $(LIBDRM_CFLAGS)
libnvfxc_a_CFLAGS = $(MESA_CFLAGS)
libnvfxc_a_CXXFLAGS = $(MESA_CXXFLAGS)
libnvfxc_a_LIBADD = \
	$(MESA_LOCATION)/src/glsl/libglsl.a \
	$(MESA_LOCATION)/src/gallium/drivers/nvfx/libnvfx.a \
	$(MESA_LOCATION)/src/gallium/drivers/nouveau/libnouveau.a \
	$(MESA_LOCATION)/src/mesa/libmesagallium.a \
	$(MESA_LOCATION)/src/mapi/glapi/libglapi.a \
	$(MESA_LOCATION)/src/gallium/auxiliary/libgallium.a
