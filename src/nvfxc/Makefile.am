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

noinst_PROGRAMS = nvfxc

nvfxc_SOURCES = main.cpp nvfxc.c dlfcn.c
nvfxc_CPPFLAGS = -I$(MESA_LOCATION)/src/glsl -I$(MESA_LOCATION)/src/mesa -I$(MESA_LOCATION)/src/mapi -I$(MESA_LOCATION)/include -I$(MESA_LOCATION)/src/gallium/include -I$(MESA_LOCATION)/src/gallium/auxiliary \
	-I$(MESA_LOCATION)/src/gallium/drivers $(LIBDRM_CFLAGS)
nvfxc_CFLAGS = $(MESA_CFLAGS)
nvfxc_CXXFLAGS = $(MESA_CXXFLAGS)
nvfxc_LDFLAGS = -L$(MESA_LOCATION)/src/glsl \
	-L$(MESA_LOCATION)/src/gallium/drivers/nvfx \
	-L$(MESA_LOCATION)/src/gallium/drivers/nouveau \
	-L$(MESA_LOCATION)/src/mesa \
	-L$(MESA_LOCATION)/src/mapi/glapi \
	-L$(MESA_LOCATION)/src/gallium/auxiliary
nvfxc_LDADD = -lglsl -lnvfx -lnouveau -lmesagallium -lglapi -lgallium


