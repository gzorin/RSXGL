AC_INIT([rsxgl],[0.1])
AC_CONFIG_SRCDIR([src/library/state.cc])
AC_CONFIG_AUX_DIR([config])
AC_CONFIG_MACRO_DIR([m4])
AM_INIT_AUTOMAKE([foreign])
AC_CONFIG_FILES(
	include/config.h
	Makefile
	src/mesa/configs/rsx
	include/Makefile
	src/cgcomp/Makefile
	src/cgcomp/nv40c
	src/drm/Makefile
	src/nvfxc/Makefile
	src/library/Makefile
	src/library/rsxgl_config.h
	src/library/GL3/rsxgl3ext.h
	src/library/GL3/rsxgl_compatibility.h
	)

# Which subdirectories get built:
RSXGL_SUBDIRS="extsrc/mesa include src/cgcomp src/drm src/nvfxc src/library"

# Determine which samples get built:
RSXGL_SAMPLES="rsxgltest rsxglgears"

AC_SUBST([RSXGL_SUBDIRS])
AC_SUBST([RSXGL_SAMPLES])

AC_PROG_CC
AC_PROG_CXX
AC_PROG_RANLIB
AC_PATH_PROG([XARGS],[xargs])
AC_PATH_PROG([PWDCMD],[pwd])
AC_PATH_PROG([RSYNC],[rsync])
AC_PATH_PROG([PATCH],[patch])

AC_LANG([C])
AC_LANG([C++])

# Get the PS3DEV environment variable:
AC_PS3DEV
AC_PREFIX_DEFAULT([${PS3DEV}/ppu])

# Find psl1ght:
AC_PSL1GHT

# PPU tools:
AC_TOOLCHAIN([ppu],[ppu],[${PS3DEV}/ppu/bin],[${PS3DEV}/ppu/lib/pkgconfig])

if test "${prefix}" == "NONE"; then
default_ppuprefix="${PS3DEV}/ppu"
else
default_ppuprefix="${prefix}/ppu"
fi

AC_TOOLCHAIN_PREFIX([ppu],[${default_ppuprefix}])
AC_TOOLCHAIN_PROG_CC([ppu])
AC_TOOLCHAIN_PROG_CXX([ppu])
AC_TOOLCHAIN_PATH_TOOL([ppu],[AR],[ar])
AC_TOOLCHAIN_PATH_TOOL([ppu],[RANLIB],[ranlib])
AC_TOOLCHAIN_PATH_TOOL([ppu],[AS],[as])
AC_TOOLCHAIN_PATH_TOOL([ppu],[STRIP],[strip])

AC_PSL1GHT_PATH_PROGS
AC_PSL1GHT_CHECK_HEADERS

# Path to the static C++ standard library, so that it can be added to libGL itself, and client programs written in C won't
# need to link by using g++.
AC_ARG_WITH([ppu-cxxlib],AS_HELP_STRING([--with-ppu-cxxlib],[full path to a static C++ standard library]),[PPU_CXXLIB=$withval],[PPU_CXXLIB=""])

# It wasn't specified - try to find it by querying g++ itself.
if test -z "${PPU_CXXLIB}"; then
   PPU_CXXLIB=`${ppu_CXX} -print-file-name=libstdc++.a`
elif test "${PPU_CXXLIB}" == "no"; then
   PPU_CXXLIB=""
fi;

if test -n "${PPU_CXXLIB}"; then
   if test -r "${PPU_CXXLIB}"; then
      AC_MSG_NOTICE([PPU_CXXLIB is set to ${PPU_CXXLIB}])
   else
      AC_MSG_NOTICE([PPU_CXXLIB is set to ${PPU_CXXLIB} but it isn't readable; client programs will need to link to libstdc++ (usually by simply using g++ as the program linker)])
      PPU_CXXLIB=""
   fi;
else
   AC_MSG_NOTICE([PPU_CXXLIB is unset; client programs will need to link to libstdc++ (usually by simply using g++ as the program linker)])
fi;

AC_SUBST([PPU_CXXLIB])

# Should the sample programs be built?
test -x "${SELF}" && test -x "${SELF_NPDRM}" && test -x "${SFO}" && test -x "${PKG}" && test -x "${SPRX}" && RSXGL_samples_possible="1";

if ! test "${RSXGL_samples_possible}" == "1"; then
AC_MSG_WARN([cannot find one or more of the PSL1GHT SDK programs (e.g., make_self_npdrm), samples cannot be built])
fi

AC_ARG_ENABLE([samples],AS_HELP_STRING([--disable-samples],[do not build the OpenGL sample programs]),[if test "$enableval" == "yes"; then RSXGL_samples=$RSXGL_samples_possible; else RSXGL_samples=0; fi],[RSXGL_samples=$RSXGL_samples_possible])
AM_CONDITIONAL([RSXGL_samples],[ test "$RSXGL_samples" == "1" ])

AM_COND_IF([RSXGL_samples],[
	AC_CONFIG_FILES([
	src/samples/Makefile
	src/samples/rsxgltest/Makefile
	src/samples/rsxglgears/Makefile
	])
])

if test "$RSXGL_samples" == "1"; then
   RSXGL_SUBDIRS="${RSXGL_SUBDIRS} src/samples"
fi

# Configure capabilities of the library:
RSXGL_CONFIG_RSX_compatibility=0
AC_ARG_ENABLE([RSX-compatibility],AS_HELP_STRING([--enable-RSX-compatibility],[configure the library to support (minimal) compatibility with older OpenGL profiles]),[if test "$enableval" == "yes"; then RSXGL_CONFIG_RSX_compatibility=1; fi],[])
AC_SUBST([RSXGL_CONFIG_RSX_compatibility])

# Samples can send debugging information back to the host used to build them; set its IP here,
# or leave it unset & it won't try to phone home:
AC_ARG_VAR([RSXGL_CONFIG_samples_host_ip],[IP address of host for samples to send reporting to])
AC_ARG_VAR([RSXGL_CONFIG_samples_host_port],[port number on host for samples to send reporting to (default is 9000)])

if test -z "${RSXGL_CONFIG_samples_host_ip}"; then
RSXGL_CONFIG_samples_host_port=0
elif test -z "${RSXGL_CONFIG_samples_host_port}"; then
RSXGL_CONFIG_samples_host_port=9000
fi

AC_SUBST([RSXGL_CONFIG_samples_host_ip])
AC_SUBST([RSXGL_CONFIG_samples_host_port])

# Set the default value of NV40ASM, passed to nv40c:
NV40ASM="\${bindir}/nv40asm"
AC_SUBST([NV40ASM])

# Find the Cg compiler:
AC_PATH_PROG([CGC],[cgc])
AC_MSG_NOTICE([path to the cgc compiler is ${CGC}])

# Find lftp
AC_PATH_PROG([LFTP],[lftp])
AC_MSG_NOTICE([path to the lftp utility is ${LFTP}])

# Look for libraries normally stored under "${PS3DEV}/portlibs"
AC_ARG_VAR([ppu_portlibs_PKG_CONFIG_PATH],[pkg-config search paths for PS3 ported libraries (default is "${PS3DEV}/portlibs/ppu/lib/pkgconfig")])
ppu_portlibs_PKG_CONFIG_PATH=${ppu_portlibs_PKG_CONFIG_PATH:-"${PS3DEV}/portlibs/ppu/lib/pkgconfig"}
AC_MSG_NOTICE([pkg-config search paths for PPU ported libraries are "${ppu_portlibs_PKG_CONFIG_PATH}"])

# Look for libpng:
AC_TOOLCHAIN_EXTPKG([ppu],[libpng],[libpng],[${ppu_portlibs_PKG_CONFIG_PATH}])

# Look for zlib:
AC_TOOLCHAIN_EXTPKG([ppu],[zlib],[zlib],[${ppu_portlibs_PKG_CONFIG_PATH}])

# Deal with mesa:
AC_ARG_VAR([MESA_LOCATION],[location of Mesa sources])
AC_ARG_VAR([LIBDRM_LOCATION],[location of the libdrm sources])

AC_CONFIG_COMMANDS([mesa],
[	mesa_builddir="${_abs_top_builddir}/extsrc/mesa";
	if ! test -a "$mesa_builddir"; then
	   mkdir -p "$mesa_builddir";
	fi

	mesa_srcdir=$(cd "$MESA_LOCATION"; ${PWDCMD});
	mesa_builddir=$(cd "$mesa_builddir"; ${PWDCMD});

	if test "$mesa_srcdir" != "$mesa_builddir"; then
	   echo "Copying Mesa files from source directory \"$mesa_srcdir\" to build directory \"$mesa_builddir\"...";
	   #cat "$_abs_top_srcdir/extsrc/mesa/rsxgl/files" | xargs -I % echo "file=\"%\"; dir=\$(dirname \$file); mkdir -p \"$mesa_builddir/\$dir\"; cp -a \"$mesa_srcdir/\$file\" \"$mesa_builddir/\$file\";" | sh;
	   "$RSYNC" -av --files-from="$_abs_top_srcdir/src/mesa/files" "$mesa_srcdir" "$mesa_builddir";
	fi;

	cp "$_abs_top_builddir/src/mesa/configs/rsx" "$mesa_builddir/configs/current";
	cp "$_abs_top_srcdir/src/mesa/Makefile-builtins" "$mesa_builddir/src/glsl/";
],
[       MESA_LOCATION="$MESA_LOCATION";
	_abs_top_builddir=$(cd "${builddir}"; ${PWDCMD});
	_abs_top_srcdir=$(cd "${srcdir}"; ${PWDCMD});
	XARGS="${XARGS}"
	PWDCMD="${PWDCMD}"
	RSYNC="${RSYNC}"
])

AC_OUTPUT
