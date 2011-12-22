CC = @PPU_CC@
CXX = @PPU_CXX@
AR = @PPU_AR@
RANLIB = @PPU_RANLIB@
LD = @PPU_LD@

NV40ASM = $(top_builddir)/src/cgcomp/nv40asm

eigen_LOCATION = $(top_srcdir)/extsrc/eigen
eigen_CPPFLAGS = -I$(eigen_LOCATION)

include $(srcdir)/../samples.am

noinst_PROGRAMS = fluids.elf

shaders = fluids.vert fluids.frag pointer.vert pointer.frag
images = pointer.png
sources = fluids.cc solver.c $(shaders) $(images)
objects = $(call shader_objs,$(shaders)) $(call image_objs,$(images))

BUILT_SOURCES = $(call shader_headers,$(shaders)) $(call image_headers,$(images)) \
	$(objs_H)
CLEANFILES = $(call shader_headers,$(shaders)) $(call shader_objs,$(shaders)) \
	$(call image_headers,$(images)) $(call image_objs,$(images)) \
	$(objs_H) $(objs_OBJS)

fluids_elf_DEPENDENCIES = $(top_builddir)/src/library/libEGL.a $(top_builddir)/src/library/libGL.a	\
	$(objects)
fluids_elf_SOURCES = ../rsxgltest/main.c		\
	../rsxgltest/math3d.cc ../rsxgltest/texture.c	\
	$(sources)

fluids_elf_CPPFLAGS = -D__RSX__ -I\$(top_srcdir)/include $(eigen_CPPFLAGS) $(PS3DEV_PPU_CPPFLAGS) $(PSL1GHT_PPU_CPPFLAGS) $(PORTLIBS_PPU_CPPFLAGS) -I$(top_srcdir)/include -I$(top_srcdir)/src/library -I$(top_builddir)/src/library -I$(top_srcdir)/src/samples/rsxgltest
fluids_elf_CFLAGS = -std=gnu99 -fgnu89-inline -O3
fluids_elf_LDFLAGS = $(PS3DEV_PPU_LDFLAGS) $(PSL1GHT_PPU_LDFLAGS) $(PORTLIBS_PPU_LDFLAGS) -L$(top_builddir)/src/library
fluids_elf_LDADD = $(objects) -lEGL -lGL -lrsx -lgcm_sys -lnet -lio -lsysutil -lsysmodule -lpng -lz -lstdc++ -lm -lrt -llv2

TITLE           :=      FLUIDS
APPID           :=      TSTRSXGL3
CONTENTID       :=      UP0001-$(APPID)_00-0000000000000000
PKGFILES        :=      release
override ICON0		:=	$(srcdir)/../brewcrab.png

pkg: fluids.pkg

all-local: pkg

clean-local:
	@rm -rf pkg fluids.self fluids.pkg 
