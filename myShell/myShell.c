#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>

void skipPipe(int* cmdIndex, bool* pipeIndxs)
{
    while (pipeIndxs[(*cmdIndex)])
        (*cmdIndex)++;
    (*cmdIndex)++; //the last cmd in pipe is with false pipeIndx
}

void unmake(bool arr[], int n)
{
    int i;
    for (i = 0; i < n; i++)
        arr[i] = false;
}

void printArgsToExec(char** args, int j)
{
    int i = 0;
    for (i; i <= j; i++)
        printf("args[%d]: %s|\n", i, args[i]);

}

void removeLeadingTabsSpacesNewlines(char buff[3200], int* buffIndex, int n)
{
    char c;
    while ((*buffIndex) < n && (c = buff[(*buffIndex)]) && (c == ' ' || c == '\t' || c == '\n'))
        (*buffIndex)++;
}

void getNextArg(char buff[3200], int* buffIndex, int n, char* arg, int* argsIndex, bool* backgroundMode, bool* isPipe, bool* toOpen)
//always returns arg. If none meaningful argument, empty argument is returned (starting with '\0')
{
    char c = 0;
    int k = 0;
    while ((*buffIndex) < n && 
            (c = buff[(*buffIndex)]) &&
            (c != ' ' && c != '\t' && c != '\n' && c!=';') &&
            k < 31)
    {        
        switch (c)
        {
            case '&':
                (*backgroundMode) = true;              
                break;
            case '|':
                (*isPipe) = true;
                break;
            case '<':
                close(0);
                (*toOpen) = true;
                (*buffIndex)++;
                removeLeadingTabsSpacesNewlines(buff, buffIndex, n);
                getNextArg(buff, buffIndex, n, arg, argsIndex, backgroundMode, isPipe, toOpen);
                open(arg, O_RDONLY);
                return;      
            case '>':
                close(1);
                (*toOpen) = true;
                (*buffIndex)++;
                removeLeadingTabsSpacesNewlines(buff, buffIndex, n);
                getNextArg(buff, buffIndex, n, arg, argsIndex, backgroundMode, isPipe, toOpen);
                open(arg, O_WRONLY | O_CREAT | O_TRUNC, 0744);
                return;
        }
        if (c == '&' || c == '|')
        {
            (*buffIndex)++;
            break;
        }
        arg[k] = c;
        (*buffIndex)++;
        k++;
    }
    arg[k] = 0;
    //printf("\narg = %s\n", arg);
    (*argsIndex)++;
}

void readCmds(char*** cmdsToExec, bool backgroundIndxs[], bool pipeIndxs[]) 
//up to 9 cmds with up to 9 args each cmd with up to 31 chars each arg
{
    char** argsToExec = malloc(10 * sizeof(char*)); 
    //will be given to execvp
    //will point to some of the meaningful args elements 
    //and the one after the last meaningful points NULL
    char* arg = malloc(32 * sizeof(char*)); //single argument

    char buff[3200], argFirstChar;
    bool toOpen = false;
    int n, buffIndex, argsIndex, cmdIndex;
    n = read(0, buff, 3200);
    buffIndex = 0;
    argsIndex = 0;
    cmdIndex = 0;
    while (buffIndex < n && cmdIndex < 9)
    {
        while (buffIndex < n && buff[buffIndex] != ';' && argsIndex < 9)
        {
            removeLeadingTabsSpacesNewlines(buff, &buffIndex, n);
            if (buffIndex < n)
            {
                getNextArg(buff, &buffIndex, n, arg, &argsIndex, &(backgroundIndxs[cmdIndex]), &(pipeIndxs[cmdIndex]), &toOpen);
                argFirstChar = arg[0]; 
                if (toOpen)
                {
                    toOpen = false;
                    argsIndex--;
                }
                else if (pipeIndxs[cmdIndex])
                {
                    argsIndex--;
                    break;
                }
                else if (argFirstChar != 0) //if the given argument is not empty
                {
                    argsToExec[argsIndex - 1] = arg; //argsIndex was incremented in getNextArg
                    arg = malloc(32 * sizeof(char));
                }
                else // if the given arg is empty
                {
                    argsIndex--;
                }
            }
        }
        if (argsIndex != 0)
        {
            argsToExec[argsIndex] = NULL;
            cmdsToExec[cmdIndex] = argsToExec;
            /*
            printArgsToExec(argsToExec, argsIndex);
            write(1, "\n", 1);
            */
            argsToExec = malloc(10 * sizeof(char*));
            argsIndex = 0;
            cmdIndex++;
        }
        buffIndex++;
    }
    cmdsToExec[cmdIndex] = NULL;
    free(argsToExec);
    free(arg);
}

void executeCmd(bool backgroundMode, char** cmd)
{
    int pidChild, pidGrandChild, status;
    if (backgroundMode)
    {
        if (pidChild = fork())
        {//parent
            waitpid(pidChild, &status, 0);
        }
        else
        {//child
            if (pidGrandChild = fork())
            {//child
                printf("PID: %d\n", pidGrandChild);
                exit(0);
            }
            else
            {//grandchild
                execvp(cmd[0], cmd);
                exit(-1);
            }
        }
    }    
    else
    {
        if (pidChild = fork())
        {//parent
            waitpid(pidChild, &status, 0);
            if (WIFSIGNALED(status))
            {
                write(1, "Wrong command\n", 15);
            }
        }
        else
        {//child
            execvp(cmd[0], cmd);
            abort();
        }
    }
}

void executePipeCmds(char*** cmdsToExec, int cmdIndex, bool* pipeIndxs)
{
    int pd[2];
    int pid, status;
    pipe(pd);
    if (pid = fork())
    {//parent
        close(0);
        dup(pd[0]);
        close(pd[0]);
        close(pd[1]);
        waitpid(pid, &status, 0);
        if (cmdIndex < 7 && pipeIndxs[cmdIndex + 1])
        {
            executePipeCmds(cmdsToExec, cmdIndex + 1, pipeIndxs);
        }
        else
        {
            execvp(cmdsToExec[cmdIndex + 1][0], cmdsToExec[cmdIndex + 1]);
        }
        exit(-1);
    }
    else
    {//child
        close(1);
        dup(pd[1]);
        close(pd[0]);
        close(pd[1]);
        execvp(cmdsToExec[cmdIndex][0], cmdsToExec[cmdIndex]);
        exit(-1);
    }
}

int main()
{
    bool close = false;
    int cmdIndex, pid, status, i, j;

    char*** cmdsToExec = malloc(10 * sizeof(char**));
    //max 9 comands on input separated by ; (last cmdsToExec is NULL)
    bool backgroundIndxs[9];
    bool pipeIndxs[8]; //pipes can be max of 8 for 9 cmds

    int stdIn = 0;
    int stdOut = 1;
    stdIn = dup(0);
    stdOut = dup(1);


    while (!close)
    {
        unmake(backgroundIndxs, 9);
        unmake(pipeIndxs, 8);
        write(1, "myShell> ", 9);

        readCmds(cmdsToExec, backgroundIndxs, pipeIndxs);
        cmdIndex = 0;
        while (cmdsToExec[cmdIndex] != NULL)
        {
            if (cmdsToExec[cmdIndex][0] && strcmp(cmdsToExec[cmdIndex][0], "exit") == 0)
            {
                close = true;
                cmdIndex++;
            }
            else
            {
                if (cmdIndex < 8 && pipeIndxs[cmdIndex])
                {
                    if ((pid = fork()) == 0)
                    {//child
                        executePipeCmds(cmdsToExec, cmdIndex, pipeIndxs);
                        exit(-1);
                    }
                    waitpid(pid, &status, 0);
                    if (!WIFEXITED(status))
                    {
                        write(1, "Wrong command\n", 15);
                    }
                    skipPipe(&cmdIndex, pipeIndxs);
                }
                else
                {
                    executeCmd(backgroundIndxs[cmdIndex], cmdsToExec[cmdIndex]);
                    cmdIndex++;
                }
            }
        }
        dup2(stdIn, 0);
        dup2(stdOut, 1);
    }

    //free allocated memory    
    i = 0;
    j = 0;
    while (cmdsToExec[i] != NULL)
    {
        while (cmdsToExec[i][j] != NULL)
        {
            free(cmdsToExec[i][j]);
            j++;
        }
        free(cmdsToExec[i]);
        i++;
    }
    free(cmdsToExec);

    return 0;
}
