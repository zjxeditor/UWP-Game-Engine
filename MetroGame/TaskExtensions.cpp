// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved
#include "pch.h"

#ifndef NDEBUG
unsigned int _mainThreadId = 0U;

#ifdef  __cplusplus
extern "C" {
#endif

    // The IsMainThread function returns true if the current thread is the app's main thread and false otherwise.
    bool IsMainThread()
    {
        return (_mainThreadId == 0U || _mainThreadId == GetCurrentThreadId());
    }

    // The IsBackgroundThread function returns false if the current thread is the app's main thread and true otherwise.
    bool IsBackgroundThread()
    {
        return (_mainThreadId == 0U || _mainThreadId != GetCurrentThreadId());
    }

    // The RecordMainThread function registers the main thread ID for use by the IsMainThread and IsBackgroundThread functions.
    void RecordMainThread()
    {
        _mainThreadId = GetCurrentThreadId();
    }

#ifdef  __cplusplus
}
#endif

#endif /* not NDEBUG */