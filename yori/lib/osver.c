/**
 * @file lib/osver.c
 *
 * Yori OS version query routines
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

#include "yoripch.h"
#include "yorilib.h"
#include <winver.h>

/**
 Previously returned value for the major OS version number.
 */
DWORD CachedMajorOsVersion;

/**
 Previously returned value for the minor OS version number.
 */
DWORD CachedMinorOsVersion;

/**
 Previously returned value for the build number.
 */
DWORD CachedBuildNumber;

/**
 Previously returned system page size.
 */
DWORD CachedPageSize;

/**
 Background support has been determined.
 */
BOOLEAN YoriLibBackgroundColorSupportDetermined;

/**
 The console supports background colors.  Only meaningful if
 YoriLibBackgroundColorSupportDetermined is TRUE.
 */
BOOLEAN YoriLibBackgroundColorSupported;

/**
 TRUE if the process is running under SSH.  Only meaningful if
 YoriLibRunningUnderSshDetermined is TRUE.
 */
BOOLEAN YoriLibRunningUnderSsh;

/**
 TRUE if whether the process is running under SSH has been determined.
 */
BOOLEAN YoriLibRunningUnderSshDetermined;

#if _WIN64
/**
 On 64 bit builds, the current process PEB is 64 bit.
 */
#define PYORI_LIB_PEB_NATIVE PYORI_LIB_PEB64
#else
/**
 On 32 bit builds, the current process PEB is 32 bit.
 */
#define PYORI_LIB_PEB_NATIVE PYORI_LIB_PEB32_NATIVE
#endif

/**
 Try to obtain Windows version numbers from the PEB directly.

 @param MajorVersion On successful completion, updated to contain the Windows
        major version number.

 @param MinorVersion On successful completion, updated to contain the Windows
        minor version number.

 @param BuildNumber On successful completion, updated to contain the Windows
        build number.
 */
__success(return)
BOOL
YoriLibGetOsVersionFromPeb(
    __out PDWORD MajorVersion,
    __out PDWORD MinorVersion,
    __out PDWORD BuildNumber
    )
{
    PYORI_LIB_PEB_NATIVE Peb;
    LONG Status;
    PROCESS_BASIC_INFORMATION BasicInfo;
    DWORD dwBytesReturned;

    if (DllNtDll.pNtQueryInformationProcess == NULL) {
        return FALSE;
    }

    Status = DllNtDll.pNtQueryInformationProcess(GetCurrentProcess(), 0, &BasicInfo, sizeof(BasicInfo), &dwBytesReturned);
    if (Status != 0) {
        return FALSE;
    }

    Peb = (PYORI_LIB_PEB_NATIVE)BasicInfo.PebBaseAddress;

    *MajorVersion = Peb->OSMajorVersion;
    *MinorVersion = Peb->OSMinorVersion;
    *BuildNumber = Peb->OSBuildNumber;

    return TRUE;
}

//
//  Disable warning about using deprecated GetVersion.
//

#if defined(_MSC_VER) && (_MSC_VER >= 1700)
#pragma warning(disable: 28159)
#endif

#if defined(__clang__)
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif

/**
 Return Windows version numbers.

 @param MajorVersion On successful completion, updated to contain the Windows
        major version number.

 @param MinorVersion On successful completion, updated to contain the Windows
        minor version number.

 @param BuildNumber On successful completion, updated to contain the Windows
        build number.
 */
VOID
YoriLibGetOsVersion(
    __out PDWORD MajorVersion,
    __out PDWORD MinorVersion,
    __out PDWORD BuildNumber
    )
{
    DWORD RawVersion;
    DWORD LocalMajorVersion;
    DWORD LocalMinorVersion;
    DWORD LocalBuildNumber;

    if (CachedMajorOsVersion != 0) {
        *MajorVersion = CachedMajorOsVersion;
        *MinorVersion = CachedMinorOsVersion;
        *BuildNumber = CachedBuildNumber;
        return;
    }

    if (DllKernel32.pGetVersionExW != NULL) {
        YORI_OS_VERSION_INFO OsVersionInfo;
        ZeroMemory(&OsVersionInfo, sizeof(OsVersionInfo));
        OsVersionInfo.dwOsVersionInfoSize = sizeof(OsVersionInfo);
        DllKernel32.pGetVersionExW(&OsVersionInfo);

        LocalMajorVersion = OsVersionInfo.dwMajorVersion;
        LocalMinorVersion = OsVersionInfo.dwMinorVersion;
        LocalBuildNumber = OsVersionInfo.dwBuildNumber;
    } else {
        RawVersion = GetVersion();

        LocalMajorVersion = LOBYTE(LOWORD(RawVersion));
        LocalMinorVersion = HIBYTE(LOWORD(RawVersion));
        LocalBuildNumber = HIWORD(RawVersion);
    }

    //
    //  On good versions of Windows, we stop here.  On broken versions of
    //  Windows, which lie about version numbers, we need to insist via
    //  a much more expensive mechanism.
    //

    if (LocalMajorVersion == 6 &&
        LocalMinorVersion == 2 &&
        LocalBuildNumber == 9200) {

        DWORD PebMajorVersion;
        DWORD PebMinorVersion;
        DWORD PebBuildNumber;

        if (YoriLibGetOsVersionFromPeb(&PebMajorVersion, &PebMinorVersion, &PebBuildNumber)) {
            LocalMajorVersion = PebMajorVersion;
            LocalMinorVersion = PebMinorVersion;
            LocalBuildNumber = PebBuildNumber;
        }
    }

    CachedMajorOsVersion = LocalMajorVersion;
    CachedMinorOsVersion = LocalMinorVersion;
    CachedBuildNumber = LocalBuildNumber;

    *MajorVersion = LocalMajorVersion;
    *MinorVersion = LocalMinorVersion;
    *BuildNumber = LocalBuildNumber;
}

/**
 Return the OS edition as a string.  On newer systems this is obtained
 directly from the system's branding provider, on older systems it needs to
 be emulated, and on really old systems it's just a string literal.

 @param Edition On successful completion, updated to contain a newly allocated
        string containing the system edition.

 @return TRUE to indicate success, FALSE to indicate failure.
 */
__success(return)
BOOL
YoriLibLoadOsEdition(
    __out PYORI_STRING Edition
    )
{
    YORI_OS_VERSION_INFO_EX OsVersionInfoEx;
    LPWSTR BrandingString;
    YORI_ALLOC_SIZE_T Length;

    YoriLibLoadWinBrandFunctions();

    //
    //  If the operating system supports asking for its brand, use that.
    //  This should exist on Vista+.
    //

    if (DllWinBrand.pBrandingFormatString != NULL) {
        BrandingString = DllWinBrand.pBrandingFormatString(L"%WINDOWS_LONG%");
        if (BrandingString == NULL) {
            return FALSE;
        }

        Length = (YORI_ALLOC_SIZE_T)wcslen(BrandingString);

        if (!YoriLibAllocateString(Edition, Length + 1)) {
            return FALSE;
        }

        Edition->LengthInChars = YoriLibSPrintf(Edition->StartOfString, _T("%s"), BrandingString);
        GlobalFree(BrandingString);
        return TRUE;
    }

    //
    //  Query the suite mask and system version.  This should only be needed
    //  for systems that predate version lies, so this can be a little
    //  careless.
    //

    OsVersionInfoEx.Core.dwOsVersionInfoSize = sizeof(OsVersionInfoEx);
    if (DllKernel32.pGetVersionExW == NULL ||
        !DllKernel32.pGetVersionExW(&OsVersionInfoEx.Core)) {
        BrandingString = _T("Windows NT");
    } else {
        DWORD WinVer;

        WinVer = OsVersionInfoEx.Core.dwMajorVersion << 16 | OsVersionInfoEx.Core.dwMinorVersion;

        switch(WinVer) {
            case 0x40000:
                if (OsVersionInfoEx.wSuiteMask & VER_SUITE_SMALLBUSINESS ||
                    OsVersionInfoEx.wSuiteMask & VER_SUITE_SMALLBUSINESS_RESTRICTED) {
                    BrandingString = _T("Small Business Server 4.x");
                } else if (OsVersionInfoEx.wSuiteMask & VER_SUITE_ENTERPRISE) {
                    BrandingString = _T("Windows NT 4.0 Enterprise Edition");
                } else if (OsVersionInfoEx.wSuiteMask & VER_SUITE_BACKOFFICE) {
                    BrandingString = _T("BackOffice Server 4.x");
                } else if (OsVersionInfoEx.wSuiteMask & VER_SUITE_TERMINAL) {
                    BrandingString = _T("Windows NT 4.0 Terminal Server Edition");
                } else if (OsVersionInfoEx.wProductType == VER_NT_SERVER ||
                           OsVersionInfoEx.wProductType == VER_NT_DOMAIN_CONTROLLER) {
                    BrandingString = _T("Windows NT 4.0 Server");
                } else if (OsVersionInfoEx.wProductType == VER_NT_WORKSTATION) {
                    BrandingString = _T("Windows NT 4.0 Workstation");
                } else {
                    BrandingString = _T("Windows NT 4.0 Unknown");
                }
                break;
            case 0x50000:
                if (OsVersionInfoEx.wSuiteMask & VER_SUITE_SMALLBUSINESS ||
                    OsVersionInfoEx.wSuiteMask & VER_SUITE_SMALLBUSINESS_RESTRICTED) {
                    BrandingString = _T("Small Business Server 2000");
                } else if (OsVersionInfoEx.wSuiteMask & VER_SUITE_ENTERPRISE) {
                    BrandingString = _T("Windows 2000 Advanced Server");
                } else if (OsVersionInfoEx.wSuiteMask & VER_SUITE_DATACENTER) {
                    BrandingString = _T("Windows 2000 DataCenter Server");
                } else if (OsVersionInfoEx.wSuiteMask & VER_SUITE_BACKOFFICE) {
                    BrandingString = _T("BackOffice 2000");
                } else if (OsVersionInfoEx.wProductType == VER_NT_SERVER ||
                           OsVersionInfoEx.wProductType == VER_NT_DOMAIN_CONTROLLER) {
                    BrandingString = _T("Windows 2000 Server");
                } else if (OsVersionInfoEx.wProductType == VER_NT_WORKSTATION) {
                    BrandingString = _T("Windows 2000 Professional");
                } else {
                    BrandingString = _T("Windows 2000 Unknown");
                }
                break;
            case 0x50001:
                if (OsVersionInfoEx.wSuiteMask & VER_SUITE_EMBEDDEDNT) {
                    BrandingString = _T("Windows XP Embedded");
                } else if (OsVersionInfoEx.wSuiteMask & VER_SUITE_PERSONAL) {
                    BrandingString = _T("Windows XP Home");
                } else {
                    BrandingString = _T("Windows XP Professional");
                }
                break;
            case 0x50002:
                if (OsVersionInfoEx.wSuiteMask & VER_SUITE_SMALLBUSINESS ||
                    OsVersionInfoEx.wSuiteMask & VER_SUITE_SMALLBUSINESS_RESTRICTED) {
                    BrandingString = _T("Small Business Server 2003");
                } else if (OsVersionInfoEx.wSuiteMask & VER_SUITE_ENTERPRISE) {
                    BrandingString = _T("Windows Server 2003 Enterprise Edition");
                } else if (OsVersionInfoEx.wSuiteMask & VER_SUITE_DATACENTER) {
                    BrandingString = _T("Windows Server 2003 Datacenter Edition");
                } else if (OsVersionInfoEx.wSuiteMask & VER_SUITE_BLADE) {
                    BrandingString = _T("Windows Server 2003 Web Edition");
                } else if (OsVersionInfoEx.wProductType == VER_NT_SERVER ||
                           OsVersionInfoEx.wProductType == VER_NT_DOMAIN_CONTROLLER) {
                    BrandingString = _T("Windows Server 2003");
                } else if (OsVersionInfoEx.wProductType == VER_NT_WORKSTATION) {
                    BrandingString = _T("Windows XP 64 bit edition");
                } else {
                    BrandingString = _T("Windows Server 2003 Unknown");
                }
                break;
            default:

                //
                //  WinBrand.dll should be available on Vista+, and SuiteMask
                //  is not available before NT 4.0 SP6, so this fallback
                //  should be somewhat accurate.
                //

                BrandingString = _T("Unknown Windows");
                break;
        }
    }

    YoriLibInitEmptyString(Edition);
    YoriLibYPrintf(Edition, _T("%s"), BrandingString);
    if (Edition->StartOfString == NULL) {
        return FALSE;
    }

    return TRUE;
}

/**
 Capture the architecture number from the running system.  Note this is the
 native architecture, not the emulated WOW architecture.

 @return The architecture number from the host system.
 */
DWORD
YoriLibGetArchitecture(VOID)
{
    YORI_SYSTEM_INFO SysInfo;
    DWORD MajorVersion;
    DWORD MinorVersion;
    DWORD BuildNumber;
    DWORD Architecture;
    WORD ProcessArch;
    WORD SystemArch;

    YoriLibGetOsVersion(&MajorVersion, &MinorVersion, &BuildNumber);

    if (DllKernel32.pIsWow64Process2 &&
        DllKernel32.pIsWow64Process2(GetCurrentProcess(), &ProcessArch, &SystemArch)) {

        Architecture = YORI_PROCESSOR_ARCHITECTURE_INTEL;

        switch(SystemArch) {
            case IMAGE_FILE_MACHINE_I386:
                Architecture = YORI_PROCESSOR_ARCHITECTURE_INTEL;
                break;
            case IMAGE_FILE_MACHINE_R3000:
            case IMAGE_FILE_MACHINE_R4000:
            case IMAGE_FILE_MACHINE_R10000:
                Architecture = YORI_PROCESSOR_ARCHITECTURE_MIPS;
                break;
            case IMAGE_FILE_MACHINE_ALPHA:
                Architecture = YORI_PROCESSOR_ARCHITECTURE_ALPHA;
                break;
            case IMAGE_FILE_MACHINE_POWERPC:
            case IMAGE_FILE_MACHINE_POWERPCFP:
                Architecture = YORI_PROCESSOR_ARCHITECTURE_PPC;
                break;
            case IMAGE_FILE_MACHINE_IA64:
                Architecture = YORI_PROCESSOR_ARCHITECTURE_IA64;
                break;
            case IMAGE_FILE_MACHINE_ARMNT:
                Architecture = YORI_PROCESSOR_ARCHITECTURE_ARM;
                break;
            case IMAGE_FILE_MACHINE_AMD64:
                Architecture = YORI_PROCESSOR_ARCHITECTURE_AMD64;
                break;
            case IMAGE_FILE_MACHINE_ARM64:
                Architecture = YORI_PROCESSOR_ARCHITECTURE_ARM64;
                break;
        }

        return Architecture;
    }

    if (MajorVersion < 4) {
        GetSystemInfo((LPSYSTEM_INFO)&SysInfo);

        Architecture = YORI_PROCESSOR_ARCHITECTURE_INTEL;

        //
        //  In old versions the wProcessorArchitecture member does not exist.
        //  For these systems, we have to look at dwProcessorType.
        //  Fortunately since these are old versions, the list is static.
        //

        switch(SysInfo.dwProcessorType) {
            case YORI_PROCESSOR_INTEL_386:
            case YORI_PROCESSOR_INTEL_486:
            case YORI_PROCESSOR_INTEL_PENTIUM:
            case YORI_PROCESSOR_INTEL_686:
                Architecture = YORI_PROCESSOR_ARCHITECTURE_INTEL;
                break;
            case YORI_PROCESSOR_MIPS_R4000:
                Architecture = YORI_PROCESSOR_ARCHITECTURE_MIPS;
                break;
            case YORI_PROCESSOR_ALPHA_21064:
                Architecture = YORI_PROCESSOR_ARCHITECTURE_ALPHA;
                break;
            case YORI_PROCESSOR_PPC_601:
            case YORI_PROCESSOR_PPC_603:
            case YORI_PROCESSOR_PPC_604:
            case YORI_PROCESSOR_PPC_620:
                Architecture = YORI_PROCESSOR_ARCHITECTURE_PPC;
                break;
        }

        return Architecture;

    } else if (DllKernel32.pGetNativeSystemInfo) {
        DllKernel32.pGetNativeSystemInfo((LPSYSTEM_INFO)&SysInfo);
    } else {
        GetSystemInfo((LPSYSTEM_INFO)&SysInfo);
    }
    Architecture = SysInfo.wProcessorArchitecture;
    return Architecture;
}

/**
 Return TRUE to indicate the target process is 32 bit, or FALSE if the target
 process is 64 bit.

 @param ProcessHandle A handle to the process to examine.

 @return TRUE to indicate the process is 32 bit, FALSE to indicate the target
         is 64 bit.
 */
__success(return)
BOOL
YoriLibIsProcess32Bit(
    __in HANDLE ProcessHandle
    )
{
    BOOL TargetProcess32Bit = TRUE;

    if (DllKernel32.pIsWow64Process != NULL) {

        //
        //  If this program is 32 bit, and it's Wow (ie., the system is 64
        //  bit), check the bitness of the target.  If this program is 64
        //  bit, check the target process.  If this program is 32 bit on a
        //  32 bit system (ie., we're 32 bit and not Wow) then the target
        //  must be 32 bit.
        // 

        if (sizeof(PVOID) == sizeof(ULONG)) {
            BOOL ThisProcessWow = FALSE;
            DllKernel32.pIsWow64Process(GetCurrentProcess(), &ThisProcessWow);

            if (ThisProcessWow) {
                DllKernel32.pIsWow64Process(ProcessHandle, &TargetProcess32Bit);
            }
        } else {
            DllKernel32.pIsWow64Process(ProcessHandle, &TargetProcess32Bit);
        }
    }

    return TargetProcess32Bit;
}

/**
 Return TRUE if the target process has a 32 bit PEB.

 @param ProcessHandle Handle to the target process.

 @return TRUE if the PEB is in 32 bit form, FALSE if it's in 64 bit form.
 */
__success(return)
BOOL
YoriLibDoesProcessHave32BitPeb(
    __in HANDLE ProcessHandle
    )
{

    UNREFERENCED_PARAMETER(ProcessHandle);

    //
    //  If the system doesn't support Wow64, this must be a 32 bit process
    //  checking another 32 bit process.
    //

    if (DllKernel32.pIsWow64Process == NULL) {
        return TRUE;
    }

    //
    //  If this is a 32 bit process, it can't debug a 64 bit process, so the
    //  target had better be 32 bit.
    //

    if (sizeof(PVOID) == sizeof(ULONG)) {

        return TRUE;
    }

    return FALSE;
}

/**
 Check if this is Nano server.  Nano is a bit strange since it uses a
 graphical console that doesn't behave like the regular one.  The official
 way to test for it is to check the registry; rather than do that, this
 routine exploits the fact that Nano uses a cut-down kernel32.dll that
 doesn't contain expected exports.  In particular, GetCurrentConsoleFontEx
 isn't supplied (which exists on Vista+) but GetConsoleScreenBufferInfoEx
 is supplied (also Vista+.)  This might break on some Vista beta build.

 @return TRUE if running on Nano server, FALSE if not.
 */
BOOLEAN
YoriLibIsNanoServer(VOID)
{
    if (DllKernel32.pGetCurrentConsoleFontEx == NULL &&
        DllKernel32.pGetConsoleScreenBufferInfoEx != NULL) {

        return TRUE;
    }

    if (DllKernel32.hDllKernel32Legacy != NULL) {

        return TRUE;
    }

    return FALSE;
}

/**
 Check if this console doesn't support background colors.  Nano ships with a
 buggy console that doesn't handle these correctly.  Outside of Nano, assume
 background color support is present; within Nano, assume it's not unless
 explicitly enabled.
 */
BOOLEAN
YoriLibDoesSystemSupportBackgroundColors(VOID)
{
    if (!YoriLibIsNanoServer()) {
        return TRUE;
    }

    if (!YoriLibBackgroundColorSupportDetermined) {
        YORI_MAX_SIGNED_T Enabled;

        YoriLibBackgroundColorSupported = FALSE;
        if (!YoriLibGetEnvironmentVariableAsNumber(_T("YORIBACKGROUND"), &Enabled)) {
            Enabled = 0;
        }

        YoriLibBackgroundColorSupportDetermined = TRUE;
        if (Enabled != 0) {
            YoriLibBackgroundColorSupported = TRUE;
        }
    }

    return YoriLibBackgroundColorSupported;
}

/**
 Indicate that the system doesn't know whether to support background colors
 or not, and support should be re-queried on next use.
 */
VOID
YoriLibResetSystemBackgroundColorSupport(VOID)
{
    YoriLibBackgroundColorSupportDetermined = FALSE;
}

/**
 Return TRUE if the program is running under an SSH connection.  Typically
 this should be transparent, but SSH isn't always full Win32 fidelity.

 @return TRUE if the process is being operated through an SSH connection,
         FALSE if not.
 */
BOOLEAN
YoriLibIsRunningUnderSsh(VOID)
{
    if (!YoriLibRunningUnderSshDetermined) {
        DWORD VarLength;

        VarLength = GetEnvironmentVariable(_T("SSH_CLIENT"), NULL, 0);
        if (VarLength > 0) {
            YoriLibRunningUnderSsh = TRUE;
        }

        YoriLibRunningUnderSshDetermined = TRUE;
    }
    return YoriLibRunningUnderSsh;
}

/**
 Return the system page size for the current system.

 @return The system page size for the current system.
 */
YORI_ALLOC_SIZE_T
YoriLibGetPageSize(VOID)
{
    YORI_SYSTEM_INFO SysInfo;
    DWORD ValidPageSize;

    //
    //  If the page size is known, return it.
    //
    if (CachedPageSize != 0) {
        return CachedPageSize;
    }


    //
    //  Ask the system for the page size.
    //
    SysInfo.dwPageSize = 0;
    GetSystemInfo((LPSYSTEM_INFO)&SysInfo);

    //
    //  Check if the page size is a power of 2, 4Kb or greater, and 1Mb or
    //  smaller.  If these criteria aren't met, assume this value is bogus.
    //

    for (ValidPageSize = 0x1000; ValidPageSize <= 0x100000; ValidPageSize = (ValidPageSize<<1)) {
        if (ValidPageSize == SysInfo.dwPageSize) {
            CachedPageSize = SysInfo.dwPageSize;
            return CachedPageSize;
        }
    }

    //
    //  Default to 4Kb, which is highly likely to be correct.
    //

    CachedPageSize = 0x1000;
    return CachedPageSize;
}

/**
 Check that the executable already has the specified subsystem version.
 If it doesn't, update the system version to have the specified value.

 @param NewMajor Verify, or possibly set, the subsystem major version to this
        value.

 @param NewMinor Verify, or possibly set, the subsystem minor version to this
        value.

 @return TRUE if the executable has this version or has been updated to it;
         FALSE if the version could not be determined or updated.
 */
BOOLEAN
YoriLibEnsureProcessSubsystemVersionAtLeast(
    __in WORD NewMajor,
    __in WORD NewMinor
    )
{
    PIMAGE_DOS_HEADER DosHeader;
    PYORILIB_PE_HEADERS PeHeaders;
    DWORD PageSize;
    DWORD OldProtect;

    DosHeader = (PIMAGE_DOS_HEADER)GetModuleHandle(NULL);
    PageSize = YoriLibGetPageSize();

    //
    //  Check that the executable headers fit on one page.  This should
    //  always happen, and this routine only manipulates one page of
    //  permissions.
    //

    if ((((DWORD_PTR)DosHeader) & ((DWORD_PTR)PageSize - 1)) != 0) {
        return FALSE;
    }

    //
    //  Check the executable looks like a DOS executable.
    //

    if (DosHeader->e_magic != IMAGE_DOS_SIGNATURE ||
        DosHeader->e_lfanew == 0 ||
        DosHeader->e_lfanew + sizeof(YORILIB_PE_HEADERS) > PageSize) {

        return FALSE;
    }

    PeHeaders = YoriLibAddToPointer(DosHeader, DosHeader->e_lfanew);

    //
    //  Check that the executable looks like an NT executable.
    //

    if (PeHeaders->Signature != IMAGE_NT_SIGNATURE ||
        PeHeaders->ImageHeader.SizeOfOptionalHeader <= FIELD_OFFSET(IMAGE_OPTIONAL_HEADER, Subsystem) + sizeof(WORD)) {

        return FALSE;
    }

    //
    //  If the executable already has a high enough version, we're done.
    //

    if (PeHeaders->OptionalHeader.MajorSubsystemVersion > NewMajor) {
        return TRUE;
    }

    if (PeHeaders->OptionalHeader.MajorSubsystemVersion == NewMajor &&
        PeHeaders->OptionalHeader.MinorSubsystemVersion >= NewMinor) {

        return TRUE;
    }

    //
    //  Give ourselves write access to the PE header, change it, and restore
    //  permissions.
    //

    if (!VirtualProtect(DosHeader, PageSize, PAGE_READWRITE, &OldProtect)) {
        return FALSE;
    }

    PeHeaders->OptionalHeader.MajorSubsystemVersion = NewMajor;
    PeHeaders->OptionalHeader.MinorSubsystemVersion = NewMinor;

    VirtualProtect(DosHeader, PageSize, OldProtect, &OldProtect);

    return TRUE;
}

// vim:sw=4:ts=4:et:
