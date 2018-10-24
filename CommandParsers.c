#include "Common.h"
#include "CommandParsers.h"

// Given command in `command`, return user name in `userName`.
// Command should contain one CRLF at least.
// `userName` should be large enough and will be terminated by null byte.
void UserCommandParser(char *command, char *userName)
{
    int currentPos = 4;
    while (isspace(command[currentPos]))
        currentPos++;
    int begin = currentPos;
    while (!IsCrLf(command[currentPos]))
        currentPos++;
    int end = currentPos;
    strncpy(userName, command + begin, end - begin);
    userName[end - begin] = 0;
}

// Given command in `command`, return password in `password`.
// Command should contain one CRLF at least.
// `password` should be large enough, it will be terminated by null byte.
void PassCommandParser(char *command, char *password)
{
    int currentPos = 4;
    while (isspace(command[currentPos]))
        currentPos++;
    int begin = currentPos;
    while (!IsCrLf(command[currentPos]))
        currentPos++;
    int end = currentPos;
    strncpy(password, command + begin, end - begin);
    password[end - begin] = 0;
}

void RetrCommandParser(char *command, char *pathName)
{
    PathParser(command + 4, pathName);
}

void StorCommandParser(char *command, char *pathName)
{
    PathParser(command + 4, pathName);
}

void PortCommandParser(char *command, struct sockaddr *address, int *addressLength)
{
    // syntax: PORT <SP> <number>,<number>,<number>,<number>,<number>,<number> <CRLF>
    int left = 4;
    int right;
    int current = 4;
    int intArr[6];
    int index = 0;
    while (index < 6)
    {
        current++;
        while (command[current] != ',' && command[current] != '\n')
            current++;
        right = current;
        char digitBuffer[128];
        strncpy(digitBuffer, command + left, right - left);
        digitBuffer[right - left] = 0;
        intArr[index] = atoi(digitBuffer);
        index++;
        left = current + 1;
    }

    struct sockaddr_in remoteAddress;
    memset(&remoteAddress, 0, sizeof(remoteAddress));
    remoteAddress.sin_family = AF_INET;
    char ipAddress[128];
    sprintf(ipAddress, "%d.%d.%d.%d", intArr[0], intArr[1], intArr[2], intArr[3]);
    remoteAddress.sin_addr.s_addr = inet_addr(ipAddress);
    remoteAddress.sin_port = htons(intArr[4] * 256 + intArr[5]);

    memcpy(address, &remoteAddress, sizeof(remoteAddress));
    *addressLength = sizeof(remoteAddress);
}

void MkdCommandParser(char *command, char *pathName)
{
    PathParser(command + 3, pathName);
}

void CwdCommandParser(char *command, char *pathName)
{
    PathParser(command + 3, pathName);
}

void PwdCommandParser(char *command, char *pathName)
{
    PathParser(command + 3, pathName);
}

// If pathName[0] == 0, then this command list cwd, else it list `pathName`.
void ListCommandParser(char *command, char *pathName)
{
    // there are two forms of LIST command
    // LIST <CRLF> or LIST <SP> <pathname>

    int current = 4;
    while (isspace(command[current]) && command[current] != '\n')
        current++;
    if (command[current] == '\n')
    {
        // LIST <CRLF> in this case
        pathName[0] = 0;
        return;
    }
    else
    {
        PathParser(command + 4, pathName);
    }
}

void RmdCommandParser(char *command, char *pathName)
{
    PathParser(command + 3, pathName);
}

void RnfrCommandParser(char *command, char *pathName)
{
    PathParser(command + 4, pathName);
}

void RntoCommandParser(char *command, char *pathName)
{
    PathParser(command + 4, pathName);
}

bool IsCrLf(char c)
{
    return c == '\n' || c == '\r';
}

// Get path name from `potential`.
//
// `potential` doesn't need to be terminated by null byte,
// but a LF is required.
//
// Return value is placed in `pathName`, terminated by a null byte.
void PathParser(char *potential, char *pathName)
{
    int current = 0;
    while (isspace(potential[current]))
        current++;

    if (potential[current] == '\"')
    {
        // this file name is quoted
        potential++;
        int begin = current;
        while (potential[current] != '\"' && potential[current] != '\n')
            current++;
        int end = potential[current] == '\"' ? current : begin;
        strncpy(pathName, potential + begin, end - begin);
        pathName[end - begin] = 0;
    }
    else
    {
        // this file name is not quoted
        int begin = current;
        while (!isspace(potential[current]))
            current++;
        int end = current;
        strncpy(pathName, potential + begin, end - begin);
        pathName[end - begin] = 0;
    }
}