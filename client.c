//
// Created by michaubob on 17.05.23.
//
#include "stdio.h"
#include "server.c"
#include "stdlib.h"
#define SHM_KEY 1321

Server * connect_to_shared_memory(int * shmid){
    *shmid = shmget(SHM_KEY, SHM_SIZE, 0666);
    if(*shmid < 0){
        return NULL;
    }

    Server * server = (Server *)shmat(*shmid, NULL, 0);
    if(server == (Server *)-1){
        return NULL;
    }

    return server;
}

int main(int argc, char *argv[]){
    if(argc != 2){
        printf("Wrong number of arguments!\n");
        return 1;
    }

    char * filename = argv[0];
    Server * server;
    int memory_id;
    server = connect_to_shared_memory(&memory_id);
    if(server == NULL){
        printf("Shared memory wasnt created yet");
        return -1;
    }


    ///check if already one connected
    if (sem_wait(&server->sem) == -1) {
        printf("sem_wait");
        if (shmdt(server) == -1) {
            perror("shmdt");
        }
        return -1;
    }

    if(server->connected){
        printf("There is one client connected already...");
        if (shmdt(server) == -1) {
            perror("shmdt");
        }
        return -1;
    }
    else{
        server->connected = true;
    }

    if (sem_post(&server->sem) == -1) {
        printf("sem_wait");
        if (shmdt(server) == -1) {
            perror("shmdt");
        }
        return -1;
    }

    /// if not connected then you can continue with client stuff




    ///detach client from shared memory before exiting
    if (shmdt(server) == -1) {
        perror("shmdt");
    }
    return 0;
}