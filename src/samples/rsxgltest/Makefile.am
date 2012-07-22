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
	fbo_inner_vert.h fbo_inner_frag.h \
	fbo_outer_vert.h fbo_outer_frag.h \
	occlusion_vpo.h occlusion_fpo.h \
	gradient_png.h \
	texcube_vert.h texcube_frag.h \
	points_vert.h points_frag.h \
	cube_vert.h cube_frag.h
CLEANFILES = draw_vpo.h draw_fpo.h draw_vpo.o draw_fpo.o \
	textures_vpo.h textures_fpo.h textures_vpo.o textures_fpo.o \
	manypoints_vpo.h manypoints_fpo.h manypoints_vpo.o manypoints_fpo.o \
	points_vpo.h points_fpo.h points_vpo.o points_fpo.o \
	cube_vpo.h cube_fpo.h cube_vpo.o cube_fpo.o \
	texcube_vpo.h texcube_fpo.h texcube_vpo.o texcube_fpo.o \
	fbo_vpo.h fbo_fpo.h fbo_vpo.o fbo_fpo.o \
	fbo_inner_vert.h fbo_inner_frag.h \
	fbo_outer_vert.h fbo_outer_frag.h \
	dice_bin.o dice_bin.h \
	nagel_bin.o nagel_bin.h \
	face_png.o face_png.h \
	gradient_png.o gradient_png.h \
	texcube_vert.h texcube_frag.h \
	points_vert.h points_frag.h \
	cube_vert.h cube_frag.h

# clear.c viewport_scissor.c buffer.c program.c draw.c uniforms.c textures.c cube.cc
clear_objects = 
clear_sources = clear.c

cube_objects = cube_vpo.o cube_fpo.o
cube_sources = cube.cc cube.vcg cube.fcg

textures_objects = textures_vpo.o textures_fpo.o dice_bin.o
textures_sources = textures.c texture.c textures.vert textures.frag dice.bin

manypoints_objects = face_png.o
manypoints_sources = manypoints.cc texture.c points.vert points.frag face.png

manycubes_objects = 
manycubes_sources = manycubes.cc cube.vert cube.frag

texcube_objects = nagel_bin.o gradient_png.o face_png.o
texcube_sources = texcube.cc texture.c texcube.vert texcube.frag nagel.bin face.png gradient.png

fbo_objects = nagel_bin.o
fbo_sources = fbo.cc texture.c fbo_inner.vert fbo_inner.frag nagel.bin

fbomrt_objects = nagel_bin.o
fbomrt_sources = fbomrt.cc texture.c fbo_inner.vert fbo_inner.frag fbo_outer.vert fbo_outer.frag nagel.bin

occlusion_objects = occlusion_vpo.o occlusion_fpo.o
occlusion_sources = occlusion.cc occlusion.vert occlusion.frag

occlusion2_objects = occlusion_vpo.o occlusion_fpo.o
occlusion2_sources = occlusion2.cc occlusion.vert occlusion.frag

copyteximage_objects = nagel_bin.o
copyteximage_sources = copyteximage.cc texture.c fbo_inner.vert fbo_inner.frag nagel.bin

feedback_objects = nagel_bin.o gradient_png.o face_png.o
feedback_sources = feedback.cc texture.c texcube.vert texcube.frag nagel.bin face.png gradient.png

objects = $(manypoints_objects)
sources = $(manypoints_sources)

rsxgltest_elf_DEPENDENCIES = $(top_builddir)/src/library/libEGL.a $(top_builddir)/src/library/libGL.a	\
	$(objects)
rsxgltest_elf_SOURCES = main.c			\
	math3d.cc sine_wave.c			\
	$(sources)

rsxgltest_elf_CPPFLAGS = -D__RSX__ -I\$(top_srcdir)/include $(eigen_CPPFLAGS) $(PSL1GHT_CPPFLAGS) -I$(top_srcdir)/include -I$(top_srcdir)/src/library -I$(top_builddir)/src/library @ppu_libpng_CPPFLAGS@ @ppu_zlib_CPPFLAGS@
rsxgltest_elf_CFLAGS = -std=gnu99 -fgnu89-inline -O3
rsxgltest_elf_LDFLAGS = -L$(top_builddir)/src/library $(PSL1GHT_LDFLAGS) @ppu_libpng_LDFLAGS@ @ppu_zlib_LDFLAGS@
rsxgltest_elf_LDADD = $(objects) -lGL -lEGL -lrsx -lgcm_sys -lnet -lio -lsysutil -lsysmodule -lrt -llv2 @ppu_libpng_LIBS@ @ppu_zlib_LIBS@

TITLE           :=      RSXGL Test
APPID           :=      TSTRSXGL0
CONTENTID       :=      UP0001-$(APPID)_00-0000000000000000
PKGFILES        :=      release
override ICON0		:=	$(srcdir)/../brewcrab.png

pkg: rsxgltest.pkg

all-local: pkg

clean-local:
	rm -rf pkg rsxgltest.self rsxgltest.pkg 
