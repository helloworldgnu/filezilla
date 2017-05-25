dnl Checks whether std::chrono::steady_clock is steady
dnl Unfortunately it is not always steady.

AC_DEFUN([CHECK_STEADY_CLOCK], [

  AC_LANG_PUSH(C++)

  AC_MSG_CHECKING([whether std::chrono::steady_clock is steady])

  AC_COMPILE_IFELSE([
    AC_LANG_PROGRAM([[
      #include <chrono>
    ]], [[
      static_assert(std::chrono::steady_clock::is_steady, "steady_clock isn't steady");
      return std::chrono::steady_clock::is_steady ? 0 : 1;
    ]])
  ], [
    AC_MSG_RESULT([yes])
  ], [
    AC_DEFINE([HAVE_UNSTEADY_STEADY_CLOCK], [1], [Define if steady_clock isn't steady])
    AC_MSG_RESULT([no])
  ])

  AC_LANG_POP(C++)
])
