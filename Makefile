build:
	gcc Entry.c Connection.c CommandParsers.c FileSystem.c -o server -pthread -Wall -Werror