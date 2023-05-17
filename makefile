all: build_server build_client

build_server:
		gcc -o server server.c -pthread -lrt

build_client:
		gcc -o client client.c -pthread -lrt