/**
 * \file shell.c
 * \brief shell functions.
 *
 * \version iLLD_1_0_0_0_0
 * \copyright Copyright (c) 2013 Infineon Technologies AG. All rights reserved.
 *
 *
 *                                 IMPORTANT NOTICE
 *
 *
 * Infineon Technologies AG (Infineon) is supplying this file for use
 * exclusively with Infineon's microcontroller products. This file can be freely
 * distributed within development tools that are supporting such microcontroller
 * products.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS".  NO WARRANTIES, WHETHER EXPRESS, IMPLIED
 * OR STATUTORY, INCLUDING, BUT NOT LIMITED TO, IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE APPLY TO THIS SOFTWARE.
 * INFINEON SHALL NOT, IN ANY CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL,
 * OR CONSEQUENTIAL DAMAGES, FOR ANY REASON WHATSOEVER.
 *
 */

//---------------------------------------------------------------------------
#include "Ifx_Shell.h"
#include "_Utilities/Ifx_Assert.h"
#include "Cpu/Std/IfxCpu_Intrinsics.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define IFX_SHELL_LLD              "%lld "
#define IFX_SHELL_LLX              "%llx "
#define IFX_SHELL_LLU              "%llu "

//---------------------------------------------------------------------------
#define IFX_SHELL_MAX_MESSAGE_SIZE 255
//---------------------------------------------------------------------------

/* Macro to detect space character */
#define ISSPACE(c)           (((c) == ' ') || ((c) == '\t'))

/* Macro to only execute parameter if echo is enabled for this shell */
#define IFX_SHELL_IF_ECHO(X) {if (shell->control.echo) {X; }}

/* Macro to write lots of spaces */
#define IFX_SHELL_WRITE_SPACES(X) \
    {int ii; for (ii = 0; ii < (X); ii++) {IfxStdIf_DPipe_print(shell->io, " "); }}

/* Macro to write lots of backspaces */
#define IFX_SHELL_WRITE_BACKSPACES(X) \
    {int ii; for (ii = 0; ii < (X); ii++) {IfxStdIf_DPipe_print(shell->io, "\b"); }}

//---------------------------------------------------------------------------
char Ifx_Shell_cmdBuffer[IFX_SHELL_CMD_LINE_SIZE * IFX_SHELL_CMD_HISTORY_SIZE];
//---------------------------------------------------------------------------
void Ifx_Shell_execute(Ifx_Shell *shell, pchar commandLine);
void Ifx_Shell_cmdEscapeProcess(Ifx_Shell *shell, char EscapeChar1, char EscapeChar2);

//---------------------------------------------------------------------------
/**
 * \brief Check whether the args is already at the end.
 * \param args The argument null-terminated string
 */
IFX_INLINE boolean Ifx_Shell_isEndOfLine(pchar args)
{
    return ((args == NULL_PTR) || (*args == IFX_SHELL_NULL_CHAR)) ? TRUE : FALSE;
}


static boolean Ifx_Shell_writeResult(Ifx_Shell *shell, Ifx_SizeT Code)
{
    Ifx_SizeT length = sizeof(Code);
    boolean   result = IfxStdIf_DPipe_write(shell->io, &Code, &length, TIME_INFINITE);

    IFX_ASSERT(IFX_VERBOSE_LEVEL_ERROR, result != FALSE);

    return result;
}


//---------------------------------------------------------------------------
boolean Ifx_Shell_showHelpSingle(pchar prefix, const void *commandList, IfxStdIf_DPipe *io)
{
    const Ifx_Shell_Command *command = commandList;

    while (command->commandLine != NULL_PTR)
    {
        if ((prefix != NULL_PTR) && (prefix[0] != IFX_SHELL_NULL_CHAR))
        {
            IfxStdIf_DPipe_print(io, "%s ", prefix);
        }

        IfxStdIf_DPipe_print(io, command->commandLine);
        IfxStdIf_DPipe_print(io, command->help);
        IfxStdIf_DPipe_print(io, ENDL);
        command = &command[1];
    }

    return TRUE;
}


boolean Ifx_Shell_showHelp(pchar args, void *shellPtr, IfxStdIf_DPipe *io)
{
    sint32     i;
    Ifx_Shell *shell = shellPtr;

    (void)args;                 /* ignore compiler warning */

    for (i = 0; i < IFX_SHELL_COMMAND_LISTS; i++)
    {
        if (shell->commandList[i] != NULL_PTR)
        {
            Ifx_Shell_showHelpSingle("", shell->commandList[i], io);
        }
    }

    return TRUE;
}


boolean Ifx_Shell_protocolStart(pchar args, void *data, IfxStdIf_DPipe *io)
{
    Ifx_Shell *shell  = data;
    boolean    Result = TRUE;

    if (Ifx_Shell_matchToken(&args, "?") != FALSE)
    {
        IfxStdIf_DPipe_print(io, "Syntax     : protocol start" ENDL);
        IfxStdIf_DPipe_print(io, "           > start a protocol" ENDL);
    }
    else if (Ifx_Shell_matchToken(&args, "start") != FALSE)
    {
        if ((shell->protocol.start != NULL_PTR) && (shell->protocol.object != NULL_PTR))
        {
            Result                  = shell->protocol.start(shell->protocol.object, io);
            shell->protocol.started = (Result != FALSE);

            if (shell->protocol.onStart != NULL_PTR)
            {
                shell->protocol.onStart(shell->protocol.object, shell->protocol.onStartData);
            }
        }
        else
        {
            Result = FALSE;
        }
    }
    else
    {}

    return Result;
}


boolean Ifx_Shell_bbProtocolStart(pchar args, void *data, IfxStdIf_DPipe *io)
{
    boolean result = TRUE;

    if (Ifx_Shell_matchToken(&args, "?") != FALSE)
    {
        IfxStdIf_DPipe_print(io, "Syntax     : protocol start" ENDL);
        IfxStdIf_DPipe_print(io, "           > start a protocol" ENDL);
    }
    else if (Ifx_Shell_matchToken(&args, "protocol") != FALSE)
    {
        result = Ifx_Shell_protocolStart(args, data, io);
    }
    else
    {}

    return result;
}


//---------------------------------------------------------------------------
void Ifx_Shell_initConfig(Ifx_Shell_Config *config)
{
    uint32 i;

    for (i = 0; i < IFX_SHELL_COMMAND_LISTS; i++)
    {
        config->commandList[i] = NULL_PTR;
    }

    config->standardIo           = NULL_PTR;
    config->echo                 = TRUE;
    config->protocol.execute     = NULL_PTR;
    config->protocol.object      = NULL_PTR;
    config->protocol.onStart     = NULL_PTR;
    config->protocol.onStartData = NULL_PTR;
    config->protocol.start       = NULL_PTR;
    config->protocol.started     = FALSE;
    config->sendResultCode       = FALSE;
    config->showPrompt           = TRUE;
    config->standardIo           = NULL_PTR;
}


boolean Ifx_Shell_init(Ifx_Shell *shell, const Ifx_Shell_Config *config)
{
    sint32 i;
    char **CmdHistory = NULL_PTR;   /* Pointer to array of pointers for command history items */

    /* Ensure state variable is cleared */
    memset(shell, 0, sizeof(*shell));

    shell->protocol               = config->protocol;
    shell->protocol.started       = FALSE;

    shell->io                     = config->standardIo;
    shell->control.showPrompt     = config->showPrompt;
    shell->control.sendResultCode = config->sendResultCode;
    shell->control.echo           = config->echo;
    shell->control.echoError      = TRUE;
    shell->control.enabled        = TRUE;

    shell->locals.escBracketNum   = IFX_SHELL_NULL_CHAR; /* Used to cache number in sequence "ESC [ 1/2/3/4 ~" */
    shell->locals.cmdState        = IFX_SHELL_CMD_STATE_NORMAL;

    /* Copy command line buffer pointer into state variable */
    shell->cmd.cmdStr = shell->locals.cmdStr;

    /* Initialize command history space and cache pointer */
    memset(Ifx_Shell_cmdBuffer, 0, sizeof(Ifx_Shell_cmdBuffer));
    shell->cmdHistory[0] = &Ifx_Shell_cmdBuffer[0];

    for (i = 0; i < IFX_SHELL_COMMAND_LISTS; i++)
    {
        shell->commandList[i] = config->commandList[i];
    }

    /* Initialize command history pointers */
    CmdHistory = shell->cmdHistory;

    for (i = 1; i < IFX_SHELL_CMD_HISTORY_SIZE; i++)
    {
        CmdHistory[i] = &CmdHistory[i - 1][IFX_SHELL_CMD_LINE_SIZE];    /* Items are just offsets into a large allocated area */
    }

    shell->cmd.historyItem = IFX_SHELL_CMD_HISTORY_NO_ITEM;

    /* Pre-load useful commands into history buffer */
    strcpy(CmdHistory[0], "help");
    strcpy(CmdHistory[1], "protocol start");

    if (shell->control.showPrompt != 0)
    {
        IfxStdIf_DPipe_print(shell->io, ENDL);
        IfxStdIf_DPipe_print(shell->io, IFX_SHELL_PROMPT);
    }

    return TRUE;
}


void Ifx_Shell_process(Ifx_Shell *shell)
{
    Ifx_SizeT          i, j;           /* Loop variables */
    Ifx_SizeT          count;
    Ifx_SizeT          readCount;
    boolean            NormalKeyPress; /* Indicates if this is a normal keypress, i.e. not part of an escape code */

    Ifx_Shell_CmdLine *Cmd         = &shell->cmd;
    char              *inputbuffer = shell->locals.inputbuffer;
    char              *cmdStr      = shell->locals.cmdStr;
    char             **CmdHistory  = shell->cmdHistory;

    if (shell->control.enabled == 0)
    {
        return;
    }

    if ((shell->protocol.object != NULL_PTR) && (shell->protocol.started != FALSE))
    {
        shell->protocol.execute(shell->protocol.object);
    }
    else
    {
        /**** NORMAL MODE ****/

        /********************************************************************************/
        /* Read all characters until enter inclusive.                                   */
        /* If the command is bigger than IFX_SHELL_CMD_SIZE, the command is ignored.    */
        /*                                                                              */
        /* Escape sequences are handled by a state machine.                             */
        /* The following escape sequences (prefix "ESC [") are supported:               */
        /*                                                                              */
        /* A  - up      B - down       C - right     D - left                           */
        /* 1~ - HOME   2~ - INSERT    3~ - DELETE   5~ - END                            */
        /*                                                                              */
        /* Backspace ('\b') is also supported.                                          */
        /********************************************************************************/

        count     = 0;
        readCount = IFX_SHELL_CMD_LINE_SIZE - count;
        IfxStdIf_DPipe_read(shell->io, &inputbuffer[count], &readCount, TIME_NULL);
        count    += readCount;

        for (i = 0; i < count; i++)
        {
            /* By default, we assume character is part of escape sequence */
            NormalKeyPress = FALSE;

            /* Process key pressed */
            switch (inputbuffer[i])
            {
            /* New line (ENTER) */
            case '\n':
            case '\r':
                /* Print new line to terminal if requested */
                IFX_SHELL_IF_ECHO(IfxStdIf_DPipe_print(shell->io, ENDL))

                /* Execute command if length is valid - i.e. not an over-full buffer
                 * (prevents attempted execution of junk) */
                if (Cmd->length < IFX_SHELL_CMD_LINE_SIZE)
                {
                    cmdStr[Cmd->length] = IFX_SHELL_NULL_CHAR;  /* Terminate cmdStr */

                    if (Cmd->historyAdd != FALSE)
                    {
                        /* Shuffle history up */
                        for (j = IFX_SHELL_CMD_HISTORY_SIZE - 1; j > 0; j--)
                        {
                            /* Copy text */
                            strncpy(CmdHistory[j], CmdHistory[j - 1], IFX_SHELL_CMD_LINE_SIZE);
                        }

                        /* Copy in new entry */
                        strncpy(CmdHistory[0], cmdStr, IFX_SHELL_CMD_LINE_SIZE);
                    }

                    /* Execute command */
                    Ifx_Shell_execute(shell, cmdStr);
                }

                /* Show prompt if in main shell */
                if (shell->control.showPrompt != 0)
                {
                    IfxStdIf_DPipe_print(shell->io, IFX_SHELL_PROMPT);
                }

                /* Reset command line buffer length */
                Cmd->length = 0;

                /* Reset command line buffer cursor position */
                Cmd->cursor = 0;

                /* Clear flag */
                Cmd->historyAdd = FALSE;

                /* Ensure we're not in command history list */
                Cmd->historyItem = IFX_SHELL_CMD_HISTORY_NO_ITEM;
                break;

            /* Backspace (may occur in middle of text if cursor location is not at end) */
            case '\b':

                if (Cmd->cursor > 0)
                {
                    /* Update on screen */
                    if (shell->control.echo != 0)
                    {
                        /* Move left one character */
                        IfxStdIf_DPipe_print(shell->io, "\b");

                        /* Update line with new characters */
                        for (j = Cmd->cursor; j < Cmd->length; j++)
                        {
                            IfxStdIf_DPipe_print(shell->io, "%c", cmdStr[j]);
                        }

                        /* Write over duplicated character at end */
                        IfxStdIf_DPipe_print(shell->io, " ");
                        IFX_SHELL_WRITE_BACKSPACES((Cmd->length - Cmd->cursor) + 1)
                    }

                    /* Update in command line variable. Shuffle text left */
                    strncpy(&cmdStr[Cmd->cursor - 1], &cmdStr[Cmd->cursor], Cmd->length - Cmd->cursor);

                    /* Terminate string at end of shorter string */
                    cmdStr[Cmd->length - 1] = IFX_SHELL_NULL_CHAR;

                    Cmd->length--;
                    Cmd->cursor--;

                    /* Command line has been modified */
                    Cmd->historyAdd = TRUE;
                }

                break;

            /* Escape character */
            case '\x1B':       /*'\x1B': */
                shell->locals.cmdState = IFX_SHELL_CMD_STATE_ESCAPE;
                break;

            /* '[' - check to see if this is second part of an escape sequence */
            case '[':

                if (shell->locals.cmdState == IFX_SHELL_CMD_STATE_ESCAPE)
                {
                    /* ESC [ pressed */
                    shell->locals.cmdState = IFX_SHELL_CMD_STATE_ESCAPE_BRACKET;
                }
                else
                {
                    NormalKeyPress = TRUE;
                }

                break;

            /* Check for supported characters in escape sequences ( ESC [ A/B/C/D ) */
            case 'A':
            case 'B':
            case 'C':
            case 'D':

                if (shell->locals.cmdState == IFX_SHELL_CMD_STATE_ESCAPE_BRACKET)
                {
                    /* Process arrow keys */
                    Ifx_Shell_cmdEscapeProcess(shell, inputbuffer[i], 0);

                    /* End of escape sequence */
                    shell->locals.cmdState = IFX_SHELL_CMD_STATE_NORMAL;
                }
                else
                {
                    NormalKeyPress = TRUE;
                }

                break;

            /* Check for supported characters in escape sequences (ESC [ 2/4/5 ~) */
            case '1':
            case '2':
            case '3':
            case '4':

                if (shell->locals.cmdState == IFX_SHELL_CMD_STATE_ESCAPE_BRACKET)
                {
                    /* Store number for use once complete escape sequence is confirmed (below) */
                    shell->locals.escBracketNum = inputbuffer[i];
                    shell->locals.cmdState      = IFX_SHELL_CMD_STATE_ESCAPE_BRACKET_NUMBER;
                }
                else
                {
                    NormalKeyPress = TRUE;
                }

                break;

            /* Check for supported characters in escape sequences (ESC [ 2/4/5 ~) */
            case '~':

                if (shell->locals.cmdState == IFX_SHELL_CMD_STATE_ESCAPE_BRACKET_NUMBER)
                {
                    /* Process home/delete/end */
                    Ifx_Shell_cmdEscapeProcess(shell, shell->locals.escBracketNum, '~');

                    /* End of escape sequence */
                    shell->locals.cmdState = IFX_SHELL_CMD_STATE_NORMAL;
                }
                else
                {
                    NormalKeyPress = TRUE;
                }

                break;

            /* Normal character - add to command string */
            default:
                NormalKeyPress = TRUE;
                break;
            }

            IFX_ASSERT(IFX_VERBOSE_LEVEL_ERROR, Cmd->length >= Cmd->cursor);    /* Sanity check */

            /* If this was a normal key press (not part of an escape sequence),
             * add it to the command string */
            if (NormalKeyPress != FALSE)
            {
                /* Ensure state machine is reset */
                shell->locals.cmdState = IFX_SHELL_CMD_STATE_NORMAL;

                /* If not filled buffer, add in this character */
                if (Cmd->length < (IFX_SHELL_CMD_LINE_SIZE - 1))
                {
                    /* Command line has been modified */
                    Cmd->historyAdd = TRUE;

                    /* Copy into command line */
                    cmdStr[Cmd->cursor] = inputbuffer[i];
                    Cmd->cursor++;

                    /* Update length of buffer */
                    Cmd->length = __max(Cmd->length, Cmd->cursor);

                    if (shell->control.echo != 0)
                    {
                        /* echo character to shell output if requested */
                        shell->locals.echo[0] = inputbuffer[i];
                        IfxStdIf_DPipe_print(shell->io, shell->locals.echo);
                    }
                }
                else
                {
                    /* Line too long - ignore further characters */
                    Cmd->historyAdd = FALSE;    /* Invalid command line */
                }
            }
        }
    }
}


void Ifx_Shell_deinit(Ifx_Shell *shell)
{
    (void)shell;                /* ignore compiler warning; */
    // tbd free necessary memory
}


pchar Ifx_Shell_skipWhitespace(pchar args)
{
    if (args != NULL_PTR)
    {
        while ((*args != IFX_SHELL_NULL_CHAR) && (ISSPACE(*args)))
        {
            args = &args[1];
        }
    }

    return args;
}


boolean Ifx_Shell_matchToken(pchar *argsPtr, pchar token)
{
    pchar   savedArguments = *argsPtr;
    char    buffer[256];
    boolean result         = FALSE;

    if (Ifx_Shell_parseToken(argsPtr, buffer, Ifx_COUNTOF(buffer)) != FALSE)
    {
        if (strcmp(token, buffer) == 0)
        {
            result = TRUE;
        }
    }

    if (result == FALSE)
    {
        // No match: don't advance pointer
        *argsPtr = savedArguments;
    }

    return result;
}


static boolean Ifx_Shell_matchCommand(pchar *argsPtr, pchar *match)
{
    boolean result         = FALSE;
    pchar   savedArguments = *argsPtr;
    pchar   savedMatch     = *match;
    char    buffer0[256];
    char    buffer1[256];

    if ((Ifx_Shell_parseToken(argsPtr, buffer0, Ifx_COUNTOF(buffer0)) != FALSE)
        && (Ifx_Shell_parseToken(match, buffer1, Ifx_COUNTOF(buffer1)) != FALSE))
    {
        if (strcmp(buffer1, buffer0) == 0)
        {
            result = TRUE;
        }
    }

    if (result == FALSE)
    {
        // No match: don't advance pointer
        *argsPtr = savedArguments;
        *match   = savedMatch;
    }

    return result;
}


boolean Ifx_Shell_parseToken(pchar *argsPtr, char *tokenBuffer, int bufferLength)
{
    int   mindex = 0;
    pchar args   = Ifx_Shell_skipWhitespace(*argsPtr);

    tokenBuffer[0] = IFX_SHELL_NULL_CHAR;

    if (args == NULL_PTR)
    {
        return FALSE;
    }

    if (*args == '\"')
    {
        args = &args[1];

        while ((*args != IFX_SHELL_NULL_CHAR) && (*args != '\"'))
        {
            if (mindex < bufferLength)
            {
                tokenBuffer[mindex] = *args;
                mindex++;
            }

            args = &args[1];
        }

        // error if no closing quote
        if (*args != '\"')
        {
            return FALSE;
        }

        args = &args[1];
    }
    else
    {
        // don't allow unquoted empty tokens
        if (*args == IFX_SHELL_NULL_CHAR)
        {
            return FALSE;
        }

        while ((*args != IFX_SHELL_NULL_CHAR) && (!ISSPACE(*args)))
        {
            if (mindex < bufferLength)
            {
                tokenBuffer[mindex] = *args;
                mindex++;
            }

            args = &args[1];
        }
    }

    // make sure string is zero terminated
    if (bufferLength > 0)
    {
        tokenBuffer[__min(mindex, bufferLength - 1)] = IFX_SHELL_NULL_CHAR;
    }

    *argsPtr = Ifx_Shell_skipWhitespace(args);

    return TRUE;
}


boolean Ifx_Shell_parseAddress(pchar *argsPtr, void **address)
{
    char    buffer[32];
    boolean result;

    *address = 0;

    if (Ifx_Shell_parseToken(argsPtr, buffer, Ifx_COUNTOF(buffer)) == FALSE)
    {
        result = FALSE;
    }
    else
    {
        result = (buffer[0] != IFX_SHELL_NULL_CHAR) && (sscanf(buffer, "%x ", (uint32 *)address) == 1);
    }

    return result;
}


boolean Ifx_Shell_parseSInt32(pchar *argsPtr, sint32 *value)
{
    sint64  value64;
    boolean result;

    *value = 0;

    if (Ifx_Shell_parseSInt64(argsPtr, &value64) == FALSE)
    {
        result = FALSE;
    }
    else
    {
        *value = (sint32)value64;
        result = TRUE;
    }

    return result;
}


boolean Ifx_Shell_parseUInt32(pchar *argsPtr, uint32 *value, boolean hex)
{
    uint64  value64;
    boolean result;

    *value = 0;

    if (Ifx_Shell_parseUInt64(argsPtr, &value64, hex) == FALSE)
    {
        result = FALSE;
    }
    else
    {
        *value = (uint32)value64;
        result = TRUE;
    }

    return result;
}


boolean Ifx_Shell_parseSInt64(pchar *argsPtr, sint64 *value)
{
    char    buffer[64];
    boolean result;

    *value = 0;

    if (Ifx_Shell_parseToken(argsPtr, buffer, Ifx_COUNTOF(buffer)) == FALSE)
    {
        result = FALSE;
    }
    else
    {
        result = (buffer[0] != IFX_SHELL_NULL_CHAR) && (sscanf(buffer, IFX_SHELL_LLD, value) == 1);
    }

    return result;
}


boolean Ifx_Shell_parseUInt64(pchar *argsPtr, uint64 *value, boolean hex)
{
    char    buffer[64];
    boolean result;

    *value = 0;

    if (Ifx_Shell_parseToken(argsPtr, buffer, Ifx_COUNTOF(buffer)) == FALSE)
    {
        result = FALSE;
    }
    else
    {
        char *bufferPointer = buffer;

        if ((buffer[0] == '0') && (buffer[1] == 'x'))
        {
            bufferPointer = &bufferPointer[2];
            hex           = TRUE;
        }

        if (hex != FALSE)
        {
            result = (bufferPointer[0] != IFX_SHELL_NULL_CHAR) && (sscanf(bufferPointer, IFX_SHELL_LLX, value) == 1);
        }
        else
        {
            result = (bufferPointer[0] != IFX_SHELL_NULL_CHAR) && (sscanf(bufferPointer, IFX_SHELL_LLU, value) == 1);
        }
    }

    return result;
}


boolean Ifx_Shell_parseFloat64(pchar *argsPtr, float64 *value)
{
    char    buffer[64];
    boolean result;

    *value = 0;

    if (Ifx_Shell_parseToken(argsPtr, buffer, Ifx_COUNTOF(buffer)) == FALSE)
    {
        result = FALSE;
    }
    else
    {
        result = (buffer[0] != IFX_SHELL_NULL_CHAR) && (sscanf(buffer, "%lf ", value) == 1);
    }

    return result;
}


boolean Ifx_Shell_parseFloat32(pchar *argsPtr, float32 *value)
{
    char    buffer[64];
    boolean result;

    *value = 0;

    if (Ifx_Shell_parseToken(argsPtr, buffer, Ifx_COUNTOF(buffer)) == FALSE)
    {
        result = FALSE;
    }
    else
    {
        result = (buffer[0] != IFX_SHELL_NULL_CHAR) && (sscanf(buffer, "%f ", value) == 1);
    }

    return result;
}


const Ifx_Shell_Command *Ifx_Shell_commandFind(const Ifx_Shell_Command *commandList, pchar commandLine, pchar *args)
{
    const Ifx_Shell_Command *command = commandList;
    const Ifx_Shell_Command *result  = NULL_PTR;

    while (command->commandLine != NULL_PTR)
    {
        pchar   commandTemp     = (pchar)command->commandLine;
        pchar   commandLineTemp = commandLine;
        char    buffer[256];
        boolean commandFound    = FALSE;

        while (Ifx_Shell_matchCommand(&commandLineTemp, &commandTemp) != FALSE)
        {
            commandFound = TRUE;
        }

        if ((commandFound != FALSE) && (Ifx_Shell_parseToken(&commandTemp, buffer, Ifx_COUNTOF(buffer)) == FALSE))
        {
            *args  = commandLineTemp;
            result = command;
            break;
        }

        command = &command[1];
    }

    return result;
}


const Ifx_Shell_Command *Ifx_Shell_commandListFind(Ifx_Shell *shell, pchar commandLine, pchar *args)
{
    int                      i;
    const Ifx_Shell_Command *shellCommand = NULL_PTR;

    for (i = 0; i < IFX_SHELL_COMMAND_LISTS; i++)
    {
        if (shell->commandList[i] != NULL_PTR)
        {
            shellCommand = Ifx_Shell_commandFind(shell->commandList[i], commandLine, args);

            if (shellCommand != NULL_PTR)
            {
                break;
            }
        }
    }

    return shellCommand;
}


void Ifx_Shell_execute(Ifx_Shell *shell, pchar commandLine)
{
    pchar                    args         = NULL_PTR;
    const Ifx_Shell_Command *shellCommand = Ifx_Shell_commandListFind(shell, commandLine, &args);

    if (shellCommand != NULL_PTR)
    {
        if (shellCommand->call(args, shellCommand->data, shell->io) != FALSE)
        {
            if (shell->control.sendResultCode != 0)
            {
                Ifx_Shell_writeResult(shell, Ifx_Shell_ResultCode_ok);
            }
        }
        else
        {
            if (shell->control.sendResultCode != 0)
            {
                Ifx_Shell_writeResult(shell, Ifx_Shell_ResultCode_nok);
            }
            else if (shell->control.echoError != 0)
            {
                IfxStdIf_DPipe_print(shell->io, "\r\nShell command error: %s" ENDL, commandLine);
            }
            else
            {}
        }
    }
    else
    {
        if (commandLine[0] != IFX_SHELL_NULL_CHAR)
        {
            if (shell->control.sendResultCode != 0)
            {
                Ifx_Shell_writeResult(shell, Ifx_Shell_ResultCode_unknown);
            }
            else if (shell->control.echoError != 0)
            {
                IfxStdIf_DPipe_print(shell->io, "\r\nUnknown command: %s" ENDL, commandLine);
            }
            else
            {}
        }
    }
}


/****************************************************************************************/
/* Processes escape sequences, including handling command history.                      */
/* The following escape sequences (prefix "ESC [") are supported:                       */
/* A - up     B - down      C - right     D - left                                      */
/* 1~ - HOME  2~ - INSERT   3~ - DELETE   4~ - END                                      */
/*                                                                                      */
/* Parameters:                                                                          */
/*      EscapeChar1 - First character to follow ESC [                                   */
/*      EscapeChar2 - Second character following ESC [ , if applicable                  */
/*                                                                                      */
/****************************************************************************************/
void Ifx_Shell_cmdEscapeProcess(Ifx_Shell *shell, char EscapeChar1, char EscapeChar2)
{
    Ifx_Shell_CmdLine *Cmd    = NULL_PTR; /* Command line editing state */
    char              *cmdStr = NULL_PTR; /* Cached pointer to command line being edited */
    sint32             i      = 0;        /* Loop variable */

    /* Validate parameters */
    boolean            result = (shell != NULL_PTR);

    IFX_ASSERT(IFX_VERBOSE_LEVEL_ERROR, result != FALSE);

    if (result == FALSE)
    {
        return;                 /* ERROR CASE - no thread data available! */
    }

    /* Cache command state and command line pointer */
    Cmd    = &shell->cmd;
    cmdStr = Cmd->cmdStr;

    /* Validate command line state */
    IFX_ASSERT(IFX_VERBOSE_LEVEL_ERROR, cmdStr != NULL_PTR);
    IFX_ASSERT(IFX_VERBOSE_LEVEL_ERROR, Cmd->cursor < IFX_SHELL_CMD_LINE_SIZE);
    IFX_ASSERT(IFX_VERBOSE_LEVEL_ERROR, Cmd->length < IFX_SHELL_CMD_LINE_SIZE);

    /* Switch on first character after ESC [ */
    switch (EscapeChar1)
    {
    case 'A':                  /* Up arrow */

        if (Cmd->historyItem == IFX_SHELL_CMD_HISTORY_NO_ITEM)
        {
            /* Not using list at the moment - take most recent item [0] */
            Cmd->historyItem = 0;
        }
        else
        {
            if (Cmd->historyItem < (IFX_SHELL_CMD_HISTORY_SIZE - 1))
            {
                /* If not already at oldest, go back one in list */
                Cmd->historyItem++;
            }
        }

        /* Copy text into buffer */
        strncpy(cmdStr, shell->cmdHistory[Cmd->historyItem], IFX_SHELL_CMD_LINE_SIZE);

        /* echo to screen if requested */
        if (shell->control.echo != 0)
        {
            IFX_SHELL_WRITE_BACKSPACES(Cmd->cursor)  /* Move cursor back to start */
            IFX_SHELL_WRITE_SPACES(Cmd->length)      /* Overwrite text with spaces */
            IFX_SHELL_WRITE_BACKSPACES(Cmd->length)  /* Move cursor back to start */
            IfxStdIf_DPipe_print(shell->io, cmdStr); /* Copy buffer to screen */
        }

        Cmd->cursor     = (Ifx_SizeT)strlen(cmdStr); /* Store cursor position */
        Cmd->length     = Cmd->cursor;               /* Store command line length */
        Cmd->historyAdd = FALSE;                     /* Don't add back to history unless modified */
        break;

    case 'B':                                        /* Down arrow */

        if ((Cmd->historyItem == IFX_SHELL_CMD_HISTORY_NO_ITEM) || (Cmd->historyItem == 0))
        {
            /* Not using list at the moment, or have dropped off the end - just clear command line */
            if (shell->control.echo != 0)
            {
                IFX_SHELL_WRITE_BACKSPACES(Cmd->cursor)       /* Move cursor back to start */
                IFX_SHELL_WRITE_SPACES(Cmd->length)           /* Overwrite text with spaces */
                IFX_SHELL_WRITE_BACKSPACES(Cmd->length)       /* Move cursor back to start */
            }

            Cmd->length      = 0;                             /* Reset command line length */
            Cmd->cursor      = 0;
            Cmd->historyItem = IFX_SHELL_CMD_HISTORY_NO_ITEM; /* Ensure we are not using list */
        }
        else
        {
            /* Within list - move to more recent entry */
            Cmd->historyItem--;

            /* Copy text into buffer */
            strncpy(cmdStr, shell->cmdHistory[Cmd->historyItem], IFX_SHELL_CMD_LINE_SIZE);

            if (shell->control.echo != 0)
            {
                IFX_SHELL_WRITE_BACKSPACES(Cmd->cursor)  /* Move cursor back to start */
                IFX_SHELL_WRITE_SPACES(Cmd->length)      /* Overwrite text with spaces */
                IFX_SHELL_WRITE_BACKSPACES(Cmd->length)  /* Move cursor back to start */
                IfxStdIf_DPipe_print(shell->io, cmdStr); /* Copy buffer to screen */
            }

            Cmd->cursor = (Ifx_SizeT)strlen(cmdStr);     /* Store cursor position */
            Cmd->length = Cmd->cursor;                   /* Store command line length */
        }

        Cmd->historyAdd = FALSE;
        break;

    case 'C':                  /* Right arrow */

        if (Cmd->cursor < Cmd->length)
        {
            /* Move cursor one place to right */
            IFX_SHELL_IF_ECHO(IfxStdIf_DPipe_print(shell->io, "%c", cmdStr[Cmd->cursor])) Cmd->cursor++;
        }

        break;

    case 'D':                  /* Left arrow */

        if (Cmd->cursor > 0)
        {
            /* Move cursor one place to left */
            IFX_SHELL_IF_ECHO(IfxStdIf_DPipe_print(shell->io, "\b")) Cmd->cursor--;
        }

        break;

    default:
        break;
    }

    /* If second character after ESC [ is ~ then switch on number */
    if (EscapeChar2 == '~')
    {
        switch (EscapeChar1)
        {
        case '1':              /* HOME - move to start of buffer */

            if (Cmd->cursor > 0)
            {
                IFX_SHELL_WRITE_BACKSPACES(Cmd->cursor) Cmd->cursor = 0;
            }

            break;

        case '2':              /* INSERT - insert blank character at cursor and move all remaining characters right one */

            if ((Cmd->cursor < Cmd->length) && (Cmd->length < (IFX_SHELL_CMD_LINE_SIZE - 1)))
            {
                /* Update on screen */
                if (shell->control.echo != FALSE)
                {
                    /* write over duplicated character at cursor */
                    IfxStdIf_DPipe_print(shell->io, " ");

                    /* Update line with new characters */
                    for (i = Cmd->cursor; i < Cmd->length; i++)
                    {
                        IfxStdIf_DPipe_print(shell->io, "%c", cmdStr[i]);
                    }

                    /* Move cursor back to new place */
                    IFX_SHELL_WRITE_BACKSPACES((Cmd->length + 1) - Cmd->cursor)
                }

                /* Update in command line variable */
                for (i = Cmd->length; i > Cmd->cursor; i--)
                {
                    cmdStr[i] = cmdStr[i - 1];                 /* Shuffle text right */
                }

                cmdStr[Cmd->length + 1] = IFX_SHELL_NULL_CHAR; /* Terminate string at end of longer string */
                cmdStr[Cmd->cursor]     = ' ';                 /* Blank character at cursor */

                Cmd->length++;                                 /* Now one character longer */
            }

            break;

        case '3':              /* DELETE - delete character to right and move all remaining characters left one */

            if (Cmd->cursor < Cmd->length)
            {
                /* Update on screen */
                if (shell->control.echo != 0)
                {
                    for (i = Cmd->cursor; i < (Cmd->length - 1); i++)
                    {
                        /* Update line with new characters */
                        IfxStdIf_DPipe_print(shell->io, "%c", cmdStr[i + 1]);
                    }

                    /* write over duplicated character at end */
                    IfxStdIf_DPipe_print(shell->io, " ");

                    /* Move cursor back to right place */
                    IFX_SHELL_WRITE_BACKSPACES(Cmd->length - Cmd->cursor)
                }

                /* Update in command line variable. Shuffle text left */
                strncpy(&cmdStr[Cmd->cursor], &cmdStr[Cmd->cursor + 1], Cmd->length - Cmd->cursor - 1);

                cmdStr[Cmd->length - 1] = IFX_SHELL_NULL_CHAR; /* Terminate string at end of shorter string */
                Cmd->length--;                                 /* Now one character shorter */
            }

            break;

        case '4':              /* END - ensure cursor is at end */

            while (Cmd->cursor < Cmd->length)
            {
                IFX_SHELL_IF_ECHO(IfxStdIf_DPipe_print(shell->io, "%c", cmdStr[Cmd->cursor])) Cmd->cursor++;
            }

            break;

        default:
            break;
        }
    }
}


void Ifx_Shell_enable(Ifx_Shell *shell)
{
    // Clear the Rx buffer!
    IfxStdIf_DPipe_clearRx(shell->io);
    // Enable the shell
    shell->control.enabled = 1;
}


void Ifx_Shell_disable(Ifx_Shell *shell)
{
    shell->control.enabled = 0;
}


void Ifx_Shell_printSyntax(const Ifx_Shell_Syntax *syntaxList, IfxStdIf_DPipe *io)
{
    const Ifx_Shell_Syntax *syntax = syntaxList;

    while (syntax->syntax != NULL_PTR)
    {
        IfxStdIf_DPipe_print(io, "Syntax     : %s" ENDL, syntax->syntax);
        IfxStdIf_DPipe_print(io, "           > %s" ENDL, syntax->description);
        syntax = &syntax[1];
    }
}
