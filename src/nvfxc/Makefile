#-*-Makefile-*-

mesa_top_srcdir = $(HOME)/mesa

TOP = $(mesa_top_srcdir)
include $(mesa_top_srcdir)/configs/current

CPPFLAGS += -I$(mesa_top_srcdir)/src/glsl -I$(mesa_top_srcdir)/src/mesa -I$(mesa_top_srcdir)/src/mapi -I$(mesa_top_srcdir)/include -I$(mesa_top_srcdir)/src/gallium/include -I$(mesa_top_srcdir)/src/gallium/auxiliary \
	-I$(mesa_top_srcdir)/src/gallium/drivers $(LIBDRM_CFLAGS)

all: nvfxc

# $(CXX) -o $@ -L$(mesa_top_srcdir)/src/glsl -lglsl -L$(mesa_top_srcdir)/src/mesa -lmesa -lmesagallium -L$(mesa_top_srcdir)/src/gallium/auxiliary -lgallium -L$(mesa_top_srcdir)/src/mapi/glapi -lglapi
# $(CXX) -o $@ -L$(mesa_top_srcdir)/src/glsl -lglsl -L$(mesa_top_srcdir)/src/mesa -lmesa -lmesagallium -L$(mesa_top_srcdir)/src/gallium/auxiliary -lgallium

# $(mesa_top_srcdir)/src/mesa/libmesagallium.a

# $(mesa_top_srcdir)/src/glsl/libglsl.a \
	$(mesa_top_srcdir)/src/mesa/libmesa.a \
	$(mesa_top_srcdir)/src/mesa/libmesagallium.a \
	$(mesa_top_srcdir)/src/gallium/auxiliary/libgallium.a \
	$(mesa_top_srcdir)/src/mapi/glapi/libglapi.a \
	$(mesa_top_srcdir)/src/gallium/drivers/nvfx/libnvfx.a \
	$(mesa_top_srcdir)/src/gallium/drivers/nouveau/libnouveau.a

nvfxc: main.o nvfxc.o dlfcn.o
	$(CXX) -o $@ $^ \
	-L$(mesa_top_srcdir)/src/glsl -lglsl \
	-L$(mesa_top_srcdir)/src/gallium/drivers/nvfx -lnvfx \
	-L$(mesa_top_srcdir)/src/gallium/drivers/nouveau -lnouveau \
	-L$(mesa_top_srcdir)/src/mesa -lmesagallium \
	-L$(mesa_top_srcdir)/src/mapi/glapi -lglapi \
	-L$(mesa_top_srcdir)/src/gallium/auxiliary -lgallium \
	$(LDFLAGS)
