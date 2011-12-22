CC = @PPU_CC@
CXX = @PPU_CXX@
AR = @PPU_AR@
RANLIB = @PPU_RANLIB@
LD = @PPU_LD@

NV40ASM = $(top_builddir)/src/cgcomp/nv40asm

eigen_LOCATION = $(top_srcdir)/extsrc/eigen
eigen_CPPFLAGS = -I$(eigen_LOCATION)

include $(srcdir)/../samples.am

noinst_PROGRAMS = glassview.elf

objs = teapot.obj teddy.obj crab.obj yamato.obj
objs_H = $(objs:.obj=_obj.h)
objs_OBJS = $(objs:.obj=_obj.o)

SUFFIXES += .obj _obj.o _obj.h

%_obj.o: %.obj
	@echo "[bin2o]: $<"
	@$(bin2o)

%_obj.h: %_obj.o
	@echo "$<"

shaders = glassview.vert glassview.frag diffuse.vert diffuse.frag 
sources = glassview.cc $(shaders) \
	$(objs)
objects = $(call shader_objs,$(shaders)) \
	$(objs_OBJS)

BUILT_SOURCES = $(call shader_headers,$(shaders)) \
	$(objs_H)
CLEANFILES = $(call shader_headers,$(shaders)) $(call shader_objs,$(shaders)) \
	$(objs_H) $(objs_OBJS)

glassview_elf_DEPENDENCIES = $(top_builddir)/src/library/libEGL.a $(top_builddir)/src/library/libGL.a	\
	$(objects)											\
	$(top_builddir)/src/assimp/libassimp.a $(top_builddir)/src/assimp/libglassimp.a
glassview_elf_SOURCES = ../rsxgltest/main.c		\
	../rsxgltest/math3d.cc ../rsxgltest/sine_wave.c	\
	$(sources)

glassview_elf_CPPFLAGS = -D__RSX__ -I\$(top_srcdir)/include $(eigen_CPPFLAGS) $(PS3DEV_PPU_CPPFLAGS) $(PSL1GHT_PPU_CPPFLAGS) $(PORTLIBS_PPU_CPPFLAGS) -I$(top_srcdir)/include -I$(top_srcdir)/src/library -I$(top_builddir)/src/library -I$(top_srcdir)/src/samples/rsxgltest -I$(top_srcdir)/extsrc/boost  -I@assimp_SOURCE_LOCATION@/code -I@assimp_SOURCE_LOCATION@/include -I@top_srcdir@/src/assimp -D__ppc__ -DASSIMP_BUILD_NO_COB_IMPORTER -DASSIMP_BUILD_NO_MD3_IMPORTER -DASSIMP_BUILD_NO_MDL_IMPORTER -DASSIMP_BUILD_NO_Q3D_IMPORTER -DASSIMP_BUILD_NO_Q3BSP_IMPORTER -DASSIMP_BUILD_NO_IFC_IMPORTER -DASSIMP_BUILD_NO_X_IMPORTER -DASSIMP_BUILD_NO_HMP_IMPORTER
glassview_elf_CFLAGS = -std=gnu99 -fgnu89-inline -O3
glassview_elf_LDFLAGS = $(PS3DEV_PPU_LDFLAGS) $(PSL1GHT_PPU_LDFLAGS) $(PORTLIBS_PPU_LDFLAGS) -L$(top_builddir)/src/library -L$(top_builddir)/src/assimp
glassview_elf_LDADD = $(objects) -lassimp -lglassimp -lEGL -lGL -lrsx -lgcm_sys -lnet -lio -lsysutil -lsysmodule -lpng -lz -lstdc++ -lm -lrt -llv2

TITLE           :=      GLASSVIEW
APPID           :=      TSTRSXGL2
CONTENTID       :=      UP0001-$(APPID)_00-0000000000000000
PKGFILES        :=      release
override ICON0		:=	$(srcdir)/../brewcrab.png

pkg: glassview.pkg

all-local: pkg

clean-local:
	@rm -rf pkg glassview.self glassview.pkg 
