include $(srcdir)/../samples.am

eigen_LOCATION = $(top_srcdir)/extsrc/eigen
eigen_CPPFLAGS = -I$(eigen_LOCATION)

noinst_PROGRAMS = rsxgltest.elf

BUILT_SOURCES = textures_vpo.h textures_fpo.h \
	dice_bin.h nagel_bin.h face_png.h \
	manypoints_vpo.h manypoints_fpo.h \
	points_vpo.h points_fpo.h \
	cube_vpo.h cube_fpo.h \
	texcube_vpo.h texcube_fpo.h \
	fbo_inner_vpo.h fbo_inner_fpo.h \
	occlusion_vpo.h occlusion_fpo.h \
	gradient_png.h
CLEANFILES = draw_vpo.h draw_fpo.h draw_vpo.o draw_fpo.o \
	textures_vpo.h textures_fpo.h textures_vpo.o textures_fpo.o \
	manypoints_vpo.h manypoints_fpo.h manypoints_vpo.o manypoints_fpo.o \
	points_vpo.h points_fpo.h points_vpo.o points_fpo.o \
	cube_vpo.h cube_fpo.h cube_vpo.o cube_fpo.o \
	texcube_vpo.h texcube_fpo.h texcube_vpo.o texcube_fpo.o \
	fbo_vpo.h fbo_fpo.h fbo_vpo.o fbo_fpo.o \
	dice_bin.o dice_bin.h \
	nagel_bin.o nagel_bin.h \
	face_png.o face_png.h \
	gradient_png.o gradient_png.h

# clear.c viewport_scissor.c buffer.c program.c draw.c uniforms.c textures.c cube.cc
clear_objects = 
clear_sources = clear.c

cube_objects = cube_vpo.o cube_fpo.o
cube_sources = cube.cc cube.vcg cube.fcg

textures_objects = textures_vpo.o textures_fpo.o dice_bin.o
textures_sources = textures.c texture.c textures.vert textures.frag dice.bin

manypoints_objects = points_vpo.o points_fpo.o face_png.o
manypoints_sources = manypoints.cc texture.c points.vert points.frag face.png

manycubes_objects = cube_vpo.o cube_fpo.o
manycubes_sources = manycubes.cc cube.vert cube.frag

texcube_objects = texcube_vpo.o texcube_fpo.o nagel_bin.o gradient_png.o face_png.o
texcube_sources = texcube.cc texture.c texcube.vert texcube.frag nagel.bin face.png gradient.png

fbo_objects = fbo_inner_vpo.o fbo_inner_fpo.o nagel_bin.o
fbo_sources = fbo.cc texture.c fbo_inner.vert fbo_inner.frag nagel.bin

occlusion_objects = occlusion_vpo.o occlusion_fpo.o
occlusion_sources = occlusion.cc occlusion.vert occlusion.frag

occlusion2_objects = occlusion_vpo.o occlusion_fpo.o
occlusion2_sources = occlusion2.cc occlusion.vert occlusion.frag

objects = $(fbo_objects)
sources = $(fbo_sources)

rsxgltest_elf_DEPENDENCIES = $(top_builddir)/src/library/libEGL.a $(top_builddir)/src/library/libGL.a	\
	$(objects)
rsxgltest_elf_SOURCES = main.c			\
	math3d.cc sine_wave.c			\
	$(sources)

rsxgltest_elf_CPPFLAGS = -D__RSX__ -I\$(top_srcdir)/include $(eigen_CPPFLAGS) $(PSL1GHT_CPPFLAGS) -I$(top_srcdir)/include -I$(top_srcdir)/src/library -I$(top_builddir)/src/library @ppu_libpng_CPPFLAGS@ @ppu_zlib_CPPFLAGS@
rsxgltest_elf_CFLAGS = -std=gnu99 -fgnu89-inline -O3
rsxgltest_elf_LDFLAGS = $(PSL1GHT_LDFLAGS) -L$(top_builddir)/src/library @ppu_libpng_LDFLAGS@ @ppu_zlib_LDFLAGS@
rsxgltest_elf_LDADD = $(objects) -lEGL -lGL -lrsx -lgcm_sys -lnet -lio -lsysutil -lsysmodule -lrt -llv2 @ppu_libpng_LIBS@ @ppu_zlib_LIBS@

TITLE           :=      RSXGL Test
APPID           :=      TSTRSXGL0
CONTENTID       :=      UP0001-$(APPID)_00-0000000000000000
PKGFILES        :=      release
override ICON0		:=	$(srcdir)/../brewcrab.png

pkg: rsxgltest.pkg

all-local: pkg

clean-local:
	rm -rf pkg rsxgltest.self rsxgltest.pkg 
