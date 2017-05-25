AC_INIT([fzshellext],[0.0.0],[tim.kosse@filezilla-project.org],[],[https://filezilla-project.org/])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([../../m4])
AC_CONFIG_AUX_DIR(../../config)
AM_INIT_AUTOMAKE([dist-bzip2 foreign])

AC_ARG_ENABLE(shellext, AS_HELP_STRING([--disable-shellext],[Disable shellextension]), \
		[build_shellext="$enableval"], [build_shellext="yes"])

AC_CANONICAL_HOST

LT_INIT
LT_LANG([C++])

if test "x$build_shellext" = "xyes"; then

AC_CACHE_SAVE

if test "X$GCC" = Xyes; then
	CFLAGS="$CFLAGS -Wall -g"
	CXXFLAGS="$CXXFLAGS -Wall -g"
fi

AC_MSG_CHECKING([whether the compiler actually targets MinGW])
matching=
cxx_v=`"$CXX" -v 2>&1`
if echo "$cxx_v" | grep -i '^target:.*$' >/dev/null 2>&1; then
	if echo "$cxx_v" | grep -i '^target:.*mingw.*$' >/dev/null 2>&1; then
		matching=1
	fi
else
	# Weird compiler, trust it in blind faith
	matching=1
fi
if test "x$matching" = "x1"; then
  AC_MSG_RESULT([yes])
else
  AC_MSG_RESULT([no])
  AC_MSG_ERROR([Compiler does not seem to target MinGW.])
fi

# Check for C++11 support
AX_CXX_COMPILE_STDCXX_11([],[mandatory])
CHECK_LIBCXX
CHECK_CXX11
AC_CACHE_SAVE

AC_CHECK_HEADER(shlobj.h,, [
		AC_MSG_ERROR([shlobj.h not found which is part of the Windows API])
	])


# Check for windres on MinGW builds
# ---------------------------------

# Small function to filter the flags for windres,
# so that only includes and defines are passed to windres.
parseflags() {
  WINDRESFLAGS=""
  for opt in "$@"
  do
      case "$opt" in
      -I*)
          WINDRESFLAGS="$WINDRESFLAGS --include-dir=${opt#-I}"
          ;;
      -D*)
          WINDRESFLAGS="$WINDRESFLAGS $opt"
          ;;
      esac
  done
}

WINDRESFLAGS=
use_resourcefile=
AC_MSG_CHECKING([whether to use windres])
if echo $host_os | grep "cygwin\|mingw" > /dev/null 2>&1; then
  AC_MSG_RESULT([yes])
  AC_CHECK_TOOL([WINDRES], [windres])
  if test "x$WINDRES" = "x"; then
    AC_MSG_ERROR([windres could not be found, please make sure this program is within your path.])
  fi
  use_resourcefile=true
  parseflags $CPPFLAGS

  FZ_CHECK_ICOPYHOOKW
else
  AC_MSG_RESULT([no])
fi
AC_SUBST(WINDRES)
AC_SUBST(WINDRESFLAGS)

# Enable DEP and ASLR linker flags
# --------------------------------

AX_CHECK_LINK_FLAG([-Wl,--dynamicbase], [LDFLAGS="$LDFLAGS -Wl,--dynamicbase"])
AX_CHECK_LINK_FLAG([-Wl,--nxcompat], [LDFLAGS="$LDFLAGS -Wl,--nxcompat"])

# Set Windows-specific linker flags
# ---------------------------------
if echo $host_os | grep "cygwin\|mingw" > /dev/null 2>&1; then
  # We currently require Vista minimum.
  AX_CHECK_LINK_FLAG([-Wl,--major-subsystem-version -Wl,6], [LDFLAGS="$LDFLAGS -Wl,--major-subsystem-version -Wl,6"])
  AX_CHECK_LINK_FLAG([-Wl,--minor-subsystem-version -Wl,0], [LDFLAGS="$LDFLAGS -Wl,--minor-subsystem-version -Wl,0"])
  AX_CHECK_LINK_FLAG([-Wl,--major-os-version -Wl,6], [LDFLAGS="$LDFLAGS -Wl,--major-os-version -Wl,6"])
  AX_CHECK_LINK_FLAG([-Wl,--minor-os-version -Wl,0], [LDFLAGS="$LDFLAGS -Wl,--minor-os-version -Wl,0"])

  # Set the Large Address Aware flag on 32bit Windows
  if ! echo $host_cpu | grep "x86_64" > /dev/null 2>&1; then
    AX_CHECK_LINK_FLAG([-Wl,--large-address-aware], [LDFLAGS="$LDFLAGS -Wl,--large-address-aware"])
  fi
fi

# Output automake conditionals
# ----------------------------

fi

AM_CONDITIONAL(USE_RESOURCEFILE, test "$use_resourcefile" = "true")
AM_CONDITIONAL(BUILD_SHELLEXT, [test "x$build_shellext" = "yes"])

AC_CONFIG_FILES(Makefile)
AC_OUTPUT
