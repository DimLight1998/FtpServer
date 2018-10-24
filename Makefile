build:
	gcc Entry.c Connection.c CommandParsers.c -o server -pthread -Wall -Werror