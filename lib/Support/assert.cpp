///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// assert.cpp                                                                //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "assert.h"

#ifdef LLVM_ON_WIN32 // SPIRV change

#include "windows.h"

void llvm_assert(_In_z_ const char *_Message,
                 _In_z_ const char *_File,
                 _In_ unsigned _Line) {
  RaiseException(STATUS_LLVM_ASSERT, 0, 0, 0);
}

// SPIRV change starts
#else

#include <assert.h>

void llvm_assert(const char* message, const char*, unsigned) {
  assert(false && message);
}

#endif
// SPIRV change ends
