AC_DEFUN([AC_PS3DEV],[
# Where is the PS3DEV toolchain?
AC_ARG_VAR([PS3DEV],[location of compiled PS3 development toolchain])
PS3DEV=${PS3DEV:-"/usr/local/ps3dev"}
AC_SUBST([PS3DEV])
])
