AC_INIT([rsxgl],[0.1])
AC_CONFIG_SRCDIR([src/library/state.cc])
AC_CONFIG_AUX_DIR([config])
AC_CONFIG_MACRO_DIR([m4])
AM_INIT_AUTOMAKE([foreign])
AC_CONFIG_FILES(
	include/config.h
	Makefile
	include/Makefile
	src/cgcomp/Makefile
	src/library/Makefile
	src/library/rsxgl_config.h
	src/library/GL3/rsxgl3ext.h
	src/library/GL3/rsxgl_compatibility.h
	src/samples/Makefile
	src/samples/rsxgltest/Makefile
	src/samples/rsxglgears/Makefile
	)

AC_PROG_CC
AC_PROG_CXX
AC_PROG_RANLIB

AC_LANG([C])
AC_LANG([C++])

AC_ARG_ENABLE([ppu-debug],AS_HELP_STRING([--enable-ppu-debug],[emit debugging information in PPU object code]),[ppu_debug=$enableval],[ppu_debug=0])
AC_ARG_ENABLE([ppu-optimize],AS_HELP_STRING([--enable-ppu-optimize],[turn on optimizations for PPU object code (-O3)]),[ppu_optimize=$enableval],[ppu_optimize=yes])

PPU_CPPFLAGS=""
PPU_C_CXXFLAGS=""
if test "$ppu_debug" == "yes"; then
   PPU_C_CXX_FLAGS="${PPU_C_CXXFLAGS} -g -O0";
else
   PPU_CPPFLAGS="${PPU_CPPFLAGS} -DNDEBUG";
fi;

if test "$ppu_optimize" == "yes"; then
   PPU_C_CXXFLAGS="${PPU_C_CXXFLAGS} -O3";
fi;

PPU_CFLAGS="${PPU_C_CXXFLAGS}"
PPU_CXXFLAGS="${PPU_C_CXXFLAGS}"

AC_SUBST([PPU_CPPFLAGS])
AC_SUBST([PPU_CFLAGS])
AC_SUBST([PPU_CXXFLAGS])

# Where is the PS3DEV toolchain?
AC_ARG_WITH([ps3dev],AS_HELP_STRING([--with-ps3dev],[location of compiled PS3 development toolchain]),[PS3DEV=$withval],[])
AC_MSG_NOTICE([PS3DEV is set to ${PS3DEV}])

PS3DEV=${PS3DEV:-"/usr/local/ps3dev"}
PS3DEV_PPU_CPPFLAGS="-I\${PS3DEV}/ppu/include"
PS3DEV_PPU_LDFLAGS="-L\${PS3DEV}/ppu/lib"

AC_SUBST([PS3DEV])
AC_SUBST([PS3DEV_PPU_CPPFLAGS])
AC_SUBST([PS3DEV_PPU_LDFLAGS])

PPU_CC=${PPU_CC:-"${PS3DEV}/ppu/bin/ppu-gcc"}
PPU_CXX=${PPU_CXX:-"${PS3DEV}/ppu/bin/ppu-g++"}
PPU_AR=${PPU_AR:-"${PS3DEV}/ppu/bin/ppu-ar"}
PPU_RANLIB=${PPU_RANLIB:-"${PS3DEV}/ppu/bin/ppu-ranlib"}
PPU_LD=${PPU_LD:-"${PS3DEV}/ppu/bin/ppu-ld"}

AC_SUBST([PPU_CC])
AC_SUBST([PPU_CXX])
AC_SUBST([PPU_AR])
AC_SUBST([PPU_RANLIB])
AC_SUBST([PPU_LD])

# Path to the static C++ standard library, so that it can be added to libGL itself, and client programs written in C won't
# need to link with g++.
AC_ARG_WITH([ppu-cxxlib],AS_HELP_STRING([--with-ppu-cxxlib],[full path to a static C++ standard library]),[PPU_CXXLIB=$withval],[PPU_CXXLIB=""])

# It wasn't specified - try to find it by querying g++ itself.
if test -z "${PPU_CXXLIB}"; then
   PPU_CXXLIB=`${PPU_CXX} -print-file-name=libstdc++.a`
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

# Configure capabilities of the library itself:
RSXGL_CONFIG_RSX_compatibility=0
AC_ARG_ENABLE([RSX-compatibility],AS_HELP_STRING([--enable-RSX-compatibility],[configure the library to support (minimal) compatibility with older OpenGL profiles]),[if test "$enableval" == "yes"; then RSXGL_CONFIG_RSX_compatibility=1; fi],[])
AC_SUBST([RSXGL_CONFIG_RSX_compatibility])

# Find the Cg compiler:
AC_PATH_PROG([CGC],[cgc])

AC_MSG_NOTICE([path to the cgc compiler is ${CGC}])

# Where is PSL1GHT?
AC_ARG_WITH([psl1ght],AS_HELP_STRING([--with-psl1ght],[location of compiled PSL1GHT]),[PSL1GHT=$withval],[])

PSL1GHT=${PSL1GHT:-"${PS3DEV}"}
PSL1GHT_PPU_CPPFLAGS="-I\${PSL1GHT}/ppu/include"
PSL1GHT_PPU_LDFLAGS="-L\${PSL1GHT}/ppu/lib"

AC_MSG_NOTICE([location of PSL1GHT is ${PSL1GHT}])

AC_SUBST([PSL1GHT])
AC_SUBST([PSL1GHT_PPU_CPPFLAGS])
AC_SUBST([PSL1GHT_PPU_LDFLAGS])

# Where are the ported libraries?
AC_ARG_WITH([portlibs],AS_HELP_STRING([--with-portlibs],[location of ported libraries]),[PORTLIBS=$withval],[])

PORTLIBS=${PORTLIBS:-"${PS3DEV}/portlibs"}
PORTLIBS_PPU_CPPFLAGS="-I\${PORTLIBS}/ppu/include"
PORTLIBS_PPU_LDFLAGS="-L\${PORTLIBS}/ppu/lib"

AC_MSG_NOTICE([PORTLIBS is set to ${PORTLIBS}])

AC_SUBST([PORTLIBS])
AC_SUBST([PORTLIBS_PPU_CPPFLAGS])
AC_SUBST([PORTLIBS_PPU_LDFLAGS])

AC_OUTPUT
