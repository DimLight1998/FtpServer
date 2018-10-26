#include "Common.h"
#include "FileSystem.h"

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