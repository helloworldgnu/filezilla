dnl Checks whether MinGW headers declare ICopyHookW

AC_DEFUN([FZ_CHECK_ICOPYHOOKW], [

  AC_LANG_PUSH(C)

  AC_MSG_CHECKING([whether MinGW headers declare ICopyHookW])
  AC_COMPILE_IFELSE([
    AC_LANG_PROGRAM([[
        #include <objbase.h>
        #include <shlobj.h>
      ]], [[
    ICopyHookW* foo;
        return 0;
      ]])
    ], [
      AC_MSG_RESULT([yes])
      AC_DEFINE([HAVE_ICOPYHOOKW], [1], [Headers delare ICopyHookW])
    ], [
      AC_MSG_RESULT([no])
    ])


  AC_LANG_POP(C)
])
