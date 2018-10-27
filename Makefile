build:
	gcc Entry.c Connection.c CommandParsers.c FileSystem.c Environment.c -o server -pthread -Wall -Werror