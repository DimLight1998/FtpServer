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
    const char *message = "Hello from handler!";
    write(connectionFd, message, strlen(message));
    close(connectionFd);
}