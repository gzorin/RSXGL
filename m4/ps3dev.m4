AC_DEFUN([AC_PS3DEV],[

# Where is the PS3DEV toolchain?
AC_ARG_WITH([ps3dev],AS_HELP_STRING([--with-ps3dev],[location of PS3 homebrew development environment]),[PS3DEV="$withval"],[])
AC_ARG_VAR([PS3DEV],[location of PS3 homebrew development environment])
export PS3DEV

if test -z "${PS3DEV}" -o "${PS3DEV}" == "no"; then
AC_MSG_ERROR([location of PS3 homebrew development environment is unspecified (via either PS3DEV environment variable, or --with-ps3dev configure option)])
else
AC_MSG_NOTICE([location of PS3 homebrew development environment is "${PS3DEV}"])
fi

AC_SUBST([PS3DEV])
])
