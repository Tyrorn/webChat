all: server client

server: server.c
	gcc -g3  -Wall server.c -o server  -lsqlite3

client: client.c
	gcc -g3  -Wall client.c -o client

.PHONY: clean

check: 
	python3 test.py ./

clean: 
	rm server client
