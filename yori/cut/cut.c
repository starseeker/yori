/**
 * @file cut/cut.c
 *
 * Yori shell filter within a line of output
 *
 * Copyright (c) 2017-2024 Malcolm J. Smith
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

#include <yoripch.h>
#include <yorilib.h>

/**
 Help text to display to the user.
 */
const
CHAR strCutHelpText[] =
        "\n"
        "Outputs a portion of an input buffer of text.\n"
        "\n"
        "CUT [-license] [-b] [-s] [-f n] [-d <delimiter chars>] [-o n] [-l n]\n"
        "    [[-i] -t <text>] [file]\n"
        "\n"
        "   -b             Use basic search criteria for files only\n"
        "   -d             The set of characters which delimit fields, default comma\n"
        "   -f n           The field number to cut\n"
        "   -i             Match text case insensitively\n"
        "   -l             The length in bytes to cut from the line or field\n"
        "   -o             The offset in bytes to cut from the line or field\n"
        "   -r             Operate on raw file offsets, not lines\n"
        "   -t text        Start matching offsets from the portion of line matching text\n"
        "   -s             Match files from all subdirectories\n"
        ;

/**
 Display usage text to the user.
 */
BOOL
CutHelp(VOID)
{
    YoriLibOutput(YORI_LIB_OUTPUT_STDOUT, _T("Cut %i.%02i\n"), YORI_VER_MAJOR, YORI_VER_MINOR);
#if YORI_BUILD_ID
    YoriLibOutput(YORI_LIB_OUTPUT_STDOUT, _T("  Build %i\n"), YORI_BUILD_ID);
#endif
    YoriLibOutput(YORI_LIB_OUTPUT_STDOUT, _T("%hs"), strCutHelpText);
    return TRUE;
}

/**
 Context describing the operations to perform on each file found.
 */
typedef struct _CUT_CONTEXT {

    /**
     TRUE if the input should be delimited by fields.  If FALSE, the input
     is delimited via bytes.
     */
    BOOLEAN FieldDelimited;

    /**
     TRUE if file enumeration is being performed recursively; FALSE if it is
     in one directory only.
     */
    BOOLEAN Recursive;

    /**
     TRUE if offsets and lengths refer to offsets in the file.  If FALSE,
     offsets refer to a range within each line.
     */
    BOOLEAN RawFile;

    /**
     TRUE if any text matching should be case insensitive.  If FALSE, text
     matching is case sensitive."
     */
    BOOLEAN CaseInsensitive;

    /**
     Start processing the line from any matching text.  If empty, the entire
     line is used.
     */
    YORI_STRING MatchText;

    /**
     For a field delimited stream, contains the NULL terminated string
     indicating one or more characters to interpret as delimiters.
     */
    LPTSTR FieldSeperator;

    /**
     The first error encountered when enumerating objects from a single arg.
     This is used to preserve file not found/path not found errors so that
     when the program falls back to interpreting the argument as a literal,
     if that still doesn't work, this is the error code that is displayed.
     */
    DWORD SavedErrorThisArg;

    /**
     For a field delimited stream, indicates the field number that should
     be output.
     */
    DWORD FieldOfInterest;

    /**
     Indicates the offset of the line or field, in bytes, that is of interest.
     */
    DWORDLONG DesiredOffset;

    /**
     Indicates the length of the range that is of interest.
     */
    DWORDLONG DesiredLength;

    /**
     Counts the number of files encountered as files are processed.
     */
    LONGLONG FilesFound;

    /**
     Counts the number of files encountered as files are processed within each
     command line argument.
     */
    LONGLONG FilesFoundThisArg;

} CUT_CONTEXT, *PCUT_CONTEXT;

/**
 Process an incoming stream from a single handle in line mode, applying the
 user requested actions.

 @param hSource The source handle containing data to process.

 @param CutContext The context that describes the actions to perform.

 @return TRUE to indicate success, FALSE to indicate failure.
 */
BOOL
CutProcessHandleLines(
    __in HANDLE hSource,
    __in PCUT_CONTEXT CutContext
    )
{
    YORI_STRING MatchingSubset;
    PVOID LineContext = NULL;
    YORI_STRING LineString;
    YORI_ALLOC_SIZE_T DesiredOffset;
    YORI_ALLOC_SIZE_T DesiredLength;

    //
    //  Truncate the desired offset and length to 32 bits.  The line
    //  processing code isn't expecting a 4Gb line, and it seems
    //  unrealistic that anyone would actually want to support one.
    //  

    DesiredOffset = (YORI_ALLOC_SIZE_T)CutContext->DesiredOffset;
    DesiredLength = (YORI_ALLOC_SIZE_T)CutContext->DesiredLength;

    YoriLibInitEmptyString(&LineString);

    while (TRUE) {
        if (!YoriLibReadLineToString(&LineString, &LineContext, hSource)) {
            break;
        }

        YoriLibInitEmptyString(&MatchingSubset);

        MatchingSubset.StartOfString = LineString.StartOfString;
        MatchingSubset.LengthInChars = LineString.LengthInChars;

        if (CutContext->MatchText.LengthInChars > 0) {
            YORI_ALLOC_SIZE_T OffsetOfMatch;
            BOOLEAN MatchFound;
            MatchFound = FALSE;
            if (CutContext->CaseInsensitive) {
                if (YoriLibFindFirstMatchingSubstringInsensitive(&MatchingSubset, 1, &CutContext->MatchText, &OffsetOfMatch)) {
                    MatchFound = TRUE;
                }
            } else {
                if (YoriLibFindFirstMatchingSubstring(&MatchingSubset, 1, &CutContext->MatchText, &OffsetOfMatch)) {
                    MatchFound = TRUE;
                }
            }

            if (MatchFound) {
                MatchingSubset.StartOfString = &MatchingSubset.StartOfString[OffsetOfMatch];
                MatchingSubset.LengthInChars = (YORI_ALLOC_SIZE_T)(MatchingSubset.LengthInChars - OffsetOfMatch);
            } else {
                MatchingSubset.LengthInChars = 0;
            }
        }

        if (CutContext->FieldDelimited) {
            YORI_ALLOC_SIZE_T CurrentField = 0;

            for (CurrentField = 0; CurrentField <= CutContext->FieldOfInterest; CurrentField++) {
                YORI_ALLOC_SIZE_T CharsBeforeSeperator;

                CharsBeforeSeperator = YoriLibCountStringNotContainingChars(&MatchingSubset, CutContext->FieldSeperator);
                if (CurrentField == CutContext->FieldOfInterest) {
                    MatchingSubset.LengthInChars = CharsBeforeSeperator;
                } else {
                    MatchingSubset.LengthInChars = MatchingSubset.LengthInChars - CharsBeforeSeperator;
                    if (MatchingSubset.LengthInChars == 0) {
                        break;
                    }
                    MatchingSubset.LengthInChars -= 1;
                    MatchingSubset.StartOfString = &MatchingSubset.StartOfString[CharsBeforeSeperator + 1];
                }
            }
        }
        if (MatchingSubset.LengthInChars > DesiredOffset) {
            MatchingSubset.StartOfString = &MatchingSubset.StartOfString[DesiredOffset];
            MatchingSubset.LengthInChars = MatchingSubset.LengthInChars - DesiredOffset;

            if (DesiredLength != 0 &&
                MatchingSubset.LengthInChars > DesiredLength) {

                MatchingSubset.LengthInChars = DesiredLength;
            }
        } else {
            MatchingSubset.LengthInChars = 0;
        }

        if (MatchingSubset.LengthInChars > 0) {
            YoriLibOutput(YORI_LIB_OUTPUT_STDOUT, _T("%y\n"), &MatchingSubset);
        }
    }

    YoriLibLineReadCloseOrCache(LineContext);
    YoriLibFreeStringContents(&LineString);

    return TRUE;
}

/**
 Process an incoming stream in raw file mode, applying the user requested
 actions.

 @param hSource The source handle containing data to process.

 @param CutContext The context that describes the actions to perform.

 @return TRUE to indicate success, FALSE to indicate failure.
 */
BOOL
CutProcessStreamOffset(
    __in HANDLE hSource,
    __in PCUT_CONTEXT CutContext
    )
{
    PUCHAR Buffer;
    DWORD BufferSize;
    DWORD BufferDisplayOffset;
    DWORD BytesReturned;
    DWORD BytesWritten;
    DWORD LengthToDisplay;
    DWORD FileType;
    LARGE_INTEGER StreamOffset;

    BufferSize = 64 * 1024;
    if (!YoriLibIsSizeAllocatable(BufferSize)) {
        BufferSize = 32 * 1024;
    }
    Buffer = YoriLibMalloc((YORI_ALLOC_SIZE_T)BufferSize);
    if (Buffer == NULL) {
        return FALSE;
    }

    //
    //  If it's a file, start at the offset requested by the user.  If it's
    //  not a file (it's a pipe), the only way to move forward is by
    //  reading.
    //

    FileType = GetFileType(hSource);

    StreamOffset.QuadPart = 0;
    if (FileType != FILE_TYPE_PIPE) {
        StreamOffset.QuadPart = CutContext->DesiredOffset;

        if (!SetFilePointer(hSource, StreamOffset.LowPart, &StreamOffset.HighPart, FILE_BEGIN)) {
            StreamOffset.QuadPart = 0;
        }
    }

    while (TRUE) {

        //
        //  Read a block of data.  On a pipe, this will block.
        //

        BytesReturned = 0;
        if (!ReadFile(hSource, Buffer, BufferSize, &BytesReturned, NULL)) {
            BytesReturned = 0;
        }

        //
        //  If we don't have data despite blocking, the operation is complete.
        //

        if (BytesReturned == 0) {
            break;
        }

        //
        //  If we haven't reached the starting point to display, loop back
        //  and read more.
        //

        if ((DWORDLONG)StreamOffset.QuadPart + BytesReturned <= CutContext->DesiredOffset) {
            StreamOffset.QuadPart += BytesReturned;
            continue;
        }

        LengthToDisplay = BytesReturned;

        //
        //  If the starting point to display is partway through the buffer,
        //  find the offset within the buffer to start displaying and cap
        //  the number of characters to display.
        //

        BufferDisplayOffset = 0;
        if ((DWORDLONG)StreamOffset.QuadPart < CutContext->DesiredOffset) {
            BufferDisplayOffset = (DWORD)(CutContext->DesiredOffset - StreamOffset.QuadPart);
            LengthToDisplay -= BufferDisplayOffset;
        }

        ASSERT(BufferDisplayOffset + LengthToDisplay == BytesReturned);

        //
        //  If the number of bytes that the user requested to display is
        //  longer than the amount we have, cap the amount to display to
        //  what the user requested.
        //

        if (CutContext->DesiredLength != 0) {
            if ((DWORDLONG)StreamOffset.QuadPart + BufferDisplayOffset + LengthToDisplay >= CutContext->DesiredOffset + CutContext->DesiredLength) {
                LengthToDisplay = (DWORD)(CutContext->DesiredOffset + CutContext->DesiredLength - StreamOffset.QuadPart - BufferDisplayOffset);
            }
        }

        //
        //  Display the buffer at the display offset for the length to
        //  display.
        //

        if (LengthToDisplay > 0) {
            if (!WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), &Buffer[BufferDisplayOffset], LengthToDisplay, &BytesWritten, NULL)) {
                break;
            }
        }

        StreamOffset.QuadPart += BufferDisplayOffset + BytesReturned;

        if (CutContext->DesiredLength != 0 &&
            (DWORDLONG)StreamOffset.QuadPart >= CutContext->DesiredOffset + CutContext->DesiredLength) {

            break;
        }
    }

    YoriLibFree(Buffer);

    return TRUE;
}

/**
 Process an incoming stream from a single handle, applying the user requested
 actions.

 @param hSource The source handle containing data to process.

 @param CutContext The context that describes the actions to perform.

 @return TRUE to indicate success, FALSE to indicate failure.
 */
BOOL
CutFilterHandle(
    __in HANDLE hSource,
    __in PCUT_CONTEXT CutContext
    )
{
    if (CutContext->RawFile) {
        return CutProcessStreamOffset(hSource, CutContext);
    } else {
        return CutProcessHandleLines(hSource, CutContext);
    }
}

/**
 A callback that is invoked when a file is found that matches a search criteria
 specified in the set of strings to enumerate.

 @param Filename Pointer to the file path that was found.

 @param FindData Information about the file.  This can be NULL if the file
        was not found by enumeration.

 @param Depth Recursion depth, unused in this application.

 @param Context Pointer to a context block the cut operation to perform and
        tracking the number of files that have been processed.

 @return TRUE to continute enumerating, FALSE to abort.
 */
BOOL
CutFileFoundCallback(
    __in PYORI_STRING Filename,
    __in_opt PWIN32_FIND_DATA FindData,
    __in DWORD Depth,
    __in PVOID Context
    )
{
    HANDLE hSource;
    PCUT_CONTEXT CutContext = (PCUT_CONTEXT)Context;

    UNREFERENCED_PARAMETER(Depth);
    UNREFERENCED_PARAMETER(FindData);

    ASSERT(YoriLibIsStringNullTerminated(Filename));

    hSource = CreateFile(Filename->StartOfString,
                         GENERIC_READ,
                         FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
                         NULL,
                         OPEN_EXISTING,
                         FILE_ATTRIBUTE_NORMAL|FILE_FLAG_BACKUP_SEMANTICS,
                         NULL);

    if (hSource == INVALID_HANDLE_VALUE) {
        if (CutContext->SavedErrorThisArg == ERROR_SUCCESS) {
            DWORD LastError = GetLastError();
            LPTSTR ErrText = YoriLibGetWinErrorText(LastError);
            YoriLibOutput(YORI_LIB_OUTPUT_STDERR, _T("cut: open file failed: %s"), ErrText);
            YoriLibFreeWinErrorText(ErrText);
        }
        return EXIT_FAILURE;
    }

    CutContext->SavedErrorThisArg = ERROR_SUCCESS;
    CutContext->FilesFound++;
    CutContext->FilesFoundThisArg++;
    CutFilterHandle(hSource, CutContext);

    CloseHandle(hSource);
    return TRUE;
}

/**
 A callback that is invoked when a directory cannot be successfully enumerated.

 @param FilePath Pointer to the file path that could not be enumerated.

 @param ErrorCode The Win32 error code describing the failure.

 @param Depth Recursion depth, ignored in this application.

 @param Context Pointer to the context block indicating whether the
        enumeration was recursive.  Recursive enumerates do not complain
        if a matching file is not in every single directory, because
        common usage expects files to be in a subset of directories only.

 @return TRUE to continute enumerating, FALSE to abort.
 */
BOOL
CutFileEnumerateErrorCallback(
    __in PYORI_STRING FilePath,
    __in DWORD ErrorCode,
    __in DWORD Depth,
    __in PVOID Context
    )
{
    YORI_STRING UnescapedFilePath;
    BOOL Result = FALSE;
    PCUT_CONTEXT CutContext = (PCUT_CONTEXT)Context;

    UNREFERENCED_PARAMETER(Depth);

    YoriLibInitEmptyString(&UnescapedFilePath);
    if (!YoriLibUnescapePath(FilePath, &UnescapedFilePath)) {
        UnescapedFilePath.StartOfString = FilePath->StartOfString;
        UnescapedFilePath.LengthInChars = FilePath->LengthInChars;
    }

    if (ErrorCode == ERROR_FILE_NOT_FOUND || ErrorCode == ERROR_PATH_NOT_FOUND) {
        if (!CutContext->Recursive) {
            CutContext->SavedErrorThisArg = ErrorCode;
        }
        Result = TRUE;
    } else {
        LPTSTR ErrText = YoriLibGetWinErrorText(ErrorCode);
        YORI_STRING DirName;
        LPTSTR FilePart;
        YoriLibInitEmptyString(&DirName);
        DirName.StartOfString = UnescapedFilePath.StartOfString;
        FilePart = YoriLibFindRightMostCharacter(&UnescapedFilePath, '\\');
        if (FilePart != NULL) {
            DirName.LengthInChars = (YORI_ALLOC_SIZE_T)(FilePart - DirName.StartOfString);
        } else {
            DirName.LengthInChars = UnescapedFilePath.LengthInChars;
        }
        YoriLibOutput(YORI_LIB_OUTPUT_STDERR, _T("cut: Enumerate of %y failed: %s"), &DirName, ErrText);
        YoriLibFreeWinErrorText(ErrText);
    }
    YoriLibFreeStringContents(&UnescapedFilePath);
    return Result;
}


#ifdef YORI_BUILTIN
/**
 The main entrypoint for the cut builtin command.
 */
#define ENTRYPOINT YoriCmd_YCUT
#else
/**
 The main entrypoint for the cut standalone application.
 */
#define ENTRYPOINT ymain
#endif

/**
 The main entrypoint for the cut cmdlet.

 @param ArgC The number of arguments.

 @param ArgV An array of arguments.

 @return Exit code of zero to indicate success, nonzero to indicate failure.
 */
DWORD
ENTRYPOINT(
    __in YORI_ALLOC_SIZE_T ArgC,
    __in YORI_STRING ArgV[]
    )
{
    HANDLE hSource = NULL;
    BOOLEAN ArgumentUnderstood;
    YORI_ALLOC_SIZE_T i;
    CUT_CONTEXT CutContext;
    BOOLEAN BasicEnumeration = FALSE;
    YORI_ALLOC_SIZE_T StartArg = 0;
    YORI_STRING Arg;
    YORI_MAX_SIGNED_T Temp;
    YORI_ALLOC_SIZE_T CharsConsumed;
    DWORD Result;

    ZeroMemory(&CutContext, sizeof(CutContext));

    for (i = 1; i < ArgC; i++) {

        ArgumentUnderstood = FALSE;

        if (YoriLibIsCommandLineOption(&ArgV[i], &Arg)) {

            if (YoriLibCompareStringWithLiteralInsensitive(&Arg, _T("?")) == 0) {
                CutHelp();
                return EXIT_SUCCESS;
            } else if (YoriLibCompareStringWithLiteralInsensitive(&Arg, _T("license")) == 0) {
                YoriLibDisplayMitLicense(_T("2017-2024"));
                return EXIT_SUCCESS;
            } else if (YoriLibCompareStringWithLiteralInsensitive(&Arg, _T("b")) == 0) {
                BasicEnumeration = TRUE;
                ArgumentUnderstood = TRUE;
            } else if (YoriLibCompareStringWithLiteralInsensitive(&Arg, _T("o")) == 0) {
                if (ArgC > i + 1) {
                    if (YoriLibStringToNumber(&ArgV[i + 1], TRUE, &Temp, &CharsConsumed)) {
                        CutContext.DesiredOffset = Temp;
                        ArgumentUnderstood = TRUE;
                        i++;
                    }
                }
            } else if (YoriLibCompareStringWithLiteralInsensitive(&Arg, _T("l")) == 0) {
                if (ArgC > i + 1) {
                    if (YoriLibStringToNumber(&ArgV[i + 1], TRUE, &Temp, &CharsConsumed)) {
                        CutContext.DesiredLength = Temp;
                        ArgumentUnderstood = TRUE;
                        i++;
                    }
                }
            } else if (YoriLibCompareStringWithLiteralInsensitive(&Arg, _T("f")) == 0) {
                if (ArgC > i + 1) {
                    if (YoriLibStringToNumber(&ArgV[i + 1], TRUE, &Temp, &CharsConsumed)) {
                        if (CutContext.RawFile) {
                            YoriLibOutput(YORI_LIB_OUTPUT_STDERR, _T("cut: Field delimiting incompatible with raw file\n"));
                        } else {
                            CutContext.FieldDelimited = TRUE;
                            CutContext.FieldOfInterest = (DWORD)Temp;
                            ArgumentUnderstood = TRUE;
                            i++;
                        }
                    }
                }
            } else if (YoriLibCompareStringWithLiteralInsensitive(&Arg, _T("d")) == 0) {
                if (ArgC > i + 1) {
                    if (CutContext.RawFile) {
                        YoriLibOutput(YORI_LIB_OUTPUT_STDERR, _T("cut: Field delimiting incompatible with raw file\n"));
                    } else {
                        CutContext.FieldDelimited = TRUE;
                        CutContext.FieldSeperator = ArgV[i + 1].StartOfString;
                        ArgumentUnderstood = TRUE;
                        i++;
                    }
                }
            } else if (YoriLibCompareStringWithLiteralInsensitive(&Arg, _T("r")) == 0) {
                if (CutContext.FieldDelimited) {
                    YoriLibOutput(YORI_LIB_OUTPUT_STDERR, _T("cut: Field delimiting incompatible with raw file\n"));
                } else {
                    CutContext.RawFile = TRUE;
                    ArgumentUnderstood = TRUE;
                }
            } else if (YoriLibCompareStringWithLiteralInsensitive(&Arg, _T("s")) == 0) {
                CutContext.Recursive = TRUE;
                ArgumentUnderstood = TRUE;
            } else if (YoriLibCompareStringWithLiteralInsensitive(&Arg, _T("i")) == 0) {
                CutContext.CaseInsensitive = TRUE;
                ArgumentUnderstood = TRUE;
            } else if (YoriLibCompareStringWithLiteralInsensitive(&Arg, _T("t")) == 0) {
                if (ArgC > i + 1) {
                    if (CutContext.RawFile) {
                        YoriLibOutput(YORI_LIB_OUTPUT_STDERR, _T("cut: Matching text incompatible with raw file\n"));
                    } else {
                        YoriLibCloneString(&CutContext.MatchText, &ArgV[i + 1]);
                        ArgumentUnderstood = TRUE;
                        i++;
                    }
                }
            } else if (YoriLibCompareStringWithLiteralInsensitive(&Arg, _T("-")) == 0) {
                ArgumentUnderstood = TRUE;
                StartArg = i + 1;
                break;
            }
        } else {
            StartArg = i;
            ArgumentUnderstood = TRUE;
        }

        if (!ArgumentUnderstood) {
            YoriLibOutput(YORI_LIB_OUTPUT_STDERR, _T("cut: Argument not understood, ignored: %y\n"), &ArgV[i]);
        }
    }

    if (CutContext.FieldSeperator == NULL) {
        CutContext.FieldSeperator = _T(",");
    }

#if YORI_BUILTIN
    YoriLibCancelEnable(FALSE);
#endif

    //
    //  Attempt to enable backup privilege so an administrator can access more
    //  objects successfully.
    //

    YoriLibEnableBackupPrivilege();

    Result = EXIT_SUCCESS;

    if (StartArg == 0 || StartArg == ArgC) {
        if (YoriLibIsStdInConsole()) {
            YoriLibOutput(YORI_LIB_OUTPUT_STDERR, _T("cut: No file or pipe for input\n"));
            YoriLibFreeStringContents(&CutContext.MatchText);
            return EXIT_FAILURE;
        }
        hSource = GetStdHandle(STD_INPUT_HANDLE);
        CutFilterHandle(hSource, &CutContext);
    } else {
        WORD MatchFlags = YORILIB_FILEENUM_RETURN_FILES;
        if (CutContext.Recursive) {
            MatchFlags |= YORILIB_FILEENUM_RECURSE_BEFORE_RETURN;
        }
        if (BasicEnumeration) {
            MatchFlags |= YORILIB_FILEENUM_BASIC_EXPANSION;
        }

        for (i = StartArg; i < ArgC; i++) {

            CutContext.FilesFoundThisArg = 0;
            CutContext.SavedErrorThisArg = ERROR_SUCCESS;

            YoriLibForEachStream(&ArgV[i],
                                 MatchFlags,
                                 0,
                                 CutFileFoundCallback,
                                 CutFileEnumerateErrorCallback,
                                 &CutContext);

            if (CutContext.FilesFoundThisArg == 0) {
                YORI_STRING FullPath;
                YoriLibInitEmptyString(&FullPath);
                if (YoriLibUserStringToSingleFilePathOrDevice(&ArgV[i], TRUE, &FullPath)) {
                    CutFileFoundCallback(&FullPath, NULL, 0, &CutContext);
                    YoriLibFreeStringContents(&FullPath);
                }
                if (CutContext.SavedErrorThisArg != ERROR_SUCCESS) {
                    YoriLibOutput(YORI_LIB_OUTPUT_STDERR, _T("cut: File or directory not found: %y\n"), &ArgV[i]);
                }
            }
        }

        if (CutContext.FilesFound == 0) {
            YoriLibOutput(YORI_LIB_OUTPUT_STDERR, _T("cut: no matching files found\n"));
            Result = EXIT_FAILURE;
        }
    }

#if !YORI_BUILTIN
    YoriLibLineReadCleanupCache();
#endif
    YoriLibFreeStringContents(&CutContext.MatchText);

    return Result;
}

// vim:sw=4:ts=4:et:
