#include "Common.h"
#include "Connection.h"

void *ConnectionHandler(void *arg)
{
    struct ConnectionHandlerParams *params = (struct ConnectionHandlerParams *)arg;
    int connectionFd = params->ConnectionFd;
    const char *rootPath = params->RootPath;
    free(params);

    HandlerEntry(connectionFd, rootPath);
    return NULL;
}

void HandlerEntry(int connectionFd, const char *rootPath)
{
    const int MaxCommandLength = 4096;

    const char *message = "Hello from handler!";
    write(connectionFd, message, strlen(message));
    close(connectionFd);

    char incomingCommand[MaxCommandLength];
    enum ClientState clientState = WaitingForInputUserName;

    while (true)
    {
        enum ClientCommand command = GetNextCommand(connectionFd, incomingCommand, MaxCommandLength);
        switch (clientState)
        {
        case WaitingForInputUserName:
        {
            if (command == UserCommand)
            {
            }
            else
            {
                continue;
            }
        }
        case WaitingForInputPassword:
        {
        }
        case WaitingForCommand:
        {
        }
        case WaitingForRenameTo:
        {
        }
        case ReceivedPassive:
        {
        }
        case ReceivedPort:
        {
        }
        }
    }
}


// Read from connectionFd until '\r\n'.
// What is read will be stored in `commandBuffer`, and type of the command will be returned.
// `commandBuffer` will NOT be teminated by null byte, you need to extract it by the first LF.
enum ClientCommand GetNextCommand(int connectionFd, char *commandBuffer, int bufferSize)
{
    memset(commandBuffer, 0, bufferSize);
    int currentPos = 0;
    while (true)
    {
        char reading;
        if (read(connectionFd, &reading, 1) > 0)
        {
            commandBuffer[currentPos] = reading;
            currentPos++;
            if (currentPos == bufferSize || reading == '\n')
                break;
        }
    }

    // decide command type
    if (strncmp("USER", commandBuffer, 4) == 0)
        return UserCommand;
    if (strncmp("PASS", commandBuffer, 4) == 0)
        return PassCommand;
    if (strncmp("RETR", commandBuffer, 4) == 0)
        return RetrCommand;
    if (strncmp("STOR", commandBuffer, 4) == 0)
        return StorCommand;
    if (strncmp("PORT", commandBuffer, 4) == 0)
        return PortCommand;
    if (strncmp("QUIT", commandBuffer, 4) == 0)
        return QuitCommand;
    if (strncmp("SYST", commandBuffer, 4) == 0)
        return SystCommand;
    if (strncmp("PASV", commandBuffer, 4) == 0)
        return PasvCommand;
    if (strncmp("TYPE", commandBuffer, 4) == 0)
        return TypeCommand;
    if (strncmp("MKD", commandBuffer, 3) == 0)
        return MkdCommand;
    if (strncmp("CWD", commandBuffer, 3) == 0)
        return CwdCommand;
    if (strncmp("PWD", commandBuffer, 3) == 0)
        return PwdCommand;
    if (strncmp("LIST", commandBuffer, 4) == 0)
        return ListCommand;
    if (strncmp("RMD", commandBuffer, 3) == 0)
        return RmdCommand;
    if (strncmp("RNFR", commandBuffer, 4) == 0)
        return RnfrCommand;
    if (strncmp("RNTO", commandBuffer, 4) == 0)
        return RntoCommand;
    return Unknown;
}