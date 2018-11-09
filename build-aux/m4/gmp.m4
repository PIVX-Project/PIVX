dnl Copyright (c) 2018 The Syndicate Core developers
dnl Distributed under the MIT software license, see the accompanying
dnl file COPYING or http://www.opensource.org/licenses/mit-license.php.

dnl
AC_DEFUN([GMP_CHECK],[
if test x"$has_gmp" != x"yes"; then
  AC_CHECK_HEADER(gmp.h,[
    AC_CHECK_LIB(gmp, __gmpz_init,[
      has_gmp=yes;
      GMP_LIBS="$GMP_LIBS -lgmp";
      AC_DEFINE(HAVE_LIBGMP, 1, [Define this symbol if libgmp is installed])
    ])
  ])
fi
])
