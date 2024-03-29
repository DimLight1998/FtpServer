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

    ReplyCommand(connectionFd, 220, "Welcome to YGGUB FTP server!");
    const int maxCommandLength = 4096;

    char incomingCommand[maxCommandLength];
    char userName[maxCommandLength];

    // this buffer will be used in many places
    char buffer[2 * maxCommandLength];
    char buffer32K[32 * 1024];
    enum ClientState clientState = WaitingForInputUserName;

    int passiveSocketFd;
    bool passiveSocketBinded = false;

    struct sockaddr_in clientAddress;

    int position = 0;

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
                PortCommandParser(incomingCommand, (struct sockaddr *)&clientAddress, &clientAddressLength);

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
                listen(passiveSocketFd, 4);
                passiveSocketBinded = true;

                int dataPort = 0;
                while (dataPort == 0)
                {
                    struct sockaddr_in dataAddressOut;
                    socklen_t sockLen;
                    getsockname(passiveSocketFd, (struct sockaddr *)&dataAddressOut, &sockLen);
                    dataPort = ntohs(dataAddressOut.sin_port);
                }

                int bigPort = dataPort / 256;
                int smallPort = dataPort % 256;
                sprintf(ipAddress + strlen(ipAddress), ",%d,%d", bigPort, smallPort);
                sprintf(buffer, "Entering Passive Mode (%s)", ipAddress);
                ReplyCommand(connectionFd, 227, buffer);

                clientState = ReceivedPassive;
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
                sprintf(buffer, "mkdir \"%s\" 2>/dev/null", absolutePath);

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
                sprintf(testCommand, "test -d \"%s%s\" 2>/dev/null", rootPath, currentPath);

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
                sprintf(buffer, "\"%s\" is your current path.", currentPath);
                ReplyCommand(connectionFd, 257, buffer);
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
                sprintf(buffer, "test -d \"%s\" 2>/dev/null", absolutePath);

                int retVal = system(buffer);

                if (retVal != 0)
                {
                    ReplyCommand(connectionFd, 550, "No such directory.");
                    break;
                }

                memset(buffer, 0, sizeof(buffer));
                sprintf(buffer, "rm -d \"%s\" 2>/dev/null", absolutePath);

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
                sprintf(testCommand, "test -e \"%s%s\" 2>/dev/null", rootPath, original);

                int retVal = system(testCommand);
                if (retVal == 0)
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
            else if (command == RestCommand)
            {
                ReplyCommand(connectionFd, 425, "No data connection.");
            }
            else
            {
                ReplyCommand(connectionFd, 500, "Unknown command.");
            }

            break;
        }
        case WaitingForRenameTo:
        {
            if (command == RntoCommand)
            {
                char newPath[pathSize];
                strcpy(newPath, currentPath);
                RntoCommandParser(incomingCommand, buffer);
                ChangeDirectory(buffer, newPath);

                memset(buffer, 0, sizeof(buffer));
                sprintf(buffer, "mv \"%s\" \"%s%s\" 2>/dev/null", renameFrom, rootPath, newPath);
                int retVal = system(buffer);
                if (retVal == 0)
                    ReplyCommand(connectionFd, 250, "File successfully renamed.");
                else
                    ReplyCommand(connectionFd, 451, "Rename/move failed.");
                clientState = WaitingForCommand;
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
            if (command == ListCommand)
            {
                int dataConnectionFd = accept(passiveSocketFd, NULL, NULL);
                ReplyCommand(connectionFd, 150, "Connected.");

                ListCommandParser(incomingCommand, buffer);
                char relativeToRoot[2048];
                strcpy(relativeToRoot, currentPath);
                if (buffer[0] != 0)
                    ChangeDirectory(buffer, relativeToRoot);
                sprintf(buffer, "%s%s", rootPath, relativeToRoot);
                ListFolder(buffer, buffer32K);
                write(dataConnectionFd, buffer32K, strlen(buffer32K));
                close(dataConnectionFd);
                passiveSocketBinded = false;
                ReplyCommand(connectionFd, 226, "Transmission done.");
            }
            else if (command == StorCommand)
            {
                int dataConnectionFd = accept(passiveSocketFd, NULL, NULL);
                ReplyCommand(connectionFd, 150, "Connected.");

                StorCommandParser(incomingCommand, buffer);
                char relativeToRoot[2048];
                strcpy(relativeToRoot, currentPath);
                ChangeDirectory(buffer, relativeToRoot);
                sprintf(buffer, "%s%s", rootPath, relativeToRoot);

                char readingBuffer[1024];
                FILE *file = fopen(buffer, "w+");
                int fileNo = fileno(file);
                lseek(fileNo, position, SEEK_SET);
                int readSize;
                while ((readSize = read(dataConnectionFd, readingBuffer, sizeof(readingBuffer))) > 0)
                {
                    write(fileNo, readingBuffer, readSize);
                }
                fclose(file);
                close(dataConnectionFd);
                passiveSocketBinded = false;
                position = 0;
                ReplyCommand(connectionFd, 226, "Transmission done.");
            }
            else if (command == RetrCommand)
            {
                int dataConnectionFd = accept(passiveSocketFd, NULL, NULL);
                ReplyCommand(connectionFd, 150, "Connected.");

                RetrCommandParser(incomingCommand, buffer);
                char relativeToRoot[2048];
                strcpy(relativeToRoot, currentPath);
                ChangeDirectory(buffer, relativeToRoot);
                sprintf(buffer, "%s%s", rootPath, relativeToRoot);

                char readingBuffer[1024];
                FILE *file = fopen(buffer, "r");
                int fileNo = fileno(file);
                lseek(fileNo, position, SEEK_SET);
                int readSize;
                while ((readSize = read(fileNo, readingBuffer, sizeof(readingBuffer))) > 0)
                {
                    write(dataConnectionFd, readingBuffer, readSize);
                }
                fclose(file);
                close(dataConnectionFd);
                position = 0;
                passiveSocketBinded = false;
                ReplyCommand(connectionFd, 226, "Transmission done.");
            }
            else if (command == RestCommand)
            {
                RestCommandParser(incomingCommand, &position);
                ReplyCommand(connectionFd, 350, "REST ok.");
                break;
            }
            else
            {
                char rep[2048];
                sprintf(rep, "Unknown, connection dropped. Command is [%s].", incomingCommand);
                ReplyCommand(connectionFd, 500, rep);
            }
            clientState = WaitingForCommand;

            break;
        }
        case ReceivedPort:
        {
            if (command == ListCommand)
            {
                ListCommandParser(incomingCommand, buffer);
                char relativeToRoot[2048];
                strcpy(relativeToRoot, currentPath);
                if (buffer[0] != 0)
                    ChangeDirectory(buffer, relativeToRoot);
                sprintf(buffer, "%s%s", rootPath, relativeToRoot);
                ListFolder(buffer, buffer32K);

                int dataConnectionFd = socket(AF_INET, SOCK_STREAM, 0);
                connect(dataConnectionFd, (struct sockaddr *)&clientAddress, sizeof(clientAddress));
                ReplyCommand(connectionFd, 150, "Connected.");

                write(dataConnectionFd, buffer32K, strlen(buffer32K));
                close(dataConnectionFd);
                ReplyCommand(connectionFd, 226, "Transmission done.");
            }
            else if (command == StorCommand)
            {
                StorCommandParser(incomingCommand, buffer);
                char relativeToRoot[2048];
                strcpy(relativeToRoot, currentPath);
                ChangeDirectory(buffer, relativeToRoot);
                sprintf(buffer, "%s%s", rootPath, relativeToRoot);

                int dataConnectionFd = socket(AF_INET, SOCK_STREAM, 0);
                connect(dataConnectionFd, (struct sockaddr *)&clientAddress, sizeof(clientAddress));
                ReplyCommand(connectionFd, 150, "Connected.");

                char readingBuffer[1024];
                FILE *file = fopen(buffer, "w+");
                int fileNo = fileno(file);
                lseek(fileNo, position, SEEK_SET);
                int readSize;
                while ((readSize = read(dataConnectionFd, readingBuffer, sizeof(readingBuffer))) > 0)
                {
                    write(fileNo, readingBuffer, readSize);
                }
                fclose(file);
                close(dataConnectionFd);
                position = 0;
                ReplyCommand(connectionFd, 226, "Transmission done.");
            }
            else if (command == RetrCommand)
            {
                RetrCommandParser(incomingCommand, buffer);
                char relativeToRoot[2048];
                strcpy(relativeToRoot, currentPath);
                ChangeDirectory(buffer, relativeToRoot);
                sprintf(buffer, "%s%s", rootPath, relativeToRoot);

                int dataConnectionFd = socket(AF_INET, SOCK_STREAM, 0);
                connect(dataConnectionFd, (struct sockaddr *)&clientAddress, sizeof(clientAddress));
                ReplyCommand(connectionFd, 150, "Connected.");

                char readingBuffer[1024];
                FILE *file = fopen(buffer, "r");
                int fileNo = fileno(file);
                lseek(fileNo, position, SEEK_SET);
                int readSize;
                while ((readSize = read(fileNo, readingBuffer, sizeof(readingBuffer))) > 0)
                {
                    write(dataConnectionFd, readingBuffer, readSize);
                }
                fclose(file);
                close(dataConnectionFd);
                position = 0;
                ReplyCommand(connectionFd, 226, "Transmission done.");
            }
            else if (command == RestCommand)
            {
                RestCommandParser(incomingCommand, &position);
                ReplyCommand(connectionFd, 350, "REST ok.");
                break;
            }
            else
            {
                ReplyCommand(connectionFd, 500, "Unknown, connection dropped.");
            }
            clientState = WaitingForCommand;

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
    if (strcmp(userName, "anonymous") == 0)
        return true;

    FILE *fp = fopen("./auth.txt", "r");
    if (fp == NULL)
        return false;
    int read;
    size_t len;
    char *line = NULL;
    bool ret = false;
    while ((read = getline(&line, &len, fp)) != -1)
    {
        char lineCopy[1024];
        strcpy(lineCopy, line);
        int i = 0;
        while (lineCopy[i] != '\n' && lineCopy[i] != '\r')
            i++;
        lineCopy[i] = 0;
        if (strcmp(lineCopy, userName) == 0)
        {
            getline(&line, &len, fp);
            char passwordBuf[1024];
            strcpy(passwordBuf, line);
            i = 0;
            while (passwordBuf[i] != '\n' && passwordBuf[i] != '\r')
                i++;
            passwordBuf[i] = 0;
            if (strcmp(passwordBuf, password) == 0)
                ret = true;
            else
                ret = false;
            break;
        }
        else
        {
            getline(&line, &len, fp);
        }
    }

    fclose(fp);
    if (line)
        free(line);
    return ret;
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
            ReplyCommand(connectionFd, 200, "Type set to I.");
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
    if (strncmp("REST", commandBuffer, 4) == 0 && isspace(commandBuffer[4]))
        return RestCommand;
    return Unknown;
}