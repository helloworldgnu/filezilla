dnl Checks C++11 support, in particular we look for unordered_map

AC_DEFUN([CHECK_CXX11], [

  AC_LANG_PUSH(C++)

  AC_MSG_CHECKING([for whether we can include <unordered_map>])

  AC_COMPILE_IFELSE([
      AC_LANG_PROGRAM([[
        #include <unordered_map>
      ]], [[
        return 0;
      ]])
    ], [
      AC_MSG_RESULT([yes])
    ], [
      AC_MSG_FAILURE([No unordered_map implementation found. Make sure you use a C++11 compiler with a matching standard library.])
    ])

  AC_LANG_POP(C++)
])
