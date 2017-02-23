#ifndef LLVM_SUPPORT_WINDTYPES_H
#define LLVM_SUPPORT_WINDTYPES_H

#ifndef LLVM_ON_WIN32

#include "llvm/Support/WinResults.h"

typedef bool BOOL;

typedef unsigned char BYTE;
typedef unsigned char* LPBYTE;

typedef int INT;
typedef unsigned int UINT;

typedef uint16_t WORD;
typedef uint32_t DWORD;
typedef uint32_t UINT32;
typedef uint64_t UINT64;

typedef size_t SIZE_T;

typedef const char* LPCSTR;

#if 0
typedef wchar_t WCHAR;
typedef wchar_t* LPWSTR;
typedef wchar_t* PWCHAR;
typedef const wchar_t* LPCWSTR;
typedef const wchar_t* PCWSTR;
#else
// This is definitely not the correct way of handling wide characters.
typedef char WCHAR;
typedef char* LPWSTR;
typedef char* PWCHAR;
typedef const char* LPCWSTR;
typedef const char* PCWSTR;
#endif

typedef void* LPVOID;
typedef const void *LPCVOID;

typedef struct _GUID {
  DWORD Data1;
  WORD  Data2;
  WORD  Data3;
  BYTE  Data4[8];
} GUID;
typedef const GUID& REFIID;

#define interface struct

interface IUnknown
{
  virtual HRESULT QueryInterface(REFIID riid, void** ppvObject) = 0;
  virtual UINT AddRef(void) = 0;
  virtual UINT Release(void) = 0;
  virtual ~IUnknown() = 0;
};

template<class T>
class CComPtr {
public:
  CComPtr() : ComPtr(nullptr) {}

  CComPtr(T* ptr) {
    ComPtr = ptr;
    if (ComPtr != nullptr) ComPtr->AddRef();
  }

  CComPtr(const CComPtr<T>& that) {
    ComPtr = that.ComPtr;
    if (ComPtr != nullptr) ComPtr->AddRef();
  }

  ~CComPtr() {
    if (ComPtr) {
      ComPtr->Release();
      ComPtr = nullptr;
    }
  }

  operator T*() const {
    return ComPtr;
  }

  T& operator*() const {
    return *ComPtr;
  }

  T* operator->() const {
    return ComPtr;
  }

  T** operator&() {
    return &ComPtr;
  }

private:
  T* ComPtr;
};

#endif // LLVM_ON_WIN32

#endif // LLVM_SUPPORT_WINTYPES_H
