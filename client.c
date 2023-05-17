//
// Created by michaubob on 17.05.23.
//
#include <stdbool.h>
#include <sys/shm.h>
#include <semaphore.h>
#include <signal.h>
#include "stdio.h"
#include "stdlib.h"

#define SHM_KEY 1111
#define SHM_SIZE 2048
#define READY 1
#define CLOSED 0

typedef struct {
    int server_status;
    pid_t server_pid;
    sem_t sem;
    sem_t calculations_finished;
    sem_t server_busy;
    //pthread_mutex_t mutex;
    int32_t array[100];
    int current_number_of_nums;
    int stat;
    bool connected;
    bool should_process;
    int sum;

}Server;

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

#define MAX_NUMBERS 100

int read_numbers_from_file(const char* filename, int32_t arr[MAX_NUMBERS]){
    FILE* file = fopen(filename, "r");
    if (file == NULL){
        perror("Unable to open file");
        return -1;
    }

    int count = 0;
    while (count < MAX_NUMBERS && fscanf(file, "%d", &arr[count]) == 1) {
        count++;
    }

    fclose(file);

    return count;  // return the number of integers read
}

int main(int argc, char *argv[]){
    if(argc != 2){
        printf("Wrong number of arguments!\n");
        return 1;
    }

    char * filename = argv[0];
    ///read the file
    int32_t numbers[MAX_NUMBERS];
    int num = read_numbers_from_file(filename, numbers);
    if(num == -1){
        printf("error reading file");
        return -1;
    }


    Server * server;
    int memory_id;
    server = connect_to_shared_memory(&memory_id);
    if(server == NULL){
        printf("Shared memory wasnt created yet");
        return -1;
    }

    if(kill(server->server_pid, 0) == -1){
        printf("Brak procesu serwera");
        return -1;
    }

    ///check if already one connected
    int busy = sem_trywait(&server->server_busy);
    if(busy != 0){
        printf("Server is busy");
        //TODO: czysczenie itp
        return -1;
    }

    printf("PRZED glownym semaforze");

    /// if not connected then you can continue with client stuff
    if (sem_wait(&server->sem) == -1) {
            printf("sem_wait");
            if (shmdt(server) == -1) {
                perror("shmdt");
            }
            return -1;
    }

    printf("po glownym semaforze");

    if (sem_wait(&server->calculations_finished) == -1) {
        printf("sem_wait");
        if (shmdt(server) == -1) {
            perror("shmdt");
        }
        return -1;
    }
    printf("po kalkulacji");

    if(server->sum == -1)
        printf("Waiting for server to calculate sum");
    else{
        printf("The sum is: %d", server->sum);
        server->sum = 0;
        server->connected = false;
        server->current_number_of_nums = 0;
        server->should_process = false;
    }

    if (sem_post(&server->sem) == -1) {
        printf("sem_wait");
        if (shmdt(server) == -1) {
            perror("shmdt");
        }
        return -1;
    }



    ///detach client from shared memory before exiting
    if (shmdt(server) == -1) {
        perror("shmdt");
    }
    return 0;
}