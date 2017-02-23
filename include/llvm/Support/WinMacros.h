#ifndef LLVM_SUPPORT_WINMACROS_H
#define LLVM_SUPPORT_WINMACROS_H

#ifndef LLVM_ON_WIN32

#define C_ASSERT(expr) static_assert((expr), "")

#define ARRAYSIZE(array) (sizeof(array) / sizeof(array[0]))

#define _countof(array) (sizeof(array) / sizeof(array[0]))

#define __declspec(x)

#define STDMETHODCALLTYPE

#endif // LLVM_ON_WIN32

#endif // LLVM_SUPPORT_WINMACROS_H
