#include "Common.h"
#include "Connection.h"

const int MaxRootLength = 256;
const int ServerSocketBacklog = 20;
const int ServerPortFallback = 21;
const char* RootPathFallback = "/tmp";

bool ParseParameters(int argc, char **argv, int *outPort, char *outRoot);

int main(int argc, char **argv)
{
    // get parameters
    int serverPort = ServerPortFallback;
    char rootPath[MaxRootLength];
    strcpy(rootPath, RootPathFallback);
    if (ParseParameters(argc, argv, &serverPort, rootPath) == false)
    {
        printf("Invalid parameters!");
        return 0;
    }
    printf("Server running at port %d with root path %s\n", serverPort, rootPath);

    int serverSocketFd;
    struct sockaddr_in serverAddress;
    serverSocketFd = socket(AF_INET, SOCK_STREAM, 0);

    // set up `serverAddress`
    memset(&serverAddress, 0, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);
    serverAddress.sin_port = htons(serverPort);

    // start listening
    bind(serverSocketFd, (struct sockaddr *)&serverAddress, sizeof(serverAddress));
    listen(serverSocketFd, ServerSocketBacklog);

    // handle each incoming connection, dispatch them to threads
    while (true)
    {
        int connectionFd = accept(serverSocketFd, NULL, NULL);
        pthread_t thread;
        struct ConnectionHandlerParams *params = malloc(sizeof(ConnectionHandler));
        params->RootPath = rootPath;
        params->ConnectionFd = connectionFd;
        pthread_create(&thread, NULL, ConnectionHandler, params);
        pthread_detach(thread);
    }
    close(serverSocketFd);

    return 0;
}

// return true if success, else false
bool ParseParameters(int argc, char **argv, int *outPort, char *outRoot)
{
    if (argc == 1)
    {
        return true;
    }
    else if (argc == 3 && strcmp(argv[1], "-port") == 0)
    {
        *outPort = atoi(argv[2]);
        return true;
    }
    else if (argc == 3 && strcmp(argv[1], "-root") == 0)
    {
        if (strlen(argv[2]) > MaxRootLength)
            return false;

        strcpy(outRoot, argv[2]);
        return true;
    }
    else if (argc == 5 && strcmp(argv[1], "-port") == 0 && strcmp(argv[3], "-root") == 0)
    {
        if (strlen(argv[4]) > MaxRootLength)
            return false;

        *outPort = atoi(argv[2]);
        strcpy(outRoot, argv[4]);
        return true;
    }
    else if (argc == 5 && strcmp(argv[1], "-root") == 0 && strcmp(argv[3], "-port") == 0)
    {
        if (strlen(argv[2]) > MaxRootLength)
            return false;

        *outPort = atoi(argv[4]);
        strcpy(outRoot, argv[2]);
        return true;
    }

    return false;
}
