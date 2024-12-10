/**
 * @file osver/osver.c
 *
 * Yori shell display operating system version
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

#include <yoripch.h>
#include <yorilib.h>

/**
 Help text to display to the user.
 */
const
CHAR strOsVerHelpText[] =
        "\n"
        "Outputs the operating system version in a specified format.\n"
        "\n"
        "OSVER [-license] [-l] [-s criteria] [<fmt>]\n"
        "\n"
        "   -l             List all known operating system builds\n"
        "   -s             Search for build names containing string or build number\n"
        "\n"
        "Format specifiers are:\n"
        "   $arch$         The processor architecture\n"
        "   $BUILD$        The build number with leading zero\n"
        "   $build$        The build number without leading zero\n"
        "   $desc$         The human friendly build description\n"
        "   $edition$      The edition string for this version\n"
        "   $MAJOR$        The major version with leading zero\n"
        "   $major$        The major version without leading zero\n"
        "   $MINOR$        The minor version with leading zero\n"
        "   $minor$        The minor version without leading zero\n";

/**
 Display usage text to the user.
 */
BOOL
OsVerHelp(VOID)
{
    YoriLibOutput(YORI_LIB_OUTPUT_STDOUT, _T("OsVer %i.%02i\n"), YORI_VER_MAJOR, YORI_VER_MINOR);
#if YORI_BUILD_ID
    YoriLibOutput(YORI_LIB_OUTPUT_STDOUT, _T("  Build %i\n"), YORI_BUILD_ID);
#endif
    YoriLibOutput(YORI_LIB_OUTPUT_STDOUT, _T("%hs"), strOsVerHelpText);
    return TRUE;
}

/**
 A context structure to pass to the function expanding variables so it knows
 what values to use.
 */
typedef struct _OSVER_VERSION_RESULT {

    /**
     The OS major version number.
     */
    DWORD MajorVersion;

    /**
     The OS minor version number.
     */
    DWORD MinorVersion;

    /**
     The OS build number.
     */
    DWORD BuildNumber;

    /**
     The OS architecture.
     */
    DWORD Architecture;

    /**
     The OS edition string.
     */
    YORI_STRING Edition;

} OSVER_VERSION_RESULT, *POSVER_VERSION_RESULT;

/**
 An association between a numeric build number and a human string
 describing the significance of that build.
 */
typedef struct _OSVER_BUILD_DESCRIPTION {

    /**
     The reported build number.
     */
    DWORD BuildNumber;

    /**
     A human readable string describing the build.
     */
    LPSTR BuildDescription;
} OSVER_BUILD_DESCRIPTION, *POSVER_BUILD_DESCRIPTION;

/**
 A table of Windows builds known to this application.
 */
const
OSVER_BUILD_DESCRIPTION OsVerBuildDescriptions[] = {
    {511,  "Windows NT 3.1"},
    {528,  "Windows NT 3.1 SP3"},
    {807,  "Windows NT 3.5"},
    {1057, "Windows NT 3.51"},
    {1381, "Windows NT 4"},
    {2195, "Windows 2000"},
    {2600, "Windows XP"},
    {3790, "Windows Server 2003/XP 64 bit"},
    {6000, "Vista"},
    {6001, "Vista SP1/Server 2008"},
    {6002, "Vista SP2/Server 2008 SP2"},
    {6003, "Vista SP2/Server 2008 SP2"},
    {7600, "Windows 7/Server 2008 R2"},
    {7601, "Windows 7 SP1/Server 2008 R2 SP1"},
    {9200, "Windows 8/Server 2012"},
    {9600, "Windows 8.1/Server 2012 R2"},
    {10240, "Windows 10 TH1 1507"},
    {10586, "Windows 10 TH2 1511"},
    {14393, "Windows 10 RS1 1607/Server 2016"},
    {15063, "Windows 10 RS2 1703"},
    {16299, "Windows 10 RS3 1709"},
    {17134, "Windows 10 RS4 1803"},
    {17763, "Windows 10 RS5 1809/Server 2019"},
    {18362, "Windows 10 19H1 1903"},
    {18363, "Windows 10 19H2 1909"},
    {19041, "Windows 10 20H1 2004"},
    {19042, "Windows 10 20H2"},
    {19043, "Windows 10 21H1"},
    {19044, "Windows 10 21H2"},
    {19045, "Windows 10 22H2"},
    {20348, "Windows Server 2022"},
    {22000, "Windows 11"},
    {22621, "Windows 11 22H2"},
    {22631, "Windows 11 23H2"},
    {26100, "Windows 11 24H2"}
};

/**
 Return a pointer to a constant string describing the build.  Note this will
 always return a string, even if the string indicates it is an unknown build.

 @param BuildNumber The numeric build number to check for.

 @return Pointer to a constant string describing the build.
 */
LPCSTR
OsVerGetBuildDescriptionString(
    __in DWORD BuildNumber
    )
{
    DWORD Index;

    for (Index = 0; Index < sizeof(OsVerBuildDescriptions)/sizeof(OsVerBuildDescriptions[0]); Index++) {
        if (BuildNumber == OsVerBuildDescriptions[Index].BuildNumber) {
            return OsVerBuildDescriptions[Index].BuildDescription;
        }
    }

    return "unknown";
}

/**
 Return the number of characters needed to describe the human readable string
 describing the build.

 @param BuildNumber The numeric build number to check for.

 @return The number of characters in the human readable string describing the
         build.
 */
YORI_ALLOC_SIZE_T
OsVerLengthOfBuildDescription(
    __in DWORD BuildNumber
    )
{
    return (YORI_ALLOC_SIZE_T)strlen(OsVerGetBuildDescriptionString(BuildNumber));
}

/**
 An association between a numeric architecture and a human string
 describing the significance of that architecture.
 */
typedef struct _OSVER_ARCHITECTURE {

    /**
     The reported architecture.
     */
    DWORD Architecture;

    /**
     A human readable string describing the architecture;
     */
    LPSTR ArchitectureString;

} OSVER_ARCHITECTURE, *POSVER_ARCHITECTURE;

/**
 A table of processor architectures known to this application.
 */
const
OSVER_ARCHITECTURE OsVerArchitecture[] = {
    {YORI_PROCESSOR_ARCHITECTURE_INTEL,  "i386"},
    {YORI_PROCESSOR_ARCHITECTURE_MIPS,   "MIPS"},
    {YORI_PROCESSOR_ARCHITECTURE_ALPHA,  "AXP"},
    {YORI_PROCESSOR_ARCHITECTURE_PPC,    "PPC"},
    {YORI_PROCESSOR_ARCHITECTURE_ARM,    "ARM"},
    {YORI_PROCESSOR_ARCHITECTURE_IA64,   "IA64"},
    {YORI_PROCESSOR_ARCHITECTURE_AXP64,  "AXP64"},
    {YORI_PROCESSOR_ARCHITECTURE_AMD64,  "AMD64"},
    {YORI_PROCESSOR_ARCHITECTURE_ARM64,  "ARM64"}
};

/**
 Display a list of build strings that match a specified criteria.

 @param MatchString Pointer to a string to match against.  Note this can be
        NULL to match everything.
 
 @return TRUE to indicate success, FALSE to indicate failure.
 */
BOOLEAN
OsVerDisplayMatchingBuilds(
    __in_opt PYORI_STRING MatchString
    )
{
    YORI_MAX_SIGNED_T llTemp;
    YORI_STRING DisplayString;
    YORI_ALLOC_SIZE_T CharsConsumed;
    YORI_ALLOC_SIZE_T CharsNeeded;
    YORI_ALLOC_SIZE_T Index;

    //
    //  If the argument is a number, look up a string by build number.
    //

    if (MatchString != NULL &&
        YoriLibStringToNumber(MatchString, TRUE, &llTemp, &CharsConsumed) &&
        CharsConsumed > 0) {

        YoriLibOutput(YORI_LIB_OUTPUT_STDOUT, _T("%lli: %hs\n"), llTemp, OsVerGetBuildDescriptionString((DWORD)llTemp));
    }

    //
    //  Look for a substring match, even if the argument is numeric.
    //
    CharsNeeded = 0;
    for (Index = 0; Index < sizeof(OsVerBuildDescriptions)/sizeof(OsVerBuildDescriptions[0]); Index++) {
        CharsConsumed = (YORI_ALLOC_SIZE_T)strlen(OsVerBuildDescriptions[Index].BuildDescription);
        if (CharsConsumed > CharsNeeded) {
            CharsNeeded = CharsConsumed;
        }
    }

    if (!YoriLibAllocateString(&DisplayString, CharsNeeded + 1)) {
        return FALSE;
    }

    for (Index = 0; Index < sizeof(OsVerBuildDescriptions)/sizeof(OsVerBuildDescriptions[0]); Index++) {
        DisplayString.LengthInChars = YoriLibSPrintf(DisplayString.StartOfString, _T("%hs"), OsVerBuildDescriptions[Index].BuildDescription);

        if (MatchString == NULL ||
            YoriLibFindFirstMatchingSubstringInsensitive(&DisplayString, 1, MatchString, NULL) != NULL) {
            YoriLibOutput(YORI_LIB_OUTPUT_STDOUT, _T("%i: %y\n"), OsVerBuildDescriptions[Index].BuildNumber, &DisplayString);
        }
    }

    YoriLibFreeStringContents(&DisplayString);

    return TRUE;
}

/**
 Return a pointer to a constant string describing the processor architecture.
 Note this will always return a string, even if the string indicates it is an
 unknown architecture.

 @param Architecture The numeric architecture number to check for.

 @return Pointer to a constant string describing the architecture.
 */
LPCSTR
OsVerGetArchitectureDescriptionString(
    __in DWORD Architecture
    )
{
    DWORD Index;

    for (Index = 0; Index < sizeof(OsVerArchitecture)/sizeof(OsVerArchitecture[0]); Index++) {
        if (Architecture == OsVerArchitecture[Index].Architecture) {
            return OsVerArchitecture[Index].ArchitectureString;
        }
    }

    return "unknown";
}

/**
 Return the number of characters needed to describe the human readable string
 describing the processor architecture.

 @param Architecture The numeric architecture number to check for.

 @return The number of characters in the human readable string describing the
         architecture.
 */
YORI_ALLOC_SIZE_T
OsVerLengthOfArchitectureDescription(
    __in DWORD Architecture
    )
{
    return (YORI_ALLOC_SIZE_T)strlen(OsVerGetArchitectureDescriptionString(Architecture));
}

/**
 A callback function to expand any known variables found when parsing the
 format string.

 @param OutputString A pointer to the output string to populate with data
        if a known variable is found.  The allocated length indicates the
        amount of the string that can be written to.

 @param VariableName The variable name to expand.

 @param Context Pointer to a @ref OSVER_VERSION_RESULT structure containing
        the data to populate.
 
 @return The number of characters successfully populated, or the number of
         characters required in order to successfully populate, or zero
         on error.
 */
YORI_ALLOC_SIZE_T
OsVerExpandVariables(
    __inout PYORI_STRING OutputString,
    __in PYORI_STRING VariableName,
    __in PVOID Context
    )
{
    YORI_ALLOC_SIZE_T CharsNeeded;
    POSVER_VERSION_RESULT OsVerContext = (POSVER_VERSION_RESULT)Context;

    if (YoriLibCompareStringWithLiteral(VariableName, _T("arch")) == 0) {
        CharsNeeded = OsVerLengthOfArchitectureDescription(OsVerContext->Architecture);
    } else if (YoriLibCompareStringWithLiteral(VariableName, _T("BUILD")) == 0) {
        CharsNeeded = 5;
    } else if (YoriLibCompareStringWithLiteral(VariableName, _T("build")) == 0) {
        CharsNeeded = 5;
    } else if (YoriLibCompareStringWithLiteral(VariableName, _T("desc")) == 0) {
        CharsNeeded = OsVerLengthOfBuildDescription(OsVerContext->BuildNumber);
    } else if (YoriLibCompareStringWithLiteral(VariableName, _T("edition")) == 0) {
        if (OsVerContext->Edition.LengthInChars == 0) {
            YoriLibLoadOsEdition(&OsVerContext->Edition);
        }
        CharsNeeded = OsVerContext->Edition.LengthInChars;
    } else if (YoriLibCompareStringWithLiteral(VariableName, _T("MAJOR")) == 0) {
        CharsNeeded = 2;
    } else if (YoriLibCompareStringWithLiteral(VariableName, _T("major")) == 0) {
        CharsNeeded = 3;
    } else if (YoriLibCompareStringWithLiteral(VariableName, _T("MINOR")) == 0) {
        CharsNeeded = 2;
    } else if (YoriLibCompareStringWithLiteral(VariableName, _T("minor")) == 0) {
        CharsNeeded = 3;
    } else {
        return 0;
    }

    if (OutputString->LengthAllocated < CharsNeeded) {
        return CharsNeeded;
    }

    if (YoriLibCompareStringWithLiteral(VariableName, _T("arch")) == 0) {
        CharsNeeded = YoriLibSPrintf(OutputString->StartOfString, _T("%hs"), OsVerGetArchitectureDescriptionString(OsVerContext->Architecture));
    } else if (YoriLibCompareStringWithLiteral(VariableName, _T("BUILD")) == 0) {
        YoriLibSPrintf(OutputString->StartOfString, _T("%05i"), OsVerContext->BuildNumber);
    } else if (YoriLibCompareStringWithLiteral(VariableName, _T("build")) == 0) {
        if (OsVerContext->MinorVersion < 100000) {
            CharsNeeded = YoriLibSPrintf(OutputString->StartOfString, _T("%i"), OsVerContext->BuildNumber);
        } else {
            CharsNeeded = 0;
        }
    } else if (YoriLibCompareStringWithLiteral(VariableName, _T("desc")) == 0) {
        CharsNeeded = YoriLibSPrintf(OutputString->StartOfString, _T("%hs"), OsVerGetBuildDescriptionString(OsVerContext->BuildNumber));
    } else if (YoriLibCompareStringWithLiteral(VariableName, _T("edition")) == 0) {
        CharsNeeded = YoriLibSPrintf(OutputString->StartOfString, _T("%y"), &OsVerContext->Edition);
    } else if (YoriLibCompareStringWithLiteral(VariableName, _T("MAJOR")) == 0) {
        YoriLibSPrintf(OutputString->StartOfString, _T("%02i"), OsVerContext->MajorVersion);
    } else if (YoriLibCompareStringWithLiteral(VariableName, _T("MINOR")) == 0) {
        YoriLibSPrintf(OutputString->StartOfString, _T("%02i"), OsVerContext->MinorVersion);
    } else if (YoriLibCompareStringWithLiteral(VariableName, _T("major")) == 0) {
        if (OsVerContext->MajorVersion < 1000) {
            CharsNeeded = YoriLibSPrintf(OutputString->StartOfString, _T("%i"), OsVerContext->MajorVersion);
        } else {
            CharsNeeded = 0;
        }
    } else if (YoriLibCompareStringWithLiteral(VariableName, _T("minor")) == 0) {
        if (OsVerContext->MinorVersion < 1000) {
            CharsNeeded = YoriLibSPrintf(OutputString->StartOfString, _T("%i"), OsVerContext->MinorVersion);
        } else {
            CharsNeeded = 0;
        }
    }

    OutputString->LengthInChars = CharsNeeded;
    return CharsNeeded;
}

/**
 Capture the architecture number from the running system.

 @param VersionResult Pointer to the version information determined so far.
        This indicates whether the OS is capable of returning an architecture,
        and will be updated on completion with the detected architecture.
 */
VOID
OsVerGetArchitecture(
    __inout POSVER_VERSION_RESULT VersionResult
    )
{
    VersionResult->Architecture = YoriLibGetArchitecture();
}

#ifdef YORI_BUILTIN
/**
 The main entrypoint for the osver builtin command.
 */
#define ENTRYPOINT YoriCmd_OSVER
#else
/**
 The main entrypoint for the osver standalone application.
 */
#define ENTRYPOINT ymain
#endif

/**
 The main entrypoint for the osver cmdlet.

 @param ArgC The number of arguments.

 @param ArgV An array of arguments.

 @return Exit code of the process indicating success or failure.
 */
DWORD
ENTRYPOINT(
    __in YORI_ALLOC_SIZE_T ArgC,
    __in YORI_STRING ArgV[]
    )
{
    OSVER_VERSION_RESULT VersionResult;
    BOOLEAN ArgumentUnderstood;
    LPTSTR FormatString = _T("\x1b[41;34;1m\x2584\x1b[42;33;1m\x2584\x1b[0m Windows version: $major$.$minor$.$build$ ($desc$), $arch$\n");
    YORI_STRING DisplayString;
    YORI_ALLOC_SIZE_T i;
    YORI_STRING Arg;
    YORI_STRING YsFormatString;
    PYORI_STRING SearchString;
    YORI_ALLOC_SIZE_T StartArg = 0;
    BOOLEAN ListAllBuilds = FALSE;

    ZeroMemory(&VersionResult, sizeof(VersionResult));
    YoriLibInitEmptyString(&YsFormatString);
    SearchString = NULL;

    for (i = 1; i < ArgC; i++) {

        ArgumentUnderstood = FALSE;
        ASSERT(YoriLibIsStringNullTerminated(&ArgV[i]));

        if (YoriLibIsCommandLineOption(&ArgV[i], &Arg)) {

            if (YoriLibCompareStringWithLiteralInsensitive(&Arg, _T("?")) == 0) {
                OsVerHelp();
                return EXIT_SUCCESS;
            } else if (YoriLibCompareStringWithLiteralInsensitive(&Arg, _T("license")) == 0) {
                YoriLibDisplayMitLicense(_T("2017-2022"));
                return EXIT_SUCCESS;
            } else if (YoriLibCompareStringWithLiteralInsensitive(&Arg, _T("l")) == 0) {
                ListAllBuilds = TRUE;
                ArgumentUnderstood = TRUE;
            } else if (YoriLibCompareStringWithLiteralInsensitive(&Arg, _T("s")) == 0) {
                if (i + 1 < ArgC) {
                    SearchString = &ArgV[i + 1];
                    i++;
                    ArgumentUnderstood = TRUE;
                }
            }
        } else {
            ArgumentUnderstood = TRUE;
            StartArg = i;
            break;
        }

        if (!ArgumentUnderstood) {
            YoriLibOutput(YORI_LIB_OUTPUT_STDOUT, _T("Argument not understood, ignored: %y\n"), &ArgV[i]);
        }
    }

    if (ListAllBuilds) {
        if (!OsVerDisplayMatchingBuilds(NULL)) {
            return EXIT_FAILURE;
        }

        return EXIT_SUCCESS;
    } else if (SearchString != NULL) {
        if (!OsVerDisplayMatchingBuilds(SearchString)) {
            return EXIT_FAILURE;
        }

        return EXIT_SUCCESS;
    }

    if (StartArg == 0) {
        YoriLibConstantString(&YsFormatString, FormatString);
    } else {
        if (!YoriLibBuildCmdlineFromArgcArgv(ArgC - StartArg, &ArgV[StartArg], TRUE, FALSE, &YsFormatString)) {
            return EXIT_FAILURE;
        }
    }

    YoriLibInitEmptyString(&DisplayString);
    YoriLibGetOsVersion(&VersionResult.MajorVersion, &VersionResult.MinorVersion, &VersionResult.BuildNumber);
    OsVerGetArchitecture(&VersionResult);
    YoriLibExpandCommandVariables(&YsFormatString, '$', FALSE, OsVerExpandVariables, &VersionResult, &DisplayString);
    if (DisplayString.StartOfString != NULL) {
        YoriLibOutput(YORI_LIB_OUTPUT_STDOUT, _T("%y"), &DisplayString);
        YoriLibFreeStringContents(&DisplayString);
    }
    YoriLibFreeStringContents(&YsFormatString);
    YoriLibFreeStringContents(&VersionResult.Edition);

    return EXIT_SUCCESS;
}

// vim:sw=4:ts=4:et:
