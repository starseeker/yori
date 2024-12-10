/**
 * @file sh/yoriproc.h
 *
 * Yori shell function declaration header file
 *
 * Copyright (c) 2017-2019 Malcolm J. Smith
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

// *** ALIAS.C ***

__success(return)
BOOL
YoriShAddAlias(
    __in PYORI_STRING Alias,
    __in PYORI_STRING Value,
    __in BOOL Internal
    );

__success(return)
BOOL
YoriShAddAliasLiteral(
    __in LPTSTR Alias,
    __in LPTSTR Value,
    __in BOOL Internal
    );

__success(return)
BOOL
YoriShDeleteAlias(
    __in PYORI_STRING Alias
    );

__success(return)
BOOL
YoriShExpandAlias(
    __inout PYORI_LIBSH_CMD_CONTEXT CmdContext
    );

__success(return)
BOOL
YoriShExpandAliasFromString(
    __in PYORI_STRING CommandString,
    __out PYORI_STRING ExpandedString
    );

VOID
YoriShClearAllAliases(VOID);

/**
 Include user defined aliases in the output of YoriShGetAliasStrings .
 */
#define YORI_SH_GET_ALIAS_STRINGS_INCLUDE_USER     (0x00000001)

/**
 Include system defined aliases in the output of YoriShGetAliasStrings .
 */
#define YORI_SH_GET_ALIAS_STRINGS_INCLUDE_INTERNAL (0x00000002)

__success(return)
BOOL
YoriShGetAliasStrings(
    __in DWORD IncludeFlags,
    __inout PYORI_STRING AliasStrings
    );

__success(return)
BOOL
YoriShLoadSystemAliases(
    __in BOOL ImportFromCmd
    );

__success(return)
BOOL
YoriShMergeChangedAliasStrings(
    __in BOOL MergeFromCmd,
    __in PYORI_STRING OldStrings,
    __in PYORI_STRING NewStrings
    );

__success(return)
BOOL
YoriShGetSystemAliasStrings(
    __in BOOL LoadFromCmd,
    __out PYORI_STRING AliasBuffer
    );

// *** BUILTIN.C ***

extern CONST YORI_SH_BUILTIN_NAME_MAPPING YoriShBuiltins[];

DWORD
YoriShBuckPass (
    __in PYORI_LIBSH_SINGLE_EXEC_CONTEXT ExecContext,
    __in YORI_ALLOC_SIZE_T ExtraArgCount,
    ...
    );

DWORD
YoriShBuckPassToCmd (
    __in PYORI_LIBSH_SINGLE_EXEC_CONTEXT ExecContext
    );

__success(return)
BOOL
YoriShExecuteNamedModuleInProc(
    __in LPTSTR ModuleFileName,
    __in PYORI_LIBSH_SINGLE_EXEC_CONTEXT ExecContext,
    __out PDWORD ExitCode
    );

DWORD
YoriShBuiltIn (
    __in PYORI_LIBSH_SINGLE_EXEC_CONTEXT ExecContext
    );

__success(return)
BOOL
YoriShExecuteBuiltinString(
    __in PYORI_STRING Expression
    );

// *** COMPLETE.C ***

VOID
YoriShClearTabCompletionMatches(
    __inout PYORI_SH_INPUT_BUFFER Buffer
    );

/**
 If this flag is set, completion should match the full path rather than a
 relative file name.
 */
#define YORI_SH_TAB_COMPLETE_FULL_PATH      (0x00000001)

/**
 If this flag is set, completion should match command history rather than
 files or arguments.
 */
#define YORI_SH_TAB_COMPLETE_HISTORY        (0x00000002)

/**
 If this flag is set, completion should navigate backwards through the list
 of matches.
 */
#define YORI_SH_TAB_COMPLETE_BACKWARDS      (0x00000004)

/**
 If this flag is set, completion is for suggestions and should not populate
 with entries except containing a complete prefix match (ie., the suggestion
 is at the end.)
 */
#define YORI_SH_TAB_SUGGESTIONS             (0x00000008)

BOOLEAN
YoriShTabCompletion(
    __inout PYORI_SH_INPUT_BUFFER Buffer,
    __in WORD TabFlags
    );

VOID
YoriShPrependParentDirectory(
    __inout PYORI_SH_INPUT_BUFFER Buffer
    );

VOID
YoriShTrimSuggestionList(
    __inout PYORI_SH_INPUT_BUFFER Buffer,
    __in PYORI_STRING NewString
    );

VOID
YoriShCompleteSuggestion(
    __inout PYORI_SH_INPUT_BUFFER Buffer
    );

// *** ENV.C ***

BOOLEAN
YoriShIsEnvironmentVariableChar(
    __in TCHAR Char
    );

__success(return != 0)
YORI_ALLOC_SIZE_T
YoriShGetEnvironmentVariableWithoutSubstitution(
    __in LPCTSTR Name,
    __out_opt _When_(Size > 0, __out) LPTSTR Variable,
    __in YORI_ALLOC_SIZE_T Size,
    __out_opt PDWORD Generation
    );

__success(return)
BOOLEAN
YoriShGetEnvironmentVariable(
    __in LPCTSTR Name,
    __out_ecount_opt(Size) LPTSTR Variable,
    __in YORI_ALLOC_SIZE_T Size,
    __out PYORI_ALLOC_SIZE_T ReturnedSize,
    __out_opt PDWORD Generation
    );

__success(return)
BOOLEAN
YoriShAllocateAndGetEnvironmentVariable(
    __in LPCTSTR Name,
    __out PYORI_STRING Value,
    __out_opt PDWORD Generation
    );

__success(return)
BOOLEAN
YoriShGetEnvironmentVariableYS(
    __in PYORI_STRING VariableName,
    __out PYORI_STRING Value
    );

__success(return)
BOOLEAN
YoriShExpandEnvironmentVariables(
    __in PYORI_STRING Expression,
    __out PYORI_STRING ResultingExpression,
    __inout_opt PYORI_ALLOC_SIZE_T CurrentOffset
    );

__success(return)
BOOLEAN
YoriShSetEnvironmentVariable(
    __in PYORI_STRING VariableName,
    __in_opt PYORI_STRING Value
    );

__success(return)
BOOLEAN
YoriShSetEnvironmentStrings(
    __in PYORI_STRING NewEnv
    );

// *** EXEC.C ***

DWORD
YoriShExecuteSingleProgram(
    __in PYORI_LIBSH_SINGLE_EXEC_CONTEXT ExecContext
    );

__success(return)
BOOL
YoriShExecuteExpressionAndCaptureOutput(
    __in PYORI_STRING Expression,
    __out PYORI_STRING ProcessOutput
    );

__success(return)
BOOL
YoriShExpandBackquotes(
    __in PYORI_STRING Expression,
    __out PYORI_STRING ResultingExpression
    );

__success(return)
BOOL
YoriShExecuteExpression(
    __in PYORI_STRING Expression
    );

// *** HISTORY.C ***

__success(return)
BOOL
YoriShAddToHistory(
    __in PYORI_STRING NewCmd,
    __in BOOLEAN IgnoreIfRepeat
    );

__success(return)
BOOL
YoriShAddToHistoryAndReallocate(
    __in PYORI_STRING NewCmd
    );

VOID
YoriShRemoveOneHistoryEntry(
    __in PYORI_SH_HISTORY_ENTRY HistoryEntry
    );

VOID
YoriShClearAllHistory(VOID);

__success(return)
BOOL
YoriShInitHistory(VOID);

__success(return)
BOOL
YoriShLoadHistoryFromFile(VOID);

__success(return)
BOOL
YoriShSaveHistoryToFile(VOID);

__success(return)
BOOL
YoriShGetHistoryStrings(
    __in DWORD MaximumNumber,
    __inout PYORI_STRING HistoryStrings
    );

// *** INPUT.C ***

__success(return)
BOOLEAN
YoriShEnsureStringHasEnoughCharacters(
    __inout PYORI_STRING String,
    __in YORI_ALLOC_SIZE_T CharactersNeeded
    );

__success(return)
BOOLEAN
YoriShReplaceInputBufferTrackDirtyRange(
    __in PYORI_SH_INPUT_BUFFER Buffer,
    __in PYORI_STRING NewString
    );

__success(return)
BOOL
YoriShGetExpression(
    __out PYORI_STRING Expression
    );

VOID
YoriShCleanupInputContext(VOID);

// *** JOB.C ***

__success(return)
BOOL
YoriShCreateNewJob(
    __in PYORI_LIBSH_SINGLE_EXEC_CONTEXT ExecContext
    );

__success(return)
BOOL
YoriShScanJobsReportCompletion(
    __in BOOL TeardownAll
    );

DWORD
YoriShGetNextJobId(
    __in DWORD PreviousJobId
    );

__success(return)
BOOL
YoriShJobSetPriority(
    __in DWORD JobId,
    __in DWORD PriorityClass
    );

__success(return)
BOOL
YoriShTerminateJob(
    __in DWORD JobId
    );

VOID
YoriShJobWait(
    __in DWORD JobId
    );

__success(return)
BOOL
YoriShGetJobOutput(
    __in DWORD JobId,
    __inout PYORI_STRING Output,
    __inout PYORI_STRING Errors
    );

__success(return)
BOOL
YoriShPipeJobOutput(
    __in DWORD JobId,
    __in_opt HANDLE hPipeOutput,
    __in_opt HANDLE hPipeErrors
    );

__success(return)
BOOL
YoriShGetJobInformation(
    __in DWORD JobId,
    __out PBOOL HasCompleted,
    __out PBOOL HasOutput,
    __out PDWORD ExitCode,
    __inout PYORI_STRING Command
    );

// *** MAIN.C ***

VOID
YoriShPreCommand(
    __in BOOLEAN EnableVt
    );

// *** PARSE.C ***

BOOLEAN
YoriShExpandEnvironmentInCmdContext(
    __inout PYORI_LIBSH_CMD_CONTEXT CmdContext
    );

__success(return)
BOOLEAN
YoriShResolveCommandToExecutable(
    __in PYORI_LIBSH_CMD_CONTEXT CmdContext,
    __out PBOOLEAN ExecutableFound
    );

// *** PROMPT.C ***
BOOL
YoriShDisplayPrompt(VOID);

BOOL
YoriShExecPreCommandString(VOID);

// *** RESTART.C ***

BOOL
YoriShSaveRestartState(VOID);

VOID
YoriShCleanupRestartSaveThreadIfCompleted(VOID);

BOOL
YoriShLoadSavedRestartState(
    __in PYORI_STRING ProcessId
    );

VOID
YoriShDiscardSavedRestartState(
    __in_opt PYORI_STRING ProcessId
    );

// *** WAIT.C ***

VOID
YoriShInitializeWaitContext(
    __out PYORI_SH_WAIT_INPUT_CONTEXT WaitContext,
    __in HANDLE WaitHandle
    );

VOID
YoriShCleanupWaitContext(
    __in PYORI_SH_WAIT_INPUT_CONTEXT WaitContext
    );

YORI_SH_WAIT_OUTCOME
YoriShWaitForProcessOrInput(
    __inout PYORI_SH_WAIT_INPUT_CONTEXT WaitContext
    );

VOID
YoriShWaitForProcessToTerminate(
    __in PYORI_LIBSH_SINGLE_EXEC_CONTEXT ExecContext
    );

// *** WINDOW.C ***

/**
 Indicates the task has succeeded and the taskbar button should be green.
 */
#define YORI_SH_TASK_SUCCESS     0x01

/**
 Indicates the task has failed and the taskbar button should be red.
 */
#define YORI_SH_TASK_FAILED      0x02

/**
 Indicates the task is in progress and the taskbar button should be yellow.
 */
#define YORI_SH_TASK_IN_PROGRESS 0x03

/**
 Indicates the task is complete and the taskbar button should be normal.
 */
#define YORI_SH_TASK_COMPLETE    0x04

BOOL
YoriShSetWindowState(
    __in DWORD State
    );

BOOLEAN
YoriShCloseWindow(VOID);

// *** YORIFULL.C / YORISTD.C / YORINONE.C ***

BOOL
YoriShRegisterDefaultAliases(VOID);

// vim:sw=4:ts=4:et:
