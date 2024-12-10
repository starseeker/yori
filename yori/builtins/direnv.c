/**
 * @file builtins/direnv.c
 *
 * Yori execute scripts based on current directory to update environment
 *
 * Copyright (c) 2019 Malcolm J. Smith
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
#include <yoricall.h>

/**
 Help text to display to the user.
 */
const
CHAR strDirenvHelpText[] =
        "\n"
        "Apply per-directory scripts to environment as current directory changes based\n"
        "on envrc.ys1 files.\n"
        "\n"
        "DIRENV [-license] [-a | -i | -u]\n"
        "\n"
        "   -a             Apply changes based on the current directory\n"
        "   -i             Install directory change monitor\n"
        "   -u             Uninstall directory change monitor\n";

/**
 Display usage text to the user.
 */
BOOL
DirenvHelp(VOID)
{
    YoriLibOutput(YORI_LIB_OUTPUT_STDOUT, _T("Direnv %i.%02i\n"), YORI_VER_MAJOR, YORI_VER_MINOR);
#if YORI_BUILD_ID
    YoriLibOutput(YORI_LIB_OUTPUT_STDOUT, _T("  Build %i\n"), YORI_BUILD_ID);
#endif
    YoriLibOutput(YORI_LIB_OUTPUT_STDOUT, _T("%hs"), strDirenvHelpText);
    return TRUE;
}

/**
 Help text to display to the user.
 */
const
CHAR strDirenvApplyHelpText[] =
        "\n"
        "Check if the current directory has changed, and if so, apply updates to\n"
        "the environment based on envrc.ys1 files.\n"
        "\n"
        "DIRENVAPPLY [-license]\n";

/**
 Display usage text to the user.
 */
BOOL
DirenvApplyHelp(VOID)
{
    YoriLibOutput(YORI_LIB_OUTPUT_STDOUT, _T("DirenvApply %i.%02i\n"), YORI_VER_MAJOR, YORI_VER_MINOR);
#if YORI_BUILD_ID
    YoriLibOutput(YORI_LIB_OUTPUT_STDOUT, _T("  Build %i\n"), YORI_BUILD_ID);
#endif
    YoriLibOutput(YORI_LIB_OUTPUT_STDOUT, _T("%hs"), strDirenvApplyHelpText);
    return TRUE;
}

/**
 The path to any previously invoked directory script.  This is remembered so
 it can be invoked to undo any actions it performed when the user leaves the
 directory.
 */
YORI_STRING DirenvPreviousExecutedScript;

/**
 The previous current directory.  This is recorded to avoid rescanning for
 scripts to invoke if the current directory has not changed.
 */
YORI_STRING DirenvPreviousCurrentDirectory;

/**
 Set to TRUE to indicate that direnv has been linked into the shell, thereby
 exposing the DIRENVAPPLY command.
 */
BOOLEAN DirenvInstalled;

/**
 Set to TRUE to indicate that direnvapply has been called and should not be
 called recursively.
 */
BOOLEAN DirenvApplyInvoked;

/**
 Notification that the module is being unloaded or the shell is exiting,
 used to indicate any pending state should be cleaned up.
 */
VOID
YORI_BUILTIN_FN
DirenvNotifyUnload(VOID)
{
    YoriLibFreeStringContents(&DirenvPreviousExecutedScript);
    YoriLibFreeStringContents(&DirenvPreviousCurrentDirectory);
}

/**
 If there is a previously executed script, invoke it again to undo the actions
 it performed, and free the previously executed script string.
 */
VOID
DirenvUndoPreviousScript(VOID)
{
    YoriLibSPrintf(&DirenvPreviousExecutedScript.StartOfString[DirenvPreviousExecutedScript.LengthInChars], _T(" -undo"));
    DirenvPreviousExecutedScript.LengthInChars += sizeof(" -undo") - 1;
    ASSERT(DirenvPreviousExecutedScript.LengthInChars < DirenvPreviousExecutedScript.LengthAllocated);
    DirenvApplyInvoked = TRUE;
    YoriCallExecuteExpression(&DirenvPreviousExecutedScript);
    DirenvApplyInvoked = FALSE;
    YoriLibFreeStringContents(&DirenvPreviousExecutedScript);
}

DWORD
YORI_BUILTIN_FN
YoriCmd_DIRENVAPPLY(
    __in YORI_ALLOC_SIZE_T ArgC,
    __in YORI_STRING ArgV[]
    );

/**
 Register the plugin to ensure its memory remains until the shell instance
 exits.  Load from the environment any state indicating the script to
 invoke in order to undo existing changes.

 @return TRUE to indicate success, FALSE to indicate failure.
 */
BOOL
DirenvInstall(VOID)
{
    YORI_STRING DirenvApplyCmd;
    YORI_ALLOC_SIZE_T ValueLength;

    YoriLibConstantString(&DirenvApplyCmd, _T("DIRENVAPPLY"));
    if (!YoriCallBuiltinRegister(&DirenvApplyCmd, YoriCmd_DIRENVAPPLY)) {
        return FALSE;
    }

    YoriCallSetUnloadRoutine(DirenvNotifyUnload);

    //
    //  There shouldn't be any string if we weren't installed.
    //  Load from the environment any undo script to invoke if this shell
    //  instance was launched from another shell instance where an active
    //  script was already active.
    //

    ASSERT(DirenvPreviousExecutedScript.LengthInChars == 0);
    ValueLength = (YORI_ALLOC_SIZE_T)GetEnvironmentVariable(_T("DIRENVACTIVESCRIPT"), NULL, 0);
    if (ValueLength == 0) {
        DirenvInstalled = TRUE;
        return TRUE;
    }

    //
    //  Allocate space for the script with the -undo argument
    //

    if (!YoriLibAllocateString(&DirenvPreviousExecutedScript, ValueLength + sizeof(" -undo") + 1)) {
        DirenvInstalled = TRUE;
        return TRUE;
    }

    DirenvPreviousExecutedScript.LengthInChars = (YORI_ALLOC_SIZE_T)
        GetEnvironmentVariable(_T("DIRENVACTIVESCRIPT"),
                               DirenvPreviousExecutedScript.StartOfString,
                               DirenvPreviousExecutedScript.LengthAllocated);

    DirenvInstalled = TRUE;
    return TRUE;
}

/**
 Install the apply command to execute between shell commands to detect
 directory changes which may indicate that scripts need to be invoked.

 @return TRUE to indicate success, FALSE to indicate failure.
 */
BOOL
DirenvInstallApplyHook(VOID)
{
    YORI_STRING YoriPostcmd;
    YORI_STRING YoriPostcmdName;
    YORI_STRING NewPrecmdComponent;
    YORI_ALLOC_SIZE_T PrecmdLength;

    //
    //  Get the current YORIPOSTCMD.  Allocate an extra three chars for
    //  " & ", plus the direnvapply command.
    //

    YoriLibConstantString(&NewPrecmdComponent, _T("direnv -a"));
    PrecmdLength = (YORI_ALLOC_SIZE_T)GetEnvironmentVariable(_T("YORIPOSTCMD"), NULL, 0);
    if (!YoriLibAllocateString(&YoriPostcmd, PrecmdLength + 3 + NewPrecmdComponent.LengthInChars + 1)) {
        return FALSE;
    }

    //
    //  See if the direnvapply command is already there, if not, insert
    //  it
    //

    YoriPostcmd.LengthInChars = (YORI_ALLOC_SIZE_T)GetEnvironmentVariable(_T("YORIPOSTCMD"), YoriPostcmd.StartOfString, YoriPostcmd.LengthAllocated);
    if (YoriLibFindFirstMatchingSubstring(&YoriPostcmd, 1, &NewPrecmdComponent, NULL) == NULL) {

        //
        //  Add " & " if the command already contains something
        //

        if (YoriPostcmd.LengthInChars > 0) {
            memcpy(&YoriPostcmd.StartOfString[YoriPostcmd.LengthInChars], _T(" & "), 3 * sizeof(TCHAR));
            YoriPostcmd.LengthInChars += 3;
        }

        //
        //  Add direnvapply
        //

        memcpy(&YoriPostcmd.StartOfString[YoriPostcmd.LengthInChars], NewPrecmdComponent.StartOfString, NewPrecmdComponent.LengthInChars * sizeof(TCHAR));
        YoriPostcmd.LengthInChars = YoriPostcmd.LengthInChars + NewPrecmdComponent.LengthInChars;

        YoriPostcmd.StartOfString[YoriPostcmd.LengthInChars] = '\0';
        YoriLibConstantString(&YoriPostcmdName, _T("YORIPOSTCMD"));
        YoriCallSetEnvironmentVariable(&YoriPostcmdName, &YoriPostcmd);
    }

    YoriLibFreeStringContents(&YoriPostcmd);
    return TRUE;
}

/**
 Uninstall the module allowing it to leave memory.

 @return TRUE to indicate success, FALSE to indicate failure.
 */
BOOL
DirenvUninstall(VOID)
{
    YORI_STRING DirenvApplyCmd;
    YoriLibConstantString(&DirenvApplyCmd, _T("DIRENVAPPLY"));
    YoriCallBuiltinUnregister(&DirenvApplyCmd, YoriCmd_DIRENVAPPLY);
    DirenvInstalled = FALSE;
    return TRUE;
}

/**
 Check if an apply command is currently registered to run between shell
 commands, and if one is found, remove it.

 @return TRUE to indicate success, FALSE to indicate failure.
 */
BOOL
DirenvUninstallApplyHook(VOID)
{
    YORI_STRING YoriPostcmd;
    YORI_STRING YoriPostcmdName;
    YORI_STRING NewPrecmdComponent;
    YORI_ALLOC_SIZE_T PrecmdLength;
    YORI_ALLOC_SIZE_T FoundOffset;
    YORI_ALLOC_SIZE_T FoundLength;

    //
    //  Get the current YORIPOSTCMD.
    //

    YoriLibConstantString(&NewPrecmdComponent, _T("direnv -a"));
    PrecmdLength = (YORI_ALLOC_SIZE_T)GetEnvironmentVariable(_T("YORIPOSTCMD"), NULL, 0);
    if (!YoriLibAllocateString(&YoriPostcmd, PrecmdLength + 1)) {
        return FALSE;
    }

    YoriPostcmd.LengthInChars = (YORI_ALLOC_SIZE_T)GetEnvironmentVariable(_T("YORIPOSTCMD"), YoriPostcmd.StartOfString, YoriPostcmd.LengthAllocated);

    //
    //  Look for direnvapply.
    //

    if (YoriLibFindFirstMatchingSubstring(&YoriPostcmd, 1, &NewPrecmdComponent, &FoundOffset) != NULL) {

        //
        //  Remove any spaces or '&' from before the direnvapply that we
        //  found.
        //

        FoundLength = NewPrecmdComponent.LengthInChars;
        while (FoundOffset > 0) {
            if (YoriPostcmd.StartOfString[FoundOffset - 1] != ' ' &&
                YoriPostcmd.StartOfString[FoundOffset - 1] != '&') {
                break;
            }
            FoundOffset--;
            FoundLength++;
        }

        //
        //  Copy the trailing portion of the string over the area to
        //  remove
        //

        if (YoriPostcmd.LengthInChars > FoundOffset + FoundLength) {
            memmove(&YoriPostcmd.StartOfString[FoundOffset], &YoriPostcmd.StartOfString[FoundOffset + FoundLength], YoriPostcmd.LengthInChars - FoundOffset - FoundLength);
        }

        // 
        //  Truncate and save
        //

        YoriPostcmd.LengthInChars = YoriPostcmd.LengthInChars - FoundLength;
        YoriPostcmd.StartOfString[YoriPostcmd.LengthInChars] = '\0';
        YoriLibConstantString(&YoriPostcmdName, _T("YORIPOSTCMD"));
        if (YoriPostcmd.LengthInChars == 0) {
            YoriCallSetEnvironmentVariable(&YoriPostcmdName, NULL);
        } else {
            YoriCallSetEnvironmentVariable(&YoriPostcmdName, &YoriPostcmd);
        }
    }
    YoriLibFreeStringContents(&YoriPostcmd);
    return TRUE;
}

/**
 Check if a whitelist is specified in DIRENVDIRLIST.  If one is specified,
 check if the directory being entered is in the whitelist.  Return TRUE to
 indicate the script can be executed, which occurs if the whitelist is
 empty or the element is found within the whitelist.

 @param Directory Pointer to a string containing the directory to check.

 @return TRUE if the script in the directory should be executed, FALSE if it
         should not be executed.
 */
BOOLEAN
DirenvScriptAllowedInDirectory(
    __in PYORI_STRING Directory
    )
{
    YORI_STRING Whitelist;
    YORI_STRING Remaining;
    YORI_STRING Component;
    LPTSTR NextComponent;

    YoriLibInitEmptyString(&Whitelist);
    if (!YoriLibAllocateAndGetEnvironmentVariable(_T("DIRENVDIRLIST"), &Whitelist)) {
        return FALSE;
    }

    if (Whitelist.LengthInChars == 0) {
        YoriLibFreeStringContents(&Whitelist);
        return TRUE;
    }

    YoriLibInitEmptyString(&Remaining);
    YoriLibInitEmptyString(&Component);

    Remaining.LengthInChars = Whitelist.LengthInChars;
    Remaining.StartOfString = Whitelist.StartOfString;

    while (Remaining.LengthInChars > 0) {
        Component.StartOfString = Remaining.StartOfString;
        Component.LengthInChars = Remaining.LengthInChars;
        NextComponent = YoriLibFindLeftMostCharacter(&Remaining, ';');

        if (NextComponent != NULL) {
            Remaining.LengthInChars = Remaining.LengthInChars - (YORI_ALLOC_SIZE_T)(NextComponent - Remaining.StartOfString) - 1;
            Remaining.StartOfString = NextComponent + 1;
            Component.LengthInChars = (YORI_ALLOC_SIZE_T)(NextComponent - Component.StartOfString);
        } else {
            Remaining.LengthInChars = 0;
        }

        YoriLibOutput(YORI_LIB_OUTPUT_STDOUT, _T("Comparing %y to %y\n"), Directory, &Component);

        if (YoriLibCompareStringInsensitive(Directory, &Component) == 0) {
            YoriLibFreeStringContents(&Whitelist);
            YoriLibOutput(YORI_LIB_OUTPUT_STDOUT, _T("Return match\n"));
            return TRUE;
        }
    }

    YoriLibOutput(YORI_LIB_OUTPUT_STDOUT, _T("Return no match found\n"));
    YoriLibFreeStringContents(&Whitelist);
    return FALSE;
}

/**
 Check for a directory change, and if one is detected, probe to find any
 script to invoke.  If there is a script to invoke that's different from
 any existing script, firstly undo the effects of the previous script,
 then invoke the new one.

 @return TRUE to indicate success, FALSE to indicate failure.
 */
DWORD
DirenvApplyInternal(VOID)
{
    YORI_ALLOC_SIZE_T CurrentDirectoryLength;
    YORI_STRING CurrentDirectory;
    YORI_STRING NewScript;
    YORI_STRING CurrentDirectorySubset;
    YORI_STRING EnvName;

    //
    //  In the vast majority of cases, this module is already installed.
    //  That may not be the case if a subshell is launched though, in which
    //  case the apply operation is implicitly installing, and loading state
    //  from the environment, which is slow but correct.
    //

    if (!DirenvInstalled) {
        DirenvInstall();
    }

    //
    //  Find what the current directory is
    //

    CurrentDirectoryLength = (YORI_ALLOC_SIZE_T)GetCurrentDirectory(0, NULL);
    if (!YoriLibAllocateString(&CurrentDirectory, CurrentDirectoryLength)) {
        return EXIT_FAILURE;
    }

    CurrentDirectoryLength = (YORI_ALLOC_SIZE_T)GetCurrentDirectory(CurrentDirectory.LengthAllocated, CurrentDirectory.StartOfString);
    if (CurrentDirectoryLength == 0 || CurrentDirectoryLength >= CurrentDirectory.LengthAllocated) {
        YoriLibFreeStringContents(&CurrentDirectory);
        return EXIT_FAILURE;
    }
    CurrentDirectory.LengthInChars = CurrentDirectoryLength;

    //
    //  If it's the same as before, no work to do
    //

    if (YoriLibCompareStringInsensitive(&CurrentDirectory, &DirenvPreviousCurrentDirectory) == 0) {
        YoriLibFreeStringContents(&CurrentDirectory);
        return EXIT_SUCCESS;
    }

    //
    //  Allocate space for the new script.  We need to reserve enough for
    //  the undo operation.
    //

    if (!YoriLibAllocateString(&NewScript, CurrentDirectory.LengthInChars + sizeof("\\envrc.ys1 -undo"))) {
        YoriLibFreeStringContents(&CurrentDirectory);
        return EXIT_FAILURE;
    }

    //
    //  Keep moving up directories from the current directory.  Once a
    //  envrc.ys1 file is found, execute it.
    //

    YoriLibInitEmptyString(&CurrentDirectorySubset);
    CurrentDirectorySubset.StartOfString = CurrentDirectory.StartOfString;
    CurrentDirectorySubset.LengthInChars = CurrentDirectory.LengthInChars;

    while (TRUE) {
        NewScript.LengthInChars = YoriLibSPrintf(NewScript.StartOfString, _T("%y\\envrc.ys1"), &CurrentDirectorySubset);

        if (GetFileAttributes(NewScript.StartOfString) != (DWORD)-1) {

            //
            //  If the script we found is the same one that's active, do
            //  nothing.
            //

            if (YoriLibCompareStringInsensitive(&NewScript, &DirenvPreviousExecutedScript) == 0) {
                YoriLibFreeStringContents(&NewScript);
                break;
            }

            if (!DirenvScriptAllowedInDirectory(&CurrentDirectorySubset)) {
                YoriLibFreeStringContents(&NewScript);
                break;
            }

            if (DirenvPreviousExecutedScript.LengthInChars > 0) {
                DirenvUndoPreviousScript();
            }

            //
            //  Push the current script into the environment where subshells
            //  can find it
            //

            YoriLibConstantString(&EnvName, _T("DIRENVACTIVESCRIPT"));
            YoriCallSetEnvironmentVariable(&EnvName, &NewScript);

            ASSERT(DirenvPreviousExecutedScript.MemoryToFree == NULL);
            memcpy(&DirenvPreviousExecutedScript, &NewScript, sizeof(YORI_STRING));
            YoriLibInitEmptyString(&NewScript);

            DirenvApplyInvoked = TRUE;
            YoriCallExecuteExpression(&DirenvPreviousExecutedScript);
            DirenvApplyInvoked = FALSE;
            break;
        }

        while (CurrentDirectorySubset.LengthInChars > 0) {
            CurrentDirectorySubset.LengthInChars--;
            if (YoriLibIsSep(CurrentDirectorySubset.StartOfString[CurrentDirectorySubset.LengthInChars])) {
                break;
            }
        }

        //
        //  No active script is found.  See if we need to undo the effects of
        //  a previous script.
        //

        if (CurrentDirectorySubset.LengthInChars == 0) {
            if (DirenvPreviousExecutedScript.LengthInChars > 0) {
                DirenvUndoPreviousScript();
            }
            YoriLibFreeStringContents(&NewScript);
            break;
        }
    }

    YoriLibFreeStringContents(&DirenvPreviousCurrentDirectory);
    memcpy(&DirenvPreviousCurrentDirectory, &CurrentDirectory, sizeof(YORI_STRING));

    return EXIT_SUCCESS;
}

/**
 Apply any changes based on envrc.ys1 files in the current directory or its
 parents.

 @param ArgC The number of arguments.

 @param ArgV The argument array.

 @return ExitCode, zero for success, nonzero for failure.
 */
DWORD
YORI_BUILTIN_FN
YoriCmd_DIRENVAPPLY(
    __in YORI_ALLOC_SIZE_T ArgC,
    __in YORI_STRING ArgV[]
    )
{
    YORI_ALLOC_SIZE_T i;
    BOOL ArgumentUnderstood;
    YORI_STRING Arg;

    for (i = 1; i < ArgC; i++) {

        ArgumentUnderstood = FALSE;

        if (YoriLibIsCommandLineOption(&ArgV[i], &Arg)) {

            if (YoriLibCompareStringWithLiteralInsensitive(&Arg, _T("?")) == 0) {
                DirenvApplyHelp();
                return EXIT_SUCCESS;
            } else if (YoriLibCompareStringWithLiteralInsensitive(&Arg, _T("license")) == 0) {
                YoriLibDisplayMitLicense(_T("2019"));
                return EXIT_SUCCESS;
            }
        }

        if (!ArgumentUnderstood) {
            YoriLibOutput(YORI_LIB_OUTPUT_STDERR, _T("Argument not understood, ignored: %y\n"), &ArgV[i]);
        }
    }

    //
    //  If this is mistakenly invoked from a script that it invokes, break the
    //  recursion by not applying further updates
    //

    if (DirenvApplyInvoked) {
        return EXIT_SUCCESS;
    }

    return DirenvApplyInternal();

}

/**
 A list of operations supported by the direnv command.
 */
typedef enum _DIRENV_OPERATION {
    DirenvOpNone = 0,
    DirenvOpInstall = 1,
    DirenvOpUninstall = 2,
    DirenvOpApply = 3
} DIRENV_OPERATION;

/**
 Install or uninstall directory change monitoring.

 @param ArgC The number of arguments.

 @param ArgV The argument array.

 @return ExitCode, zero for success, nonzero for failure.
 */
DWORD
YORI_BUILTIN_FN
YoriCmd_DIRENV(
    __in YORI_ALLOC_SIZE_T ArgC,
    __in YORI_STRING ArgV[]
    )
{
    BOOL ArgumentUnderstood;
    YORI_ALLOC_SIZE_T i;
    YORI_STRING Arg;
    DIRENV_OPERATION Op;

    Op = DirenvOpNone;

    YoriLibLoadNtDllFunctions();
    YoriLibLoadKernel32Functions();

    for (i = 1; i < ArgC; i++) {

        ArgumentUnderstood = FALSE;
        ASSERT(YoriLibIsStringNullTerminated(&ArgV[i]));

        if (YoriLibIsCommandLineOption(&ArgV[i], &Arg)) {

            if (YoriLibCompareStringWithLiteralInsensitive(&Arg, _T("?")) == 0) {
                DirenvHelp();
                return EXIT_SUCCESS;
            } else if (YoriLibCompareStringWithLiteralInsensitive(&Arg, _T("license")) == 0) {
                YoriLibDisplayMitLicense(_T("2019"));
                return EXIT_SUCCESS;
            } else if (YoriLibCompareStringWithLiteralInsensitive(&Arg, _T("a")) == 0) {
                ArgumentUnderstood = TRUE;
                Op = DirenvOpApply;
            } else if (YoriLibCompareStringWithLiteralInsensitive(&Arg, _T("i")) == 0) {
                ArgumentUnderstood = TRUE;
                Op = DirenvOpInstall;
            } else if (YoriLibCompareStringWithLiteralInsensitive(&Arg, _T("u")) == 0) {
                ArgumentUnderstood = TRUE;
                Op = DirenvOpUninstall;
            }
        }

        if (!ArgumentUnderstood) {
            YoriLibOutput(YORI_LIB_OUTPUT_STDERR, _T("Argument not understood, ignored: %y\n"), &ArgV[i]);
        }
    }

    if (Op == DirenvOpNone) {
        YoriLibOutput(YORI_LIB_OUTPUT_STDERR, _T("direnv: operation not specified\n"));
        return EXIT_FAILURE;
    }

    if (Op == DirenvOpApply) {
        return DirenvApplyInternal();
    }

    if (Op == DirenvOpInstall) {

        if (!DirenvInstalled) {
            DirenvInstall();
        }

        if (!DirenvInstallApplyHook()) {
            return EXIT_FAILURE;
        }
    }

    if (Op == DirenvOpUninstall) {
        if (DirenvInstalled) {
            DirenvUninstall();
        }

        if (!DirenvUninstallApplyHook()) {
            return EXIT_FAILURE;
        }
    }

    return EXIT_SUCCESS;
}

// vim:sw=4:ts=4:et:
