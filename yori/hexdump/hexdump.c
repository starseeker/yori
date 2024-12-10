/**
 * @file hexdump/hexdump.c
 *
 * Yori shell display a file or files in hexadecimal form
 *
 * Copyright (c) 2017-2021 Malcolm J. Smith
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

#pragma warning(disable: 4220) // Varargs matches remaining parameters

/**
 Help text to display to the user.
 */
const
CHAR strHexDumpHelpText[] =
        "\n"
        "Output the contents of one or more files in hex.\n"
        "\n"
        "HEXDUMP [-license] [-b] [-d] [-g1|-g2|-g4|-g8|-i] [-hc] [-ho]\n"
        "        [-l length] [-o offset] [-bin|-r] [-s] [-w] [<file>...]\n"
        "\n"
        "   -b             Use basic search criteria for files only\n"
        "   -bin           Process a stream of hex back into binary\n"
        "   -d             Display the differences between two files\n"
        "   -g             Number of bytes per display group\n"
        "   -hc            Hide character display\n"
        "   -ho            Hide offset within buffer\n"
        "   -i             C-style include output\n"
        "   -l             Length of the section to display\n"
        "   -o             Offset within the stream to display\n"
        "   -r             Reverse process hexdump hex back into binary\n"
        "   -s             Process files from all subdirectories\n"
        "   -w             Display wide (16 bit) characters\n";

/**
 Display usage text to the user.
 */
BOOL
HexDumpHelp(VOID)
{
    YoriLibOutput(YORI_LIB_OUTPUT_STDOUT, _T("HexDump %i.%02i\n"), YORI_VER_MAJOR, YORI_VER_MINOR);
#if YORI_BUILD_ID
    YoriLibOutput(YORI_LIB_OUTPUT_STDOUT, _T("  Build %i\n"), YORI_BUILD_ID);
#endif
    YoriLibOutput(YORI_LIB_OUTPUT_STDOUT, _T("%hs"), strHexDumpHelpText);
    return TRUE;
}

/**
 Context passed to the callback which is invoked for each file found.
 */
typedef struct _HEXDUMP_CONTEXT {

    /**
     Records the total number of files processed.
     */
    YORI_MAX_SIGNED_T FilesFound;

    /**
     Records the total number of files processed within a single command line
     argument.
     */
    YORI_MAX_SIGNED_T FilesFoundThisArg;

    /**
     Offset within each stream to display.
     */
    YORI_MAX_SIGNED_T OffsetToDisplay;

    /**
     Length within each stream to display.
     */
    YORI_MAX_SIGNED_T LengthToDisplay;

    /**
     Number of bytes to display per group.
     */
    DWORD BytesPerGroup;

    /**
     The first error encountered when enumerating objects from a single arg.
     This is used to preserve file not found/path not found errors so that
     when the program falls back to interpreting the argument as a literal,
     if that still doesn't work, this is the error code that is displayed.
     */
    DWORD SavedErrorThisArg;

    /**
     If TRUE, hide the offset display within the buffer.
     */
    BOOLEAN HideOffset;

    /**
     If TRUE, hide the character display within the buffer.
     */
    BOOLEAN HideCharacters;

    /**
     If TRUE, display characters as wide.
     */
    BOOLEAN WideCharacters;

    /**
     If TRUE, output with C-style include output.
     */
    BOOLEAN CStyleInclude;

    /**
     TRUE if file enumeration is being performed recursively; FALSE if it is
     in one directory only.
     */
    BOOLEAN Recursive;

} HEXDUMP_CONTEXT, *PHEXDUMP_CONTEXT;

/**
 Check if a character is a valid hex digit.

 @param Char Character to check.

 @return TRUE if the character is hex, FALSE if not.
 */
BOOLEAN
HexDumpIsHexDigit(
    __in TCHAR Char
    )
{
    if ((Char < '0' || Char > '9') &&
        (Char < 'a' || Char > 'f') &&
        (Char < 'A' || Char > 'F')) {

        return FALSE;
    }

    return TRUE;
}

/**
 Check if the string starts with a consecutive section of hex characters.

 @param String Pointer to the string to check.

 @param DigitsToCheck The number of characters to check if they are hex.

 @return TRUE if all digits specified are hex, FALSE if not.
 */
BOOL
HexDumpDoesStringStartWithHexDigits(
    __in PYORI_STRING String,
    __in DWORD DigitsToCheck
    )
{
    DWORD Index;
    TCHAR Char;

    if (DigitsToCheck > String->LengthInChars) {
        return FALSE;
    }

    for (Index = 0; Index < DigitsToCheck; Index++) {
        Char = String->StartOfString[Index];
        if (!HexDumpIsHexDigit(Char)) {
            return FALSE;
        }
    }

    return TRUE;
}

/**
 A structure describing the format of hex encoded text and a buffer to
 populate with binary data.
 */
typedef struct _HEXDUMP_REVERSE_CONTEXT {

    /**
     Indicates the number of characters to ignore at the beginning of the
     line. This is nonzero to ignore any offset information, which is
     meaningless here.
     */
    YORI_ALLOC_SIZE_T CharsInInputLineToIgnore;

    /**
     Indicates the number of bytes per word.  This program can process 1, 2,
     4 or 8.
     */
    UCHAR BytesPerWord;

    /**
     The number of words per line.  Since the line length is fixed at
     YORI_LIB_HEXDUMP_BYTES_PER_LINE, this is really just that value divided
     by BytesPerWord.
     */
    UCHAR WordsPerLine;

    /**
     TRUE if all whitespace should be ignored and the input should be parsed
     as a sequential hex stream.  FALSE for a regular format.
     */
    BOOLEAN NoWhitespace;

    /**
     The buffer to populate with data as each line is parsed.  This may point
     to StaticOutputBuffer below, or may be a heap allocation in binary mode.
     */
    PUCHAR OutputBuffer;

    /**
     A small buffer which is used initially.  This is all that is needed for
     reverse mode, but may be insufficient when processing arbitrary length
     binary lines.
     */
    UCHAR StaticOutputBuffer[YORI_LIB_HEXDUMP_BYTES_PER_LINE];

    /**
     The number of bytes of OutputBuffer that have been allocated.
     */
    YORI_ALLOC_SIZE_T BytesAllocated;

    /**
     The number of bytes of OutputBuffer that have been filled.  Note that
     on the final line the process needs to end when the output buffer is
     not completely filled.
     */
    YORI_ALLOC_SIZE_T BytesThisLine;
} HEXDUMP_REVERSE_CONTEXT, *PHEXDUMP_REVERSE_CONTEXT;

/**
 Detect the format of hex encoded text.

 @param Line Pointer to a line of hex encoded text to test.

 @param SupportBinary If TRUE, succeed for a series of hex digits with no
        whitespace.

 @param ReverseContext Pointer to the reverse hex dump context.  On output
        it is populated with the format of the data.

 @return TRUE to indicate success, FALSE to indicate failure including
         because the format is not understood.
 */
BOOL
HexDumpDetectReverseFormatFromLine(
    __in PYORI_STRING Line,
    __in BOOLEAN SupportBinary,
    __out PHEXDUMP_REVERSE_CONTEXT ReverseContext
    )
{
    YORI_STRING Substring;

    ReverseContext->NoWhitespace = FALSE;
    ReverseContext->CharsInInputLineToIgnore = 0;

    //
    //  Look if the line starts with an offset.  If so, we need to ignore it
    //  when converting back to binary.
    //

    if (HexDumpDoesStringStartWithHexDigits(Line, 8)) {

        // 
        //  Check for 10 chars because there's 8 of hex, followed by a colon
        //  and space.
        //

        if (Line->LengthInChars >= 10) {
            if (Line->StartOfString[8] == ':') {
                ReverseContext->CharsInInputLineToIgnore = 8 + 2;
            } else if (Line->StartOfString[8] == '`') {

                //
                //  If the 9th char is a `, that implies it's a seperator
                //  between two 8 char sets of hex.  Verify that assumption.
                //

                YoriLibInitEmptyString(&Substring);
                Substring.StartOfString = &Line->StartOfString[9];
                Substring.LengthInChars = Line->LengthInChars - 9;
                if (HexDumpDoesStringStartWithHexDigits(&Substring, 8)) {
                    if (Substring.LengthInChars >= 10) {
                        if (Substring.StartOfString[8] == ':') {
                            ReverseContext->CharsInInputLineToIgnore = 8 * 2 + 3;
                        }
                    }
                }
            }
        }
    }

    if (Line->LengthInChars <= ReverseContext->CharsInInputLineToIgnore) {
        return FALSE;
    }

    YoriLibInitEmptyString(&Substring);
    Substring.StartOfString = &Line->StartOfString[ReverseContext->CharsInInputLineToIgnore];
    Substring.LengthInChars = Line->LengthInChars - ReverseContext->CharsInInputLineToIgnore;


    //
    //  8 hex digits with a space == DWORDs
    //

    if (Substring.LengthInChars >= 2 * 4 &&
        HexDumpDoesStringStartWithHexDigits(&Substring, 2 * 4) &&
        (Substring.LengthInChars == 2 * 4 || Substring.StartOfString[2 * 4] == ' ')) {

        ReverseContext->BytesPerWord = 4;

    //
    //  4 hex digits with a space == WORDs
    //
    } else if (Substring.LengthInChars >= 2 * 2 &&
               HexDumpDoesStringStartWithHexDigits(&Substring, 2 * 2) &&
               (Substring.LengthInChars == 2 * 2 || Substring.StartOfString[2 * 2] == ' ')) {

        ReverseContext->BytesPerWord = 2;

    //
    //  2 hex digits with a space == BYTEs
    //
    } else if (Substring.LengthInChars >= 2 * 1 &&
               HexDumpDoesStringStartWithHexDigits(&Substring, 2 * 1) &&
               (Substring.LengthInChars == 2 * 1 || Substring.StartOfString[2 * 1] == ' ')) {

        ReverseContext->BytesPerWord = 1;
    //
    //  8 hex digits, backquote, 8 hex digits == QWORDs
    //
    } else if (Substring.LengthInChars >= 2 * 4 + 1 + 2 * 4 &&
        HexDumpDoesStringStartWithHexDigits(&Substring, 2 * 4) &&
        Substring.StartOfString[2 * 4] == '`') {

        Substring.LengthInChars -= 2 * 4 + 1;
        Substring.StartOfString += 2 * 4 + 1;

        if (HexDumpDoesStringStartWithHexDigits(&Substring, 2 * 4) &&
            (Substring.LengthInChars == 2 * 4 || Substring.StartOfString[2 * 4] == ' ')) {

            ReverseContext->BytesPerWord = 8;
        }
    //
    //  2 hex digits, no space and binary support...just assume bytes
    //
    } else if (SupportBinary &&
               Substring.LengthInChars >= 2 * 1 &&
               HexDumpDoesStringStartWithHexDigits(&Substring, 2 * 1)) {
        ReverseContext->BytesPerWord = 1;
        ReverseContext->NoWhitespace = TRUE;
    }

    //
    //  If we don't know how to parse it, fail
    //

    if (ReverseContext->BytesPerWord == 0) {
        return FALSE;
    }

    ReverseContext->WordsPerLine = (UCHAR)(YORI_LIB_HEXDUMP_BYTES_PER_LINE / ReverseContext->BytesPerWord);

    return TRUE;
}

/**
 Process a word of hex encoded text into binary.

 @param String Pointer to a string containing a word of hex encoded text.

 @param ReverseContext Pointer to the reverse hex dump context.  On output
        it is populated with the binary form of data.

 @return TRUE to indicate success, FALSE to indicate failure.
 */
BOOL
HexDumpReverseParseByte(
    __in PYORI_STRING String,
    __inout PHEXDUMP_REVERSE_CONTEXT ReverseContext
    )
{
    UCHAR Value = 0;
    UCHAR ThisByte = 0;
    TCHAR SourceChar;
    DWORD BytePart;

    if (String->LengthInChars < (YORI_ALLOC_SIZE_T)ReverseContext->BytesPerWord * 2) {
        return FALSE;
    }

    ThisByte = 0;
    for (BytePart = 0; BytePart < 2; BytePart++) {
        ThisByte = (UCHAR)(ThisByte << 4);
        SourceChar = String->StartOfString[BytePart];
        if (SourceChar >= 'a' && SourceChar <= 'f') {
            ThisByte = (UCHAR)(ThisByte + SourceChar - 'a' + 10);
        } else if (SourceChar >= 'A' && SourceChar <= 'F') {
            ThisByte = (UCHAR)(ThisByte + SourceChar - 'A' + 10);
        } else if (SourceChar >= '0' && SourceChar <= '9') {
            ThisByte = (UCHAR)(ThisByte + SourceChar - '0');
        } else {
            return FALSE;
        }
    }
    Value = ThisByte;

    ASSERT(ReverseContext->BytesThisLine + sizeof(Value) <= ReverseContext->BytesAllocated);
    memcpy(&ReverseContext->OutputBuffer[ReverseContext->BytesThisLine], &Value, sizeof(Value));
    ReverseContext->BytesThisLine += sizeof(Value);

    return TRUE;
}

/**
 Process a word of hex encoded text into binary.

 @param String Pointer to a string containing a word of hex encoded text.

 @param ReverseContext Pointer to the reverse hex dump context.  On output
        it is populated with the binary form of data.

 @return TRUE to indicate success, FALSE to indicate failure.
 */
BOOL
HexDumpReverseParseWord(
    __in PYORI_STRING String,
    __inout PHEXDUMP_REVERSE_CONTEXT ReverseContext
    )
{
    WORD Value = 0;
    UCHAR ThisByte = 0;
    TCHAR SourceChar;
    DWORD Index;
    DWORD BytePart;

    if (String->LengthInChars < (YORI_ALLOC_SIZE_T)ReverseContext->BytesPerWord * 2) {
        return FALSE;
    }

    for (Index = 0; Index < ReverseContext->BytesPerWord; Index++) {
        Value = (WORD)(Value << 8);
        ThisByte = 0;
        for (BytePart = 0; BytePart < 2; BytePart++) {
            ThisByte = (UCHAR)(ThisByte << 4);
            SourceChar = String->StartOfString[Index * 2 + BytePart];
            if (SourceChar >= 'a' && SourceChar <= 'f') {
                ThisByte = (UCHAR)(ThisByte + SourceChar - 'a' + 10);
            } else if (SourceChar >= 'A' && SourceChar <= 'F') {
                ThisByte = (UCHAR)(ThisByte + SourceChar - 'A' + 10);
            } else if (SourceChar >= '0' && SourceChar <= '9') {
                ThisByte = (UCHAR)(ThisByte + SourceChar - '0');
            } else {
                return FALSE;
            }
        }
        Value = (WORD)(Value | ThisByte);
    }

    ASSERT(ReverseContext->BytesThisLine + sizeof(Value) <= ReverseContext->BytesAllocated);
    memcpy(&ReverseContext->OutputBuffer[ReverseContext->BytesThisLine], &Value, sizeof(Value));
    ReverseContext->BytesThisLine += sizeof(Value);

    return TRUE;
}

/**
 Process a word of hex encoded text into binary.

 @param String Pointer to a string containing a word of hex encoded text.

 @param ReverseContext Pointer to the reverse hex dump context.  On output
        it is populated with the binary form of data.

 @return TRUE to indicate success, FALSE to indicate failure.
 */
BOOL
HexDumpReverseParseDword(
    __in PYORI_STRING String,
    __inout PHEXDUMP_REVERSE_CONTEXT ReverseContext
    )
{
    DWORD Value = 0;
    UCHAR ThisByte = 0;
    TCHAR SourceChar;
    DWORD Index;
    DWORD BytePart;

    if (String->LengthInChars < (YORI_ALLOC_SIZE_T)ReverseContext->BytesPerWord * 2) {
        return FALSE;
    }

    for (Index = 0; Index < ReverseContext->BytesPerWord; Index++) {
        Value = Value << 8;
        ThisByte = 0;
        for (BytePart = 0; BytePart < 2; BytePart++) {
            ThisByte = (UCHAR)(ThisByte << 4);
            SourceChar = String->StartOfString[Index * 2 + BytePart];
            if (SourceChar >= 'a' && SourceChar <= 'f') {
                ThisByte = (UCHAR)(ThisByte + SourceChar - 'a' + 10);
            } else if (SourceChar >= 'A' && SourceChar <= 'F') {
                ThisByte = (UCHAR)(ThisByte + SourceChar - 'A' + 10);
            } else if (SourceChar >= '0' && SourceChar <= '9') {
                ThisByte = (UCHAR)(ThisByte + SourceChar - '0');
            } else {
                return FALSE;
            }
        }
        Value = Value | ThisByte;
    }

    ASSERT(ReverseContext->BytesThisLine + sizeof(Value) <= ReverseContext->BytesAllocated);
    memcpy(&ReverseContext->OutputBuffer[ReverseContext->BytesThisLine], &Value, sizeof(Value));
    ReverseContext->BytesThisLine += sizeof(Value);

    return TRUE;
}

/**
 Process a word of hex encoded text into binary.

 @param String Pointer to a string containing a word of hex encoded text.

 @param ReverseContext Pointer to the reverse hex dump context.  On output
        it is populated with the binary form of data.

 @return TRUE to indicate success, FALSE to indicate failure.
 */
BOOL
HexDumpReverseParseDwordLong(
    __in PYORI_STRING String,
    __inout PHEXDUMP_REVERSE_CONTEXT ReverseContext
    )
{
    DWORDLONG Value = 0;
    UCHAR ThisByte = 0;
    TCHAR SourceChar;
    UCHAR Index;
    UCHAR BytePart;
    UCHAR ByteShift = 0;

    if (String->LengthInChars < (YORI_ALLOC_SIZE_T)ReverseContext->BytesPerWord * 2) {
        return FALSE;
    }

    for (Index = 0; Index < ReverseContext->BytesPerWord + 1; Index++) {
        if (Index == 4 && String->StartOfString[Index * 2] == '`') {
            ByteShift++;
            continue;
        }
        Value = Value << 8;
        ThisByte = 0;
        for (BytePart = 0; BytePart < 2; BytePart++) {
            ThisByte = (UCHAR)(ThisByte << 4);
            SourceChar = String->StartOfString[Index * 2 + BytePart - ByteShift];
            if (SourceChar >= 'a' && SourceChar <= 'f') {
                ThisByte = (UCHAR)(ThisByte + SourceChar - 'a' + 10);
            } else if (SourceChar >= 'A' && SourceChar <= 'F') {
                ThisByte = (UCHAR)(ThisByte + SourceChar - 'A' + 10);
            } else if (SourceChar >= '0' && SourceChar <= '9') {
                ThisByte = (UCHAR)(ThisByte + SourceChar - '0');
            } else {
                return FALSE;
            }
        }
        Value = Value | ThisByte;
    }

    ASSERT(ReverseContext->BytesThisLine + sizeof(Value) <= ReverseContext->BytesAllocated);
    memcpy(&ReverseContext->OutputBuffer[ReverseContext->BytesThisLine], &Value, sizeof(Value));
    ReverseContext->BytesThisLine += sizeof(Value);

    return TRUE;
}

/**
 Process a line of hex encoded text into binary.  The format must have been
 determined prior to this point.

 @param Line Pointer to a line of hex encoded text.

 @param ReverseContext Pointer to the reverse hex dump context.  On input
        indicates the format and it is populated with the binary form on
        output.

 @return TRUE to indicate success, FALSE to indicate failure.
 */
BOOLEAN
HexDumpReverseParseLine(
    __in PYORI_STRING Line,
    __inout PHEXDUMP_REVERSE_CONTEXT ReverseContext
    )
{
    YORI_STRING Substring;
    YORI_ALLOC_SIZE_T StartChar;
    YORI_ALLOC_SIZE_T Index;
    BOOLEAN StopProcessing = FALSE;

    YoriLibInitEmptyString(&Substring);
    if (Line->LengthInChars < ReverseContext->CharsInInputLineToIgnore) {
        return FALSE;
    }

    ReverseContext->BytesThisLine = 0;

    for (Index = 0; Index < ReverseContext->WordsPerLine; Index++) {
        StartChar = ReverseContext->CharsInInputLineToIgnore + Index * (ReverseContext->BytesPerWord * 2 + 1);

        //
        //  8 byte words have a seperator, so they consist of 17 raw chars
        //

        if (ReverseContext->BytesPerWord == 8 && Index > 0) {
            StartChar = StartChar + Index;
        }

        if (Line->LengthInChars <= StartChar) {
            return TRUE;
        }

        Substring.StartOfString = &Line->StartOfString[StartChar];
        Substring.LengthInChars = Line->LengthInChars - StartChar;

        switch(ReverseContext->BytesPerWord) {
            case 1:
                if (!HexDumpReverseParseByte(&Substring, ReverseContext)) {
                    StopProcessing = TRUE;
                }
                break;
            case 2:
                if (!HexDumpReverseParseWord(&Substring, ReverseContext)) {
                    StopProcessing = TRUE;
                }
                break;
            case 4:
                if (!HexDumpReverseParseDword(&Substring, ReverseContext)) {
                    StopProcessing = TRUE;
                }
                break;
            case 8:
                if (!HexDumpReverseParseDwordLong(&Substring, ReverseContext)) {
                    StopProcessing = TRUE;
                }
                break;
            default:
                return FALSE;
        }

        if (StopProcessing) {
            return TRUE;
        }
    }

    return TRUE;
}

/**
 Convert a single stream of hex encoded input into binary output.  The format
 of the input is detected heuristically, but is expected to be one that can
 be output by hexdump.  That implies it may have a leading offset field or
 characters at the end of each line.  For that reason, this is not suitable
 for interpreting a flat array of hex data.

 @param hSource Handle to a source stream containing hex encoded data.

 @param HexDumpContext Pointer to hex dump context.  Currently unused for
        reverse processing.

 @return TRUE to indicate success, FALSE to indicate failure.
 */
BOOL
HexDumpReverseProcessStream(
    __in HANDLE hSource,
    __in PHEXDUMP_CONTEXT HexDumpContext
    )
{
    PVOID LineContext = NULL;
    YORI_STRING LineString;
    HANDLE OutputHandle;
    DWORD BytesWritten;
    HEXDUMP_REVERSE_CONTEXT ReverseContext;

    YoriLibInitEmptyString(&LineString);
    HexDumpContext->FilesFound++;
    HexDumpContext->FilesFoundThisArg++;

    if (!YoriLibReadLineToString(&LineString, &LineContext, hSource)) {
        return TRUE;
    }

    if (!HexDumpDetectReverseFormatFromLine(&LineString, FALSE, &ReverseContext)) {
        YoriLibOutput(YORI_LIB_OUTPUT_STDERR, _T("hexdump: not a stream of hex digits\n"));
        YoriLibLineReadCloseOrCache(LineContext);
        YoriLibFreeStringContents(&LineString);
        return FALSE;
    }

    ReverseContext.OutputBuffer = ReverseContext.StaticOutputBuffer;
    ReverseContext.BytesAllocated = sizeof(ReverseContext.StaticOutputBuffer);
    OutputHandle = GetStdHandle(STD_OUTPUT_HANDLE);

    while (TRUE) {

        if (!HexDumpReverseParseLine(&LineString, &ReverseContext)) {
            break;
        }

        WriteFile(OutputHandle, ReverseContext.OutputBuffer, ReverseContext.BytesThisLine, &BytesWritten, NULL);

        if (!YoriLibReadLineToString(&LineString, &LineContext, hSource)) {
            break;
        }
    }

    YoriLibLineReadCloseOrCache(LineContext);
    YoriLibFreeStringContents(&LineString);

    return TRUE;
}

/**
 Process a line of hex encoded text into binary.  The format must have been
 determined prior to this point.

 @param Line Pointer to a line of hex encoded text.

 @param ErrorChar On failure, updated to indicate the location of the parse
        error.

 @param ReverseContext Pointer to the reverse hex dump context.  On input
        indicates the format and it is populated with the binary form on
        output.

 @return TRUE to indicate success, FALSE to indicate failure.
 */
BOOL
HexDumpBinaryParseLine(
    __in PYORI_STRING Line,
    __out_opt PYORI_ALLOC_SIZE_T ErrorChar,
    __inout PHEXDUMP_REVERSE_CONTEXT ReverseContext
    )
{
    YORI_STRING Substring;
    YORI_ALLOC_SIZE_T StartChar;
    YORI_ALLOC_SIZE_T Index;
    BOOL Result;

    if (ErrorChar != NULL) {
        *ErrorChar = 0;
    }

    YoriLibInitEmptyString(&Substring);

    //
    //  Trim trailing spaces.
    //

    while (Line->LengthInChars > 0 &&
           Line->StartOfString[Line->LengthInChars - 1] == ' ') {

        Line->LengthInChars--;
    }

    ReverseContext->BytesThisLine = 0;

    Index = 0;
    Result = TRUE;

    while(TRUE) {

        if (ReverseContext->NoWhitespace) {
            StartChar = Index * (ReverseContext->BytesPerWord * 2);
        } else {
            StartChar = Index * (ReverseContext->BytesPerWord * 2 + 1);
        }

        //
        //  8 byte words have a seperator, so they consist of 17 raw chars
        //

        if (ReverseContext->BytesPerWord == 8 && Index > 0) {
            StartChar = StartChar + Index;
        }

        //
        //  If the first char to parse is at the end of the line, this line
        //  is finished successfully.  Note that it might be beyond the end
        //  of the line due to a space, but that still constitutes success.
        //

        if (StartChar >= Line->LengthInChars) {
            break;
        }

        //
        //  If the line has more characters but not enough for a word, that
        //  indicates parse failure.  We don't know what these characters
        //  are for.
        //

        if (StartChar + ReverseContext->BytesPerWord * 2 > Line->LengthInChars) {
            Result = FALSE;
            break;
        }

        //
        //  If there is more data to output than the buffer can hold, grow
        //  the buffer.  Since the buffer is reused across lines, grow it
        //  substantially to avoid doing this too often.
        //

        if (ReverseContext->BytesThisLine + ReverseContext->BytesPerWord > ReverseContext->BytesAllocated) {
            PUCHAR NewBuffer;
            DWORD BytesDesired;
            DWORD BytesRequired;
            YORI_ALLOC_SIZE_T NewLength;

            BytesRequired = ReverseContext->BytesThisLine + ReverseContext->BytesPerWord;
            BytesDesired = ReverseContext->BytesAllocated;
            BytesDesired = BytesDesired * 4;
            if (BytesDesired < BytesRequired) {
                BytesDesired = BytesRequired;
            }

            if (BytesDesired < 0x800) {
                BytesDesired = 0x800;
            }

            NewLength = YoriLibMaximumAllocationInRange(BytesRequired, BytesDesired);
            if (NewLength == 0) {
                Result = FALSE;
                break;
            }

            NewBuffer = YoriLibMalloc(NewLength);
            if (NewBuffer == NULL) {
                Result = FALSE;
                break;
            }

            memcpy(NewBuffer, ReverseContext->OutputBuffer, ReverseContext->BytesThisLine);
            if (ReverseContext->OutputBuffer != ReverseContext->StaticOutputBuffer) {
                YoriLibFree(ReverseContext->OutputBuffer);
            }

            ReverseContext->OutputBuffer = NewBuffer;
            ReverseContext->BytesAllocated = NewLength;
        }

        Substring.StartOfString = &Line->StartOfString[StartChar];
        Substring.LengthInChars = Line->LengthInChars - StartChar;

        switch(ReverseContext->BytesPerWord) {
            case 1:
                if (!HexDumpReverseParseByte(&Substring, ReverseContext)) {
                    Result = FALSE;
                }
                break;
            case 2:
                if (!HexDumpReverseParseWord(&Substring, ReverseContext)) {
                    Result = FALSE;
                }
                break;
            case 4:
                if (!HexDumpReverseParseDword(&Substring, ReverseContext)) {
                    Result = FALSE;
                }
                break;
            case 8:
                if (!HexDumpReverseParseDwordLong(&Substring, ReverseContext)) {
                    Result = FALSE;
                }
                break;
            default:
                Result = FALSE;
                break;
        }

        if (Result == FALSE) {
            break;
        }

        Index++;
    }

    if (Result == FALSE && ErrorChar != NULL) {
        *ErrorChar = StartChar;
    }

    return Result;
}

/**
 Convert a single stream of hex encoded input into binary output.  The format
 of the input can contain any supported word length.

 @param hSource Handle to a source stream containing hex encoded data.

 @param HexDumpContext Pointer to hex dump context.  Currently unused for
        reverse processing.

 @return TRUE to indicate success, FALSE to indicate failure.
 */
BOOL
HexDumpBinaryProcessStream(
    __in HANDLE hSource,
    __in PHEXDUMP_CONTEXT HexDumpContext
    )
{
    PVOID LineContext = NULL;
    YORI_STRING LineString;
    HANDLE OutputHandle;
    DWORD BytesWritten;
    DWORDLONG LineNumber;
    HEXDUMP_REVERSE_CONTEXT ReverseContext;
    YORI_ALLOC_SIZE_T ErrorChar;
    YORI_ALLOC_SIZE_T Index;
    YORI_ALLOC_SIZE_T DigitsPerWord;

    YoriLibInitEmptyString(&LineString);
    HexDumpContext->FilesFound++;
    HexDumpContext->FilesFoundThisArg++;

    LineNumber = 0;

    if (!YoriLibReadLineToString(&LineString, &LineContext, hSource)) {
        return TRUE;
    }

    LineNumber++;

    if (!HexDumpDetectReverseFormatFromLine(&LineString, TRUE, &ReverseContext)) {
        YoriLibOutput(YORI_LIB_OUTPUT_STDERR, _T("hexdump: not a stream of hex digits, line %lli\n"), LineNumber);
        YoriLibLineReadCloseOrCache(LineContext);
        YoriLibFreeStringContents(&LineString);
        return FALSE;
    }

    if (ReverseContext.CharsInInputLineToIgnore != 0) {
        YoriLibOutput(YORI_LIB_OUTPUT_STDERR, _T("hexdump: not a stream of hex digits, line %lli\n"), LineNumber);
        YoriLibLineReadCloseOrCache(LineContext);
        YoriLibFreeStringContents(&LineString);
        return FALSE;
    }
    ReverseContext.OutputBuffer = ReverseContext.StaticOutputBuffer;
    ReverseContext.BytesAllocated = sizeof(ReverseContext.StaticOutputBuffer);

    OutputHandle = GetStdHandle(STD_OUTPUT_HANDLE);

    while (TRUE) {

        if (!HexDumpBinaryParseLine(&LineString, &ErrorChar, &ReverseContext)) {
            if (ErrorChar < LineString.LengthInChars) {
                DigitsPerWord = 2 * ReverseContext.BytesPerWord;
                if (ErrorChar + DigitsPerWord > LineString.LengthInChars) {
                    YoriLibOutput(YORI_LIB_OUTPUT_STDERR, _T("hexdump: insufficient digits for a word, line %lli position %i\n"), LineNumber, ErrorChar);
                } else {
                    for (Index = 0; Index < DigitsPerWord; Index++) {
                        TCHAR Char;
                        Char = LineString.StartOfString[ErrorChar + Index];
                        if (!HexDumpIsHexDigit(Char)) {
                            if (YoriLibIsCharPrintable(Char)) {
                                YoriLibOutput(YORI_LIB_OUTPUT_STDERR, _T("hexdump: not a hex digit, line %lli position %i '%c'\n"), LineNumber, ErrorChar + Index, Char);
                            } else {
                                YoriLibOutput(YORI_LIB_OUTPUT_STDERR, _T("hexdump: not a hex digit, line %lli position %i <unprintable>\n"), LineNumber, ErrorChar + Index);
                            }
                            break;
                        }
                    }
                    if (Index == DigitsPerWord) {
                        YoriLibOutput(YORI_LIB_OUTPUT_STDERR, _T("hexdump: not a stream of hex digits, line %lli position %i\n"), LineNumber, ErrorChar);
                    }
                }
            }
            break;
        }

        if (ReverseContext.BytesThisLine > 0) {
            WriteFile(OutputHandle, ReverseContext.OutputBuffer, ReverseContext.BytesThisLine, &BytesWritten, NULL);
        }

        if (!YoriLibReadLineToString(&LineString, &LineContext, hSource)) {
            break;
        }

        LineNumber++;
    }

    if (ReverseContext.OutputBuffer != ReverseContext.StaticOutputBuffer) {
        YoriLibFree(ReverseContext.OutputBuffer);
        ReverseContext.OutputBuffer = NULL;
    }

    YoriLibLineReadCloseOrCache(LineContext);
    YoriLibFreeStringContents(&LineString);

    return TRUE;
}


/**
 Process a single opened stream, enumerating through all lines and displaying
 the set requested by the user.

 @param hSource The opened source stream.

 @param HexDumpContext Pointer to context information specifying which lines to
        display.
 
 @return TRUE to indicate success, FALSE to indicate failure.
 */
BOOL
HexDumpProcessStream(
    __in HANDLE hSource,
    __in PHEXDUMP_CONTEXT HexDumpContext
    )
{
    PUCHAR Buffer;
    YORI_ALLOC_SIZE_T BufferSize;
    YORI_ALLOC_SIZE_T BufferReadOffset;
    YORI_ALLOC_SIZE_T BufferDisplayOffset;
    DWORD BytesReturned;
    YORI_ALLOC_SIZE_T LengthToDisplay;
    DWORD DisplayFlags;
    DWORD FileType;
    DWORD SectorSize;
    LARGE_INTEGER StreamOffset;
    BOOLEAN LimitDisplayToEvenLine;

    HexDumpContext->FilesFound++;
    HexDumpContext->FilesFoundThisArg++;

    BufferSize = YoriLibMaximumAllocationInRange(16 * 1024, 64 * 1024);
    Buffer = YoriLibMalloc(BufferSize);
    if (Buffer == NULL) {
        return FALSE;
    }
    DisplayFlags = 0;
    SectorSize = 0;
    if (!HexDumpContext->HideOffset) {
        DisplayFlags |= YORI_LIB_HEX_FLAG_DISPLAY_LARGE_OFFSET;
    }
    if (!HexDumpContext->HideCharacters) {
        if (HexDumpContext->WideCharacters) {
            DisplayFlags |= YORI_LIB_HEX_FLAG_DISPLAY_WCHARS;
        } else {
            DisplayFlags |= YORI_LIB_HEX_FLAG_DISPLAY_CHARS;
        }
    }
    if (HexDumpContext->CStyleInclude) {
        DisplayFlags |= YORI_LIB_HEX_FLAG_C_STYLE;
    }

    //
    //  If it's a file, start at the offset requested by the user.  If it's
    //  not a file (it's a pipe), the only way to move forward is by
    //  reading.
    //

    FileType = GetFileType(hSource);

    StreamOffset.QuadPart = 0;
    if (FileType != FILE_TYPE_PIPE) {
        SectorSize = YoriLibGetHandleSectorSize(hSource);
        StreamOffset.QuadPart = HexDumpContext->OffsetToDisplay;

        if (SectorSize != 0) {
            StreamOffset.LowPart = StreamOffset.LowPart & (~(SectorSize - 1));
        }

        if (!SetFilePointer(hSource, StreamOffset.LowPart, &StreamOffset.HighPart, FILE_BEGIN)) {
            StreamOffset.QuadPart = 0;
        }
    }

    BufferReadOffset = 0;

    while (TRUE) {

        //
        //  Read a block of data.  On a pipe, this will block.
        //

        BytesReturned = 0;
        ASSERT(BufferReadOffset < BufferSize);
        if (!ReadFile(hSource, Buffer + BufferReadOffset, BufferSize - BufferReadOffset, &BytesReturned, NULL)) {
            BytesReturned = 0;
        }

        //
        //  Add back whatever data was carried over from previous reads to
        //  the amount from this read.  If we don't have data from either
        //  source despite blocking, the operation is complete.
        //

        BytesReturned = BytesReturned + BufferReadOffset;
        if (BytesReturned == 0) {
            break;
        }

        //
        //  If we haven't reached the starting point to display, loop back
        //  and read more.
        //

        if (StreamOffset.QuadPart + BytesReturned <= HexDumpContext->OffsetToDisplay) {
            StreamOffset.QuadPart += BytesReturned;
            BufferReadOffset = 0;
            continue;
        }

        LengthToDisplay = (YORI_ALLOC_SIZE_T)BytesReturned;

        //
        //  If the starting point to display is partway through the buffer,
        //  find the offset within the buffer to start displaying and cap
        //  the number of characters to display.
        //

        BufferDisplayOffset = 0;
        if (StreamOffset.QuadPart < HexDumpContext->OffsetToDisplay) {
            BufferDisplayOffset = (YORI_ALLOC_SIZE_T)(HexDumpContext->OffsetToDisplay - StreamOffset.QuadPart);
            LengthToDisplay = LengthToDisplay - BufferDisplayOffset;
        }

        ASSERT(BufferDisplayOffset + LengthToDisplay == (YORI_ALLOC_SIZE_T)BytesReturned);

        //
        //  If the number of bytes that the user requested to display is
        //  longer than the amount we have, cap the amount to display to
        //  what the user requested.
        //

        LimitDisplayToEvenLine = TRUE;
        if (HexDumpContext->LengthToDisplay != 0) {
            if (StreamOffset.QuadPart + BufferDisplayOffset + LengthToDisplay >= HexDumpContext->OffsetToDisplay + HexDumpContext->LengthToDisplay) {
                LengthToDisplay = (YORI_ALLOC_SIZE_T)(HexDumpContext->OffsetToDisplay + HexDumpContext->LengthToDisplay - StreamOffset.QuadPart - BufferDisplayOffset);
                LimitDisplayToEvenLine = FALSE;
            }
        }

        //
        //  If ReadFile didn't return any new data, but we still have data
        //  leftover, then display what we have.
        //

        if (BytesReturned == BufferReadOffset) {
            LimitDisplayToEvenLine = FALSE;
        }

        //
        //  Try to display a multiple of HEXDUMP_BYTES_PER_LINE.  If there's
        //  more data, count how many bytes are leftover.  This number will
        //  be copied to the beginning of the buffer after display.
        //

        if (LimitDisplayToEvenLine) {
            BufferReadOffset = LengthToDisplay % YORI_LIB_HEXDUMP_BYTES_PER_LINE;
            LengthToDisplay = LengthToDisplay - BufferReadOffset;
        }

        //
        //  Display the buffer at the display offset for the length to
        //  display.
        //

        if (LengthToDisplay > 0) {
            if (!YoriLibHexDump((LPCSTR)&Buffer[BufferDisplayOffset], StreamOffset.QuadPart + BufferDisplayOffset, LengthToDisplay, HexDumpContext->BytesPerGroup, DisplayFlags)) {
                break;
            }
        }

        //
        //  If there is leftover data, copy it to the beginning of the buffer.
        //

        if (BufferReadOffset > 0 && BufferDisplayOffset + LengthToDisplay != 0) {
            memmove(Buffer, &Buffer[BufferDisplayOffset + LengthToDisplay], BufferReadOffset);
        }

        //
        //  Move the stream forward to the end of the buffer that was
        //  displayed.  There may be more data in the buffer, which will
        //  be handled on the next loop iteration.
        //

        StreamOffset.QuadPart += BufferDisplayOffset + LengthToDisplay;

        if (!LimitDisplayToEvenLine) {
            break;
        }

        if (HexDumpContext->LengthToDisplay != 0 &&
            StreamOffset.QuadPart >= HexDumpContext->OffsetToDisplay + HexDumpContext->LengthToDisplay) {

            break;
        }
    }

    YoriLibFree(Buffer);

    return TRUE;
}

/**
 Prototype for a function which can perform the user's requested action on a
 successfully opened stream.
 */
typedef BOOL HEXDUMP_PROCESS_STREAM_FN(HANDLE hSource, PHEXDUMP_CONTEXT HexDumpContext);

/**
 Pointer to a function which can perform the user's requested action on a
 successfully opened stream.
 */
typedef HEXDUMP_PROCESS_STREAM_FN *PHEXDUMP_PROCESS_STREAM_FN;

/**
 A callback that is invoked when a file is found that matches a search criteria
 specified in the set of strings to enumerate.  This function takes a function
 pointer to invoke in order to process any successfully opened file.

 @param FilePath Pointer to the file path that was found.

 @param FileInfo Information about the file.  Can be NULL if the file being
        opened was not found by enumeration.

 @param HexDumpContext Pointer to the hexdump context structure indicating the
        action to perform and populated with the file and line count found.

 @param Fn Pointer to a function to invoke if the file can be successfully
        opened.  This function is expected to perform the requested processing
        on the file.

 @return TRUE to continute enumerating, FALSE to abort.
 */
BOOL
HexDumpCommonFileFoundCallback(
    __in PYORI_STRING FilePath,
    __in_opt PWIN32_FIND_DATA FileInfo,
    __in PHEXDUMP_CONTEXT HexDumpContext,
    __in PHEXDUMP_PROCESS_STREAM_FN Fn
    )
{
    HANDLE FileHandle;

    ASSERT(YoriLibIsStringNullTerminated(FilePath));

    if (FileInfo == NULL ||
        (FileInfo->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {

        FileHandle = CreateFile(FilePath->StartOfString,
                                GENERIC_READ,
                                FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                NULL,
                                OPEN_EXISTING,
                                FILE_ATTRIBUTE_NORMAL | FILE_FLAG_BACKUP_SEMANTICS,
                                NULL);

        if (FileHandle == NULL || FileHandle == INVALID_HANDLE_VALUE) {
            if (HexDumpContext->SavedErrorThisArg == ERROR_SUCCESS) {
                DWORD LastError = GetLastError();
                LPTSTR ErrText = YoriLibGetWinErrorText(LastError);
                YoriLibOutput(YORI_LIB_OUTPUT_STDERR, _T("hexdump: open of %y failed: %s"), FilePath, ErrText);
                YoriLibFreeWinErrorText(ErrText);
            }
            return TRUE;
        }

        HexDumpContext->SavedErrorThisArg = ERROR_SUCCESS;
        Fn(FileHandle, HexDumpContext);

        CloseHandle(FileHandle);
    }

    return TRUE;
}

/**
 A callback that is invoked when a file is found that matches a search criteria
 specified in the set of strings to enumerate.

 @param FilePath Pointer to the file path that was found.

 @param FileInfo Information about the file.  Can be NULL if the file being
        opened was not found by enumeration.

 @param Depth Specifies recursion depth.  Ignored in this application.

 @param Context Pointer to the hexdump context structure indicating the
        action to perform and populated with the file and line count found.

 @return TRUE to continute enumerating, FALSE to abort.
 */
BOOL
HexDumpFileFoundCallback(
    __in PYORI_STRING FilePath,
    __in_opt PWIN32_FIND_DATA FileInfo,
    __in DWORD Depth,
    __in PVOID Context
    )
{
    PHEXDUMP_CONTEXT HexDumpContext = (PHEXDUMP_CONTEXT)Context;
    UNREFERENCED_PARAMETER(Depth);
    return HexDumpCommonFileFoundCallback(FilePath, FileInfo, HexDumpContext, HexDumpProcessStream);
}

/**
 A callback that is invoked when a file is found that matches a search criteria
 specified in the set of strings to enumerate.

 @param FilePath Pointer to the file path that was found.

 @param FileInfo Information about the file.  Can be NULL if the file being
        opened was not found by enumeration.

 @param Depth Specifies recursion depth.  Ignored in this application.

 @param Context Pointer to the hexdump context structure indicating the
        action to perform and populated with the file and line count found.

 @return TRUE to continute enumerating, FALSE to abort.
 */
BOOL
HexDumpReverseFileFoundCallback(
    __in PYORI_STRING FilePath,
    __in_opt PWIN32_FIND_DATA FileInfo,
    __in DWORD Depth,
    __in PVOID Context
    )
{
    PHEXDUMP_CONTEXT HexDumpContext = (PHEXDUMP_CONTEXT)Context;
    UNREFERENCED_PARAMETER(Depth);
    return HexDumpCommonFileFoundCallback(FilePath, FileInfo, HexDumpContext, HexDumpReverseProcessStream);
}

/**
 A callback that is invoked when a file is found that matches a search criteria
 specified in the set of strings to enumerate.

 @param FilePath Pointer to the file path that was found.

 @param FileInfo Information about the file.  Can be NULL if the file being
        opened was not found by enumeration.

 @param Depth Specifies recursion depth.  Ignored in this application.

 @param Context Pointer to the hexdump context structure indicating the
        action to perform and populated with the file and line count found.

 @return TRUE to continute enumerating, FALSE to abort.
 */
BOOL
HexDumpBinaryFileFoundCallback(
    __in PYORI_STRING FilePath,
    __in_opt PWIN32_FIND_DATA FileInfo,
    __in DWORD Depth,
    __in PVOID Context
    )
{
    PHEXDUMP_CONTEXT HexDumpContext = (PHEXDUMP_CONTEXT)Context;
    UNREFERENCED_PARAMETER(Depth);
    return HexDumpCommonFileFoundCallback(FilePath, FileInfo, HexDumpContext, HexDumpBinaryProcessStream);
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
HexDumpFileEnumerateErrorCallback(
    __in PYORI_STRING FilePath,
    __in DWORD ErrorCode,
    __in DWORD Depth,
    __in PVOID Context
    )
{
    YORI_STRING UnescapedFilePath;
    BOOL Result = FALSE;
    PHEXDUMP_CONTEXT HexDumpContext = (PHEXDUMP_CONTEXT)Context;

    UNREFERENCED_PARAMETER(Depth);

    YoriLibInitEmptyString(&UnescapedFilePath);
    if (!YoriLibUnescapePath(FilePath, &UnescapedFilePath)) {
        UnescapedFilePath.StartOfString = FilePath->StartOfString;
        UnescapedFilePath.LengthInChars = FilePath->LengthInChars;
    }

    if (ErrorCode == ERROR_FILE_NOT_FOUND || ErrorCode == ERROR_PATH_NOT_FOUND) {
        if (!HexDumpContext->Recursive) {
            HexDumpContext->SavedErrorThisArg = ErrorCode;
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
        YoriLibOutput(YORI_LIB_OUTPUT_STDERR, _T("Enumerate of %y failed: %s"), &DirName, ErrText);
        YoriLibFreeWinErrorText(ErrText);
    }
    YoriLibFreeStringContents(&UnescapedFilePath);
    return Result;
}


/**
 Context corresponding to a single source when displaying differences
 between two sources.
 */
typedef struct _HEXDUMP_ONE_OBJECT {

    /**
     A full path expanded for this source.
     */
    YORI_STRING FullFileName;

    /**
     A handle to the source of this data.
     */
    HANDLE FileHandle;

    /**
     A buffer to hold data read from this source.
     */
    PUCHAR Buffer;

    /**
     The number of bytes read from this source.
     */
    DWORD BytesReturned;

    /**
     Set to TRUE if a read operation from this source has failed.
     */
    BOOL ReadFailed;

    /**
     The number of bytes to display for a given line from this
     buffer.  This is recalculated for each line based on the source's
     buffer length.
     */
    YORI_ALLOC_SIZE_T DisplayLength;
} HEXDUMP_ONE_OBJECT, *PHEXDUMP_ONE_OBJECT;

/**
 Display the differences between two files in hex form.

 @param FileA The name of the first file, without any full path expansion.

 @param FileB The name of the second file, without any full path expansion.

 @param HexDumpContext Pointer to the context indicating display parameters.

 @return TRUE to indicate success, FALSE to indicate failure.
 */
BOOL
HexDumpDisplayDiff(
    __in PYORI_STRING FileA,
    __in PYORI_STRING FileB,
    __in PHEXDUMP_CONTEXT HexDumpContext
    )
{
    HEXDUMP_ONE_OBJECT Objects[2];
    YORI_ALLOC_SIZE_T BufferSize;
    YORI_ALLOC_SIZE_T BufferOffset;
    YORI_ALLOC_SIZE_T LengthToDisplay;
    YORI_ALLOC_SIZE_T LengthThisLine;
    DWORD DisplayFlags;
    LARGE_INTEGER StreamOffset;
    DWORD Count;
    BOOL Result = FALSE;
    BOOL LineDifference;

    BufferSize = YoriLibMaximumAllocationInRange(16 * 1024, 64 * 1024);
    DisplayFlags = 0;
    if (!HexDumpContext->HideOffset) {
        DisplayFlags |= YORI_LIB_HEX_FLAG_DISPLAY_LARGE_OFFSET;
    }
    if (!HexDumpContext->HideCharacters) {
        if (HexDumpContext->WideCharacters) {
            DisplayFlags |= YORI_LIB_HEX_FLAG_DISPLAY_WCHARS;
        } else {
            DisplayFlags |= YORI_LIB_HEX_FLAG_DISPLAY_CHARS;
        }
    }
    StreamOffset.QuadPart = HexDumpContext->OffsetToDisplay;

    ZeroMemory(Objects, sizeof(Objects));

    for (Count = 0; Count < sizeof(Objects)/sizeof(Objects[0]); Count++) {

        //
        //  Resolve the file to a full path
        //

        YoriLibInitEmptyString(&Objects[Count].FullFileName);
        if (Count == 0) {
            if (!YoriLibUserStringToSingleFilePath(FileA, TRUE, &Objects[Count].FullFileName)) {
                YoriLibInitEmptyString(&Objects[Count].FullFileName);
                goto Exit;
            }
        } else {
            if (!YoriLibUserStringToSingleFilePath(FileB, TRUE, &Objects[Count].FullFileName)) {
                YoriLibInitEmptyString(&Objects[Count].FullFileName);
                goto Exit;
            }
        }

        //
        //  Open each file
        //

        Objects[Count].FileHandle = CreateFile(Objects[Count].FullFileName.StartOfString,
                                               GENERIC_READ,
                                               FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                               NULL,
                                               OPEN_EXISTING,
                                               FILE_ATTRIBUTE_NORMAL | FILE_FLAG_BACKUP_SEMANTICS,
                                               NULL);

        if (Objects[Count].FileHandle == NULL || Objects[Count].FileHandle == INVALID_HANDLE_VALUE) {
            DWORD LastError = GetLastError();
            LPTSTR ErrText = YoriLibGetWinErrorText(LastError);
            YoriLibOutput(YORI_LIB_OUTPUT_STDERR, _T("hexdump: open of %y failed: %s"), &Objects[Count].FullFileName, ErrText);
            YoriLibFreeWinErrorText(ErrText);
            goto Exit;
        }

        //
        //  Allocate a read buffer for the file
        //

        Objects[Count].Buffer = YoriLibMalloc(BufferSize);
        if (Objects[Count].Buffer == NULL) {
            goto Exit;
        }

        //
        //  Seek to the requested offset in the file.  Note that in the diff
        //  case we have files, so seeking is valid.
        //

        SetFilePointer(Objects[Count].FileHandle, StreamOffset.LowPart, &StreamOffset.HighPart, FILE_BEGIN);
        if (GetLastError() != NO_ERROR) {
            DWORD LastError = GetLastError();
            LPTSTR ErrText = YoriLibGetWinErrorText(LastError);
            YoriLibOutput(YORI_LIB_OUTPUT_STDERR, _T("hexdump: seek of %y failed: %s"), &Objects[Count].FullFileName, ErrText);
            YoriLibFreeWinErrorText(ErrText);
            goto Exit;
        }
    }

    while (TRUE) {

        //
        //  Read from each file
        //

        for (Count = 0; Count < sizeof(Objects)/sizeof(Objects[0]); Count++) {
            Objects[Count].ReadFailed = FALSE;
            Objects[Count].BytesReturned = 0;
            if (!ReadFile(Objects[Count].FileHandle, Objects[Count].Buffer, BufferSize, &Objects[Count].BytesReturned, NULL)) {
                Objects[Count].ReadFailed = TRUE;
                Objects[Count].BytesReturned = 0;
            } else if (Objects[Count].BytesReturned == 0) {
                Objects[Count].ReadFailed = TRUE;
            }
        }

        //
        //  If we've finished both sources, we are done.
        //

        if (Objects[0].ReadFailed && Objects[1].ReadFailed) {
            break;
        }

        //
        //  Display the maximum of what was read between the two
        //

        if (Objects[0].BytesReturned > Objects[1].BytesReturned) {
            LengthToDisplay = (YORI_ALLOC_SIZE_T)Objects[0].BytesReturned;
        } else {
            LengthToDisplay = (YORI_ALLOC_SIZE_T)Objects[1].BytesReturned;
        }

        //
        //  Truncate the display to the range the user requested
        //

        if (HexDumpContext->LengthToDisplay != 0) {
            if (StreamOffset.QuadPart + LengthToDisplay >= HexDumpContext->OffsetToDisplay + HexDumpContext->LengthToDisplay) {
                LengthToDisplay = (YORI_ALLOC_SIZE_T)(HexDumpContext->OffsetToDisplay + HexDumpContext->LengthToDisplay - StreamOffset.QuadPart);
                if (LengthToDisplay == 0) {
                    break;
                }
            }
        }

        BufferOffset = 0;

        while(BufferOffset < LengthToDisplay) {

            //
            //  Check each line to see if it's different
            //

            LineDifference = FALSE;
            if (LengthToDisplay - BufferOffset >= 16) {
                LengthThisLine = 16;
            } else {
                LengthThisLine = LengthToDisplay - BufferOffset;
            }
            for (Count = 0; Count < sizeof(Objects)/sizeof(Objects[0]); Count++) {
                Objects[Count].DisplayLength = LengthThisLine;
                if (BufferOffset + LengthThisLine > (YORI_ALLOC_SIZE_T)Objects[Count].BytesReturned) {
                    LineDifference = TRUE;
                    Objects[Count].DisplayLength = 0;
                    if (Objects[Count].BytesReturned > BufferOffset) {
                        Objects[Count].DisplayLength = (YORI_ALLOC_SIZE_T)Objects[0].BytesReturned - BufferOffset;
                    }
                }
            }

            if (!LineDifference &&
                memcmp(&Objects[0].Buffer[BufferOffset], &Objects[1].Buffer[BufferOffset], LengthThisLine) != 0) {
                LineDifference = TRUE;
            }

            //
            //  If it's different, display it
            //

            if (LineDifference) {
                if (!YoriLibHexDiff(StreamOffset.QuadPart + BufferOffset,
                                    (LPCSTR)&Objects[0].Buffer[BufferOffset],
                                    Objects[0].DisplayLength,
                                    (LPCSTR)&Objects[1].Buffer[BufferOffset],
                                    Objects[1].DisplayLength,
                                    HexDumpContext->BytesPerGroup,
                                    DisplayFlags)) {
                    break;
                }
            }

            //
            //  Move to the next line
            //

            BufferOffset = BufferOffset + LengthThisLine;
        }

        StreamOffset.QuadPart += LengthToDisplay;
    }

Exit:

    //
    //  Clean up state from each source
    //

    for (Count = 0; Count < sizeof(Objects)/sizeof(Objects[0]); Count++) {
        if (Objects[Count].FileHandle != NULL && Objects[Count].FileHandle != INVALID_HANDLE_VALUE) {
#if defined(_MSC_VER) && (_MSC_VER >= 1700)
#pragma warning(suppress: 6001) // Analyze doesn't trust ZeroMemory, or
                                // doesn't understand the array math
#endif
            CloseHandle(Objects[Count].FileHandle);
        }
        if (Objects[Count].Buffer != NULL) {
            YoriLibFree(Objects[Count].Buffer);
        }
        YoriLibFreeStringContents(&Objects[Count].FullFileName);
    }

    return Result;
}

#ifdef YORI_BUILTIN
/**
 The main entrypoint for the hexdump builtin command.
 */
#define ENTRYPOINT YoriCmd_HEXDUMP
#else
/**
 The main entrypoint for the hexdump standalone application.
 */
#define ENTRYPOINT ymain
#endif

/**
 The main entrypoint for the hexdump cmdlet.

 @param ArgC The number of arguments.

 @param ArgV An array of arguments.

 @return Exit code of the process, zero indicating success or nonzero on
         failure.
 */
DWORD
ENTRYPOINT(
    __in YORI_ALLOC_SIZE_T ArgC,
    __in YORI_STRING ArgV[]
    )
{
    BOOLEAN ArgumentUnderstood;
    YORI_ALLOC_SIZE_T i;
    YORI_ALLOC_SIZE_T StartArg = 0;
    WORD MatchFlags;
    YORI_ALLOC_SIZE_T CharsConsumed;
    BOOLEAN BasicEnumeration = FALSE;
    BOOLEAN DiffMode = FALSE;
    BOOLEAN BinaryEncode = FALSE;
    BOOLEAN Reverse = FALSE;
    HEXDUMP_CONTEXT HexDumpContext;
    YORI_STRING Arg;

    ZeroMemory(&HexDumpContext, sizeof(HexDumpContext));
    HexDumpContext.BytesPerGroup = 4;

    for (i = 1; i < ArgC; i++) {

        ArgumentUnderstood = FALSE;
        ASSERT(YoriLibIsStringNullTerminated(&ArgV[i]));

        if (YoriLibIsCommandLineOption(&ArgV[i], &Arg)) {

            if (YoriLibCompareStringWithLiteralInsensitive(&Arg, _T("?")) == 0) {
                HexDumpHelp();
                return EXIT_SUCCESS;
            } else if (YoriLibCompareStringWithLiteralInsensitive(&Arg, _T("license")) == 0) {
                YoriLibDisplayMitLicense(_T("2017-2023"));
                return EXIT_SUCCESS;
            } else if (YoriLibCompareStringWithLiteralInsensitive(&Arg, _T("b")) == 0) {
                BasicEnumeration = TRUE;
                ArgumentUnderstood = TRUE;
            } else if (YoriLibCompareStringWithLiteralInsensitive(&Arg, _T("bin")) == 0) {
                BinaryEncode = TRUE;
                Reverse = FALSE;
                HexDumpContext.CStyleInclude = FALSE;
                ArgumentUnderstood = TRUE;
            } else if (YoriLibCompareStringWithLiteralInsensitive(&Arg, _T("d")) == 0) {
                DiffMode = TRUE;
                HexDumpContext.CStyleInclude = FALSE;
                ArgumentUnderstood = TRUE;
            } else if (YoriLibCompareStringWithLiteralInsensitive(&Arg, _T("g1")) == 0) {
                HexDumpContext.BytesPerGroup = 1;
                HexDumpContext.CStyleInclude = FALSE;
                ArgumentUnderstood = TRUE;
            } else if (YoriLibCompareStringWithLiteralInsensitive(&Arg, _T("g2")) == 0) {
                HexDumpContext.BytesPerGroup = 2;
                HexDumpContext.CStyleInclude = FALSE;
                ArgumentUnderstood = TRUE;
            } else if (YoriLibCompareStringWithLiteralInsensitive(&Arg, _T("g4")) == 0) {
                HexDumpContext.BytesPerGroup = 4;
                HexDumpContext.CStyleInclude = FALSE;
                ArgumentUnderstood = TRUE;
            } else if (YoriLibCompareStringWithLiteralInsensitive(&Arg, _T("g8")) == 0) {
                HexDumpContext.BytesPerGroup = 8;
                HexDumpContext.CStyleInclude = FALSE;
                ArgumentUnderstood = TRUE;
            } else if (YoriLibCompareStringWithLiteralInsensitive(&Arg, _T("hc")) == 0) {
                HexDumpContext.HideCharacters = TRUE;
                ArgumentUnderstood = TRUE;
            } else if (YoriLibCompareStringWithLiteralInsensitive(&Arg, _T("ho")) == 0) {
                HexDumpContext.HideOffset = TRUE;
                ArgumentUnderstood = TRUE;
            } else if (YoriLibCompareStringWithLiteralInsensitive(&Arg, _T("i")) == 0) {
                DiffMode = FALSE;
                HexDumpContext.CStyleInclude = TRUE;
                HexDumpContext.HideOffset = TRUE;
                HexDumpContext.HideCharacters = TRUE;
                ArgumentUnderstood = TRUE;
            } else if (YoriLibCompareStringWithLiteralInsensitive(&Arg, _T("l")) == 0) {
                if (ArgC > i + 1) {
                    YoriLibStringToNumber(&ArgV[i + 1], TRUE, &HexDumpContext.LengthToDisplay, &CharsConsumed);
                    i++;
                    ArgumentUnderstood = TRUE;
                }
            } else if (YoriLibCompareStringWithLiteralInsensitive(&Arg, _T("o")) == 0) {
                if (ArgC > i + 1) {
                    YoriLibStringToNumber(&ArgV[i + 1], TRUE, &HexDumpContext.OffsetToDisplay, &CharsConsumed);
                    i++;
                    ArgumentUnderstood = TRUE;
                }
            } else if (YoriLibCompareStringWithLiteralInsensitive(&Arg, _T("r")) == 0) {
                Reverse = TRUE;
                BinaryEncode = FALSE;
                HexDumpContext.CStyleInclude = FALSE;
                ArgumentUnderstood = TRUE;
            } else if (YoriLibCompareStringWithLiteralInsensitive(&Arg, _T("s")) == 0) {
                HexDumpContext.Recursive = TRUE;
                ArgumentUnderstood = TRUE;
            } else if (YoriLibCompareStringWithLiteralInsensitive(&Arg, _T("w")) == 0) {
                HexDumpContext.WideCharacters = TRUE;
                ArgumentUnderstood = TRUE;
            } else if (YoriLibCompareStringWithLiteralInsensitive(&Arg, _T("-")) == 0) {
                StartArg = i + 1;
                ArgumentUnderstood = TRUE;
                break;
            }
        } else {
            ArgumentUnderstood = TRUE;
            StartArg = i;
            break;
        }

        if (!ArgumentUnderstood) {
            YoriLibOutput(YORI_LIB_OUTPUT_STDERR, _T("Argument not understood, ignored: %y\n"), &ArgV[i]);
        }
    }

#if YORI_BUILTIN
    YoriLibCancelEnable(FALSE);
#endif

    //
    //  Attempt to enable backup privilege so an administrator can access more
    //  objects successfully.
    //

    YoriLibEnableBackupPrivilege();

    if (DiffMode) {
        if (StartArg == 0 || StartArg + 2 > ArgC) {
            YoriLibOutput(YORI_LIB_OUTPUT_STDERR, _T("hexdump: insufficient arguments\n"));
            return EXIT_FAILURE;
        }

        if (!HexDumpDisplayDiff(&ArgV[StartArg], &ArgV[StartArg + 1], &HexDumpContext)) {
            return EXIT_FAILURE;
        }
        return EXIT_SUCCESS;
    }

    //
    //  If no file name is specified, use stdin; otherwise open
    //  the file and use that
    //

    if (StartArg == 0 || StartArg == ArgC) {
        if (YoriLibIsStdInConsole()) {
            YoriLibOutput(YORI_LIB_OUTPUT_STDERR, _T("No file or pipe for input\n"));
            return EXIT_FAILURE;
        }

        if (BinaryEncode) {
            HexDumpBinaryProcessStream(GetStdHandle(STD_INPUT_HANDLE), &HexDumpContext);
        } else if (Reverse) {
            HexDumpReverseProcessStream(GetStdHandle(STD_INPUT_HANDLE), &HexDumpContext);
        } else {
            HexDumpProcessStream(GetStdHandle(STD_INPUT_HANDLE), &HexDumpContext);
        }
    } else {
        MatchFlags = YORILIB_FILEENUM_RETURN_FILES | YORILIB_FILEENUM_DIRECTORY_CONTENTS;
        if (HexDumpContext.Recursive) {
            MatchFlags |= YORILIB_FILEENUM_RECURSE_BEFORE_RETURN | YORILIB_FILEENUM_RECURSE_PRESERVE_WILD;
        }
        if (BasicEnumeration) {
            MatchFlags |= YORILIB_FILEENUM_BASIC_EXPANSION;
        }

        for (i = StartArg; i < ArgC; i++) {

            HexDumpContext.FilesFoundThisArg = 0;
            HexDumpContext.SavedErrorThisArg = ERROR_SUCCESS;

            if (BinaryEncode) {
                YoriLibForEachStream(&ArgV[i],
                                     MatchFlags,
                                     0,
                                     HexDumpBinaryFileFoundCallback,
                                     HexDumpFileEnumerateErrorCallback,
                                     &HexDumpContext);
            } else if (Reverse) {
                YoriLibForEachStream(&ArgV[i],
                                     MatchFlags,
                                     0,
                                     HexDumpReverseFileFoundCallback,
                                     HexDumpFileEnumerateErrorCallback,
                                     &HexDumpContext);
            } else {
                YoriLibForEachStream(&ArgV[i],
                                     MatchFlags,
                                     0,
                                     HexDumpFileFoundCallback,
                                     HexDumpFileEnumerateErrorCallback,
                                     &HexDumpContext);
            }

            if (HexDumpContext.FilesFoundThisArg == 0) {
                YORI_STRING FullPath;
                YoriLibInitEmptyString(&FullPath);
                if (YoriLibUserStringToSingleFilePathOrDevice(&ArgV[i], TRUE, &FullPath)) {
                    if (BinaryEncode) {
                        HexDumpBinaryFileFoundCallback(&FullPath, NULL, 0, &HexDumpContext);
                    } else if (Reverse) {
                        HexDumpReverseFileFoundCallback(&FullPath, NULL, 0, &HexDumpContext);
                    } else {
                        HexDumpFileFoundCallback(&FullPath, NULL, 0, &HexDumpContext);
                    }
                    YoriLibFreeStringContents(&FullPath);
                }
                if (HexDumpContext.SavedErrorThisArg != ERROR_SUCCESS) {
                    YoriLibOutput(YORI_LIB_OUTPUT_STDERR, _T("File or directory not found: %y\n"), &ArgV[i]);
                }
            }
        }
    }

#if !YORI_BUILTIN
    YoriLibLineReadCleanupCache();
#endif

    if (HexDumpContext.FilesFound == 0) {
        YoriLibOutput(YORI_LIB_OUTPUT_STDERR, _T("hexdump: no matching files found\n"));
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

// vim:sw=4:ts=4:et:
