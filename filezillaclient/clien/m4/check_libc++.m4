dnl Checks whether we need to pass -std=libc++ to CXXFLAGS. Sadly this is needed on OS X \
dnl which for some insane reason defaults to an ancient stdlibc++ :(
dnl To check for this, we try to use std::forward from <utility>

AC_DEFUN([CHECK_LIBCXX], [

  AC_LANG_PUSH(C++)

  AC_MSG_CHECKING([for whether we need -stdlib=libc++])

  AC_COMPILE_IFELSE([
    AC_LANG_PROGRAM([[
      #include <utility>
    ]], [[
      int x = 23;
      int y = std::forward<int>(x);
      return x == y ? 0 : 1;
    ]])
  ], [
    AC_MSG_RESULT([no])
  ], [
    CXXFLAGS="$CXXFLAGS -stdlib=libc++"
    LDFLAGS="$LDFLAGS -stdlib=libc++"

    AC_COMPILE_IFELSE([
      AC_LANG_PROGRAM([[
        #include <utility>
      ]], [[
        int x = 23;
        int y = std::forward<int>(x);
        return x == y ? 0 : 1;
      ]])
    ], [
      AC_MSG_RESULT([yes])
    ], [
      AC_MSG_FAILURE([std::forward in <utility> is not available or seems unusable.])
    ])
  ])

  AC_LANG_POP(C++)
])
