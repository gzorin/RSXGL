include $(srcdir)/../samples.am

eigen_LOCATION = $(top_srcdir)/extsrc/eigen
eigen_CPPFLAGS = -I$(eigen_LOCATION)

noinst_PROGRAMS = rsxglgears.elf

BUILT_SOURCES = rsxglgears_vpo.h rsxglgears_fpo.h
CLEANFILES = rsxglgears_vpo.h rsxglgears_fpo.h rsxglgears_vpo.o rsxglgears_fpo.o

sources = rsxglglears.c
objects = rsxglgears_vpo.o rsxglgears_fpo.o

rsxglgears_elf_DEPENDENCIES = $(top_builddir)/src/library/libEGL.a $(top_builddir)/src/library/libGL.a \
	$(objects)
rsxglgears_elf_SOURCES = ../rsxgltest/main.c	\
	../rsxgltest/math3d.cc			\
	rsxglgears.c rsxglgears.vert rsxglgears.frag

rsxglgears_elf_CPPFLAGS = -D__RSX__ -I\$(top_srcdir)/include $(eigen_CPPFLAGS) $(PSL1GHT_CPPFLAGS) -I$(top_srcdir)/include -I$(top_srcdir)/src/library -I$(top_builddir)/src/library -I$(top_srcdir)/src/samples/rsxgltest
rsxglgears_elf_CFLAGS = -std=gnu99 -fgnu89-inline -O3
rsxglgears_elf_LDFLAGS = -L$(top_builddir)/src/library $(PSL1GHT_LDFLAGS)
rsxglgears_elf_LDADD = $(objects) -lGL -lEGL -lrsx -lgcm_sys -lnet -lio -lsysutil -lsysmodule -lrt -llv2

TITLE           :=      RSXGLGEARS
APPID           :=      TSTRSXGL1
CONTENTID       :=      UP0001-$(APPID)_00-0000000000000000
PKGFILES        :=      release
override ICON0		:=	$(srcdir)/../brewcrab.png

pkg: rsxglgears.pkg

all-local: pkg

clean-local:
	rm -rf pkg rsxglgears.self rsxglgears.pkg 
