#ifndef FTPSERVER_CONNECTION_H
#define FTPSERVER_CONNECTION_H

struct ConnectionHandlerParams
{
    const char* RootPath;
    int ConnectionFd;
};

void* ConnectionHandler(void*);
void HandlerEntry(int connectionFd, const char *rootPath);

#endif