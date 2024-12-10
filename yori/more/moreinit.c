/**
 * @file more/moreinit.c
 *
 * Yori shell more initialization
 *
 * Copyright (c) 2017-2022 Malcolm J. Smith
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "more.h"


/**
 Given a console state, return the width and height of the viewport area.

 @param ScreenInfo Pointer to the console state, as returned from
        GetConsoleScreenBufferInfo.

 @param ViewportWidth On successful completion, populated with the new width
        of the viewport.

 @param ViewportHeight On successful completion, populated with the new height
        of the viewport.
 */
VOID
MoreGetViewportDimensions(
    __in PCONSOLE_SCREEN_BUFFER_INFO ScreenInfo,
    __out PYORI_ALLOC_SIZE_T ViewportWidth,
    __out PYORI_ALLOC_SIZE_T ViewportHeight
    )
{
    *ViewportWidth = ScreenInfo->dwSize.X;
    *ViewportHeight = (YORI_ALLOC_SIZE_T)(ScreenInfo->srWindow.Bottom - ScreenInfo->srWindow.Top);
}

/**
 Given console dimensions, allocate viewport structures.

 @param ViewportWidth The width of the viewport structures to allocate.

 @param ViewportHeight The height of the viewport structures to allocate.

 @param DisplayViewportLines On successful completion, populated with a zeroed
        array of viewport lines to display.

 @param StagingViewportLines On successful completion, populated with a zeroed
        array of viewport lines to use as staging.

 @return TRUE to indicate success, meaning both structures are allocated and
         returned.  FALSE if neither are being returned.
 */
__success(return)
BOOLEAN
MoreAllocateViewportStructures(
    __in YORI_ALLOC_SIZE_T ViewportWidth,
    __in YORI_ALLOC_SIZE_T ViewportHeight,
    __out PMORE_LOGICAL_LINE * DisplayViewportLines,
    __out PMORE_LOGICAL_LINE * StagingViewportLines
    )
{

    UNREFERENCED_PARAMETER(ViewportWidth);

    *DisplayViewportLines = YoriLibMalloc(sizeof(MORE_LOGICAL_LINE) * ViewportHeight);
    if (*DisplayViewportLines == NULL) {
        return FALSE;
    }
    ZeroMemory(*DisplayViewportLines, sizeof(MORE_LOGICAL_LINE) * ViewportHeight);

    *StagingViewportLines = YoriLibMalloc(sizeof(MORE_LOGICAL_LINE) * ViewportHeight);
    if (*StagingViewportLines == NULL) {
        YoriLibFree(*DisplayViewportLines);
        return FALSE;
    }

    ZeroMemory(*StagingViewportLines, sizeof(MORE_LOGICAL_LINE) * ViewportHeight);
    return TRUE;
}

/**
 Initialize a MoreContext with settings indicating where the data should come
 from, and launch a background thread to commence ingesting the data.

 @param MoreContext Pointer to the more context whose state should be
        initialized.

 @param ArgCount The number of elements in the ArgStrings array.  This can be
        zero if input should come from a pipe, not from a set of files.

 @param ArgStrings Pointer to an array of YORI_STRINGs for each argument
        that represents a criteria of one or more files to ingest.  This can
        be NULL if ingestion should come from a pipe, not from files.

 @param Recursive TRUE if the set of files should be enumerated recursively.
        FALSE if they should be interpreted as files and not recursed.

 @param BasicEnumeration TRUE if enumeration should not expand {}, [], or
        similar operators.  FALSE if these should be expanded.

 @param DebugDisplay TRUE if the program should use the debug display which
        clears the screen and writes internal buffers on each change.  This
        is much slower than just telling the console about changes but helps
        to debug the state of the program.

 @param SuspendPagination TRUE if pagination should not be enabled by default,
        so the program should just display whatever it has ingested in real
        time until input indicates to pause.

 @param WaitForMore TRUE if when reading files, this program should
        continually wait for more data to be added.  This is useful where a
        file is being extended continually by another program, but it implies
        that this program cannot move to the next file.  FALSE if this program
        should read until the end of each file and move to the next.

 @return TRUE to indicate successful completion, meaning a background thread
         is executing and this should be drained with @ref MoreGracefulExit.
         FALSE to indicate initialization was unsuccessful, and the
         MoreContext should be cleaned up with @ref MoreCleanupContext.
 */
BOOLEAN
MoreInitContext(
    __inout PMORE_CONTEXT MoreContext,
    __in YORI_ALLOC_SIZE_T ArgCount,
    __in_opt PYORI_STRING ArgStrings,
    __in BOOLEAN Recursive,
    __in BOOLEAN BasicEnumeration,
    __in BOOLEAN DebugDisplay,
    __in BOOLEAN SuspendPagination,
    __in BOOLEAN WaitForMore
    )
{
    CONSOLE_SCREEN_BUFFER_INFO ScreenInfo;
    YORI_ALLOC_SIZE_T Index;
    DWORD ThreadId;
    UCHAR Color;
    UCHAR SearchColors[5] = {
        FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY,
        FOREGROUND_GREEN | FOREGROUND_INTENSITY,
        FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY,
        FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_INTENSITY,
        FOREGROUND_RED | FOREGROUND_INTENSITY
    };


    ZeroMemory(MoreContext, sizeof(MORE_CONTEXT));

    MoreContext->Recursive = Recursive;
    MoreContext->BasicEnumeration = BasicEnumeration;
    MoreContext->DebugDisplay = DebugDisplay;
    MoreContext->SuspendPagination = SuspendPagination;
    MoreContext->WaitForMore = WaitForMore;
    MoreContext->TabWidth = 4;

    YoriLibInitializeListHead(&MoreContext->PhysicalLineList);
    YoriLibInitializeListHead(&MoreContext->FilteredPhysicalLineList);
    MoreContext->PhysicalLineMutex = CreateMutex(NULL, FALSE, NULL);
    if (MoreContext->PhysicalLineMutex == NULL) {
        return FALSE;
    }

    MoreContext->PhysicalLineAvailableEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (MoreContext->PhysicalLineAvailableEvent == NULL) {
        return FALSE;
    }

    MoreContext->ShutdownEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (MoreContext->ShutdownEvent == NULL) {
        return FALSE;
    }

    if (!GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &ScreenInfo)) {
        return FALSE;
    }

    //
    //  Ingest depends on knowing the default color, which can happen before
    //  any output.  Force set it here.
    //

    YoriLibVtSetDefaultColor(ScreenInfo.wAttributes);
    MoreContext->InitialColor = YoriLibVtGetDefaultColor();

    for (Index = 0; Index < MORE_MAX_SEARCHES; Index++) {
        YoriLibInitEmptyString(&MoreContext->SearchStrings[Index]);
        MoreContext->SearchContext[Index].ColorIndex = (UCHAR)-1;
        if (Index < sizeof(SearchColors)/sizeof(SearchColors[0])) {
            Color = (UCHAR)(((MoreContext->InitialColor & 0xF0) >> 4) | (SearchColors[Index] << 4));
        } else {
            Color = (UCHAR)((MoreContext->InitialColor & 0xF0) | SearchColors[Index % 5]);
        }
        MoreContext->SearchColors[Index] = Color;
    }

    MoreContext->SearchColorIndex = 0;

    MoreGetViewportDimensions(&ScreenInfo, &MoreContext->ViewportWidth, &MoreContext->ViewportHeight);

    if (!MoreAllocateViewportStructures(MoreContext->ViewportWidth, MoreContext->ViewportHeight, &MoreContext->DisplayViewportLines, &MoreContext->StagingViewportLines)) {
        return FALSE;
    }


    MoreContext->InputSourceCount = ArgCount;
    MoreContext->InputSources = ArgStrings;

    MoreContext->IngestThread = CreateThread(NULL, 0, MoreIngestThread, MoreContext, 0, &ThreadId);
    if (MoreContext->IngestThread == NULL) {
        return FALSE;
    }

    return TRUE;
}

/**
 Clean up any state on the MoreContext.

 @param MoreContext Pointer to the more context whose state should be cleaned
        up.
 */
VOID
MoreCleanupContext(
    __inout PMORE_CONTEXT MoreContext
    )
{
    YORI_ALLOC_SIZE_T Index;

    ASSERT(YoriLibIsListEmpty(&MoreContext->PhysicalLineList));
    ASSERT(YoriLibIsListEmpty(&MoreContext->FilteredPhysicalLineList));

    if (MoreContext->DisplayViewportLines != NULL) {
        YoriLibFree(MoreContext->DisplayViewportLines);
        MoreContext->DisplayViewportLines = NULL;
    }

    if (MoreContext->StagingViewportLines != NULL) {
        YoriLibFree(MoreContext->StagingViewportLines);
        MoreContext->StagingViewportLines = NULL;
    }

    if (MoreContext->PhysicalLineAvailableEvent != NULL) {
        CloseHandle(MoreContext->PhysicalLineAvailableEvent);
        MoreContext->PhysicalLineAvailableEvent = NULL;
    }

    if (MoreContext->ShutdownEvent != NULL) {
        CloseHandle(MoreContext->ShutdownEvent);
        MoreContext->ShutdownEvent = NULL;
    }

    if (MoreContext->PhysicalLineMutex != NULL) {
        CloseHandle(MoreContext->PhysicalLineMutex);
        MoreContext->PhysicalLineMutex = NULL;
    }

    if (MoreContext->IngestThread != NULL) {
        CloseHandle(MoreContext->IngestThread);
        MoreContext->IngestThread = NULL;
    }

    for (Index = 0; Index < MORE_MAX_SEARCHES; Index++) {
        YoriLibFreeStringContents(&MoreContext->SearchStrings[Index]);
        MoreContext->SearchContext[Index].ColorIndex = (UCHAR)-1;
    }

    MoreContext->SearchColorIndex = 0;
}

/**
 Indicate that the ingest thread should terminate, wait for it to die, and
 clean up any state.

 @param MoreContext Pointer to the more context whose state should be cleaned
        up.
 */
VOID
MoreGracefulExit(
    __inout PMORE_CONTEXT MoreContext
    )
{
    PYORI_LIST_ENTRY ListEntry;
    PMORE_PHYSICAL_LINE PhysicalLine;
    YORI_ALLOC_SIZE_T Index;

    YoriLibCancelSet();
    SetEvent(MoreContext->ShutdownEvent);
    WaitForSingleObject(MoreContext->IngestThread, INFINITE);
    for (Index = 0; Index < MoreContext->ViewportHeight; Index++) {
        YoriLibFreeStringContents(&MoreContext->DisplayViewportLines[Index].Line);
    }

    ListEntry = YoriLibGetNextListEntry(&MoreContext->FilteredPhysicalLineList, NULL);
    while (ListEntry != NULL) {
        PhysicalLine = CONTAINING_RECORD(ListEntry, MORE_PHYSICAL_LINE, LineList);
        YoriLibRemoveListItem(ListEntry);
        ListEntry = YoriLibGetNextListEntry(&MoreContext->FilteredPhysicalLineList, NULL);
    }

    ListEntry = YoriLibGetNextListEntry(&MoreContext->PhysicalLineList, NULL);
    while (ListEntry != NULL) {
        PhysicalLine = CONTAINING_RECORD(ListEntry, MORE_PHYSICAL_LINE, LineList);
        YoriLibRemoveListItem(ListEntry);
        YoriLibFreeStringContents(&PhysicalLine->LineContents);
        YoriLibDereference(PhysicalLine->MemoryToFree);
        ListEntry = YoriLibGetNextListEntry(&MoreContext->PhysicalLineList, NULL);
    }

    MoreCleanupContext(MoreContext);
}

// vim:sw=4:ts=4:et:
