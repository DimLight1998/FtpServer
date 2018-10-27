#ifndef FTPSERVER_FILESYSTEM_H
#define FTPSERVER_FILESYSTEM_H

void ChangeDirectory(char *append, char *origin);
void SimplifyPath(char *path);
void ListFolder(char *realPath, char *buffer);
void GetPathRelativeToRoot(char *currentPath, char *pathRelativeToCurrent, char *out);

#endif