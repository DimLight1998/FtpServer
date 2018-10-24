#ifndef FTPSERVER_COMMAND_PASERS_H
#define FTPSERVER_COMMAND_PASERS_H

#include "Common.h"

void UserCommandParser(char *command, char *userName);
void PassCommandParser(char *command, char *password);
void RetrCommandParser(char *command, char *pathName);
void StorCommandParser(char *command, char *pathName);
void PortCommandParser(char *command, struct sockaddr *address, int *addressLength);
void TypeCommandParser(char *command, bool *isBinary);
void MkdCommandParser(char *command, char *pathName);
void CwdCommandParser(char *command, char *pathName);
void PwdCommandParser(char *command, char *pathName);
void ListCommandParser(char *command, char *pathName);
void RmdCommandParser(char *command, char *pathName);
void RnfrCommandParser(char *command, char *pathName);
void RntoCommandParser(char *command, char *pathName);
bool IsCrLf(char c);
void PathParser(char *potential, char *pathName);

#endif