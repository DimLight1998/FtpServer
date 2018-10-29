#include "Common.h"
#include "FileSystem.h"

// if `append` is absolute, put simplified `append` in `origin`;
// if `append` is relative, put simplified `origin`/`append` in `origin`
void ChangeDirectory(char *append, char *origin)
{
    if (append[0] == '/')
    {
        SimplifyPath(append);
        strncpy(origin, append, strlen(append) + 1);
    }
    else
    {
        origin[strlen(origin) + 1] = 0;
        origin[strlen(origin)] = '/';
        strncpy(origin + strlen(origin), append, strlen(append) + 1);
        SimplifyPath(origin);
    }
}

// `path` should starts with '/'
void SimplifyPath(char *path)
{
    const int maxLength = 4096;
    char simplifiedPath[maxLength];
    memset(simplifiedPath, 0, sizeof(simplifiedPath));

    // points to the end (excluded) of simplified path
    int simplifiedCurrent = 0;

    // points to the begin of the section (included)
    int sectionBegin = 0;
    // points to the end of the section (excluded)
    int sectionEnd = 1;
    while (path[sectionEnd] != '\0' && path[sectionEnd] != '/')
        sectionEnd++;

    while (true)
    {
        // empty section
        if ((sectionEnd - sectionBegin == 1) || (sectionEnd - sectionBegin == 2 && path[sectionBegin + 1] == '.'))
        {
        }
        else if (sectionEnd - sectionBegin == 3 && path[sectionBegin + 1] == '.' && path[sectionBegin + 2] == '.')
        {
            if (simplifiedCurrent != 0)
            {
                simplifiedCurrent--;
                while (simplifiedCurrent > 0 && simplifiedPath[simplifiedCurrent] != '/')
                    simplifiedCurrent--;
            }
        }
        else
        {
            strncpy(&simplifiedPath[simplifiedCurrent], path + sectionBegin, sectionEnd - sectionBegin);
            simplifiedCurrent += sectionEnd - sectionBegin;
        }

        if (path[sectionEnd] == 0)
            break;
        sectionBegin = sectionEnd;
        sectionEnd++;
        while (path[sectionEnd] != '\0' && path[sectionEnd] != '/')
            sectionEnd++;
    }

    simplifiedPath[simplifiedCurrent] = '\0';
    strncpy(path, simplifiedPath, simplifiedCurrent + 1);
    if (simplifiedCurrent == 0)
    {
        path[0] = '/';
        path[1] = 0;
    }
}

void ListFolder(char *realPath, char *buffer)
{
    buffer[0] = 0;
    char command[2048];
    memset(command, 0, sizeof(command));
    sprintf(command, "ls -al %s", realPath);
    FILE *fp = popen(command, "r");
    char line[1024];
    while (fgets(line, sizeof(line), fp) != NULL)
        strncpy(buffer + strlen(buffer), line, strlen(line) + 1);

    // the first line of `buffer` contains text like 'total 28K', remove the first line
    char tempBuffer[32 * 1024];
    strcpy(tempBuffer, buffer);
    int current = 0;
    while (tempBuffer[current] != '\r' && tempBuffer[current] != '\n')
        current++;
    while (tempBuffer[current] == '\r' || tempBuffer[current] == '\n')
        current++;
    strcpy(buffer, tempBuffer + current);
}

void GetPathRelativeToRoot(char *currentPath, char *pathRelativeToCurrent, char *out)
{
    char buffer[4096];
    strcpy(buffer, currentPath);
    ChangeDirectory(pathRelativeToCurrent, buffer);
    strcpy(out, buffer);
}