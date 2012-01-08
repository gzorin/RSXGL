AC_DEFUN([AC_PS3DEV],[

# Where is the PS3DEV toolchain?
AC_ARG_WITH([ps3dev],AS_HELP_STRING([--with-ps3dev],[location of PS3 homebrew development environment]),[PS3DEV="$withval"],[])
AC_ARG_VAR([PS3DEV],[location of PS3 homebrew development environment])
export PS3DEV

PS3DEV=${PS3DEV:-"/usr/local/ps3dev"}

AC_MSG_NOTICE([location of PS3 homebrew development environment is "${PS3DEV}"])

AC_SUBST([PS3DEV])
])
