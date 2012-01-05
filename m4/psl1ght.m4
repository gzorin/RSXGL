AC_DEFUN([AC_PSL1GHT],[

# Where is PSL1GHT?
AC_ARG_WITH([psl1ght],AS_HELP_STRING([--with-psl1ght],[location of compiled PSL1GHT]),[PSL1GHT=$withval],[])

PSL1GHT=${PSL1GHT:-"${PS3DEV}/libpsl1ght"}

# Check for PSL1GHT's rsx/gcm_sys.h:
AC_TOOLCHAIN_PUSH([host])
CPPFLAGS="-I${PSL1GHT}/ppu/include ${CPPFLAGS}"
AC_CHECK_HEADER([rsx/gcm_sys.h],[],[AC_MSG_ERROR([cannot include PSL1GHT's <rsx/gcm_sys.h> (PSL1GHT==\"${PSL1GHT}\")])])
AC_TOOLCHAIN_POP([host])

AC_MSG_NOTICE([location of PSL1GHT is ${PSL1GHT}])
PSL1GHT_CPPFLAGS="-I\${PSL1GHT}/ppu/include"
PSL1GHT_LDFLAGS="-L\${PSL1GHT}/ppu/lib"

AC_SUBST([PSL1GHT])
AC_SUBST([PSL1GHT_CPPFLAGS])
AC_SUBST([PSL1GHT_LDFLAGS])

# Find various PSL1GHT tools:
save_PATH="${PATH}"; PATH="${PSL1GHT}/bin:${PATH}"
AC_PATH_PROG([SELF],[fself.py])
AC_PATH_PROG([SELF_NPDRM],[make_self_npdrm])
AC_PATH_PROG([SFO],[sfo.py])
AC_PATH_PROG([PKG],[pkg.py])
AC_PATH_PROG([SPRX],[sprxlinker])
PATH="${save_PATH}"

])
