#ifndef FTPSERVER_CONNECTION_H
#define FTPSERVER_CONNECTION_H

struct ConnectionHandlerParams
{
    const char *RootPath;
    int ConnectionFd;
};

enum ClientState
{
    WaitingForInputUserName,
    WaitingForInputPassword,
    WaitingForCommand,
    WaitingForRenameTo,
    ReceivedPassive,
    ReceivedPort
};

enum ClientCommand
{
    UserCommand,
    PassCommand,
    RetrCommand,
    StorCommand,
    PortCommand,
    QuitCommand,
    SystCommand,
    PasvCommand,
    TypeCommand,
    MkdCommand,
    CwdCommand,
    PwdCommand,
    ListCommand,
    RmdCommand,
    RnfrCommand,
    RntoCommand,
    Unknown
};

void *ConnectionHandler(void *);
void HandlerEntry(int connectionFd, const char *rootPath);

// this function will return type of the command, and save the command in `commandBuffer`
enum ClientCommand GetNextCommand(int connectionFd, char *commandBuffer, int bufferSize);

#endif