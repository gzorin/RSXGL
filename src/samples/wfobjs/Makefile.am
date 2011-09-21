CC = @PPU_CC@
CXX = @PPU_CXX@
AR = @PPU_AR@
RANLIB = @PPU_RANLIB@
LD = @PPU_LD@

NV40ASM = $(top_builddir)/src/cgcomp/nv40asm

eigen_LOCATION = $(top_srcdir)/extsrc/eigen
eigen_CPPFLAGS = -I$(eigen_LOCATION)

include $(srcdir)/../samples.am

noinst_PROGRAMS = wfobjs.elf
noinst_LIBRARIES = libobjTester.a

objTester_srcdir = $(top_srcdir)/extsrc/objTester/objTester
libobjTester_a_SOURCES = $(objTester_srcdir)/objLoader.cpp obj_parser.cpp $(objTester_srcdir)/list.cpp $(objTester_srcdir)/string_extra.cpp
libobjTester_a_CPPFLAGS = -I$(objTester_srcdir)

BUILT_SOURCES = wfobjs_vpo.h wfobjs_fpo.h \
	cornell_box_obj_bin.h cornell_box_mtl_bin.h \
	crab_obj_bin.h
CLEANFILES = wfobjs_vpo.h wfobjs_fpo.h wfobjs_vpo.o wfobjs_fpo.o

sources = wfobjs.cc \
	cornell_box_obj.bin cornell_box_mtl.bin \
	crab_obj.bin
objects = wfobjs_vpo.o wfobjs_fpo.o \
	cornell_box_obj_bin.o cornell_box_mtl_bin.o \
	crab_obj_bin.o

wfobjs_elf_DEPENDENCIES = libobjTester.a $(top_builddir)/src/library/libEGL.a $(top_builddir)/src/library/libGL.a \
	$(objects)
wfobjs_elf_SOURCES = ../rsxgltest/main.c	\
	../rsxgltest/math3d.cc ../rsxgltest/sine_wave.c			\
	$(sources)

wfobjs_elf_CPPFLAGS = -D__RSX__ -I\$(top_srcdir)/include $(eigen_CPPFLAGS) $(PS3DEV_PPU_CPPFLAGS) $(PSL1GHT_PPU_CPPFLAGS) $(PORTLIBS_PPU_CPPFLAGS) -I$(top_srcdir)/include -I$(top_srcdir)/src/library -I$(top_builddir)/src/library -I$(top_srcdir)/src/samples/rsxgltest -I$(objTester_srcdir)
wfobjs_elf_CFLAGS = -std=gnu99 -fgnu89-inline -O3
wfobjs_elf_LDFLAGS = $(PS3DEV_PPU_LDFLAGS) $(PSL1GHT_PPU_LDFLAGS) $(PORTLIBS_PPU_LDFLAGS) -L$(top_builddir)/src/library
wfobjs_elf_LDADD = $(objects) libobjTester.a -lEGL -lGL -lrsx -lgcm_sys -lnet -lio -lsysutil -lsysmodule -lpng -lz -lstdc++ -lm -lrt -llv2

TITLE           :=      WFOBJS
APPID           :=      TSTRSXGL2
CONTENTID       :=      UP0001-$(APPID)_00-0000000000000000
PKGFILES        :=      release
override ICON0		:=	$(srcdir)/../brewcrab.png

pkg: wfobjs.pkg

all-local: pkg

clean-local:
	rm -rf pkg wfobjs.self wfobjs.pkg 
