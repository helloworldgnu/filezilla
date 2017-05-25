dnl Checks for thread_local support

AC_DEFUN([CHECK_THREAD_LOCAL], [

  AC_LANG_PUSH(C++)

  AC_MSG_CHECKING([for thread_local])

  AC_COMPILE_IFELSE([
    AC_LANG_PROGRAM([[
    ]], [[
      thread_local static int foo = 0;
      return 0;
    ]])
  ], [
    AC_MSG_RESULT([yes])
  ], [
    AC_DEFINE([HAVE_NO_THREAD_LOCAL], [1], [Define if thread_local isn't supported])
    AC_MSG_RESULT([no])
  ])

  AC_LANG_POP(C++)
])
