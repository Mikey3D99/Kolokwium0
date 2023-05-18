all: build_server build_client

build_server:
		gcc -g -o server server.c -pthread -lrt

build_client:
		gcc -g -o client client.c -pthread -lrt