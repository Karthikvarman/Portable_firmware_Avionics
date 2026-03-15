#ifndef STDDEF_H
#define STDDEF_H
#ifndef NULL
#define NULL ((void *)0)
#endif
#ifdef __clang__
typedef __SIZE_TYPE__ size_t;
typedef __PTRDIFF_TYPE__ ptrdiff_t;
typedef __WCHAR_TYPE__ wchar_t;
#else
typedef unsigned int size_t;
typedef int ptrdiff_t;
typedef int wchar_t;
#endif
#endif