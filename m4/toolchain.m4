#-*-autoconf-*-
AC_DEFUN_ONCE([_AC_TOOLCHAIN_STACK],[

function create_stack ()
{
    name=${1}
    declare -a ${name};
}

function stack_height ()
{
    name=${1}
    eval echo \${#${name}[*]}
}

function push_stack ()
{
    name=${1}
    height=$(stack_height ${name})
    eval ${name}[${height}]=\"${2}\"
}

function pop_stack ()
{
    name=${1}
    height=$(stack_height ${name})
    eval unset ${name}[[${height}-1]]
}

function stack_top ()
{
    name=${1}
    height=$(stack_height ${name})
    eval echo \${${name}[${height}-1]}
}

# "tag", "variables"
function create_toolchain_stack ()
{
    tag=${1}
    shift

    for varname; do
	create_stack "${tag}_${varname}"
    done
}

# "tag","variable","value"
function push_toolchain_stack ()
{
    tag=${1}
    shift

    while test -n "${1}"; do
	varname=${1}
	oldvalue=$(eval echo \${${varname}})
	shift

	newvalue=${1}
	shift

	push_stack "${tag}_${varname}" "${oldvalue}"
	eval ${varname}=\"${newvalue}\"
    done
}

function pop_toolchain_stack ()
{
    tag=${1}
    shift

    while test -n "${1}"; do
	varname=${1}
	shift

	oldvalue=$(stack_top "${tag}_${varname}")
	eval ${varname}=\"${oldvalue}\"

	pop_stack "${tag}_${varname}"
    done
}

])

## Push language variables: CC, CPP, CXX, CXXCPP, OBJC, OBJCPP, OBJCXXCPP
## CFLAGS, CXXFLAGS, CPPFLAGS, LDFLAGS, LIBS
## So that check headers, etc., work.
#AC_TOOLCHAIN_PUSH(tag)
AC_DEFUN([AC_TOOLCHAIN_PUSH],[
push_toolchain_stack "ac_$1_toolchain_stack" \
		     "PATH" "${$1_PATH}:${PATH}" \
		     "PKG_CONFIG_PATH" "${$1_PKG_CONFIG_PATH}:${PKG_CONFIG_PATH}" \
		     "CPP" "${$1_CPP}" \
		     "CPPFLAGS" "${$1_CPPFLAGS}" \
		     "CC" "${$1_CC}" \
		     "CFLAGS" "${$1_CFLAGS}" \
		     "OBJC" "${$1_OBJC}" \
		     "OBJCPP" "${$1_OBJCPP}" \
		     "CXX" "${$1_CXX}" \
		     "CXXFLAGS" "${$1_CXXFLAGS}" \
		     "OBJCXX" "${$1_OBJCXX}" \
		     "OBJCXXCPP" "${$1_OBJCXXCPP}" \
		     "LDFLAGS" "${$1_LDFLAGS}" \
		     "LIBS" "${$1_LIBS}"
])

#AC_TOOLCHAIN_POP(tag)
AC_DEFUN([AC_TOOLCHAIN_POP],[
pop_toolchain_stack "ac_$1_toolchain_stack" "PATH" "PKG_CONFIG_PATH" "CPP" "CPPFLAGS" "CC" "CFLAGS" "OBJC" "OBJCPP" "CXX" "CXXFLAGS" "OBJCXX" "OBJCXXCPP" "LDFLAGS" "LIBS"
])

AC_DEFUN([AC_TOOLCHAIN],[
AC_REQUIRE([_AC_TOOLCHAIN_STACK])dnl
AC_ARG_VAR([$1_PATH],      [path to use to search for "$1" toolchain programs])dnl
AC_ARG_VAR([$1_PKG_CONFIG_PATH],[path to use to search for packages built for "$1" toolchain])dnl

# Defaults for PATH, PKG_CONFIG_PATH
test -n "$3" && $1_PATH=${$1_PATH:-"$3"}
test -n "$4" && $1_PKG_CONFIG_PATH=${$1_PKG_CONFIG_PATH:-"$4"}

ac_$1_toolchain=yes

if test -n "$2"; then
ac_$1_toolchain_prefix=$2-
else
ac_$1_toolchain_prefix=""
fi

ac_$1_toolchain_path=$3

create_toolchain_stack "ac_$1_toolchain_stack" "PATH" "PKG_CONFIG_PATH" "CPP" "CPPFLAGS" "CC" "CFLAGS" "OBJC" "OBJCPP" "CXX" "CXXFLAGS" "OBJCXX" "OBJCXXCPP" "LDFLAGS" "LIBS"

AC_SUBST([$1_PATH])
AC_SUBST([$1_PKG_CONFIG_PATH])
])

AC_DEFUN([AC_TOOLCHAIN_PREFIX],[
AC_ARG_WITH([$1-prefix],AS_HELP_STRING([--with-$1-prefix=PREFIX],[install products built with the "$1" toolchain in PREFIX]),[$1prefix="${withval}"],[$1prefix="$2"])
AC_MSG_NOTICE([products built with "$1" toolchain will be stored in: ${$1prefix}])

$1_bindir="${$1prefix}/bin"
$1_libdir="${$1prefix}/lib"
$1_includedir="${$1prefix}/include"
$1_libdir="${$1prefix}/lib"
$1_sbindir="${$1prefix}/sbin"
$1_sysconfdir="${$1prefix}/etc"
$1_datarootdir="${$1prefix}/share"
$1_datadir="${$1prefix}/share"
$1_mandir="${$1prefix}/man"

AC_SUBST([$1prefix])
AC_SUBST([$1_bindir])
AC_SUBST([$1_libdir])
AC_SUBST([$1_includedir])
AC_SUBST([$1_libdir])
AC_SUBST([$1_sbindir])
AC_SUBST([$1_sysconfdir])
AC_SUBST([$1_datarootdir])
AC_SUBST([$1_datadir])
AC_SUBST([$1_mandir])
])

#AC_TOOLCHAIN_PATH_TOOL(tag,var,prog,value-if-not-found)
AC_DEFUN([AC_TOOLCHAIN_PATH_TOOL],[
push_toolchain_stack "ac_$1_toolchain_stack" "PATH" "${$1_PATH}:${PATH}"
prog="$3"
AC_PATH_PROG($1_$2,"${ac_$1_toolchain_prefix}${prog}",$4)
pop_toolchain_stack "ac_$1_toolchain_stack" "PATH"
])

#AC_TOOLCHAIN_PATH_TOOLS(tag,var,progs,value-if-not-found)
AC_DEFUN([AC_TOOLCHAIN_PATH_TOOLS],[
push_toolchain_stack "ac_$1_toolchain_stack" "PATH" "${$1_PATH}:${PATH}"
progs=""
for prog in $3; do
    progs="${progs} ${ac_$1_toolchain_prefix}${prog}"
done
AC_PATH_PROGS([$1_$2],${progs},$4)
pop_toolchain_stack "ac_$1_toolchain_stack" "PATH"
])

#AC_TOOLCHAIN_PROG_CC(tag,search-list)
AC_DEFUN([AC_TOOLCHAIN_PROG_CC],
[AC_LANG_PUSH(C)dnl
AC_ARG_VAR([$1_CC],     [C compiler command for "$1" toolchain])dnl
AC_ARG_VAR([$1_CFLAGS], [C compiler flags for "$1" toolchain])dnl
AC_ARG_VAR([$1_LDFLAGS],     [linker flags for "$1" toolchain])dnl
AC_ARG_VAR([$1_LIBS], [libraries to pass to the "$1" toolchain linker])dnl
AC_ARG_VAR([$1_CPPFLAGS],     [preprocessor flags for "$1" toolchain])dnl
m4_ifval([$2],
      [AC_TOOLCHAIN_PATH_TOOLS([$1],CC, [$2])],
[AC_TOOLCHAIN_PATH_TOOL([$1],CC, gcc)
])

test -z "${$1_CC}" && AC_MSG_FAILURE([no acceptable C compiler found in \$PATH])

# Provide some information about the compiler.
_AS_ECHO_LOG([checking for _AC_LANG compiler version])
ac_compiler=${$1_CC}
for ac_option in --version -v -V -qversion; do
  _AC_DO_LIMIT([$ac_compiler $ac_option >&AS_MESSAGE_LOG_FD])
done

AC_TOOLCHAIN_PUSH([$1])

m4_expand_once([_AC_COMPILER_EXEEXT])[]dnl
m4_expand_once([_AC_COMPILER_OBJEXT])[]dnl
_AC_LANG_COMPILER_GNU
if test $ac_compiler_gnu = yes; then
  $1_GCC=yes
else
  $1_GCC=
fi
_AC_PROG_CC_G
_AC_PROG_CC_C89

AC_TOOLCHAIN_POP([$1])

AC_SUBST([$1_CFLAGS])
AC_SUBST([$1_LDFLAGS])
AC_SUBST([$1_LIBS])
AC_SUBST([$1_CPPFLAGS])

AC_LANG_POP(C)dnl
])# AC_PROG_CC

#AC_TOOLCHAIN_PROG_CPP(tag,search-list)
AC_DEFUN([AC_TOOLCHAIN_PROG_CPP],
[AC_REQUIRE([AC_PROG_CC])dnl
AC_ARG_VAR([$1_CPP],      [C preprocessor for "$1" toolchain])dnl
AC_ARG_VAR([$1_CPPFLAGS],      [(Objective) C/C++ preprocessor flags, e.g. -I<include dir>
	     if you have headers in a nonstandard directory <include dir>])dnl
AC_LANG_PUSH(C)dnl
AC_MSG_CHECKING([how to run the C preprocessor])
if test -z "${$1_CPP}"; then
  AC_CACHE_VAL([ac_cv_$1_prog_CPP],
  [dnl
    # Double quotes because CPP needs to be expanded
    for $1_CPP in "${$1_CC} -E" "${$1_CC} -E -traditional-cpp" "/lib/cpp"
    do
      _AC_PROG_PREPROC_WORKS_IFELSE([break])
    done
    ac_cv_$1_prog_CPP=${$1_CPP}
  ])dnl
  $1_CPP=$ac_cv_$1_prog_CPP
else
  ac_cv_$1_prog_CPP=${$1_CPP}
fi
AC_MSG_RESULT([${$1_CPP}])
_AC_PROG_PREPROC_WORKS_IFELSE([],
		[AC_MSG_FAILURE([C preprocessor "${$1_CPP}" fails sanity check])])
AC_SUBST($1_CPP)dnl
AC_SUBST($1_CPPFLAGS)dnl
AC_LANG_POP(C)dnl
])# AC_PROG_CPP

#AC_TOOLCHAIN_PROG_CXX(tag,search-list)
AC_DEFUN([AC_TOOLCHAIN_PROG_CXX],
[AC_LANG_PUSH(C++)dnl
AC_ARG_VAR([$1_CXX],      [C++ compiler command for "$1" toolchain])dnl
AC_ARG_VAR([$1_CXXFLAGS], [C++ compiler flags for "$1" toolchain])dnl
AC_ARG_VAR([$1_LDFLAGS],     [linker flags for "$1" toolchain])dnl
AC_ARG_VAR([$1_LIBS], [libraries to pass to the "$1" toolchain linker])dnl
AC_ARG_VAR([$1_CPPFLAGS],     [preprocessor flags for "$1" toolchain])dnl
_AC_ARG_VAR_PRECIOUS([$1_CCC])dnl
if test -z "${$1_CXX}"; then
  if test -n "${$1_CCC}"; then
    $1_CXX=${$1_CCC}
  else
    AC_TOOLCHAIN_PATH_TOOLS([$1],CXX,
		   [m4_default([$2],
			       [g++ c++ gpp aCC CC cxx cc++ cl.exe FCC KCC RCC xlC_r xlC])],
		   "${ac_$1_toolchain_prefix}g++")
  fi
fi
# Provide some information about the compiler.
_AS_ECHO_LOG([checking for _AC_LANG compiler version])
ac_compiler=${$1_CXX}
for ac_option in --version -v -V -qversion; do
  _AC_DO_LIMIT([$ac_compiler $ac_option >&AS_MESSAGE_LOG_FD])
done

AC_TOOLCHAIN_PUSH([$1])

m4_expand_once([_AC_COMPILER_EXEEXT])[]dnl
m4_expand_once([_AC_COMPILER_OBJEXT])[]dnl
_AC_LANG_COMPILER_GNU
if test $ac_compiler_gnu = yes; then
  GXX=yes
else
  GXX=
fi
_AC_PROG_CXX_G

AC_TOOLCHAIN_POP([$1])

AC_SUBST([$1_CXXFLAGS])
AC_SUBST([$1_LDFLAGS])
AC_SUBST([$1_LIBS])
AC_SUBST([$1_CPPFLAGS])

AC_LANG_POP(C++)dnl
])# AC_PROG_CXX

#AC_TOOLCHAIN_PROG_CXXCPP(tag,search-list)
#AC_TOOLCHAIN_PROG_OBJC(tag,search-list)
#AC_TOOLCHAIN_PROG_OBJCPP(tag,search-list)
#AC_TOOLCHAIN_PROG_OBJCXX(tag,search-list)
#AC_TOOLCHAIN_PROG_OBJCXXCPP(tag,search-list)

#AC_TOOLCHAIN_EXTPKG(toolchain-tag,variable-prefix,modules)
AC_DEFUN([AC_TOOLCHAIN_EXTPKG],[
save_PKG_CONFIG_PATH="${PKG_CONFIG_PATH}"

if test -n "${$1_PKG_CONFIG_PATH}"; then PKG_CONFIG_PATH="${$1_PKG_CONFIG_PATH}:${PKG_CONFIG_PATH}"; fi
AC_EXTPKG([$1_$2],[$3])

PKG_CONFIG_PATH="${save_PKG_CONFIG_PATH}"
])
