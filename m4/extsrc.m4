dnl AC_EXTSRC
dnl
dnl AC_EXTSRC(name,help,source-location,install-location,configure-command,build-command,clean-command,[default-libs],[default-cflags],[default-lflags])
dnl
dnl Add an external package that we will want to configure and build. Its source should be distributed with our package,
dnl but give the option to supply a different source or install location.
dnl
dnl If you specify an install location, then the package will not be built, and we will use the one from the install location.
dnl If you do not specify a source location, then the package will be built from the source in our extsrc directory.
dnl If you do specify a source location, then the package will be built from the the source in that location.
AC_DEFUN([AC_EXTSRC_INIT],
	[
	_extsrcdir=$1
	_abs_srcdir=$(cd "${srcdir}"; /bin/pwd)
	default_extsrcdir=${_extsrcdir:-"${_abs_srcdir}/extsrc"}
	AC_ARG_WITH([extsrc],AS_HELP_STRING([--with-extsrc],[set path to external package sources]),[extsrcdir="$withval"],[extsrcdir="${default_extsrcdir}"])
	AC_MSG_NOTICE([location of external package sources is ${extsrcdir}])

	extsrc_build_commands=""
	extsrc_clean_commands=""
	AC_SUBST([extsrcdir])
	AC_SUBST([extsrc_build_commands])
	AC_SUBST([extsrc_clean_commands])
	])

AC_DEFUN([AC_EXTSRC],
	[
	AC_ARG_WITH([$1-source],AS_HELP_STRING([--with-$1-source],[set the path to the source for $1, $2]),[])

	_name="$1"
	_help="$2"
	_source_location="$3"

	default_source_LOCATION=${extsrcdir}/${_source_location:-"${_name}"}

	if test "${with_$1_source}" == "no"; then
	$1_SOURCE_LOCATION="no";
	else
	$1_SOURCE_LOCATION=${with_$1_source:-${default_source_LOCATION}}
	fi

	AC_SUBST([$1_SOURCE_LOCATION])

	])

dnl AC_EXTSRC_CHECK
dnl AC_EXTSRC_CHECK(name,relative-pathname,[required])
dnl Checks to see if a file calle ${relative-pathname} relative to the $name_SOURCE_LOCATION
dnl set in AC_EXT_SRC exists. Sets and AC_SUBST's a variable called $1_source_found,
dnl and also creates an automake conditional cammed $1_SOURCE_FOUND
AC_DEFUN([AC_EXTSRC_CHECK],
	[
	_name=$1
	_relative_test_pathname=$2
	_required=$3

	_source_location_varname="${_name}_SOURCE_LOCATION"
	_SOURCE_LOCATION=${!_source_location_varname}

	# Maybe _SOURCE_LOCATION is set to "no"?
	if test "${_required}" == "required" -a "${_SOURCE_LOCATION}" == "no"; then
	AC_MSG_ERROR([Source for $1 is required, but attempt was made to configure without it])
	fi;

	# Look for the file:
	_test_pathname="${_SOURCE_LOCATION}/${_relative_test_pathname}"
	AC_CHECK_FILE([$_test_pathname],[$1_source_found="yes"],[$1_source_found="no"])

	# It was found:
	if test "${$1_source_found}" == yes; then
	AC_MSG_NOTICE([Source for $1 found in $_SOURCE_LOCATION])
	# It was not found, but is required:
	elif test "${_required}" == "required"; then
	AC_MSG_ERROR([Source for $1 is required, but not found (checked in $_SOURCE_LOCATION)])
	# It was not found, but is not required:
	else
	AC_MSG_NOTICE([Source for $1 was not found, but is not required (checked in $_SOURCE_LOCATION)])
	fi

	AC_SUBST([$1_source_found])
	AM_CONDITIONAL([$1_SOURCE_FOUND],[ test "${$1_source_found}" == "yes" ])

	])
