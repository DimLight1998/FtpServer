#include "Common.h"
#include "Connection.h"
#include "CommandParsers.h"
#include "FileSystem.h"
#include "Environment.h"

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
    const int pathSize = 2048;
    char currentPath[pathSize];
    memset(currentPath, 0, pathSize);
    currentPath[0] = '/';
    char renameFrom[2 * pathSize];

    ReplyCommand(connectionFd, 200, "Welcome to YGGUB FTP server!");
    const int maxCommandLength = 4096;

    char incomingCommand[maxCommandLength];
    char userName[maxCommandLength];

    // this buffer will be used in many places
    char buffer[2 * maxCommandLength];
    enum ClientState clientState = WaitingForInputUserName;

    int passiveSocketFd;
    bool passiveSocketBinded = false;

    struct sockaddr_in clientAddress;

    while (true)
    {
        enum ClientCommand command = GetNextCommand(connectionFd, incomingCommand, maxCommandLength);
        if (IsSimpleCommand(command))
        {
            HandleSimpleCommand(command, incomingCommand, connectionFd);
            continue;
        }

        switch (clientState)
        {
        case WaitingForInputUserName:
        {
            if (command == UserCommand)
            {
                UserCommandParser(incomingCommand, userName);
                if (strlen(userName) == 0)
                {
                    ReplyCommand(connectionFd, 530, "This is a private system - No anonymous login.");
                }
                else
                {
                    sprintf(buffer, "User %s OK. Password required.", userName);
                    ReplyCommand(connectionFd, 331, buffer);
                    clientState = WaitingForInputPassword;
                }
            }
            else if (command == PassCommand)
            {
                ReplyCommand(connectionFd, 530, "Please tell me who you are.");
            }
            else
            {
                ReplyCommand(connectionFd, 530, "You aren\'t logged in.");
            }

            break;
        }
        case WaitingForInputPassword:
        {
            if (command == PassCommand)
            {
                PassCommandParser(incomingCommand, buffer);
                bool isValidUserPass = VerifyUser(userName, buffer);
                if (isValidUserPass)
                {
                    ReplyCommand(connectionFd, 230, "OK. Current restricted directory is /.");
                    clientState = WaitingForCommand;
                }
                else
                {
                    ReplyCommand(connectionFd, 530, "Login authentication failed.");
                }
            }
            else if (command == UserCommand)
            {
                clientState = WaitingForInputUserName;
                UserCommandParser(incomingCommand, userName);
                if (strlen(userName) == 0)
                {
                    ReplyCommand(connectionFd, 530, "This is a private system - No anonymous login.");
                }
                else
                {
                    sprintf(buffer, "User %s OK. Password required.", userName);
                    ReplyCommand(connectionFd, 331, buffer);
                    clientState = WaitingForInputPassword;
                }
            }
            else
            {
                ReplyCommand(connectionFd, 530, "You aren\'t logged in.");
            }

            break;
        }
        case WaitingForCommand:
        {
            if (command == UserCommand || command == PassCommand)
            {
                ReplyCommand(connectionFd, 530, "You are already logged in.");
            }
            else if (command == RetrCommand)
            {
                ReplyCommand(connectionFd, 425, "No data connection.");
            }
            else if (command == StorCommand)
            {
                ReplyCommand(connectionFd, 425, "No data connection.");
            }
            else if (command == PortCommand)
            {
                int clientAddressLength;
                PortCommandParser(incomingCommand, (struct sockaddr *)&clientAddress, clientAddressLength);

                ReplyCommand(connectionFd, 200, "PORT command successful.");
                clientState = ReceivedPort;
            }
            else if (command == PasvCommand)
            {
                // in this case, we need to get local ip and a valid port
                if (passiveSocketBinded)
                    close(passiveSocketFd);

                GetLocalIp(buffer);
                for (int i = 0; i < strlen(buffer); i++)
                    if (buffer[i] == '.')
                        buffer[i] = ',';
                char ipAddress[64];
                memset(ipAddress, 0, sizeof(ipAddress));
                strcpy(ipAddress, buffer);

                passiveSocketFd = socket(AF_INET, SOCK_STREAM, 0);
                struct sockaddr_in dataAddress;
                memset(&dataAddress, 0, sizeof(dataAddress));
                dataAddress.sin_family = AF_INET;
                dataAddress.sin_addr.s_addr = htonl(INADDR_ANY);
                dataAddress.sin_port = htons(0);
                bind(passiveSocketFd, (struct sockaddr *)&dataAddress, sizeof(dataAddress));
                passiveSocketBinded = true;

                int dataPort = 0;
                while (dataPort == 0)
                {
                    struct sockaddr_in dataAddressOut;
                    socklen_t sockLen;
                    getsockname(passiveSocketFd, (struct sockaddr *)&dataAddressOut, &sockLen);
                    dataPort = dataAddressOut.sin_port;
                }

                int bigPort = dataPort / 256;
                int smallPort = dataPort % 256;
                sprintf(ipAddress + strlen(ipAddress), ",%d,%d", bigPort, smallPort);
                sprintf(buffer, "Entering Passive Mode (%s)", ipAddress);
                ReplyCommand(connectionFd, 227, buffer);

                ClientState = ReceivedPassive;
            }
            else if (command == MkdCommand)
            {
                char newPathRelative[pathSize];
                strncpy(newPathRelative, currentPath, strlen(currentPath) + 1);

                MkdCommandParser(incomingCommand, buffer);
                ChangeDirectory(buffer, newPathRelative);

                // now `newPathRelative` holds relative path
                char absolutePath[2 * pathSize];
                memset(absolutePath, 0, sizeof(absolutePath));
                strncpy(absolutePath, rootPath, strlen(rootPath) + 1);
                strncpy(absolutePath + strlen(absolutePath), newPathRelative, strlen(newPathRelative) + 1);

                memset(buffer, 0, sizeof(buffer));
                strcpy(buffer, "mkdir ");
                strncpy(buffer + strlen(buffer), absolutePath, strlen(absolutePath) + 1);

                int retVal = system(buffer);
                if (retVal == 0)
                    ReplyCommand(connectionFd, 257, "Success");
                else
                    ReplyCommand(connectionFd, 550, "Cannot make directory");
            }
            else if (command == CwdCommand)
            {
                char pathBackup[pathSize];
                strncpy(pathBackup, currentPath, strlen(currentPath) + 1);
                CwdCommandParser(incomingCommand, buffer);
                ChangeDirectory(buffer, currentPath);

                char testCommand[2 * pathSize];
                memset(testCommand, 0, sizeof(testCommand));
                strcpy(testCommand, "test -d ");
                strncpy(testCommand + strlen(testCommand), rootPath, strlen(rootPath) + 1);
                strncpy(testCommand + strlen(testCommand), currentPath, strlen(currentPath) + 1);

                int retVal = system(testCommand);
                if (retVal == 0)
                {
                    char *messageOk = "OK. Current directory is ";
                    strncpy(buffer, messageOk, strlen(messageOk) + 1);
                    strncpy(buffer + strlen(buffer), currentPath, strlen(currentPath) + 1);
                    ReplyCommand(connectionFd, 250, buffer);
                }
                else
                {
                    ReplyCommand(connectionFd, 550, "Cannot change directory.");
                    strncpy(currentPath, pathBackup, strlen(pathBackup) + 1);
                }
            }
            else if (command == PwdCommand)
            {
                ReplyCommand(connectionFd, 257, currentPath);
            }
            else if (command == ListCommand)
            {
                ReplyCommand(connectionFd, 425, "No data connection.");
            }
            else if (command == RmdCommand)
            {
                char newPathRelative[pathSize];
                strncpy(newPathRelative, currentPath, strlen(currentPath) + 1);

                RmdCommandParser(incomingCommand, buffer);
                ChangeDirectory(buffer, newPathRelative);

                // now `newPathRelative` holds relative path
                if (strcmp(newPathRelative, "/") == 0)
                {
                    ReplyCommand(connectionFd, 550, "No permission.");
                    break;
                }

                char absolutePath[2 * pathSize];
                memset(absolutePath, 0, sizeof(absolutePath));
                strncpy(absolutePath, rootPath, strlen(rootPath) + 1);
                strncpy(absolutePath + strlen(absolutePath), newPathRelative, strlen(newPathRelative) + 1);

                memset(buffer, 0, sizeof(buffer));
                strcpy(buffer, "test -d ");
                strncpy(buffer + strlen(buffer), absolutePath, strlen(absolutePath) + 1);

                int retVal = system(buffer);

                if (retVal != 0)
                {
                    ReplyCommand(connectionFd, 550, "No such directory.");
                    break;
                }

                memset(buffer, 0, sizeof(buffer));
                strcpy(buffer, "rm -d ");
                strncpy(buffer + strlen(buffer), absolutePath, strlen(absolutePath) + 1);

                retVal = system(buffer);
                if (retVal == 0)
                    ReplyCommand(connectionFd, 250, "Directory successfully removed.");
                else
                    ReplyCommand(connectionFd, 550, "Directory not empty.");
            }
            else if (command == RnfrCommand)
            {
                char original[pathSize];
                strcpy(original, currentPath);
                RnfrCommandParser(incomingCommand, buffer);
                ChangeDirectory(buffer, original);

                // check if is a file/directory
                char testCommand[2 * pathSize];
                memset(testCommand, 0, sizeof(testCommand));
                strcpy(testCommand, "test -e ");
                strncpy(testCommand + strlen(testCommand), rootPath, strlen(rootPath) + 1);
                strncpy(testCommand + strlen(testCommand), original, strlen(original) + 1);

                int retVal = system(testCommand);
                if(retVal == 0)
                {
                    ReplyCommand(connectionFd, 350, "RNFR accepted.");
                    strncpy(renameFrom, rootPath, strlen(rootPath) + 1);
                    strncpy(renameFrom + strlen(renameFrom), original, strlen(original) + 1);
                    clientState = WaitingForRenameTo;
                }
                else
                {
                    ReplyCommand(connectionFd, 550, "Sorry, but that file doesn\'t exist.");
                }
            }
            else if (command == RntoCommand)
            {
                ReplyCommand(connectionFd, 503, "Need RNFR before RNTO.");
            }
            else
            {
                ReplyCommand(connectionFd, 500, "Unknown command.");
            }

            break;
        }
        case WaitingForRenameTo:
        {
            if(command == RntoCommand) 
            {
                char newPath[pathSize];
                strcpy(newPath, currentPath);
                RntoCommandParser(incomingCommand, buffer);
                ChangeDirectory(buffer, newPath);

                // TODO
            }
            else
            {
                ReplyCommand(connectionFd, 503, "Bad sequence.");
                clientState = WaitingForCommand;
            }
            break;
        }
        case ReceivedPassive:
        {
            // TODO
            break;
        }
        case ReceivedPort:
        {
            // TODO
            break;
        }
        }
    }
}

void ReplyCommand(int connectionFd, int statusCode, char *message)
{
    int length = strlen(message);
    char *reply = malloc(length + 20);
    sprintf(reply, "%d %s\r\n", statusCode, message);
    write(connectionFd, reply, strlen(reply));
    free(reply);
}

bool VerifyUser(char *userName, char *password)
{
    // TODO complete this function
    return true;
}

bool IsSimpleCommand(enum ClientCommand command)
{
    return command == QuitCommand || command == SystCommand || command == TypeCommand;
}

void HandleSimpleCommand(enum ClientCommand command, char *commandDetail, int connectionFd)
{
    if (command == QuitCommand)
    {
        ReplyCommand(connectionFd, 221, "Goodbye.");
        close(connectionFd);
    }
    else if (command == SystCommand)
    {
        ReplyCommand(connectionFd, 215, "UNIX Type: L8");
    }
    else if (command == TypeCommand)
    {
        bool isBinary;
        TypeCommandParser(commandDetail, &isBinary);
        if (isBinary)
            ReplyCommand(connectionFd, 200, "Type is now 8 bit binary.");
        else
            ReplyCommand(connectionFd, 504, "Unknown type.");
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
    if (strncmp("USER", commandBuffer, 4) == 0 && isspace(commandBuffer[4]))
        return UserCommand;
    if (strncmp("PASS", commandBuffer, 4) == 0 && isspace(commandBuffer[4]))
        return PassCommand;
    if (strncmp("RETR", commandBuffer, 4) == 0 && isspace(commandBuffer[4]))
        return RetrCommand;
    if (strncmp("STOR", commandBuffer, 4) == 0 && isspace(commandBuffer[4]))
        return StorCommand;
    if (strncmp("PORT", commandBuffer, 4) == 0 && isspace(commandBuffer[4]))
        return PortCommand;
    if (strncmp("QUIT", commandBuffer, 4) == 0 && isspace(commandBuffer[4]))
        return QuitCommand;
    if (strncmp("SYST", commandBuffer, 4) == 0 && isspace(commandBuffer[4]))
        return SystCommand;
    if (strncmp("PASV", commandBuffer, 4) == 0 && isspace(commandBuffer[4]))
        return PasvCommand;
    if (strncmp("TYPE", commandBuffer, 4) == 0 && isspace(commandBuffer[4]))
        return TypeCommand;
    if (strncmp("MKD", commandBuffer, 3) == 0 && isspace(commandBuffer[3]))
        return MkdCommand;
    if (strncmp("CWD", commandBuffer, 3) == 0 && isspace(commandBuffer[3]))
        return CwdCommand;
    if (strncmp("PWD", commandBuffer, 3) == 0 && isspace(commandBuffer[3]))
        return PwdCommand;
    if (strncmp("LIST", commandBuffer, 4) == 0 && isspace(commandBuffer[4]))
        return ListCommand;
    if (strncmp("RMD", commandBuffer, 3) == 0 && isspace(commandBuffer[3]))
        return RmdCommand;
    if (strncmp("RNFR", commandBuffer, 4) == 0 && isspace(commandBuffer[4]))
        return RnfrCommand;
    if (strncmp("RNTO", commandBuffer, 4) == 0 && isspace(commandBuffer[4]))
        return RntoCommand;
    return Unknown;
}