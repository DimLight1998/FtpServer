#include "Environment.h"
#include "Common.h"

void GetLocalIp(char *ipAddressOut)
{
    char *command = "ifconfig | sed -En 's/127.0.0.1//;s/.*inet (addr:)?(([0-9]*\\.){3}[0-9]*).*/\\2/p'";
    char buf[128];
    FILE *fp = popen(command, "r");
    fgets(buf, 128, fp);
    pclose(fp);
    int end = 0;
    while (!isspace(buf[end]))
        end++;
    strncpy(ipAddressOut, buf, end);
    ipAddressOut[end] = '\0';
}
