// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved
#pragma once

#ifndef NDEBUG

#ifdef  __cplusplus
extern "C" {
#endif

    // Called at startup to remember the main thread id for debug assertion checking
    void RecordMainThread(void);

    // Called in debug mode only for thread context assertion checks
    bool IsMainThread(void);

    // Called in debug mode only for thread context assertion checks
    bool IsBackgroundThread(void);

#ifdef  __cplusplus
}
#endif

#endif /* not NDEBUG */