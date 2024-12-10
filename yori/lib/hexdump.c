/**
 * @file lib/hexdump.c
 *
 * Yori display a large hex buffer
 *
 * Copyright (c) 2018-2023 Malcolm J. Smith
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

#include "yoripch.h"
#include "yorilib.h"

/**
 A lookup table of hex digits to use when generating strings.
 */
static UCHAR HexDigits[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };

/**
 Return the character representation of a single hex digit.
 */
#define HEX_DIGIT_FROM_VALUE(x) HexDigits[x & 0x0F];

/**
 Return the string representation for a hex digit (in the range 0-15.)

 @param Value The numberic representation of the digit.

 @return The character representation of the digit.
 */
TCHAR
YoriLibHexDigitFromValue(
    __in DWORD Value
    )
{
    return HEX_DIGIT_FROM_VALUE(Value);
}

/**
 Generate a line of up to YORI_LIB_HEXDUMP_BYTES_PER_LINE in of bytes to
 include into a C file.

 @param Output Pointer to a string to populate with the result.

 @param Buffer Pointer to the start of the buffer.

 @param BytesToDisplay Number of bytes to display, can be equal to or less
        than YORI_LIB_HEXDUMP_BYTES_PER_LINE.

 @param MoreFollowing If TRUE, the line should be terminated with a comma
        because more data remains.  If FALSE, this is the final line and
        it should be terminated with a newline.

 @return TRUE to indicate success, FALSE to indicate failure.
 */
BOOL
YoriLibHexByteCStyle(
    __inout PYORI_STRING Output,
    __in_ecount(BytesToDisplay) UCHAR CONST * Buffer,
    __in YORI_ALLOC_SIZE_T BytesToDisplay,
    __in BOOLEAN MoreFollowing
    )
{
    UCHAR WordToDisplay = 0;
    DWORD WordIndex;
    BOOL DisplayWord;
    YORI_ALLOC_SIZE_T OutputIndex = 0;
    YORI_STRING Subset;

    if (BytesToDisplay > YORI_LIB_HEXDUMP_BYTES_PER_LINE) {
        return FALSE;
    }

    Subset.StartOfString = &Output->StartOfString[OutputIndex];
    Subset.LengthAllocated = Output->LengthAllocated - OutputIndex;
    Subset.LengthInChars = YoriLibSPrintfS(Subset.StartOfString,
                                           Subset.LengthAllocated,
                                           _T("        "));
    OutputIndex = OutputIndex + Subset.LengthInChars;

    for (WordIndex = 0; WordIndex < YORI_LIB_HEXDUMP_BYTES_PER_LINE / sizeof(WordToDisplay); WordIndex++) {

        WordToDisplay = 0;
        DisplayWord = FALSE;

        if (WordIndex * sizeof(WordToDisplay) < BytesToDisplay) {
            DisplayWord = TRUE;
            WordToDisplay = (UCHAR)Buffer[WordIndex];
        }

        if (DisplayWord) {
            Subset.StartOfString = &Output->StartOfString[OutputIndex];
            Subset.LengthAllocated = Output->LengthAllocated - OutputIndex;

            if (WordIndex + 1 == BytesToDisplay && !MoreFollowing) {
                Subset.LengthInChars = YoriLibSPrintfS(Subset.StartOfString,
                                                       Subset.LengthAllocated,
                                                       _T("%02x"),
                                                       WordToDisplay);
            } else {
                Subset.LengthInChars = YoriLibSPrintfS(Subset.StartOfString,
                                                       Subset.LengthAllocated,
                                                       _T("%02x, "),
                                                       WordToDisplay);
            }
            OutputIndex = OutputIndex + Subset.LengthInChars;
        }
    }
    Output->LengthInChars = OutputIndex;

    return TRUE;
}

/**
 Generate a line of up to YORI_LIB_HEXDUMP_BYTES_PER_LINE in units of one
 UCHAR.

 @param Output Pointer to a string to populate with the result.

 @param Buffer Pointer to the start of the buffer.

 @param BytesToDisplay Number of bytes to display, can be equal to or less
        than YORI_LIB_HEXDUMP_BYTES_PER_LINE.

 @param HilightBits The set of bytes that should be hilighted.  This is
        a bitmask with YORI_LIB_HEXDUMP_BYTES_PER_LINE bits where the high
        order bit corresponds to the first byte.

 @param DisplaySeperator If TRUE, display a character at the midpoint of
        each line.

 @return TRUE to indicate success, FALSE to indicate failure.
 */
BOOL
YoriLibHexByteLine(
    __inout PYORI_STRING Output,
    __in_ecount(BytesToDisplay) UCHAR CONST * Buffer,
    __in YORI_ALLOC_SIZE_T BytesToDisplay,
    __in DWORD HilightBits,
    __in BOOLEAN DisplaySeperator
    )
{
    UCHAR WordToDisplay = 0;
    DWORD WordIndex;
    BOOL DisplayWord;
    DWORD ByteIndex;
    YORI_ALLOC_SIZE_T OutputIndex = 0;
    DWORD CurrentBit = (0x1 << (YORI_LIB_HEXDUMP_BYTES_PER_LINE - sizeof(WordToDisplay)));

    if (BytesToDisplay > YORI_LIB_HEXDUMP_BYTES_PER_LINE) {
        return FALSE;
    }

    for (WordIndex = 0; WordIndex < YORI_LIB_HEXDUMP_BYTES_PER_LINE / sizeof(WordToDisplay); WordIndex++) {

        if (DisplaySeperator && WordIndex == YORI_LIB_HEXDUMP_BYTES_PER_LINE / (sizeof(WordToDisplay) * 2)) {
            if (OutputIndex + 1 < Output->LengthAllocated) {
                Output->StartOfString[OutputIndex] = ':';
                Output->StartOfString[OutputIndex + 1] = ' ';
                OutputIndex += 2;
            }
        }

        WordToDisplay = 0;
        DisplayWord = FALSE;

        for (ByteIndex = 0; ByteIndex < sizeof(WordToDisplay); ByteIndex++) {
            if (WordIndex * sizeof(WordToDisplay) + ByteIndex < BytesToDisplay) {
                DisplayWord = TRUE;
                WordToDisplay = (UCHAR)(WordToDisplay + (Buffer[WordIndex * sizeof(WordToDisplay) + ByteIndex] << (ByteIndex * 8)));
            }
        }

        if (DisplayWord && OutputIndex + sizeof(WordToDisplay) * 2 + 11 < Output->LengthAllocated) {
            YORI_STRING Subset;
            Subset.StartOfString = &Output->StartOfString[OutputIndex];
            Subset.LengthAllocated = Output->LengthAllocated - OutputIndex;
            Subset.LengthInChars = 0;
            if (HilightBits) {
                Subset.StartOfString[Subset.LengthInChars++] = 0x1b;
                Subset.StartOfString[Subset.LengthInChars++] = '[';
                Subset.StartOfString[Subset.LengthInChars++] = '0';
                if (HilightBits & CurrentBit) {
                    Subset.StartOfString[Subset.LengthInChars++] = ';';
                    Subset.StartOfString[Subset.LengthInChars++] = '1';
                }
                Subset.StartOfString[Subset.LengthInChars++] = 'm';
            }
            Subset.StartOfString[Subset.LengthInChars++] = HEX_DIGIT_FROM_VALUE(WordToDisplay >> 4);
            Subset.StartOfString[Subset.LengthInChars++] = HEX_DIGIT_FROM_VALUE(WordToDisplay & 0x0f);
            if (HilightBits) {
                Subset.StartOfString[Subset.LengthInChars++] = 0x1b;
                Subset.StartOfString[Subset.LengthInChars++] = '[';
                Subset.StartOfString[Subset.LengthInChars++] = '0';
                Subset.StartOfString[Subset.LengthInChars++] = 'm';
            }
            Subset.StartOfString[Subset.LengthInChars++] = ' ';
            OutputIndex = OutputIndex + Subset.LengthInChars;
        } else {
            for (ByteIndex = 0;
                 OutputIndex < Output->LengthAllocated && ByteIndex < (sizeof(WordToDisplay) * 2 + 1);
                 ByteIndex++) {

                Output->StartOfString[OutputIndex] = ' ';
                OutputIndex++;
            }
        }

        CurrentBit = CurrentBit >> sizeof(WordToDisplay);
    }
    Output->LengthInChars = OutputIndex;

    return TRUE;
}

/**
 Generate a line of up to YORI_LIB_HEXDUMP_BYTES_PER_LINE in units of one
 WORD.

 @param Output Pointer to a string to populate with the result.

 @param Buffer Pointer to the start of the buffer.

 @param BytesToDisplay Number of bytes to display, can be equal to or less
        than YORI_LIB_HEXDUMP_BYTES_PER_LINE.

 @param HilightBits The set of bytes that should be hilighted.  This is
        a bitmask with YORI_LIB_HEXDUMP_BYTES_PER_LINE bits where the high
        order bit corresponds to the first byte.

 @param DisplaySeperator If TRUE, display a character at the midpoint of
        each line.

 @return TRUE to indicate success, FALSE to indicate failure.
 */
BOOL
YoriLibHexWordLine(
    __inout PYORI_STRING Output,
    __in UCHAR CONST * Buffer,
    __in YORI_ALLOC_SIZE_T BytesToDisplay,
    __in DWORD HilightBits,
    __in BOOLEAN DisplaySeperator
    )
{
    WORD WordToDisplay = 0;
    DWORD WordIndex;
    BOOL DisplayWord;
    DWORD ByteIndex;
    YORI_ALLOC_SIZE_T OutputIndex = 0;
    DWORD CurrentBit = (0x3 << (YORI_LIB_HEXDUMP_BYTES_PER_LINE - sizeof(WordToDisplay)));

    if (BytesToDisplay > YORI_LIB_HEXDUMP_BYTES_PER_LINE) {
        return FALSE;
    }

    for (WordIndex = 0; WordIndex < YORI_LIB_HEXDUMP_BYTES_PER_LINE / sizeof(WordToDisplay); WordIndex++) {

        if (DisplaySeperator && WordIndex == YORI_LIB_HEXDUMP_BYTES_PER_LINE / (sizeof(WordToDisplay) * 2)) {
            if (OutputIndex + 1 < Output->LengthAllocated) {
                Output->StartOfString[OutputIndex] = ':';
                Output->StartOfString[OutputIndex + 1] = ' ';
                OutputIndex += 2;
            }
        }

        WordToDisplay = 0;
        DisplayWord = FALSE;

        for (ByteIndex = 0; ByteIndex < sizeof(WordToDisplay); ByteIndex++) {
            if (WordIndex * sizeof(WordToDisplay) + ByteIndex < BytesToDisplay) {
                DisplayWord = TRUE;
                WordToDisplay = (WORD)(WordToDisplay + (Buffer[WordIndex * sizeof(WordToDisplay) + ByteIndex] << (ByteIndex * 8)));
            }
        }

        if (DisplayWord && OutputIndex + sizeof(WordToDisplay) * 2 + 11 < Output->LengthAllocated) {
            YORI_STRING Subset;
            Subset.StartOfString = &Output->StartOfString[OutputIndex];
            Subset.LengthAllocated = Output->LengthAllocated - OutputIndex;
            Subset.LengthInChars = 0;
            if (HilightBits) {
                Subset.StartOfString[Subset.LengthInChars++] = 0x1b;
                Subset.StartOfString[Subset.LengthInChars++] = '[';
                Subset.StartOfString[Subset.LengthInChars++] = '0';
                if (HilightBits & CurrentBit) {
                    Subset.StartOfString[Subset.LengthInChars++] = ';';
                    Subset.StartOfString[Subset.LengthInChars++] = '1';
                }
                Subset.StartOfString[Subset.LengthInChars++] = 'm';
            }
            Subset.StartOfString[Subset.LengthInChars++] = HEX_DIGIT_FROM_VALUE(WordToDisplay >> 12);
            Subset.StartOfString[Subset.LengthInChars++] = HEX_DIGIT_FROM_VALUE(WordToDisplay >> 8);
            Subset.StartOfString[Subset.LengthInChars++] = HEX_DIGIT_FROM_VALUE(WordToDisplay >> 4);
            Subset.StartOfString[Subset.LengthInChars++] = HEX_DIGIT_FROM_VALUE(WordToDisplay);
            if (HilightBits) {
                Subset.StartOfString[Subset.LengthInChars++] = 0x1b;
                Subset.StartOfString[Subset.LengthInChars++] = '[';
                Subset.StartOfString[Subset.LengthInChars++] = '0';
                Subset.StartOfString[Subset.LengthInChars++] = 'm';
            }
            Subset.StartOfString[Subset.LengthInChars++] = ' ';
            OutputIndex = OutputIndex + Subset.LengthInChars;
        } else {
            for (ByteIndex = 0;
                 OutputIndex < Output->LengthAllocated && ByteIndex < (sizeof(WordToDisplay) * 2 + 1);
                 ByteIndex++) {

                Output->StartOfString[OutputIndex] = ' ';
                OutputIndex++;
            }
        }

        CurrentBit = CurrentBit >> sizeof(WordToDisplay);
    }
    Output->LengthInChars = OutputIndex;

    return TRUE;
}


/**
 Generate a line of up to YORI_LIB_HEXDUMP_BYTES_PER_LINE in units of one
 DWORD.

 @param Output Pointer to a string to populate with the result.

 @param Buffer Pointer to the start of the buffer.

 @param BytesToDisplay Number of bytes to display, can be equal to or less
        than YORI_LIB_HEXDUMP_BYTES_PER_LINE.

 @param HilightBits The set of bytes that should be hilighted.  This is
        a bitmask with YORI_LIB_HEXDUMP_BYTES_PER_LINE bits where the high
        order bit corresponds to the first byte.

 @param DisplaySeperator If TRUE, display a character at the midpoint of
        each line.

 @return TRUE to indicate success, FALSE to indicate failure.
 */
BOOL
YoriLibHexDwordLine(
    __inout PYORI_STRING Output,
    __in UCHAR CONST * Buffer,
    __in YORI_ALLOC_SIZE_T BytesToDisplay,
    __in DWORD HilightBits,
    __in BOOLEAN DisplaySeperator
    )
{
    DWORD WordToDisplay = 0;
    DWORD WordIndex;
    BOOL DisplayWord;
    DWORD ByteIndex;
    YORI_ALLOC_SIZE_T OutputIndex = 0;
    DWORD CurrentBit = (0xf << (YORI_LIB_HEXDUMP_BYTES_PER_LINE - sizeof(WordToDisplay)));

    if (BytesToDisplay > YORI_LIB_HEXDUMP_BYTES_PER_LINE) {
        return FALSE;
    }

    for (WordIndex = 0; WordIndex < YORI_LIB_HEXDUMP_BYTES_PER_LINE / sizeof(WordToDisplay); WordIndex++) {

        if (DisplaySeperator && WordIndex == YORI_LIB_HEXDUMP_BYTES_PER_LINE / (sizeof(WordToDisplay) * 2)) {
            if (OutputIndex + 1 < Output->LengthAllocated) {
                Output->StartOfString[OutputIndex] = ':';
                Output->StartOfString[OutputIndex + 1] = ' ';
                OutputIndex += 2;
            }
        }

        WordToDisplay = 0;
        DisplayWord = FALSE;

        for (ByteIndex = 0; ByteIndex < sizeof(WordToDisplay); ByteIndex++) {
            if (WordIndex * sizeof(WordToDisplay) + ByteIndex < BytesToDisplay) {
                DisplayWord = TRUE;
                WordToDisplay = WordToDisplay + ((DWORD)Buffer[WordIndex * sizeof(WordToDisplay) + ByteIndex] << (ByteIndex * 8));
            }
        }

        if (DisplayWord && OutputIndex + sizeof(WordToDisplay) * 2 + 11 < Output->LengthAllocated) {
            YORI_STRING Subset;
            Subset.StartOfString = &Output->StartOfString[OutputIndex];
            Subset.LengthAllocated = Output->LengthAllocated - OutputIndex;
            Subset.LengthInChars = 0;
            if (HilightBits) {
                Subset.StartOfString[Subset.LengthInChars++] = 0x1b;
                Subset.StartOfString[Subset.LengthInChars++] = '[';
                Subset.StartOfString[Subset.LengthInChars++] = '0';
                if (HilightBits & CurrentBit) {
                    Subset.StartOfString[Subset.LengthInChars++] = ';';
                    Subset.StartOfString[Subset.LengthInChars++] = '1';
                }
                Subset.StartOfString[Subset.LengthInChars++] = 'm';
            }
            Subset.StartOfString[Subset.LengthInChars++] = HEX_DIGIT_FROM_VALUE(WordToDisplay >> 28);
            Subset.StartOfString[Subset.LengthInChars++] = HEX_DIGIT_FROM_VALUE(WordToDisplay >> 24);
            Subset.StartOfString[Subset.LengthInChars++] = HEX_DIGIT_FROM_VALUE(WordToDisplay >> 20);
            Subset.StartOfString[Subset.LengthInChars++] = HEX_DIGIT_FROM_VALUE(WordToDisplay >> 16);
            Subset.StartOfString[Subset.LengthInChars++] = HEX_DIGIT_FROM_VALUE(WordToDisplay >> 12);
            Subset.StartOfString[Subset.LengthInChars++] = HEX_DIGIT_FROM_VALUE(WordToDisplay >> 8);
            Subset.StartOfString[Subset.LengthInChars++] = HEX_DIGIT_FROM_VALUE(WordToDisplay >> 4);
            Subset.StartOfString[Subset.LengthInChars++] = HEX_DIGIT_FROM_VALUE(WordToDisplay);
            if (HilightBits) {
                Subset.StartOfString[Subset.LengthInChars++] = 0x1b;
                Subset.StartOfString[Subset.LengthInChars++] = '[';
                Subset.StartOfString[Subset.LengthInChars++] = '0';
                Subset.StartOfString[Subset.LengthInChars++] = 'm';
            }
            Subset.StartOfString[Subset.LengthInChars++] = ' ';
            OutputIndex = OutputIndex + Subset.LengthInChars;
        } else {
            for (ByteIndex = 0;
                 OutputIndex < Output->LengthAllocated && ByteIndex < (sizeof(WordToDisplay) * 2 + 1);
                 ByteIndex++) {

                Output->StartOfString[OutputIndex] = ' ';
                OutputIndex++;
            }
        }

        CurrentBit = CurrentBit >> sizeof(WordToDisplay);
    }
    Output->LengthInChars = OutputIndex;

    return TRUE;
}

/**
 Generate a line of up to YORI_LIB_HEXDUMP_BYTES_PER_LINE in units of one
 DWORDLONG.

 @param Output Pointer to a string to populate with the result.

 @param Buffer Pointer to the start of the buffer.

 @param BytesToDisplay Number of bytes to display, can be equal to or less
        than YORI_LIB_HEXDUMP_BYTES_PER_LINE.

 @param HilightBits The set of bytes that should be hilighted.  This is
        a bitmask with YORI_LIB_HEXDUMP_BYTES_PER_LINE bits where the high
        order bit corresponds to the first byte.

 @param DisplaySeperator If TRUE, display a character at the midpoint of
        each line.

 @return TRUE to indicate success, FALSE to indicate failure.
 */
BOOL
YoriLibHexDwordLongLine(
    __inout PYORI_STRING Output,
    __in UCHAR CONST * Buffer,
    __in YORI_ALLOC_SIZE_T BytesToDisplay,
    __in DWORD HilightBits,
    __in BOOLEAN DisplaySeperator
    )
{
    DWORDLONG WordToDisplay = 0;
    DWORD WordIndex;
    BOOL DisplayWord;
    DWORD ByteIndex;
    YORI_ALLOC_SIZE_T OutputIndex = 0;
    DWORD CurrentBit = (0xff << (YORI_LIB_HEXDUMP_BYTES_PER_LINE - sizeof(WordToDisplay)));

    if (BytesToDisplay > YORI_LIB_HEXDUMP_BYTES_PER_LINE) {
        return FALSE;
    }

    for (WordIndex = 0; WordIndex < YORI_LIB_HEXDUMP_BYTES_PER_LINE / sizeof(WordToDisplay); WordIndex++) {

        if (DisplaySeperator && WordIndex == YORI_LIB_HEXDUMP_BYTES_PER_LINE / (sizeof(WordToDisplay) * 2)) {
            if (OutputIndex + 1 < Output->LengthAllocated) {
                Output->StartOfString[OutputIndex] = ':';
                Output->StartOfString[OutputIndex + 1] = ' ';
                OutputIndex += 2;
            }
        }

        WordToDisplay = 0;
        DisplayWord = FALSE;

        for (ByteIndex = 0; ByteIndex < sizeof(WordToDisplay); ByteIndex++) {
            if (WordIndex * sizeof(WordToDisplay) + ByteIndex < BytesToDisplay) {
                DisplayWord = TRUE;
                WordToDisplay = WordToDisplay + ((DWORDLONG)Buffer[WordIndex * sizeof(WordToDisplay) + ByteIndex] << (ByteIndex * 8));
            }
        }

        if (DisplayWord && OutputIndex + sizeof(WordToDisplay) * 2 + 12 < Output->LengthAllocated) {
            LARGE_INTEGER DisplayValue;
            YORI_STRING Subset;
            Subset.StartOfString = &Output->StartOfString[OutputIndex];
            Subset.LengthAllocated = Output->LengthAllocated - OutputIndex;
            Subset.LengthInChars = 0;
            DisplayValue.QuadPart = WordToDisplay;
            if (HilightBits) {
                Subset.StartOfString[Subset.LengthInChars++] = 0x1b;
                Subset.StartOfString[Subset.LengthInChars++] = '[';
                Subset.StartOfString[Subset.LengthInChars++] = '0';
                if (HilightBits & CurrentBit) {
                    Subset.StartOfString[Subset.LengthInChars++] = ';';
                    Subset.StartOfString[Subset.LengthInChars++] = '1';
                }
                Subset.StartOfString[Subset.LengthInChars++] = 'm';
            }
            Subset.StartOfString[Subset.LengthInChars++] = HEX_DIGIT_FROM_VALUE(DisplayValue.HighPart >> 28);
            Subset.StartOfString[Subset.LengthInChars++] = HEX_DIGIT_FROM_VALUE(DisplayValue.HighPart >> 24);
            Subset.StartOfString[Subset.LengthInChars++] = HEX_DIGIT_FROM_VALUE(DisplayValue.HighPart >> 20);
            Subset.StartOfString[Subset.LengthInChars++] = HEX_DIGIT_FROM_VALUE(DisplayValue.HighPart >> 16);
            Subset.StartOfString[Subset.LengthInChars++] = HEX_DIGIT_FROM_VALUE(DisplayValue.HighPart >> 12);
            Subset.StartOfString[Subset.LengthInChars++] = HEX_DIGIT_FROM_VALUE(DisplayValue.HighPart >> 8);
            Subset.StartOfString[Subset.LengthInChars++] = HEX_DIGIT_FROM_VALUE(DisplayValue.HighPart >> 4);
            Subset.StartOfString[Subset.LengthInChars++] = HEX_DIGIT_FROM_VALUE(DisplayValue.HighPart);
            Subset.StartOfString[Subset.LengthInChars++] = '`';
            Subset.StartOfString[Subset.LengthInChars++] = HEX_DIGIT_FROM_VALUE(DisplayValue.LowPart >> 28);
            Subset.StartOfString[Subset.LengthInChars++] = HEX_DIGIT_FROM_VALUE(DisplayValue.LowPart >> 24);
            Subset.StartOfString[Subset.LengthInChars++] = HEX_DIGIT_FROM_VALUE(DisplayValue.LowPart >> 20);
            Subset.StartOfString[Subset.LengthInChars++] = HEX_DIGIT_FROM_VALUE(DisplayValue.LowPart >> 16);
            Subset.StartOfString[Subset.LengthInChars++] = HEX_DIGIT_FROM_VALUE(DisplayValue.LowPart >> 12);
            Subset.StartOfString[Subset.LengthInChars++] = HEX_DIGIT_FROM_VALUE(DisplayValue.LowPart >> 8);
            Subset.StartOfString[Subset.LengthInChars++] = HEX_DIGIT_FROM_VALUE(DisplayValue.LowPart >> 4);
            Subset.StartOfString[Subset.LengthInChars++] = HEX_DIGIT_FROM_VALUE(DisplayValue.LowPart);
            if (HilightBits) {
                Subset.StartOfString[Subset.LengthInChars++] = 0x1b;
                Subset.StartOfString[Subset.LengthInChars++] = '[';
                Subset.StartOfString[Subset.LengthInChars++] = '0';
                Subset.StartOfString[Subset.LengthInChars++] = 'm';
            }
            Subset.StartOfString[Subset.LengthInChars++] = ' ';
            OutputIndex = OutputIndex + Subset.LengthInChars;
        } else {
            for (ByteIndex = 0;
                 OutputIndex < Output->LengthAllocated && ByteIndex < (sizeof(WordToDisplay) * 2 + 1);
                 ByteIndex++) {

                Output->StartOfString[OutputIndex] = ' ';
                OutputIndex++;
            }
        }
        CurrentBit = CurrentBit >> sizeof(WordToDisplay);
    }
    Output->LengthInChars = OutputIndex;

    return TRUE;
}

/**
 Convert the buffer offset to a string.  This is really the same hex
 generation as elsewhere in the module.  It can be 64 or 32 bit, and is
 followed with a colon and space.

 @param String Pointer to a string to populate with the offset.  The length
        of the string is updated within this routine.

 @param StartOfBufferOffset The buffer offset to render.

 @param DumpFlags The flags for the dump, indicating whether any offset should
        be displayed, and whether that offset should be 32 or 64 bit.
 */
VOID
YoriLibHexDumpWriteOffset(
    __inout PYORI_STRING String,
    __in LONGLONG StartOfBufferOffset,
    __in DWORD DumpFlags
    )
{
    LARGE_INTEGER Offset;

    Offset.QuadPart = StartOfBufferOffset;

    if (DumpFlags & YORI_LIB_HEX_FLAG_DISPLAY_LARGE_OFFSET) {
        if (String->LengthAllocated >= sizeof(Offset) * 2 + 3) {
            String->StartOfString[String->LengthInChars++] = HEX_DIGIT_FROM_VALUE(Offset.HighPart >> 28);
            String->StartOfString[String->LengthInChars++] = HEX_DIGIT_FROM_VALUE(Offset.HighPart >> 24);
            String->StartOfString[String->LengthInChars++] = HEX_DIGIT_FROM_VALUE(Offset.HighPart >> 20);
            String->StartOfString[String->LengthInChars++] = HEX_DIGIT_FROM_VALUE(Offset.HighPart >> 16);
            String->StartOfString[String->LengthInChars++] = HEX_DIGIT_FROM_VALUE(Offset.HighPart >> 12);
            String->StartOfString[String->LengthInChars++] = HEX_DIGIT_FROM_VALUE(Offset.HighPart >> 8);
            String->StartOfString[String->LengthInChars++] = HEX_DIGIT_FROM_VALUE(Offset.HighPart >> 4);
            String->StartOfString[String->LengthInChars++] = HEX_DIGIT_FROM_VALUE(Offset.HighPart);
            String->StartOfString[String->LengthInChars++] = '`';
            String->StartOfString[String->LengthInChars++] = HEX_DIGIT_FROM_VALUE(Offset.LowPart >> 28);
            String->StartOfString[String->LengthInChars++] = HEX_DIGIT_FROM_VALUE(Offset.LowPart >> 24);
            String->StartOfString[String->LengthInChars++] = HEX_DIGIT_FROM_VALUE(Offset.LowPart >> 20);
            String->StartOfString[String->LengthInChars++] = HEX_DIGIT_FROM_VALUE(Offset.LowPart >> 16);
            String->StartOfString[String->LengthInChars++] = HEX_DIGIT_FROM_VALUE(Offset.LowPart >> 12);
            String->StartOfString[String->LengthInChars++] = HEX_DIGIT_FROM_VALUE(Offset.LowPart >> 8);
            String->StartOfString[String->LengthInChars++] = HEX_DIGIT_FROM_VALUE(Offset.LowPart >> 4);
            String->StartOfString[String->LengthInChars++] = HEX_DIGIT_FROM_VALUE(Offset.LowPart);
            String->StartOfString[String->LengthInChars++] = ':';
            String->StartOfString[String->LengthInChars++] = ' ';
        }
    } else if (DumpFlags & YORI_LIB_HEX_FLAG_DISPLAY_OFFSET) {
        if (String->LengthAllocated >= sizeof(Offset.LowPart) * 2 + 2) {
            String->StartOfString[String->LengthInChars++] = HEX_DIGIT_FROM_VALUE(Offset.LowPart >> 28);
            String->StartOfString[String->LengthInChars++] = HEX_DIGIT_FROM_VALUE(Offset.LowPart >> 24);
            String->StartOfString[String->LengthInChars++] = HEX_DIGIT_FROM_VALUE(Offset.LowPart >> 20);
            String->StartOfString[String->LengthInChars++] = HEX_DIGIT_FROM_VALUE(Offset.LowPart >> 16);
            String->StartOfString[String->LengthInChars++] = HEX_DIGIT_FROM_VALUE(Offset.LowPart >> 12);
            String->StartOfString[String->LengthInChars++] = HEX_DIGIT_FROM_VALUE(Offset.LowPart >> 8);
            String->StartOfString[String->LengthInChars++] = HEX_DIGIT_FROM_VALUE(Offset.LowPart >> 4);
            String->StartOfString[String->LengthInChars++] = HEX_DIGIT_FROM_VALUE(Offset.LowPart);
            String->StartOfString[String->LengthInChars++] = ':';
            String->StartOfString[String->LengthInChars++] = ' ';
        }
    }

}

/**
 Generate a line of hex string into a caller supplied buffer.

 @param Buffer Pointer to the buffer to generate.

 @param StartOfBufferOffset If the buffer displayed to this call is part of
        a larger logical stream of data, this value indicates the offset of
        this buffer within the larger logical stream.  This is used for
        display only.

 @param BufferLength The length of the buffer, in bytes.

 @param BytesPerWord The number of bytes to display at a time.

 @param DumpFlags Flags for the operation.

 @param MoreFollowing If TRUE, the line should be terminated with a comma
        because more data remains.  If FALSE, this is the final line and
        it should be terminated with a newline.

 @param LineBuffer Pointer to a caller allocated line buffer.  This routine
        attempts to be buffer safe, although for correct output the caller
        is expected to supply a buffer large enough to contain the output
        for an entire line given the flags specified.
 */
VOID
YoriLibHexLineToString(
    __in CONST UCHAR * Buffer,
    __in LONGLONG StartOfBufferOffset,
    __in YORI_ALLOC_SIZE_T BufferLength,
    __in DWORD BytesPerWord,
    __in DWORD DumpFlags,
    __in BOOLEAN MoreFollowing,
    __inout PYORI_STRING LineBuffer
    )
{
    YORI_STRING Subset;
    DWORD WordIndex;
    YORI_ALLOC_SIZE_T BytesToDisplay;
    UCHAR CharToDisplay;
    TCHAR TCharToDisplay;

    YoriLibInitEmptyString(&Subset);

    Subset.StartOfString = LineBuffer->StartOfString;
    Subset.LengthInChars = 0;
    Subset.LengthAllocated = LineBuffer->LengthAllocated;

    //
    //  If the caller requested to display the buffer offset for each
    //  line, display it
    //

    YoriLibHexDumpWriteOffset(&Subset, StartOfBufferOffset, DumpFlags);

    //
    //  Advance the buffer
    //

    LineBuffer->LengthInChars = LineBuffer->LengthInChars + Subset.LengthInChars;
    Subset.StartOfString += Subset.LengthInChars;
    Subset.LengthAllocated = Subset.LengthAllocated - Subset.LengthInChars;
    Subset.LengthInChars = 0;

    //
    //  Figure out how many hex bytes can be displayed on this line
    //

    BytesToDisplay = BufferLength;
    if (BytesToDisplay > YORI_LIB_HEXDUMP_BYTES_PER_LINE) {
        BytesToDisplay = YORI_LIB_HEXDUMP_BYTES_PER_LINE;
    }

    //
    //  Depending on the requested display format, generate the data.
    //

    if (DumpFlags & YORI_LIB_HEX_FLAG_C_STYLE) {
        YoriLibHexByteCStyle(&Subset, Buffer, BytesToDisplay, MoreFollowing);
    } else if (BytesPerWord == 1) {
        YoriLibHexByteLine(&Subset, Buffer, BytesToDisplay, 0, FALSE);
    } else if (BytesPerWord == 2) {
        YoriLibHexWordLine(&Subset, Buffer, BytesToDisplay, 0, FALSE);
    } else if (BytesPerWord == 4) {
        YoriLibHexDwordLine(&Subset, Buffer, BytesToDisplay, 0, FALSE);
    } else if (BytesPerWord == 8) {
        YoriLibHexDwordLongLine(&Subset, Buffer, BytesToDisplay, 0, FALSE);
    }

    //
    //  Advance the buffer
    //

    LineBuffer->LengthInChars = LineBuffer->LengthInChars + Subset.LengthInChars;
    Subset.StartOfString += Subset.LengthInChars;
    Subset.LengthAllocated = Subset.LengthAllocated - Subset.LengthInChars;
    Subset.LengthInChars = 0;

    //
    //  If the caller requested characters after the hex output, generate
    //  those.
    //

    if (DumpFlags & (YORI_LIB_HEX_FLAG_DISPLAY_CHARS | YORI_LIB_HEX_FLAG_DISPLAY_WCHARS)) {
        if (LineBuffer->LengthInChars < LineBuffer->LengthAllocated) {
            Subset.StartOfString[0] = ' ';
            Subset.StartOfString++;
            LineBuffer->LengthInChars++;
        }
        if (DumpFlags & YORI_LIB_HEX_FLAG_DISPLAY_CHARS) {
            for (WordIndex = 0; WordIndex < YORI_LIB_HEXDUMP_BYTES_PER_LINE; WordIndex++) {
                if (WordIndex < BytesToDisplay) {
                    CharToDisplay = Buffer[WordIndex];
                    if (!YoriLibIsCharPrintable(CharToDisplay)) {
                        CharToDisplay = '.';
                    }
                } else {
                    CharToDisplay = ' ';
                }
                Subset.StartOfString[0] = CharToDisplay;
                Subset.StartOfString++;
                LineBuffer->LengthInChars++;
                if (LineBuffer->LengthInChars == LineBuffer->LengthAllocated) {
                    break;
                }
            }
        } else {
            UCHAR Low;
            UCHAR High;
            for (WordIndex = 0; WordIndex < YORI_LIB_HEXDUMP_BYTES_PER_LINE / sizeof(TCHAR); WordIndex++) {
                if (WordIndex * sizeof(TCHAR) < BytesToDisplay) {
                    Low = Buffer[WordIndex * sizeof(TCHAR)];
                    High = Buffer[WordIndex * sizeof(TCHAR) + 1];
                    TCharToDisplay = (TCHAR)((High << 8) + Low);
                    if (!YoriLibIsCharPrintable(TCharToDisplay)) {
                        TCharToDisplay = '.';
                    }
                } else {
                    TCharToDisplay = ' ';
                }
                Subset.StartOfString[0] = TCharToDisplay;
                Subset.StartOfString++;
                LineBuffer->LengthInChars++;
                if (LineBuffer->LengthInChars == LineBuffer->LengthAllocated) {
                    break;
                }
            }
        }
    }
}

/**
 Display a buffer in hex format.

 @param Buffer Pointer to the buffer to display.

 @param StartOfBufferOffset If the buffer displayed to this call is part of
        a larger logical stream of data, this value indicates the offset of
        this buffer within the larger logical stream.  This is used for
        display only.

 @param BufferLength The length of the buffer, in bytes.

 @param BytesPerWord The number of bytes to display at a time.

 @param DumpFlags Flags for the operation.

 @return TRUE to indicate success, FALSE to indicate failure.
 */
BOOL
YoriLibHexDump(
    __in LPCSTR Buffer,
    __in LONGLONG StartOfBufferOffset,
    __in YORI_ALLOC_SIZE_T BufferLength,
    __in DWORD BytesPerWord,
    __in DWORD DumpFlags
    )
{
    DWORD LineCount = (BufferLength + YORI_LIB_HEXDUMP_BYTES_PER_LINE - 1) / YORI_LIB_HEXDUMP_BYTES_PER_LINE;
    DWORD LineIndex;
    LONGLONG DisplayBufferOffset;
    YORI_STRING OutputBuffer;
    YORI_STRING LineBuffer;
    PUCHAR CurrentBuffer;
    YORI_ALLOC_SIZE_T BufferRemaining;
    BOOLEAN MoreFollowing;
    YORI_ALLOC_SIZE_T CharsPerLine;
    HANDLE hOut;
    YORI_ALLOC_SIZE_T AllocSize;

    if (BytesPerWord != 1 && BytesPerWord != 2 && BytesPerWord != 4 && BytesPerWord != 8) {
        return FALSE;
    }

    //
    //  16 chars per byte: 6 chars to initiate a highlight; 4 to end it; and 4
    //  is the worst case for the data itself, being two hex digits, a space,
    //  and a character.
    //

    CharsPerLine = 16 * YORI_LIB_HEXDUMP_BYTES_PER_LINE + 32;

    //
    //  Allocate a chunk of memory to generate lines into.  The value below is
    //  big enough to batch writes but isn't specifically meaningful.
    //

    AllocSize = YoriLibMaximumAllocationInRange(60 * 1024, 4 * 1024 * 1024);
    if (!YoriLibAllocateString(&OutputBuffer, AllocSize)) {
        return FALSE;
    }
    YoriLibInitEmptyString(&LineBuffer);

    DisplayBufferOffset = StartOfBufferOffset;
    CurrentBuffer = (PUCHAR)Buffer;
    BufferRemaining = BufferLength;
    MoreFollowing = TRUE;
    hOut = GetStdHandle(STD_OUTPUT_HANDLE);

    for (LineIndex = 0; LineIndex < LineCount; LineIndex++) {

        if (LineIndex + 1 == LineCount) {
            MoreFollowing = FALSE;
        }

        //
        //  Write a new line to the end of the buffer and add a newline.
        //

        LineBuffer.StartOfString = &OutputBuffer.StartOfString[OutputBuffer.LengthInChars];
        LineBuffer.LengthInChars = 0;
        LineBuffer.LengthAllocated = CharsPerLine;

        YoriLibHexLineToString(CurrentBuffer, DisplayBufferOffset, BufferRemaining, BytesPerWord, DumpFlags, MoreFollowing, &LineBuffer);

        if (LineBuffer.LengthInChars < LineBuffer.LengthAllocated) {
            LineBuffer.StartOfString[LineBuffer.LengthInChars] = '\n';
            LineBuffer.LengthInChars++;
        }

        OutputBuffer.LengthInChars = OutputBuffer.LengthInChars + LineBuffer.LengthInChars;

        //
        //  If the buffer is full write it out.
        //

        if (OutputBuffer.LengthAllocated - OutputBuffer.LengthInChars <= CharsPerLine) {
            YoriLibOutputString(hOut, 0, &OutputBuffer);
            OutputBuffer.LengthInChars = 0;
        }


        CurrentBuffer = YoriLibAddToPointer(CurrentBuffer, YORI_LIB_HEXDUMP_BYTES_PER_LINE);
        BufferRemaining = BufferRemaining - YORI_LIB_HEXDUMP_BYTES_PER_LINE;
        DisplayBufferOffset = DisplayBufferOffset + YORI_LIB_HEXDUMP_BYTES_PER_LINE;
    }

    if (OutputBuffer.LengthInChars > 0) {
        YoriLibOutputString(hOut, 0, &OutputBuffer);
        OutputBuffer.LengthInChars = 0;
    }

    YoriLibFreeStringContents(&OutputBuffer);
    return TRUE;
}

/**
 Display two buffers side by side in hex format.

 @param StartOfBufferOffset If the buffer displayed to this call is part of
        a larger logical stream of data, this value indicates the offset of
        this buffer within the larger logical stream.  This is used for
        display only.

 @param Buffer1 Pointer to the first buffer to display.

 @param Buffer1Length The length of the first buffer, in bytes.

 @param Buffer2 Pointer to the second buffer to display.

 @param Buffer2Length The length of the second buffer, in bytes.

 @param BytesPerWord The number of bytes to display at a time.

 @param DumpFlags Flags for the operation.

 @return TRUE to indicate success, FALSE to indicate failure.
 */
BOOL
YoriLibHexDiff(
    __in LONGLONG StartOfBufferOffset,
    __in LPCSTR Buffer1,
    __in YORI_ALLOC_SIZE_T Buffer1Length,
    __in LPCSTR Buffer2,
    __in YORI_ALLOC_SIZE_T Buffer2Length,
    __in DWORD BytesPerWord,
    __in DWORD DumpFlags
    )
{
    YORI_ALLOC_SIZE_T LineCount;
    YORI_ALLOC_SIZE_T LineIndex;
    YORI_ALLOC_SIZE_T WordIndex;
    DWORD BufferIndex;
    YORI_ALLOC_SIZE_T BytesToDisplay;
    DWORD HilightBits;
    DWORD CurrentBit;
    UCHAR CharToDisplay;
    TCHAR TCharToDisplay;
    LARGE_INTEGER DisplayBufferOffset;
    LPCSTR BufferToDisplay;
    LPCSTR Buffers[2];
    YORI_ALLOC_SIZE_T BufferLengths[2];
    YORI_STRING LineBuffer;
    YORI_STRING Subset;

    if (BytesPerWord != 1 && BytesPerWord != 2 && BytesPerWord != 4 && BytesPerWord != 8) {
        return FALSE;
    }

    if (!YoriLibAllocateString(&LineBuffer, 32 * YORI_LIB_HEXDUMP_BYTES_PER_LINE + 64)) {
        return FALSE;
    }

    Subset.StartOfString = LineBuffer.StartOfString;
    Subset.LengthInChars = 0;
    Subset.LengthAllocated = LineBuffer.LengthAllocated;

    DisplayBufferOffset.QuadPart = StartOfBufferOffset;

    if (Buffer1Length > Buffer2Length) {
        LineCount = (Buffer1Length + YORI_LIB_HEXDUMP_BYTES_PER_LINE - 1) / YORI_LIB_HEXDUMP_BYTES_PER_LINE;
    } else {
        LineCount = (Buffer2Length + YORI_LIB_HEXDUMP_BYTES_PER_LINE - 1) / YORI_LIB_HEXDUMP_BYTES_PER_LINE;
    }

    Buffers[0] = Buffer1;
    Buffers[1] = Buffer2;
    BufferLengths[0] = Buffer1Length;
    BufferLengths[1] = Buffer2Length;

    for (LineIndex = 0; LineIndex < LineCount; LineIndex++) {

        //
        //  If the caller requested to display the buffer offset for each
        //  line, display it
        //

        YoriLibHexDumpWriteOffset(&Subset, DisplayBufferOffset.QuadPart, DumpFlags);
        DisplayBufferOffset.QuadPart += YORI_LIB_HEXDUMP_BYTES_PER_LINE;

        //
        //  Advance the buffer
        //

        LineBuffer.LengthInChars = LineBuffer.LengthInChars + Subset.LengthInChars;
        Subset.StartOfString += Subset.LengthInChars;
        Subset.LengthAllocated = Subset.LengthAllocated - Subset.LengthInChars;
        Subset.LengthInChars = 0;

        //
        //  For this line, calculate a set of bits corresponding to bytes
        //  that are different
        //

        HilightBits = 0;
        for (WordIndex = 0; WordIndex < YORI_LIB_HEXDUMP_BYTES_PER_LINE; WordIndex++) {
            HilightBits = HilightBits << 1;
            BytesToDisplay = LineIndex * YORI_LIB_HEXDUMP_BYTES_PER_LINE + WordIndex;
            if (BufferLengths[0] > BytesToDisplay && BufferLengths[1] > BytesToDisplay) {
                if (Buffers[0][BytesToDisplay] != Buffers[1][BytesToDisplay]) {
                    HilightBits = HilightBits | 1;
                }
            } else {
                HilightBits = HilightBits | 1;
            }
        }

        for (BufferIndex = 0; BufferIndex < 2; BufferIndex++) {

            //
            //  Figure out how many hex bytes can be displayed on this line
            //

            if (LineIndex * YORI_LIB_HEXDUMP_BYTES_PER_LINE >= BufferLengths[BufferIndex]) {
                BytesToDisplay = 0;
                BufferToDisplay = NULL;
            } else {
                BytesToDisplay = BufferLengths[BufferIndex] - LineIndex * YORI_LIB_HEXDUMP_BYTES_PER_LINE;
                BufferToDisplay = &Buffers[BufferIndex][LineIndex * YORI_LIB_HEXDUMP_BYTES_PER_LINE];
                if (BytesToDisplay > YORI_LIB_HEXDUMP_BYTES_PER_LINE) {
                    BytesToDisplay = YORI_LIB_HEXDUMP_BYTES_PER_LINE;
                }
            }


            //
            //  Depending on the requested display format, display the data.
            //

            if (BytesPerWord == 1) {
                YoriLibHexByteLine(&Subset, (CONST UCHAR *)BufferToDisplay, BytesToDisplay, HilightBits, TRUE);
            } else if (BytesPerWord == 2) {
                YoriLibHexWordLine(&Subset, (CONST UCHAR *)BufferToDisplay, BytesToDisplay, HilightBits, TRUE);
            } else if (BytesPerWord == 4) {
                YoriLibHexDwordLine(&Subset, (CONST UCHAR *)BufferToDisplay, BytesToDisplay, HilightBits, TRUE);
            } else if (BytesPerWord == 8) {
                YoriLibHexDwordLongLine(&Subset, (CONST UCHAR *)BufferToDisplay, BytesToDisplay, HilightBits, TRUE);
            }

            //
            //  Advance the buffer
            //

            LineBuffer.LengthInChars = LineBuffer.LengthInChars + Subset.LengthInChars;
            Subset.StartOfString += Subset.LengthInChars;
            Subset.LengthAllocated = Subset.LengthAllocated - Subset.LengthInChars;
            Subset.LengthInChars = 0;

            //
            //  If the caller requested characters after the hex output,
            //  generate them.
            //

            if ((DumpFlags & (YORI_LIB_HEX_FLAG_DISPLAY_CHARS | YORI_LIB_HEX_FLAG_DISPLAY_WCHARS)) &&
                Subset.LengthAllocated >= (7 * YORI_LIB_HEXDUMP_BYTES_PER_LINE)) {

                if (LineBuffer.LengthInChars < LineBuffer.LengthAllocated) {
                    Subset.StartOfString[0] = ' ';
                    Subset.StartOfString++;
                    LineBuffer.LengthInChars++;
                }
                if (DumpFlags & YORI_LIB_HEX_FLAG_DISPLAY_CHARS) {
                    CurrentBit = (0x1 << (YORI_LIB_HEXDUMP_BYTES_PER_LINE - sizeof(CharToDisplay)));
                    for (WordIndex = 0; WordIndex < YORI_LIB_HEXDUMP_BYTES_PER_LINE; WordIndex++) {
                        if (WordIndex < BytesToDisplay) {
                            CharToDisplay = BufferToDisplay[WordIndex];
                            if (CharToDisplay < 32) {
                                CharToDisplay = '.';
                            }
                        } else {
                            CharToDisplay = ' ';
                        }

                        Subset.StartOfString[Subset.LengthInChars++] = 0x1b;
                        Subset.StartOfString[Subset.LengthInChars++] = '[';
                        Subset.StartOfString[Subset.LengthInChars++] = '0';
                        if (HilightBits & CurrentBit) {
                            Subset.StartOfString[Subset.LengthInChars++] = ';';
                            Subset.StartOfString[Subset.LengthInChars++] = '1';
                        }
                        Subset.StartOfString[Subset.LengthInChars++] = 'm';
                        Subset.StartOfString[Subset.LengthInChars++] = CharToDisplay;

                        LineBuffer.LengthInChars = LineBuffer.LengthInChars + Subset.LengthInChars;
                        Subset.StartOfString += Subset.LengthInChars;
                        Subset.LengthAllocated = Subset.LengthAllocated - Subset.LengthInChars;
                        Subset.LengthInChars = 0;

                        CurrentBit = CurrentBit >> sizeof(CharToDisplay);
                    }
                } else {
                    UCHAR Low;
                    UCHAR High;
                    CurrentBit = (0x3 << (YORI_LIB_HEXDUMP_BYTES_PER_LINE - sizeof(TCharToDisplay)));
                    for (WordIndex = 0; WordIndex < YORI_LIB_HEXDUMP_BYTES_PER_LINE / sizeof(TCHAR); WordIndex++) {
                        if (WordIndex < BytesToDisplay) {
                            Low = BufferToDisplay[WordIndex * sizeof(TCHAR)];
                            High = BufferToDisplay[WordIndex * sizeof(TCHAR) + 1];
                            TCharToDisplay = (TCHAR)((High << 8) + Low);
                            if (TCharToDisplay < 32) {
                                TCharToDisplay = '.';
                            }
                        } else {
                            TCharToDisplay = ' ';
                        }

                        Subset.StartOfString[Subset.LengthInChars++] = 0x1b;
                        Subset.StartOfString[Subset.LengthInChars++] = '[';
                        Subset.StartOfString[Subset.LengthInChars++] = '0';
                        if (HilightBits & CurrentBit) {
                            Subset.StartOfString[Subset.LengthInChars++] = ';';
                            Subset.StartOfString[Subset.LengthInChars++] = '1';
                        }
                        Subset.StartOfString[Subset.LengthInChars++] = 'm';
                        Subset.StartOfString[Subset.LengthInChars++] = TCharToDisplay;

                        LineBuffer.LengthInChars = LineBuffer.LengthInChars + Subset.LengthInChars;
                        Subset.StartOfString += Subset.LengthInChars;
                        Subset.LengthAllocated = Subset.LengthAllocated - Subset.LengthInChars;
                        Subset.LengthInChars = 0;

                        CurrentBit = CurrentBit >> sizeof(TCharToDisplay);
                    }
                }
            }

            if (BufferIndex == 0 && Subset.LengthAllocated >= sizeof(" | ") - 1) {
                Subset.StartOfString[Subset.LengthInChars++] = ' ';
                Subset.StartOfString[Subset.LengthInChars++] = '|';
                Subset.StartOfString[Subset.LengthInChars++] = ' ';
                LineBuffer.LengthInChars = LineBuffer.LengthInChars + Subset.LengthInChars;
                Subset.StartOfString += Subset.LengthInChars;
                Subset.LengthAllocated = Subset.LengthAllocated - Subset.LengthInChars;
                Subset.LengthInChars = 0;
            }
        }

        if (LineBuffer.LengthInChars < LineBuffer.LengthAllocated) {
            Subset.StartOfString[0] = '\n';
            Subset.StartOfString++;
            LineBuffer.LengthInChars++;
        }
        YoriLibOutput(YORI_LIB_OUTPUT_STDOUT, _T("%y"), &LineBuffer);
        LineBuffer.LengthInChars = 0;
        Subset.StartOfString = LineBuffer.StartOfString;
        Subset.LengthInChars = LineBuffer.LengthInChars;
        Subset.LengthAllocated = LineBuffer.LengthAllocated;
    }

    YoriLibFreeStringContents(&LineBuffer);
    return TRUE;
}


// vim:sw=4:ts=4:et:
