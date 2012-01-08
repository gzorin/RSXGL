AC_DEFUN([AC_EXTPKG],[

AC_REQUIRE([PKG_PROG_PKG_CONFIG])dnl
AC_ARG_VAR([$1][_CPPFLAGS],[C preprocessor flags for $1, overriding pkg-config])dnl
AC_ARG_VAR([$1][_LDFLAGS],[linker flags for $1, overriding pkg-config])dnl
AC_ARG_VAR([$1][_LIBS],[library flags for $1, overriding pkg-config])dnl

if test -n "$3"; then
   tmp_PKG_CONFIG_PATH="$3"
else
   tmp_PKG_CONFIG_PATH="${PKG_CONFIG_PATH}"
fi

pkg_failed=no
AC_MSG_CHECKING([for $1 (PKG_CONFIG_PATH is "${tmp_PKG_CONFIG_PATH}")])

save_PKG_CONFIG_PATH="${PKG_CONFIG_PATH}";
export PKG_CONFIG_PATH="${tmp_PKG_CONFIG_PATH}"
_PKG_CONFIG([$1][_CPPFLAGS],[cflags],[$2])
_PKG_CONFIG([$1][_LDFLAGS],[libs-only-L],[$2])
_PKG_CONFIG([$1][_LIBS],[libs-only-l],[$2])
export PKG_CONFIG_PATH="${save_PKG_CONFIG_PATH}"

m4_define([_PKG_TEXT], [Alternatively, you may set the environment variables $1[]_CPPFLAGS
and $1[]_LIBS to avoid the need to call pkg-config.
See the pkg-config man page for more details.])

if test $pkg_failed = yes; then
   	AC_MSG_RESULT([no])
        _PKG_SHORT_ERRORS_SUPPORTED
        if test $_pkg_short_errors_supported = yes; then
	        $1[]_PKG_ERRORS=`PKG_CONFIG_PATH="${tmp_PKG_CONFIG_PATH}" $PKG_CONFIG --short-errors --print-errors --cflags --libs "$2" 2>&1`
        else 
	        $1[]_PKG_ERRORS=`PKG_CONFIG_PATH="${tmp_PKG_CONFIG_PATH}" $PKG_CONFIG --print-errors --cflags --libs "$2" 2>&1`
        fi
	# Put the nasty error message in config.log where it belongs
	echo "$$1[]_PKG_ERRORS" >&AS_MESSAGE_LOG_FD

	m4_default([$4], [AC_MSG_ERROR(
[Package requirements ($2) were not met:

$$1_PKG_ERRORS

Consider adjusting the PKG_CONFIG_PATH environment variable if you
installed software in a non-standard prefix.

_PKG_TEXT])[]dnl
        ])
elif test $pkg_failed = untried; then
     	AC_MSG_RESULT([no])
	m4_default([$4], [AC_MSG_FAILURE(
[The pkg-config script could not be found or is too old.  Make sure it
is in your PATH or set the PKG_CONFIG environment variable to the full
path to pkg-config.

_PKG_TEXT

To get pkg-config, see <http://pkg-config.freedesktop.org/>.])[]dnl
        ])
else
	$1[]_CPPFLAGS=$pkg_cv_[]$1[]_CPPFLAGS
	$1[]_LDFLAGS=$pkg_cv_[]$1[]_LDFLAGS
	$1[]_LIBS=$pkg_cv_[]$1[]_LIBS
        AC_MSG_RESULT([yes])
	AC_SUBST($1[]_CPPFLAGS)
	AC_SUBST($1[]_LDFLAGS)
	AC_SUBST($1[]_LIBS)
fi[]dnl

])
