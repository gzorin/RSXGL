AC_DEFUN([AC_PSL1GHT],[

# Where is PSL1GHT?
AC_ARG_WITH([psl1ght],AS_HELP_STRING([--with-psl1ght],[location of compiled PSL1GHT SDK]),[PSL1GHT="$withval"],[])
AC_ARG_VAR([PSL1GHT],[location of compiled PSL1GHT SDK (default is ${PS3DEV}/libpsl1ght)])
AC_ARG_VAR([PSL1GHT_PATH],[location of PSL1GHT SDK utility programs (e.g., make_self_npdrm; default is ${PS3DEV}/bin)])

PSL1GHT=${PSL1GHT:-"${PS3DEV}/libpsl1ght"}
PSL1GHT_PATH=${PSL1GHT_PATH:-"${PS3DEV}/bin"}

if test -z "${PSL1GHT}" -o "${PSL1GHT}" == "no"; then
AC_MSG_ERROR([location of PSL1GHT SDK is unspecified (via either PSL1GHT environment variable, or --with-psl1ght configure option)])
else
AC_MSG_NOTICE([location of PSL1GHT SDK is "${PSL1GHT}"])
fi

PSL1GHT_CPPFLAGS="-I\${PSL1GHT}/ppu/include"
PSL1GHT_LDFLAGS="-L\${PSL1GHT}/ppu/lib"

AC_SUBST([PSL1GHT])
AC_SUBST([PSL1GHT_CPPFLAGS])
AC_SUBST([PSL1GHT_LDFLAGS])
])

AC_DEFUN([AC_PSL1GHT_PATH_PROGS],[
# Find various PSL1GHT tools:
AC_MSG_NOTICE([looking for PSL1GHT utility programs (PSL1GHT_PATH is "${PSL1GHT_PATH}")])
save_PATH="${PATH}"; PATH="${PSL1GHT_PATH}:${PATH}"
AC_PATH_PROG([SELF],[fself.py])
AC_PATH_PROG([SELF_NPDRM],[make_self_npdrm])
AC_PATH_PROG([SFO],[sfo.py])
AC_PATH_PROG([PKG],[pkg.py])
AC_PATH_PROG([SPRX],[sprxlinker])
PATH="${save_PATH}"
])

AC_DEFUN([AC_PSL1GHT_CHECK_HEADERS],[
# Check for PSL1GHT's rsx/gcm_sys.h:
AC_TOOLCHAIN_PUSH([ppu])
CPPFLAGS="-I${PSL1GHT}/ppu/include ${CPPFLAGS}"
AC_CHECK_HEADER([rsx/gcm_sys.h],[],[AC_MSG_ERROR([cannot include PSL1GHT's <rsx/gcm_sys.h> (PSL1GHT==\"${PSL1GHT}\")])])
AC_TOOLCHAIN_POP([ppu])
])
