/**
 * @file libwin/label.c
 *
 * Yori window label control
 *
 * Copyright (c) 2019-2020 Malcolm J. Smith
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
#include "yoriwin.h"
#include "winpriv.h"

/**
 Specifies legitimate values for horizontal text alignment within the
 label.
 */
typedef enum _YORI_WIN_TEXT_ALIGNMENT {
    YoriWinTextAlignLeft = 0,
    YoriWinTextAlignCenter = 1,
    YoriWinTextAlignRight = 2
} YORI_WIN_TEXT_ALIGNMENT;

/**
 Specifies legitimate values for vertical text alignment within the
 label.
 */
typedef enum _YORI_WIN_TEXT_VERTICAL_ALIGNMENT {
    YoriWinTextVerticalAlignTop = 0,
    YoriWinTextVerticalAlignCenter = 1,
    YoriWinTextVerticalAlignBottom = 2
} YORI_WIN_TEXT_VERTICAL_ALIGNMENT;

/**
 A structure describing the contents of a label control.
 */
typedef struct _YORI_WIN_CTRL_LABEL {

    /**
     A common header for all controls
     */
    YORI_WIN_CTRL Ctrl;

    /**
     The display text of the label
     */
    YORI_STRING Caption;

    /**
     The offset within the Caption string for which character is the keyboard
     accelerator that should be highlighted when the user presses the Alt key.
     */
    YORI_ALLOC_SIZE_T AcceleratorOffset;

    /**
     Specifies if the text should be rendered to the left, center, or right of
     each line horizontally.
     */
    YORI_WIN_TEXT_ALIGNMENT TextAlign;

    /**
     Specifies if the text should be rendered at the top, center, or bottom of
     the control vertically.
     */
    YORI_WIN_TEXT_VERTICAL_ALIGNMENT TextVerticalAlign;

    /**
     Style flags.
     */
    DWORD Style;

    /**
     The attributes to display text in.
     */
    WORD TextAttributes;

    /**
     The attributes to display the accelerator character in.
     */
    WORD AcceleratorTextAttributes;

    /**
     TRUE if the label should display the accelerator character, FALSE if it
     should not.  This becomes TRUE when the user presses the Alt key.
     */
    BOOLEAN DisplayAccelerator;

} YORI_WIN_CTRL_LABEL, *PYORI_WIN_CTRL_LABEL;

/**
 Return TRUE to indicate that the character should always break a line
 regardless of the length of the line.

 @param Char The character to test.

 @return TRUE to indicate that this character will force a break for a new
         line.
 */
BOOLEAN
YoriWinLabelIsCharHardBreakChar(
    __in TCHAR Char
    )
{
    if (Char == '\r' ||
        Char == '\n') {

        return TRUE;
    }
    return FALSE;
}

/**
 Return TRUE to indicate that the character can be used to break a line if the
 line is too long, and output can continue on the next line.

 @param Char The character to test.

 @return TRUE to indicate that this character is a good point to break for a
         new line.
 */
BOOLEAN
YoriWinLabelIsCharSoftBreakChar(
    __in TCHAR Char
    )
{
    if (Char == ' ') {
        return TRUE;
    }
    return FALSE;
}

/**
 Return TRUE to indicate that the character should not be displayed if it was
 used to break lines.

 @param Char The character to test.

 @return TRUE to indicate that this character does not require display if a
         line break occurred.
 */
BOOLEAN
YoriWinLabelShouldSwallowBreakChar(
    __in TCHAR Char
    )
{
    if (Char == ' ' ||
        Char == '\r' ||
        Char == '\n') {

        return TRUE;
    }
    return FALSE;
}

/**
 Given a string which is remaining to display and the size of the control,
 calculate which subset of text should be displayed on the next line, indicate
 if the next line contains a keyboard accelerator and the offset to that
 accelerator, and update the remaining string and offset to the accelerator
 in preparation to process the next line.

 @param Remaining Pointer to a string which requires display.

 @param ClientWidth Specifies the width of the control.

 @param Display On output, updated to contain the subset of the remaining
        string to render on the next line.

 @param AcceleratorFound On output, set to TRUE to indicate that an
        accelerator was found and OffsetToAcceleratorInDisplay is meaningful.

 @param OffsetToAcceleratorInDisplay On output, set to indicate the offset in
        Display which is the keyboard accelerator.  This is only meaningful
        if AcceleratorFound was set to TRUE.

 @param RemainingOffsetToAccelerator On output, set to the offset within the
        Remaining string that contains the keyboard accelerator.
 */
VOID
YoriWinLabelGetNextDisplayLine(
    __inout PYORI_STRING Remaining,
    __in YORI_ALLOC_SIZE_T ClientWidth,
    __out PYORI_STRING Display,
    __inout PBOOLEAN AcceleratorFound,
    __out PYORI_ALLOC_SIZE_T OffsetToAcceleratorInDisplay,
    __inout PYORI_ALLOC_SIZE_T RemainingOffsetToAccelerator
    )
{
    YORI_ALLOC_SIZE_T PotentialBreakOffset;
    YORI_ALLOC_SIZE_T MaxLengthOfLine;
    YORI_ALLOC_SIZE_T CharsToDisplayThisLine;
    YORI_ALLOC_SIZE_T CharsToConsumeThisLine;
    BOOLEAN BreakCharFound;
    TCHAR TestChar;
    BOOLEAN SoftTruncationRequired;

    //
    //  Check if the text is longer than can fit on one line
    //

    MaxLengthOfLine = Remaining->LengthInChars;
    CharsToDisplayThisLine = MaxLengthOfLine;
    SoftTruncationRequired = FALSE;

    //
    //  Look along the line for any explicit newline to break on.  Note this
    //  can look beyond the maximum length, because any character beyond the
    //  maximum that is removed doesn't count against the maximum.
    //

    BreakCharFound = FALSE;

    for (PotentialBreakOffset = 0; PotentialBreakOffset < Remaining->LengthInChars; PotentialBreakOffset++) {
        TestChar = Remaining->StartOfString[PotentialBreakOffset];
        if (YoriWinLabelIsCharHardBreakChar(TestChar)) {
            BreakCharFound = TRUE;
            break;
        }

        //
        //  If the text is wider than the control, and the char is not a
        //  soft break character, go to the previous char and do soft break
        //  processing from there.
        //

        if (PotentialBreakOffset >= ClientWidth) {
            if (!YoriWinLabelIsCharSoftBreakChar(TestChar) ||
                !YoriWinLabelShouldSwallowBreakChar(TestChar)) {

                MaxLengthOfLine = PotentialBreakOffset;
                SoftTruncationRequired = TRUE;
                break;
            }
        }
    }

    //
    //  If the text is longer than a line and no explicit newline was found,
    //  count backwards from the width of the control for the first soft break
    //  character.  If none are found display the number of chars as will fit.
    //  If one is found, treat that as the break point.
    //

    if (SoftTruncationRequired && !BreakCharFound) {
        PotentialBreakOffset = MaxLengthOfLine - 1;
        BreakCharFound = TRUE;
        while (!YoriWinLabelIsCharSoftBreakChar(Remaining->StartOfString[PotentialBreakOffset])) {
            if (PotentialBreakOffset == 0) {
                BreakCharFound = FALSE;
                break;
            }
            PotentialBreakOffset--;
        }
    }

    if (SoftTruncationRequired && !BreakCharFound) {
        PotentialBreakOffset = MaxLengthOfLine;
        BreakCharFound = TRUE;
    }

    //
    //  Display the string after removing the break char
    //

    if (BreakCharFound) {
        CharsToDisplayThisLine = PotentialBreakOffset;
    }

    ASSERT(CharsToDisplayThisLine <= ClientWidth);

    //
    //  Consume all following break chars (these aren't displayed
    //  anywhere)
    //

    CharsToConsumeThisLine = CharsToDisplayThisLine;

    if (CharsToConsumeThisLine < Remaining->LengthInChars) {
        while (YoriWinLabelShouldSwallowBreakChar(Remaining->StartOfString[CharsToConsumeThisLine])) {
            CharsToConsumeThisLine++;
            if (CharsToConsumeThisLine >= Remaining->LengthInChars) {
                break;
            }
        }
    }

    Display->StartOfString = Remaining->StartOfString;
    Display->LengthInChars = CharsToDisplayThisLine;

    if (*RemainingOffsetToAccelerator < CharsToConsumeThisLine) {
        *OffsetToAcceleratorInDisplay = *RemainingOffsetToAccelerator;
        *RemainingOffsetToAccelerator = 0;
        *AcceleratorFound = TRUE;
    } else {
        *OffsetToAcceleratorInDisplay = 0;
        *RemainingOffsetToAccelerator = (*RemainingOffsetToAccelerator) - CharsToConsumeThisLine;
    }

    Remaining->StartOfString += CharsToConsumeThisLine;
    Remaining->LengthInChars = Remaining->LengthInChars - CharsToConsumeThisLine;
}

/**
 Consume any characters from the beginning of the string which would be
 nonvisible break characters.

 @param Remaining Pointer to the string to display.  This can be modified
        on output to advance beyond any nonvisible characters.

 @param AcceleratorFound Pointer to a boolean value which will be set to
        TRUE to indicate that the accelerator was consumed as part of the
        trimming process.

 @param RemainingOffsetToAccelerator Pointer to a value specifying the
        character offset to the accelerator character.  This will be
        modified within this routine if the string is advanced to indicate
        the offset to the accelerator within the remaining string.
 */
VOID
YoriWinLabelTrimSwallowChars(
    __in PYORI_STRING Remaining,
    __inout PBOOLEAN AcceleratorFound,
    __inout PYORI_ALLOC_SIZE_T RemainingOffsetToAccelerator
    )
{
    while (Remaining->LengthInChars > 0) {
        if (!YoriWinLabelShouldSwallowBreakChar(Remaining->StartOfString[0])) {
            break;
        }
        Remaining->StartOfString++;
        Remaining->LengthInChars--;
        if (*RemainingOffsetToAccelerator > 0) {
            (*RemainingOffsetToAccelerator)--;
        } else {
            *AcceleratorFound = TRUE;
        }
    }
}


/**
 Count the number of lines which will need to have text rendered on them at
 a specified control width.

 @param Text Pointer to the text to display.

 @param CtrlWidth Specifies the width of the control.

 @param MaximumWidth Optionally refers to a location to update with the
        longest line length observed within the text string.

 @return The number of lines of text within the control.
 */
YORI_ALLOC_SIZE_T
YoriWinLabelCountLinesRequiredForText(
    __in PYORI_STRING Text,
    __in YORI_ALLOC_SIZE_T CtrlWidth,
    __out_opt PYORI_ALLOC_SIZE_T MaximumWidth
    )
{
    YORI_STRING Remaining;
    YORI_STRING DisplayLine;
    YORI_ALLOC_SIZE_T OffsetToAcceleratorInDisplay;
    YORI_ALLOC_SIZE_T RemainingOffsetToAccelerator;
    YORI_ALLOC_SIZE_T LinesNeeded;
    YORI_ALLOC_SIZE_T LargestWidthFound;
    BOOLEAN AcceleratorFound;

    LinesNeeded = 0;
    LargestWidthFound = 0;
    AcceleratorFound = FALSE;
    Remaining.StartOfString = Text->StartOfString;
    Remaining.LengthInChars = Text->LengthInChars;
    RemainingOffsetToAccelerator = 0;

    //
    //  Swallow any leading characters that would not be displayed
    //

    YoriWinLabelTrimSwallowChars(&Remaining, &AcceleratorFound, &RemainingOffsetToAccelerator);

    while (Remaining.LengthInChars > 0) {
        YoriWinLabelGetNextDisplayLine(&Remaining,
                                       CtrlWidth,
                                       &DisplayLine,
                                       &AcceleratorFound,
                                       &OffsetToAcceleratorInDisplay,
                                       &RemainingOffsetToAccelerator);

        if (DisplayLine.LengthInChars > LargestWidthFound) {
            LargestWidthFound = DisplayLine.LengthInChars;
        }

        LinesNeeded++;
    }

    if (MaximumWidth != NULL) {
        *MaximumWidth = LargestWidthFound;
    }

    return LinesNeeded;
}

/**
 Count the number of lines which will need to have text rendered on them.

 @param Label Pointer to the label control which implicitly provides
        dimensions as well as text to display.

 @return The number of lines of text within the control.
 */
DWORD
YoriWinLabelCountLinesRequired(
    __in PYORI_WIN_CTRL_LABEL Label
    )
{
    COORD ClientSize;
    YoriWinGetControlClientSize(&Label->Ctrl, &ClientSize);
    return YoriWinLabelCountLinesRequiredForText(&Label->Caption, ClientSize.X, NULL);
}

/**
 Parse a string that may contain an ampersand indicating the presence of an
 accelerator char.  Optionally return a string with the ampersand removed,
 the char following the ampersand that should be used as the accelerator,
 the offset of the accelerator in the output string that should be
 highlighted, and the number of characters in the display string.

 @param RawString Pointer to a string which may contain an accelerator.

 @param ParsedString Optionally points to a preallocated string to populate
        with the text where the accelerator ampersand has been removed.

 @param AcceleratorChar Optionally points to a character to update with the
        found accelerator character.

 @param HighlightOffset Optionally points to a variable to receive the offset
        within the parsed string to highlight when the user holds down Alt.

 @param DisplayLength Optionally points to a variable to receive the number of
        characters to display in ParsedString.  This is the same as
        ParsedString->LengthInChars, but is seperated to allow the length to
        be found without populating a buffer.
 */
VOID
YoriWinLabelParseAccelerator(
    __in PCYORI_STRING RawString,
    __inout_opt PYORI_STRING ParsedString,
    __out_opt TCHAR* AcceleratorChar,
    __out_opt PYORI_ALLOC_SIZE_T HighlightOffset,
    __out_opt PYORI_ALLOC_SIZE_T DisplayLength
    )
{
    YORI_ALLOC_SIZE_T WriteIndex;
    YORI_ALLOC_SIZE_T ReadIndex;
    BOOLEAN AcceleratorFound;

    WriteIndex = 0;
    AcceleratorFound = FALSE;

    if (HighlightOffset) {
        *HighlightOffset = 0;
    }

    if (AcceleratorChar) {
        *AcceleratorChar = '\0';
    }

    for (ReadIndex = 0; ReadIndex < RawString->LengthInChars; ReadIndex++) {
        if (RawString->StartOfString[ReadIndex] == '&' &&
            ReadIndex + 1 < RawString->LengthInChars) {

            ReadIndex++;

            if (!AcceleratorFound &&
                RawString->StartOfString[ReadIndex] != '&') {

                AcceleratorFound = TRUE;
                if (HighlightOffset) {
                    *HighlightOffset = WriteIndex;
                }

                if (AcceleratorChar) {
                    *AcceleratorChar = RawString->StartOfString[ReadIndex];
                }
            }
        }

        if (ParsedString) {
            ParsedString->StartOfString[WriteIndex] = RawString->StartOfString[ReadIndex];
        }
        WriteIndex++;
    }

    if (ParsedString) {
        ParsedString->StartOfString[WriteIndex] = '\0';
        ParsedString->LengthInChars = WriteIndex;
    }

    if (DisplayLength) {
        *DisplayLength = WriteIndex;
    }
}

/**
 Render a blank line within the label where no text is present.

 @param Label Pointer to the label control.

 @param ClientSize Specifies the dimensions of the label control.

 @param TextAttributes The color to use to render the cells.

 @param LineIndex The index of the line to render within the control.
 */
VOID
YoriWinLabelClearClientLine(
    __in PYORI_WIN_CTRL_LABEL Label,
    __in PCOORD ClientSize,
    __in WORD TextAttributes,
    __in WORD LineIndex
    )
{
    WORD CharIndex;

    for (CharIndex = 0; CharIndex < (DWORD)ClientSize->X; CharIndex++) {
        YoriWinSetControlClientCell(&Label->Ctrl, CharIndex, LineIndex, ' ', TextAttributes);
    }
}

/**
 Draw the label with its current state applied.

 @param Label Pointer to the label to draw.

 @return TRUE to indicate success, FALSE to indicate failure.
 */
BOOLEAN
YoriWinLabelPaint(
    __in PYORI_WIN_CTRL_LABEL Label
    )
{
    WORD WinAttributes;
    WORD TextAttributes;
    WORD CharAttributes;
    WORD StartColumn;
    WORD CellIndex;
    WORD CharIndex;
    WORD LineIndex;
    COORD ClientSize;
    DWORD LineCount;
    DWORD StartLine;
    YORI_STRING Remaining;
    YORI_STRING DisplayLine;
    BOOLEAN AcceleratorPreviouslyFound;
    BOOLEAN AcceleratorFound;
    YORI_ALLOC_SIZE_T OffsetToAcceleratorInDisplay;
    YORI_ALLOC_SIZE_T RemainingOffsetToAccelerator;

    YoriWinGetControlClientSize(&Label->Ctrl, &ClientSize);
    LineCount = YoriWinLabelCountLinesRequired(Label);
    if (LineCount > (DWORD)ClientSize.Y) {
        StartLine = 0;
    } else {
        if (Label->TextVerticalAlign == YoriWinTextVerticalAlignTop) {
            StartLine = 0;
        } else if (Label->TextVerticalAlign == YoriWinTextVerticalAlignBottom) {
            StartLine = ClientSize.Y - LineCount;
        } else {
            StartLine = (ClientSize.Y - LineCount) / 2;
        }
    }

    WinAttributes = Label->Ctrl.DefaultAttributes;
    TextAttributes = Label->TextAttributes;

    if (Label->Ctrl.AcceleratorChar == 0) {
        AcceleratorPreviouslyFound = TRUE;
    } else {
        AcceleratorPreviouslyFound = FALSE;
    }
    AcceleratorFound = FALSE;
    Remaining.StartOfString = Label->Caption.StartOfString;
    Remaining.LengthInChars = Label->Caption.LengthInChars;
    RemainingOffsetToAccelerator = Label->AcceleratorOffset;

    //
    //  Swallow any leading characters that would not be displayed
    //

    YoriWinLabelTrimSwallowChars(&Remaining, &AcceleratorFound, &RemainingOffsetToAccelerator);
    if (AcceleratorFound) {
        AcceleratorPreviouslyFound = TRUE;
    }

    for (LineIndex = 0; LineIndex < (DWORD)ClientSize.Y; LineIndex++) {
        if (LineIndex < StartLine || StartLine + LineCount <= LineIndex) {
            YoriWinLabelClearClientLine(Label, &ClientSize, WinAttributes, LineIndex);
        } else {
            YoriWinLabelGetNextDisplayLine(&Remaining,
                                           ClientSize.X,
                                           &DisplayLine,
                                           &AcceleratorFound,
                                           &OffsetToAcceleratorInDisplay,
                                           &RemainingOffsetToAccelerator);

            ASSERT(DisplayLine.LengthInChars <= (DWORD)ClientSize.X && DisplayLine.LengthInChars > 0);

            //
            //  Calculate the starting cell for the text from the left based
            //  on alignment specification
            //
            StartColumn = 0;
            if (Label->TextAlign == YoriWinTextAlignRight) {
                StartColumn = (WORD)(ClientSize.X - DisplayLine.LengthInChars);
            } else if (Label->TextAlign == YoriWinTextAlignCenter) {
                StartColumn = (WORD)((ClientSize.X - DisplayLine.LengthInChars) / 2);
            }

            //
            //  Pad area before the text
            //

            for (CellIndex = 0; CellIndex < StartColumn; CellIndex++) {
                YoriWinSetControlClientCell(&Label->Ctrl, CellIndex, LineIndex, ' ', WinAttributes);
            }

            //
            //  Render the text and highlight the accelerator if it's in
            //  scope and highlighting is enabled
            //

            for (CharIndex = 0; CharIndex < DisplayLine.LengthInChars; CharIndex++) {
                if (Label->DisplayAccelerator &&
                    AcceleratorFound &&
                    !AcceleratorPreviouslyFound &&
                    OffsetToAcceleratorInDisplay == CharIndex) {

                    CharAttributes = Label->AcceleratorTextAttributes;

                } else {
                    CharAttributes = TextAttributes;
                }
                YoriWinSetControlClientCell(&Label->Ctrl, (WORD)(StartColumn + CharIndex), LineIndex, DisplayLine.StartOfString[CharIndex], CharAttributes);
            }

            //
            //  Pad the area after the text
            //

            for (CellIndex = (WORD)(StartColumn + DisplayLine.LengthInChars); CellIndex < (DWORD)(ClientSize.X); CellIndex++) {
                YoriWinSetControlClientCell(&Label->Ctrl, CellIndex, LineIndex, ' ', WinAttributes);
            }

            if (AcceleratorFound) {
                AcceleratorPreviouslyFound = TRUE;
            }
        }
    }

    return TRUE;
}

/**
 Set the text attributes within the label to a value and repaint the control.
 Note this refers to the attributes of the text within the label, not the
 entire label area.

 @param CtrlHandle Pointer to a label control.

 @param TextAttributes The new attributes to use.
 */
VOID
YoriWinLabelSetTextAttributes(
    __in PYORI_WIN_CTRL_HANDLE CtrlHandle,
    __in WORD TextAttributes
    )
{
    PYORI_WIN_CTRL_LABEL Label;
    PYORI_WIN_CTRL Ctrl;
    Ctrl = (PYORI_WIN_CTRL)CtrlHandle;
    Label = CONTAINING_RECORD(Ctrl, YORI_WIN_CTRL_LABEL, Ctrl);
    Label->TextAttributes = TextAttributes;
    YoriWinLabelPaint(Label);
}

/**
 Change the text within a label control.

 @param CtrlHandle Pointer to the label control.

 @param Caption Pointer to the new string to display in the label.  This can
        be empty but must not be NULL.

 @return TRUE to indicate success, FALSE to indicate failure.
 */
BOOLEAN
YoriWinLabelSetCaption(
    __in PYORI_WIN_CTRL_HANDLE CtrlHandle,
    __in PCYORI_STRING Caption
    )
{
    PYORI_WIN_CTRL_LABEL Label;
    PYORI_WIN_CTRL Ctrl;
    YORI_STRING CaptionCopy;
    Ctrl = (PYORI_WIN_CTRL)CtrlHandle;
    Label = CONTAINING_RECORD(Ctrl, YORI_WIN_CTRL_LABEL, Ctrl);

    if (!YoriLibAllocateString(&CaptionCopy, Caption->LengthInChars + 1)) {
        return FALSE;
    }

    if (Label->Style & YORI_WIN_LABEL_NO_ACCELERATOR) {
        memcpy(CaptionCopy.StartOfString, Caption->StartOfString, Caption->LengthInChars * sizeof(TCHAR));
        CaptionCopy.StartOfString[Caption->LengthInChars] = '\0';
        CaptionCopy.LengthInChars = Caption->LengthInChars;
    } else {
        YoriWinLabelParseAccelerator(Caption,
                                     &CaptionCopy,
                                     &Label->Ctrl.AcceleratorChar,
                                     &Label->AcceleratorOffset,
                                     NULL);
    }

    YoriLibFreeStringContents(&Label->Caption);
    memcpy(&Label->Caption, &CaptionCopy, sizeof(YORI_STRING));

    //
    //  If the control is initialized, repaint it.  Note this function can
    //  be called as part of control creation, in which case there's no
    //  parent to paint on yet.
    //

    if (Label->Ctrl.Parent != NULL) {
        YoriWinLabelPaint(Label);
    }
    return TRUE;
}

/**
 Set the size and location of a label control, and redraw the contents.

 @param CtrlHandle Pointer to the label to resize or reposition.

 @param CtrlRect Specifies the new size and position of the label.

 @return TRUE to indicate success, FALSE to indicate failure.
 */
BOOLEAN
YoriWinLabelReposition(
    __in PYORI_WIN_CTRL_HANDLE CtrlHandle,
    __in PSMALL_RECT CtrlRect
    )
{
    PYORI_WIN_CTRL Ctrl = (PYORI_WIN_CTRL)CtrlHandle;
    PYORI_WIN_CTRL_LABEL Label;

    Ctrl = (PYORI_WIN_CTRL)CtrlHandle;
    Label = CONTAINING_RECORD(Ctrl, YORI_WIN_CTRL_LABEL, Ctrl);

    if (!YoriWinControlReposition(Ctrl, CtrlRect)) {
        return FALSE;
    }

    YoriWinLabelPaint(Label);
    return TRUE;
}


/**
 Process input events for a label control.

 @param Ctrl Pointer to the label control.

 @param Event Pointer to the input event.

 @return TRUE to indicate that the event was processed and no further
         processing should occur.  FALSE to indicate that regular processing
         should continue (although this does not imply that no processing
         has already occurred.)
 */
BOOLEAN
YoriWinLabelEventHandler(
    __in PYORI_WIN_CTRL Ctrl,
    __in PYORI_WIN_EVENT Event
    )
{
    PYORI_WIN_CTRL_LABEL Label;
    Label = CONTAINING_RECORD(Ctrl, YORI_WIN_CTRL_LABEL, Ctrl);
    switch(Event->EventType) {
        case YoriWinEventParentDestroyed:
            YoriLibFreeStringContents(&Label->Caption);
            YoriWinDestroyControl(Ctrl);
            YoriLibDereference(Label);
            break;
        case YoriWinEventDisplayAccelerators:
            Label->DisplayAccelerator = TRUE;
            YoriWinLabelPaint(Label);
            break;
        case YoriWinEventHideAccelerators:
            Label->DisplayAccelerator = FALSE;
            YoriWinLabelPaint(Label);
            break;
    }

    return FALSE;
}

/**
 Create a label control and add it to a window.  This is destroyed when the
 window is destroyed.

 @param ParentHandle Pointer to the parent control.

 @param Size Specifies the location and size of the label.

 @param Caption Specifies the text to display on the label.

 @param Style Specifies style flags for the label.

 @return Pointer to the newly created control or NULL on failure.
 */
PYORI_WIN_CTRL_HANDLE
YoriWinLabelCreate(
    __in PYORI_WIN_CTRL_HANDLE ParentHandle,
    __in PSMALL_RECT Size,
    __in PYORI_STRING Caption,
    __in DWORD Style
    )
{
    PYORI_WIN_CTRL_LABEL Label;
    PYORI_WIN_CTRL Parent;
    PYORI_WIN_WINDOW TopLevelWindow;
    PYORI_WIN_WINDOW_MANAGER_HANDLE WinMgrHandle;

    Parent = (PYORI_WIN_CTRL)ParentHandle;

    Label = YoriLibReferencedMalloc(sizeof(YORI_WIN_CTRL_LABEL));
    if (Label == NULL) {
        return NULL;
    }

    ZeroMemory(Label, sizeof(YORI_WIN_CTRL_LABEL));

    if (Style & YORI_WIN_LABEL_STYLE_RIGHT_ALIGN) {
        Label->TextAlign = YoriWinTextAlignRight;
    } else if (Style & YORI_WIN_LABEL_STYLE_CENTER) {
        Label->TextAlign = YoriWinTextAlignCenter;
    }

    if (Style & YORI_WIN_LABEL_STYLE_BOTTOM_ALIGN) {
        Label->TextVerticalAlign = YoriWinTextVerticalAlignBottom;
    } else if (Style & YORI_WIN_LABEL_STYLE_VERTICAL_CENTER) {
        Label->TextVerticalAlign = YoriWinTextVerticalAlignCenter;
    }

    Label->Style = Style;

    if (!YoriWinLabelSetCaption(&Label->Ctrl, Caption)) {
        YoriLibDereference(Label);
        return NULL;
    }

    Label->Ctrl.NotifyEventFn = YoriWinLabelEventHandler;
    if (!YoriWinCreateControl(Parent, Size, FALSE, FALSE, &Label->Ctrl)) {
        YoriLibFreeStringContents(&Label->Caption);
        YoriLibDereference(Label);
        return NULL;
    }

    Label->TextAttributes = Label->Ctrl.DefaultAttributes;

    TopLevelWindow = YoriWinGetTopLevelWindow(&Label->Ctrl);
    WinMgrHandle = YoriWinGetWindowManagerHandle(TopLevelWindow);

    Label->AcceleratorTextAttributes = YoriWinMgrDefaultColorLookup(WinMgrHandle, YoriWinColorAcceleratorDefault);

    if (Parent->Parent != NULL) {
        Label->Ctrl.RelativeToParentClient = FALSE;
    }

    YoriWinLabelPaint(Label);

    return &Label->Ctrl;
}


// vim:sw=4:ts=4:et:
