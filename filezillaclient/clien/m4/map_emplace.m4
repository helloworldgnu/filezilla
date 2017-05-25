dnl Checks whether std::map has emplace

AC_DEFUN([FZ_CHECK_MAP_EMPLACE], [

  AC_LANG_PUSH(C++)

  AC_MSG_CHECKING([whether std::map has emplace])
  AC_COMPILE_IFELSE([
    AC_LANG_PROGRAM([[
        #include <map>
      ]], [[
	std::map<int, int> m;
	m.emplace(std::make_pair(2, 3));
        return 0;
      ]])
    ], [
      AC_MSG_RESULT([yes])
      AC_DEFINE([HAVE_MAP_EMPLACE], [1], [std::map has emplace])
    ], [
      AC_MSG_RESULT([no])
    ])


  AC_LANG_POP(C++)
])
