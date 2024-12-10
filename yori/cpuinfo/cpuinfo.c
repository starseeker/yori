/**
 * @file cpuinfo/cpuinfo.c
 *
 * Yori shell display cpu topology information
 *
 * Copyright (c) 2019-2023 Malcolm J. Smith
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
#include <winperf.h>

/**
 Help text to display to the user.
 */
const
CHAR strCpuInfoHelpText[] =
        "\n"
        "Display cpu topology information.\n"
        "\n"
        "CPUINFO [-license] [-a] [-c] [-g] [-n] [-s] [-w ms] [<fmt>]\n"
        "\n"
        "   -a             Display all information\n"
        "   -c             Display information about processor cores\n"
        "   -g             Display information about processor groups\n"
        "   -n             Display information about NUMA nodes\n"
        "   -s             Display information about processor sockets\n"
        "   -w ms          Wait time to measure CPU utilization\n"
        "\n"
        "Format specifiers are:\n"
        "   $CORECOUNT$            The number of processor cores\n"
        "   $EFFICIENCYCORECOUNT$  The number of low power processor cores\n"
        "   $GROUPCOUNT$           The number of processor groups\n"
        "   $LOGICALCOUNT$         The number of logical processors\n"
        "   $NUMANODECOUNT$        The number of NUMA nodes\n"
        "   $PERFORMANCECORECOUNT$ The number of high performance processor cores\n"
        "   $UTILIZATION$          The current CPU utilization\n";

/**
 Display usage text to the user.
 */
BOOL
CpuInfoHelp(VOID)
{
    YoriLibOutput(YORI_LIB_OUTPUT_STDOUT, _T("CpuInfo %i.%02i\n"), YORI_VER_MAJOR, YORI_VER_MINOR);
#if YORI_BUILD_ID
    YoriLibOutput(YORI_LIB_OUTPUT_STDOUT, _T("  Build %i\n"), YORI_BUILD_ID);
#endif
    YoriLibOutput(YORI_LIB_OUTPUT_STDOUT, _T("%hs"), strCpuInfoHelpText);
    return TRUE;
}

/**
 Output a 64 bit integer.

 @param LargeInt A large integer to output.

 @param NumberBase Specifies the numeric base to use.  Should be 10 for
        decimal or 16 for hex.

 @param OutputString Pointer to a string to populate with the contents of
        the variable.

 @return The number of characters populated into the variable, or the number
         of characters required to successfully populate the contents into
         the variable.
 */
YORI_ALLOC_SIZE_T
CpuInfoOutputLargeInteger(
    __in LARGE_INTEGER LargeInt,
    __in WORD NumberBase,
    __inout PYORI_STRING OutputString
    )
{
    YORI_STRING String;
    TCHAR StringBuffer[32];
    YORI_MAX_SIGNED_T QuadPart;

    YoriLibInitEmptyString(&String);
    String.StartOfString = StringBuffer;
    String.LengthAllocated = sizeof(StringBuffer)/sizeof(StringBuffer[0]);

    QuadPart = (YORI_MAX_SIGNED_T)LargeInt.QuadPart;
    YoriLibNumberToString(&String, QuadPart, NumberBase, 0, ' ');

    if (OutputString->LengthAllocated >= String.LengthInChars) {
        memcpy(OutputString->StartOfString, String.StartOfString, String.LengthInChars * sizeof(TCHAR));
    }

    return String.LengthInChars;
}

/**
 Context about cpuinfo state that is passed between query and string
 expansion.
 */
typedef struct _CPUINFO_CONTEXT {

    /**
     The number of bytes in the ProcInfo allocation.
     */
    YORI_ALLOC_SIZE_T BytesInBuffer;

    /**
     The time to wait when measuring CPU utilization.
     */
    DWORD WaitTime;

    /**
     The CPU utilization in hundredths of a percent.
     */
    DWORD Utilization;

    /**
     TRUE if the CPU utilization has already been loaded.
     */
    BOOLEAN UtilizationLoaded;

    /**
     TRUE if the CPU topological information has already been loaded.
     */
    BOOLEAN TopologyLoaded;

    /**
     An array of entries describing information about the current system.
     */
    PYORI_SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX ProcInfo;

    /**
     The total number of processor cores discovered in the above information.
     */
    LARGE_INTEGER CoreCount;

    /**
     The total number of low power processor cores discovered in the above
     information.
     */
    LARGE_INTEGER EfficiencyCoreCount;

    /**
     The total number of high performance processor cores discovered in the
     above information.
     */
    LARGE_INTEGER PerformanceCoreCount;

    /**
     The total number of logical processors discovered in the above
     information.
     */
    LARGE_INTEGER LogicalProcessorCount;

    /**
     The total number of NUMA nodes discovered in the above information.
     */
    LARGE_INTEGER NumaNodeCount;

    /**
     The total number of processor groups discovered in the above information.
     */
    LARGE_INTEGER GroupCount;

} CPUINFO_CONTEXT, *PCPUINFO_CONTEXT;

/**
 Scan through the performance counter information looking for counter "6"
 which is CPU utilization, on instance "_Total" for all processors.  If
 _Total is not available, just take the first processor.

 @param PerfData Pointer to the performance counter information.

 @param IdleTime On successful completion, updated to contain the amount of
        time the processor has been idle since system start.

 @return TRUE to indicate success, FALSE to indicate that the coutner was not
         found.
 */
__success(return)
BOOLEAN
CpuInfoCaptureProcessorIdleTime(
    __in PPERF_DATA_BLOCK PerfData,
    __out PDWORDLONG IdleTime
    )
{
    PPERF_OBJECT_TYPE PerfObject;
    PPERF_INSTANCE_DEFINITION PerfInstance;
    PPERF_COUNTER_DEFINITION PerfCounter;
    PPERF_COUNTER_BLOCK PerfBlock;
    DWORD PerfDataObjectIndex;
    DWORD TargetInstance;
    DWORD CounterIndex;
    YORI_STRING InstanceString;

    YoriLibInitEmptyString(&InstanceString);

    PerfObject = YoriLibAddToPointer(PerfData, PerfData->HeaderLength);
    for (PerfDataObjectIndex = 0; PerfDataObjectIndex < PerfData->NumObjectTypes; PerfDataObjectIndex++) {

        PerfCounter = YoriLibAddToPointer(PerfObject, PerfObject->HeaderLength);

        //
        //  The CPU counter has instances.
        //

        if (PerfObject->NumInstances > 0) {

            PerfInstance = YoriLibAddToPointer(PerfObject, PerfObject->DefinitionLength);
            PerfBlock = YoriLibAddToPointer(PerfInstance, PerfInstance->ByteLength);

            //
            //  Look for an instance called "_Total"
            //

            for (TargetInstance = 0; TargetInstance < (DWORD)PerfObject->NumInstances; TargetInstance++) {

                InstanceString.StartOfString = YoriLibAddToPointer(PerfInstance, PerfInstance->NameOffset);
                InstanceString.LengthInChars = (YORI_ALLOC_SIZE_T)PerfInstance->NameLength / sizeof(TCHAR);
                if (InstanceString.LengthInChars > 0) {
                    InstanceString.LengthInChars--;
                }

                if (YoriLibCompareStringWithLiteralInsensitive(&InstanceString, _T("_Total")) == 0) {
                    break;
                }

                PerfBlock = YoriLibAddToPointer(PerfInstance, PerfInstance->ByteLength);
                PerfInstance = YoriLibAddToPointer(PerfBlock, PerfBlock->ByteLength);
            }

            //
            //  Failing that, older versions of the OS don't include _Total if
            //  there's only one processor, so just take the first
            //

            if (TargetInstance == (DWORD)PerfObject->NumInstances) {
                TargetInstance = 0;
                PerfInstance = YoriLibAddToPointer(PerfObject, PerfObject->DefinitionLength);
            }

            //
            //  Within that instance, look for a counter with a value of 6,
            //  being processor utilization
            //

            PerfCounter = YoriLibAddToPointer(PerfObject, PerfObject->HeaderLength);
            for (CounterIndex = 0; CounterIndex < PerfObject->NumCounters; CounterIndex++) {
                if (PerfCounter->CounterNameTitleIndex == 6 &&
                    PerfCounter->CounterSize == sizeof(DWORDLONG)) {
                    PDWORDLONG Value;

                    Value = YoriLibAddToPointer(PerfBlock, PerfCounter->CounterOffset);
                    *IdleTime = *Value;
                    return TRUE;
                }
                PerfCounter = YoriLibAddToPointer(PerfCounter, PerfCounter->ByteLength);
            }
        }
        PerfObject = YoriLibAddToPointer(PerfObject, PerfObject->TotalByteLength);
    }

    return FALSE;
}

/**
 Determine the amount of processor utilization.  Note this requires sleeping
 for a period of time, so it is deferred until it is required.

 @param CpuInfoContext Pointer to the CpuInfo context to populate with
        processor utilization information.

 @return TRUE to indicate success, FALSE to indicate failure.
 */
BOOLEAN
CpuInfoLoadProcessorUtilization(
    __inout PCPUINFO_CONTEXT CpuInfoContext
    )
{
    DWORD BufferSize;
    DWORD BufferPopulated;
    PPERF_DATA_BLOCK PerfData;
    DWORD TimeIndex;
    DWORD Err;
    LARGE_INTEGER TimeValue[2];
    DWORDLONG IdleValue[2];

    UNREFERENCED_PARAMETER(CpuInfoContext);

    YoriLibLoadAdvApi32Functions();

    if (DllAdvApi32.pRegQueryValueExW == NULL) {
        return FALSE;
    }

    BufferSize = 64 * 1024;
    if (!YoriLibIsSizeAllocatable(BufferSize)) {
        BufferSize = 32 * 1024;
    }
    PerfData = YoriLibMalloc((YORI_ALLOC_SIZE_T)BufferSize);
    if (PerfData == NULL) {
        return FALSE;
    }

    for (TimeIndex = 0; TimeIndex < 2; TimeIndex++) {
        Err = ERROR_SUCCESS;

        //
        //  Query the registry, resizing the buffer if it's too small.
        //

        while (TRUE) {
            BufferPopulated = BufferSize;
            Err = DllAdvApi32.pRegQueryValueExW(HKEY_PERFORMANCE_DATA, _T("238"), NULL, NULL, (LPBYTE)PerfData, &BufferPopulated);
            if (Err != ERROR_MORE_DATA) {
                break;
            }

            YoriLibFree(PerfData);
            if (BufferSize <= 16 * 1024 * 1024) {
                if (YoriLibIsSizeAllocatable(BufferSize * 4)) {
                    BufferSize = BufferSize * 4;
                } else if (BufferSize < YORI_MAX_ALLOC_SIZE) {
                    BufferSize = YORI_MAX_ALLOC_SIZE;
                } else {
                    return FALSE;
                }
            }

            PerfData = YoriLibMalloc((YORI_ALLOC_SIZE_T)BufferSize);
            if (PerfData == NULL) {
                return FALSE;
            }
        }

        if (Err != ERROR_SUCCESS) {
            YoriLibOutput(YORI_LIB_OUTPUT_STDERR, _T("RegQueryValueExW failed %i\n"), Err);
            YoriLibFree(PerfData);
            return FALSE;
        }

        TimeValue[TimeIndex].QuadPart = PerfData->PerfTime100nSec.QuadPart;
        if (!CpuInfoCaptureProcessorIdleTime(PerfData, &IdleValue[TimeIndex])) {
            YoriLibOutput(YORI_LIB_OUTPUT_STDERR, _T("Cannot find idle time in performance data\n"));
            YoriLibFree(PerfData);
            return FALSE;
        }

        Sleep(CpuInfoContext->WaitTime);
    }
    YoriLibFree(PerfData);

    {
        DWORDLONG TimeDelta;
        DWORDLONG IdleDelta;
        DWORD BusyPercent;
        DWORD IdlePercent;

        TimeDelta = TimeValue[1].QuadPart - TimeValue[0].QuadPart;
        IdleDelta = IdleValue[1] - IdleValue[0];

        if (IdleDelta > TimeDelta) {
            IdleDelta = TimeDelta;
        }
        IdlePercent = (DWORD)(IdleDelta * 10000 / TimeDelta);
        BusyPercent = 10000 - IdlePercent;

        CpuInfoContext->Utilization = BusyPercent;
        CpuInfoContext->UtilizationLoaded = TRUE;
    }

    return TRUE;
}


/**
 A callback function to expand any known variables found when parsing the
 format string.

 @param OutputBuffer A pointer to the output buffer to populate with data
        if a known variable is found.

 @param VariableName The variable name to expand.

 @param Context Pointer to a structure containing the data to populate.
 
 @return The number of characters successfully populated, or the number of
         characters required in order to successfully populate, or zero
         on error.
 */
YORI_ALLOC_SIZE_T
CpuInfoExpandVariables(
    __inout PYORI_STRING OutputBuffer,
    __in PYORI_STRING VariableName,
    __in PVOID Context
    )
{
    YORI_ALLOC_SIZE_T CharsNeeded;
    PCPUINFO_CONTEXT CpuInfoContext = (PCPUINFO_CONTEXT)Context;

    if (YoriLibCompareStringWithLiteral(VariableName, _T("CORECOUNT")) == 0) {
        return CpuInfoOutputLargeInteger(CpuInfoContext->CoreCount, 10, OutputBuffer);
    } else if (YoriLibCompareStringWithLiteral(VariableName, _T("EFFICIENCYCORECOUNT")) == 0) {
        return CpuInfoOutputLargeInteger(CpuInfoContext->EfficiencyCoreCount, 10, OutputBuffer);
    } else if (YoriLibCompareStringWithLiteral(VariableName, _T("GROUPCOUNT")) == 0) {
        return CpuInfoOutputLargeInteger(CpuInfoContext->GroupCount, 10, OutputBuffer);
    } else if (YoriLibCompareStringWithLiteral(VariableName, _T("LOGICALCOUNT")) == 0) {
        return CpuInfoOutputLargeInteger(CpuInfoContext->LogicalProcessorCount, 10, OutputBuffer);
    } else if (YoriLibCompareStringWithLiteral(VariableName, _T("NUMANODECOUNT")) == 0) {
        return CpuInfoOutputLargeInteger(CpuInfoContext->NumaNodeCount, 10, OutputBuffer);
    } else if (YoriLibCompareStringWithLiteral(VariableName, _T("PERFORMANCECORECOUNT")) == 0) {
        return CpuInfoOutputLargeInteger(CpuInfoContext->PerformanceCoreCount, 10, OutputBuffer);
    } else if (YoriLibCompareStringWithLiteral(VariableName, _T("UTILIZATION")) == 0) {
        if (!CpuInfoContext->UtilizationLoaded) {
            CpuInfoLoadProcessorUtilization(CpuInfoContext);
        }
        CharsNeeded = 7;
        if (OutputBuffer->LengthAllocated > CharsNeeded) {
            CharsNeeded = YoriLibSPrintf(OutputBuffer->StartOfString, _T("%i.%02i%%"), CpuInfoContext->Utilization / 100, CpuInfoContext->Utilization % 100);
        }
    } else {
        return 0;
    }

    return CharsNeeded;
}

//
//  The CPUINFO_CONTEXT structure records a pointer to an array and the size
//  of the array in bytes.  Unfortunately I can't see a way to describe this
//  relationship for older analyzers, so they believe accessing array elements
//  is walking off the end of the buffer.  Because this seems specific to
//  older versions, the suppression is limited to those also.
//

#if defined(_MSC_VER) && (_MSC_VER >= 1500) && (_MSC_VER <= 1600)
#pragma warning(push)
#pragma warning(disable: 6385)
#endif

/**
 Parse the array of information about processor topologies and count the
 number of elements in each.

 @param CpuInfoContext Pointer to the context which describes the processor
        information verbosely and should have summary counts populated.

 @return TRUE to indicate success, FALSE to indicate failure.
 */
BOOL
CpuInfoCountSummaries(
    __inout PCPUINFO_CONTEXT CpuInfoContext
    )
{
    PYORI_SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX Entry;
    DWORD CurrentOffset = 0;
    DWORD GroupIndex;
    DWORD LogicalProcessorIndex;
    DWORD_PTR LogicalProcessorMask;
    PYORI_PROCESSOR_GROUP_AFFINITY Group;

    Entry = CpuInfoContext->ProcInfo;

    while (Entry != NULL) {

        if (Entry->Relationship == YoriProcessorRelationProcessorCore) {
            CpuInfoContext->CoreCount.QuadPart++;
            for (GroupIndex = 0; GroupIndex < Entry->u.Processor.GroupCount; GroupIndex++) {
                Group = &Entry->u.Processor.GroupMask[GroupIndex];
                for (LogicalProcessorIndex = 0; LogicalProcessorIndex < 8 * sizeof(DWORD_PTR); LogicalProcessorIndex++) {
                    LogicalProcessorMask = 1;
                    LogicalProcessorMask = LogicalProcessorMask<<LogicalProcessorIndex;
                    if (Group->Mask & LogicalProcessorMask) {
                        CpuInfoContext->LogicalProcessorCount.QuadPart++;
                    }
                }
            }
            if (Entry->u.Processor.EfficiencyClass == 0) {
                CpuInfoContext->EfficiencyCoreCount.QuadPart++;
            } else {
                CpuInfoContext->PerformanceCoreCount.QuadPart++;
            }
        } else if (Entry->Relationship == YoriProcessorRelationNumaNode) {
            CpuInfoContext->NumaNodeCount.QuadPart++;
        } else if (Entry->Relationship == YoriProcessorRelationGroup) {
            CpuInfoContext->GroupCount.QuadPart = Entry->u.Group.ActiveGroupCount;
        }

        CurrentOffset += Entry->SizeInBytes;
        if (CurrentOffset >= CpuInfoContext->BytesInBuffer) {
            break;
        }
        Entry = YoriLibAddToPointer(CpuInfoContext->ProcInfo, CurrentOffset);
    }

    //
    //  On homogenous systems, all cores will report efficiency class zero,
    //  which is the most efficient class.  For human compatibility,
    //  report these as performance cores instead.
    //

    if (CpuInfoContext->PerformanceCoreCount.QuadPart == 0) {
        CpuInfoContext->PerformanceCoreCount.QuadPart = CpuInfoContext->EfficiencyCoreCount.QuadPart;
        CpuInfoContext->EfficiencyCoreCount.QuadPart = 0;
    }

    return TRUE;
}

/**
 Display a list of logical processor numbers.

 @param GroupIndex The group that contains these logical processors.

 @param Processors A bitmask of processors.

 @return TRUE to indicate success, FALSE to indicate failure.
 */
BOOL
CpuInfoDisplayProcessorMask(
    __in WORD GroupIndex,
    __in DWORD_PTR Processors
    )
{
    DWORD LogicalProcessorIndex;
    DWORD_PTR LogicalProcessorMask;

    YoriLibOutput(YORI_LIB_OUTPUT_STDOUT, _T("  Processors: "));

    for (LogicalProcessorIndex = 0; LogicalProcessorIndex < 8 * sizeof(DWORD_PTR); LogicalProcessorIndex++) {
        LogicalProcessorMask = 1;
        LogicalProcessorMask = LogicalProcessorMask<<LogicalProcessorIndex;
        if (Processors & LogicalProcessorMask) {
            YoriLibOutput(YORI_LIB_OUTPUT_STDOUT, _T("%i "), GroupIndex * 64 + LogicalProcessorIndex);
        }
    }
    YoriLibOutput(YORI_LIB_OUTPUT_STDOUT, _T("\n"));

    return TRUE;
}

/**
 Display processors within each processor core.

 @param CpuInfoContext Pointer to the context containing processor layout.

 @return TRUE to indicate success, FALSE to indicate failure.
 */
BOOL
CpuInfoDisplayCores(
    __in PCPUINFO_CONTEXT CpuInfoContext
    )
{
    PYORI_SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX Entry;
    DWORD CurrentOffset = 0;
    DWORD CoreIndex = 0;
    DWORD GroupIndex;
    BOOLEAN DisplayPerformanceLabel;

    DisplayPerformanceLabel = FALSE;

    if (CpuInfoContext->PerformanceCoreCount.QuadPart != 0 && 
        CpuInfoContext->EfficiencyCoreCount.QuadPart != 0) {

        DisplayPerformanceLabel = TRUE;
    }

    Entry = CpuInfoContext->ProcInfo;

    while (Entry != NULL) {

        if (Entry->Relationship == YoriProcessorRelationProcessorCore) {
            if (CoreIndex > 0) {
                YoriLibOutput(YORI_LIB_OUTPUT_STDOUT, _T("\n"));
            }

            if (DisplayPerformanceLabel) {
                if (Entry->u.Processor.EfficiencyClass) {
                    YoriLibOutput(YORI_LIB_OUTPUT_STDOUT, _T("Core %i (performance)\n"), CoreIndex);
                } else {
                    YoriLibOutput(YORI_LIB_OUTPUT_STDOUT, _T("Core %i (efficiency)\n"), CoreIndex);
                }
            } else {
                YoriLibOutput(YORI_LIB_OUTPUT_STDOUT, _T("Core %i\n"), CoreIndex);
            }

            CoreIndex++;
            for (GroupIndex = 0; GroupIndex < Entry->u.Processor.GroupCount; GroupIndex++) {
                CpuInfoDisplayProcessorMask(Entry->u.Processor.GroupMask[GroupIndex].Group, Entry->u.Processor.GroupMask[GroupIndex].Mask);
            }
        }

        CurrentOffset += Entry->SizeInBytes;
        if (CurrentOffset >= CpuInfoContext->BytesInBuffer) {
            break;
        }
        Entry = YoriLibAddToPointer(CpuInfoContext->ProcInfo, CurrentOffset);
    }

    return TRUE;
}

/**
 Display processors within each processor group.

 @param CpuInfoContext Pointer to the context containing processor layout.

 @return TRUE to indicate success, FALSE to indicate failure.
 */
BOOL
CpuInfoDisplayGroups(
    __in PCPUINFO_CONTEXT CpuInfoContext
    )
{
    PYORI_SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX Entry;
    DWORD CurrentOffset = 0;
    WORD GroupIndex;
    BOOLEAN DisplayedGroup = FALSE;

    Entry = CpuInfoContext->ProcInfo;

    while (Entry != NULL) {

        if (Entry->Relationship == YoriProcessorRelationGroup) {
            if (DisplayedGroup) {
                YoriLibOutput(YORI_LIB_OUTPUT_STDOUT, _T("\n"));
            }

            for (GroupIndex = 0; GroupIndex < Entry->u.Group.MaximumGroupCount; GroupIndex++) {
                YoriLibOutput(YORI_LIB_OUTPUT_STDOUT, _T("Group %i\n"), GroupIndex);
                CpuInfoDisplayProcessorMask(GroupIndex, Entry->u.Group.GroupInfo[GroupIndex].ActiveProcessorMask);
                DisplayedGroup = TRUE;
            }
        }

        CurrentOffset += Entry->SizeInBytes;
        if (CurrentOffset >= CpuInfoContext->BytesInBuffer) {
            break;
        }
        Entry = YoriLibAddToPointer(CpuInfoContext->ProcInfo, CurrentOffset);
    }

    return TRUE;
}

/**
 Display processors within each NUMA node.

 @param CpuInfoContext Pointer to the context containing processor layout.

 @return TRUE to indicate success, FALSE to indicate failure.
 */
BOOL
CpuInfoDisplayNuma(
    __in PCPUINFO_CONTEXT CpuInfoContext
    )
{
    PYORI_SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX Entry;
    DWORD CurrentOffset = 0;
    BOOLEAN DisplayedNode = FALSE;

    Entry = CpuInfoContext->ProcInfo;

    while (Entry != NULL) {

        if (Entry->Relationship == YoriProcessorRelationNumaNode) {
            if (DisplayedNode) {
                YoriLibOutput(YORI_LIB_OUTPUT_STDOUT, _T("\n"));
            }
            YoriLibOutput(YORI_LIB_OUTPUT_STDOUT, _T("Numa Node %i\n"), Entry->u.NumaNode.NodeNumber);
            CpuInfoDisplayProcessorMask(Entry->u.NumaNode.GroupMask.Group, Entry->u.NumaNode.GroupMask.Mask);
            DisplayedNode = TRUE;
        }

        CurrentOffset += Entry->SizeInBytes;
        if (CurrentOffset >= CpuInfoContext->BytesInBuffer) {
            break;
        }
        Entry = YoriLibAddToPointer(CpuInfoContext->ProcInfo, CurrentOffset);
    }

    return TRUE;
}

/**
 Display processors within each processor package.

 @param CpuInfoContext Pointer to the context containing processor layout.

 @return TRUE to indicate success, FALSE to indicate failure.
 */
BOOL
CpuInfoDisplaySockets(
    __in PCPUINFO_CONTEXT CpuInfoContext
    )
{
    PYORI_SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX Entry;
    DWORD CurrentOffset = 0;
    DWORD SocketIndex = 0;
    DWORD GroupIndex;

    Entry = CpuInfoContext->ProcInfo;

    while (Entry != NULL) {

        if (Entry->Relationship == YoriProcessorRelationProcessorPackage) {
            if (SocketIndex > 0) {
                YoriLibOutput(YORI_LIB_OUTPUT_STDOUT, _T("\n"));
            }
            YoriLibOutput(YORI_LIB_OUTPUT_STDOUT, _T("Socket %i\n"), SocketIndex);
            SocketIndex++;
            for (GroupIndex = 0; GroupIndex < Entry->u.Processor.GroupCount; GroupIndex++) {
                CpuInfoDisplayProcessorMask(Entry->u.Processor.GroupMask[GroupIndex].Group, Entry->u.Processor.GroupMask[GroupIndex].Mask);
            }
        }

        CurrentOffset += Entry->SizeInBytes;
        if (CurrentOffset >= CpuInfoContext->BytesInBuffer) {
            break;
        }
        Entry = YoriLibAddToPointer(CpuInfoContext->ProcInfo, CurrentOffset);
    }

    return TRUE;
}

#if defined(_MSC_VER) && (_MSC_VER >= 1500) && (_MSC_VER <= 1600)
#pragma warning(pop)
#endif

/**
 Load processor information from GetLogicalProcessorInformationEx.

 @param CpuInfoContext Pointer to the context to populate with processor
        information.

 @return TRUE to indicate success, FALSE to indicate failure.
 */
BOOL
CpuInfoLoadProcessorInfo(
    __inout PCPUINFO_CONTEXT CpuInfoContext
    )
{
    DWORD Err;
    DWORD BytesInBuffer;

    //
    //  Query processor information from the system.  This needs to allocate
    //  memory as needed to populate, so loop while the buffer is too small
    //  in order to allocate the correct amount.
    //

    Err = ERROR_SUCCESS;
    BytesInBuffer = 0;
    while(TRUE) {
        if (DllKernel32.pGetLogicalProcessorInformationEx(YoriProcessorRelationAll, CpuInfoContext->ProcInfo, &BytesInBuffer)) {
            CpuInfoContext->BytesInBuffer = (YORI_ALLOC_SIZE_T)BytesInBuffer;
            Err = ERROR_SUCCESS;
            break;
        }

        Err = GetLastError();
        if (Err == ERROR_INSUFFICIENT_BUFFER) {
            if (CpuInfoContext->ProcInfo != NULL) {
                YoriLibFree(CpuInfoContext->ProcInfo);
                CpuInfoContext->ProcInfo = NULL;
            }

            if (!YoriLibIsSizeAllocatable(BytesInBuffer)) {
                Err = ERROR_NOT_ENOUGH_MEMORY;
                break;
            }

            CpuInfoContext->BytesInBuffer = (YORI_ALLOC_SIZE_T)BytesInBuffer;
            CpuInfoContext->ProcInfo = YoriLibMalloc(CpuInfoContext->BytesInBuffer);
            if (CpuInfoContext->ProcInfo == NULL) {
                Err = ERROR_NOT_ENOUGH_MEMORY;
                break;
            }
        } else {
            break;
        }
    }

    //
    //  If the above query failed, indicate why.
    //

    if (Err != ERROR_SUCCESS) {
        LPTSTR ErrText;
        if (CpuInfoContext->ProcInfo != NULL) {
            YoriLibFree(CpuInfoContext->ProcInfo);
        }
        ErrText = YoriLibGetWinErrorText(Err);
        YoriLibOutput(YORI_LIB_OUTPUT_STDERR, _T("Query failed: %s"), ErrText);
        YoriLibFreeWinErrorText(ErrText);
        return FALSE;
    }

    return TRUE;
}

/**
 Load processor information from GetLogicalProcessorInformation and translate
 the result into the output that would have come from
 GetLogicalProcessorInformationEx.  This is only done on systems without
 GetLogicalProcessorInformationEx, meaning they only have a single processor
 group.

 @param CpuInfoContext Pointer to the context to populate with processor
        information.

 @return TRUE to indicate success, FALSE to indicate failure.
 */
BOOL
CpuInfoLoadAndUpconvertProcessorInfo(
    __inout PCPUINFO_CONTEXT CpuInfoContext
    )
{
    DWORD Err;
    DWORD BytesInBuffer = 0;
    DWORD BytesRequired;
    DWORD CurrentOffset;
    DWORD NewOffset;
    DWORD_PTR ProcessorsFound;
    DWORD_PTR ProcessorMask;
    PYORI_SYSTEM_LOGICAL_PROCESSOR_INFORMATION ProcInfo = NULL;
    PYORI_SYSTEM_LOGICAL_PROCESSOR_INFORMATION Entry;
    PYORI_SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX NewEntry;

    //
    //  Query processor information from the system.  This needs to allocate
    //  memory as needed to populate, so loop while the buffer is too small
    //  in order to allocate the correct amount.
    //

    Err = ERROR_SUCCESS;
    while(TRUE) {
        if (DllKernel32.pGetLogicalProcessorInformation(ProcInfo, &BytesInBuffer)) {
            Err = ERROR_SUCCESS;
            break;
        }

        Err = GetLastError();
        if (Err == ERROR_INSUFFICIENT_BUFFER) {
            if (ProcInfo != NULL) {
                YoriLibFree(ProcInfo);
                ProcInfo = NULL;
            }
            
            if (!YoriLibIsSizeAllocatable(BytesInBuffer)) {
                Err = ERROR_NOT_ENOUGH_MEMORY;
                break;
            }

            ProcInfo = YoriLibMalloc((YORI_ALLOC_SIZE_T)BytesInBuffer);
            if (ProcInfo == NULL) {
                Err = ERROR_NOT_ENOUGH_MEMORY;
                break;
            }
        } else {
            break;
        }
    }

    //
    //  If the above query failed, indicate why.
    //

    if (Err != ERROR_SUCCESS) {
        LPTSTR ErrText;
        if (ProcInfo != NULL) {
            YoriLibFree(ProcInfo);
        }
        ErrText = YoriLibGetWinErrorText(Err);
        YoriLibOutput(YORI_LIB_OUTPUT_STDERR, _T("Query failed: %s"), ErrText);
        YoriLibFreeWinErrorText(ErrText);
        return FALSE;
    }

    if (ProcInfo == NULL) {
        YoriLibOutput(YORI_LIB_OUTPUT_STDERR, _T("cpuinfo: no processors\n"));
        return FALSE;
    }

    //
    //  Count the amount of memory needed for the full structures.
    //

    BytesRequired = 0;
    CurrentOffset = 0;
    Entry = ProcInfo;

    while (Entry != NULL) {

        if (Entry->Relationship == YoriProcessorRelationProcessorCore) {
            BytesRequired += sizeof(YORI_SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX);
        } else if (Entry->Relationship == YoriProcessorRelationNumaNode) {
            BytesRequired += sizeof(YORI_SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX);
        } else if (Entry->Relationship == YoriProcessorRelationCache) {
            BytesRequired += sizeof(YORI_SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX);
        } else if (Entry->Relationship == YoriProcessorRelationProcessorPackage) {
            BytesRequired += sizeof(YORI_SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX);
        }

        CurrentOffset += sizeof(YORI_SYSTEM_LOGICAL_PROCESSOR_INFORMATION);
        if (CurrentOffset >= BytesInBuffer) {
            break;
        }
        Entry = YoriLibAddToPointer(ProcInfo, CurrentOffset);
    }

    //
    //  An extra one for the group information, which can only have one group.
    //

    BytesRequired += sizeof(YORI_SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX);

    if (!YoriLibIsSizeAllocatable(BytesRequired)) {
        YoriLibFree(ProcInfo);
        return FALSE;
    }

    CpuInfoContext->BytesInBuffer = (YORI_ALLOC_SIZE_T)BytesRequired;
    CpuInfoContext->ProcInfo = YoriLibMalloc((YORI_ALLOC_SIZE_T)BytesRequired);
    if (CpuInfoContext->ProcInfo == NULL) {
        YoriLibFree(ProcInfo);
        return FALSE;
    }

    ZeroMemory(CpuInfoContext->ProcInfo, BytesRequired);

    //
    //  Port over the existing downlevel entries to the new format.
    //

    NewOffset = 0;
    CurrentOffset = 0;
    ProcessorsFound = 0;
    Entry = ProcInfo;
    NewEntry = CpuInfoContext->ProcInfo;

    //
    //  The first entry is for processor groups, but we don't know anything
    //  about them yet.  Reserve this entry and skip it.
    //

    NewEntry->Relationship = YoriProcessorRelationGroup;
    NewEntry->SizeInBytes = sizeof(YORI_SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX);
    NewEntry->u.Group.MaximumGroupCount = 1;
    NewEntry->u.Group.ActiveGroupCount = 1;
    NewOffset += NewEntry->SizeInBytes;
    NewEntry = YoriLibAddToPointer(CpuInfoContext->ProcInfo, NewOffset);

    while (Entry != NULL) {

        if (Entry->Relationship == YoriProcessorRelationProcessorCore) {
            NewEntry->Relationship = YoriProcessorRelationProcessorCore;
            NewEntry->SizeInBytes = sizeof(YORI_SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX);
            NewEntry->u.Processor.Flags = Entry->u.ProcessorCore.Flags;
            NewEntry->u.Processor.EfficiencyClass = 0;
            NewEntry->u.Processor.GroupCount = 1;
            NewEntry->u.Processor.GroupMask[0].Mask = Entry->ProcessorMask;
            ProcessorsFound |= Entry->ProcessorMask;
        } else if (Entry->Relationship == YoriProcessorRelationNumaNode) {
            NewEntry->Relationship = YoriProcessorRelationNumaNode;
            NewEntry->SizeInBytes = sizeof(YORI_SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX);
            NewEntry->u.NumaNode.NodeNumber = Entry->u.NumaNode.NodeNumber;
            NewEntry->u.NumaNode.GroupMask.Mask = Entry->ProcessorMask;

        } else if (Entry->Relationship == YoriProcessorRelationCache) {
            NewEntry->Relationship = YoriProcessorRelationCache;
            NewEntry->SizeInBytes = sizeof(YORI_SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX);
            memcpy(&NewEntry->u.Cache.Cache, &Entry->u.Cache, sizeof(YORI_PROCESSOR_CACHE_DESCRIPTOR));
            NewEntry->u.Cache.GroupMask.Mask = Entry->ProcessorMask;
        } else if (Entry->Relationship == YoriProcessorRelationProcessorPackage) {
            NewEntry->Relationship = YoriProcessorRelationProcessorPackage;
            NewEntry->SizeInBytes = sizeof(YORI_SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX);
            NewEntry->u.Processor.Flags = Entry->u.ProcessorCore.Flags;
            NewEntry->u.Processor.EfficiencyClass = 0;
            NewEntry->u.Processor.GroupCount = 1;
            NewEntry->u.Processor.GroupMask[0].Mask = Entry->ProcessorMask;
        }

        CurrentOffset += sizeof(YORI_SYSTEM_LOGICAL_PROCESSOR_INFORMATION);
        NewOffset += NewEntry->SizeInBytes;
        if (CurrentOffset >= BytesInBuffer) {
            break;
        }
        Entry = YoriLibAddToPointer(ProcInfo, CurrentOffset);
        NewEntry = YoriLibAddToPointer(CpuInfoContext->ProcInfo, NewOffset);
    }

    //
    //  Now populate group information.
    //

    NewEntry = CpuInfoContext->ProcInfo;
    NewEntry->u.Group.GroupInfo[0].ActiveProcessorMask = ProcessorsFound;

    ProcessorMask = 1;
    for (CurrentOffset = 0; CurrentOffset < (sizeof(DWORD_PTR) * 8); CurrentOffset++) {
        ProcessorMask = ProcessorMask<<1;
        if (ProcessorsFound & ProcessorMask) {
            NewEntry->u.Group.GroupInfo[0].MaximumProcessorCount++;
            NewEntry->u.Group.GroupInfo[0].ActiveProcessorCount++;
        }
    }

    YoriLibFree(ProcInfo);

    return TRUE;
}

#ifdef YORI_BUILTIN
/**
 The main entrypoint for the cpuinfo builtin command.
 */
#define ENTRYPOINT YoriCmd_YCPUINFO
#else
/**
 The main entrypoint for the cpuinfo standalone application.
 */
#define ENTRYPOINT ymain
#endif

/**
 The main entrypoint for the cpuinfo cmdlet.

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
    BOOLEAN DisplayCores = FALSE;
    BOOLEAN DisplayGroups = FALSE;
    BOOLEAN DisplayNuma = FALSE;
    BOOLEAN DisplaySockets = FALSE;
    BOOLEAN DisplayFormatString = TRUE;
    BOOLEAN InsertNewline = FALSE;
    BOOLEAN DisplayGraph = TRUE;
    YORI_STRING Arg;
    CPUINFO_CONTEXT CpuInfoContext;
    YORI_STRING DisplayString;
    YORI_STRING AllocatedFormatString;
    LPTSTR DefaultFormatString = _T("Core count: $CORECOUNT$\n")
                                 _T("Performance core count: $PERFORMANCECORECOUNT$\n")
                                 _T("Efficiency core count: $EFFICIENCYCORECOUNT$\n")
                                 _T("Group count: $GROUPCOUNT$\n")
                                 _T("Logical processors: $LOGICALCOUNT$\n")
                                 _T("Numa nodes: $NUMANODECOUNT$\n");

    ZeroMemory(&CpuInfoContext, sizeof(CpuInfoContext));
    CpuInfoContext.WaitTime = 300;
    for (i = 1; i < ArgC; i++) {

        ArgumentUnderstood = FALSE;
        ASSERT(YoriLibIsStringNullTerminated(&ArgV[i]));

        if (YoriLibIsCommandLineOption(&ArgV[i], &Arg)) {

            if (YoriLibCompareStringWithLiteralInsensitive(&Arg, _T("?")) == 0) {
                CpuInfoHelp();
                return EXIT_SUCCESS;
            } else if (YoriLibCompareStringWithLiteralInsensitive(&Arg, _T("license")) == 0) {
                YoriLibDisplayMitLicense(_T("2019-2023"));
                return EXIT_SUCCESS;
            } else if (YoriLibCompareStringWithLiteralInsensitive(&Arg, _T("a")) == 0) {
                DisplayCores = TRUE;
                DisplayGroups = TRUE;
                DisplayNuma = TRUE;
                DisplaySockets = TRUE;
                ArgumentUnderstood = TRUE;
            } else if (YoriLibCompareStringWithLiteralInsensitive(&Arg, _T("c")) == 0) {
                DisplayCores = TRUE;
                DisplayFormatString = FALSE;
                ArgumentUnderstood = TRUE;
            } else if (YoriLibCompareStringWithLiteralInsensitive(&Arg, _T("g")) == 0) {
                DisplayGroups = TRUE;
                DisplayFormatString = FALSE;
                ArgumentUnderstood = TRUE;
            } else if (YoriLibCompareStringWithLiteralInsensitive(&Arg, _T("n")) == 0) {
                DisplayNuma = TRUE;
                DisplayFormatString = FALSE;
                ArgumentUnderstood = TRUE;
            } else if (YoriLibCompareStringWithLiteralInsensitive(&Arg, _T("s")) == 0) {
                DisplaySockets = TRUE;
                DisplayFormatString = FALSE;
                ArgumentUnderstood = TRUE;
            } else if (YoriLibCompareStringWithLiteralInsensitive(&Arg, _T("w")) == 0 &&
                       i + 1 < ArgC) {

                YORI_MAX_SIGNED_T llTemp;
                YORI_ALLOC_SIZE_T CharsConsumed;

                llTemp = 0;
                if (YoriLibStringToNumber(&ArgV[i + 1], TRUE, &llTemp, &CharsConsumed) &&
                    CharsConsumed > 0) {

                    CpuInfoContext.WaitTime = (DWORD)llTemp;
                }
                i = i + 1;
                ArgumentUnderstood = TRUE;
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

    //
    //  If the Win7 API is not present, should fall back to the 2003 API and
    //  emulate the Win7 one.  If neither are present this app can't output
    //  topologicalinformation.
    //

    if (DllKernel32.pGetLogicalProcessorInformationEx != NULL) {
        if (!CpuInfoLoadProcessorInfo(&CpuInfoContext)) {
            return EXIT_FAILURE;
        }
        CpuInfoContext.TopologyLoaded = TRUE;
    } else if (DllKernel32.pGetLogicalProcessorInformation != NULL) {
        if (!CpuInfoLoadAndUpconvertProcessorInfo(&CpuInfoContext)) {
            return EXIT_FAILURE;
        }
        CpuInfoContext.TopologyLoaded = TRUE;
    }

    //
    //  Parse the processor information into summary counts.
    //

    if (CpuInfoContext.TopologyLoaded) {
        CpuInfoCountSummaries(&CpuInfoContext);
    } else if (DisplayCores || DisplayGroups || DisplayNuma || DisplaySockets) {
        YoriLibOutput(YORI_LIB_OUTPUT_STDERR, _T("OS support not present\n"));
        return EXIT_FAILURE;
    }

    if (DisplayCores) {
        CpuInfoDisplayCores(&CpuInfoContext);
        InsertNewline = TRUE;
    }

    if (DisplayGroups) {
        if (InsertNewline) {
            YoriLibOutput(YORI_LIB_OUTPUT_STDOUT, _T("\n"));
        }
        CpuInfoDisplayGroups(&CpuInfoContext);
        InsertNewline = TRUE;
    }

    if (DisplayNuma) {
        if (InsertNewline) {
            YoriLibOutput(YORI_LIB_OUTPUT_STDOUT, _T("\n"));
        }
        CpuInfoDisplayNuma(&CpuInfoContext);
        InsertNewline = TRUE;
    }

    if (DisplaySockets) {
        if (InsertNewline) {
            YoriLibOutput(YORI_LIB_OUTPUT_STDOUT, _T("\n"));
        }
        CpuInfoDisplaySockets(&CpuInfoContext);
        InsertNewline = TRUE;
    }

    //
    //  Obtain a format string.
    //

    YoriLibInitEmptyString(&AllocatedFormatString);
    if (StartArg > 0) {
        DisplayGraph = FALSE;
        if (!YoriLibBuildCmdlineFromArgcArgv(ArgC - StartArg, &ArgV[StartArg], TRUE, FALSE, &AllocatedFormatString)) {
            YoriLibFree(CpuInfoContext.ProcInfo);
            return EXIT_FAILURE;
        }
    } else {
        if (DisplayFormatString) {
            if (CpuInfoContext.TopologyLoaded) {
                YoriLibConstantString(&AllocatedFormatString, DefaultFormatString);
            }
        }
    }


    if (DisplayGraph) {
        if (InsertNewline) {
            YoriLibOutput(YORI_LIB_OUTPUT_STDOUT, _T("\n"));
        }

        if (!CpuInfoContext.UtilizationLoaded) {
            CpuInfoLoadProcessorUtilization(&CpuInfoContext);
        }

        if (CpuInfoContext.UtilizationLoaded) {
            YoriLibOutput(YORI_LIB_OUTPUT_STDOUT, _T("Utilization: %i.%02i%%\n"), CpuInfoContext.Utilization / 100, CpuInfoContext.Utilization % 100);
            YoriLibDisplayBarGraph(GetStdHandle(STD_OUTPUT_HANDLE), CpuInfoContext.Utilization / 10, 500, 750);
        }

        InsertNewline = FALSE;
    }

    //
    //  Output the format string with summary counts.
    //

    if (AllocatedFormatString.LengthInChars > 0) {
        if (InsertNewline) {
            YoriLibOutput(YORI_LIB_OUTPUT_STDOUT, _T("\n"));
        }
        YoriLibInitEmptyString(&DisplayString);
        YoriLibExpandCommandVariables(&AllocatedFormatString, '$', FALSE, CpuInfoExpandVariables, &CpuInfoContext, &DisplayString);
        if (DisplayString.StartOfString != NULL) {
            YoriLibOutput(YORI_LIB_OUTPUT_STDOUT, _T("%y"), &DisplayString);
            YoriLibFreeStringContents(&DisplayString);
        }
    }

    YoriLibFreeStringContents(&AllocatedFormatString);
    ASSERT(!CpuInfoContext.TopologyLoaded || CpuInfoContext.ProcInfo != NULL);
    if (CpuInfoContext.ProcInfo != NULL) {
        YoriLibFree(CpuInfoContext.ProcInfo);
    }

    return EXIT_SUCCESS;
}

// vim:sw=4:ts=4:et:
