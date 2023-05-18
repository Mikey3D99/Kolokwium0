//
// Created by michaubob on 17.05.23.
//
#include <stdbool.h>
#include <sys/shm.h>
#include <semaphore.h>
#include <signal.h>
#include "stdio.h"
#include "stdlib.h"
#include <malloc.h>
#include <string.h>
#include <unistd.h>

#define SHM_KEY 8434
#define SHM_SIZE 2048
#define READY 1
#define CLOSED 0

typedef struct {
    int server_status;
    pid_t server_pid;
    sem_t sem;
    sem_t calculations_finished;
    sem_t server_busy;
    int32_t * mem_for_numbers;
    bool asc_desc;
    int number_of_elems;
    int stat;
    bool connected;
    int shm_key;

}Server;



int32_t * create_and_attach_shared_memory_for_numbers(Server* server, int number_of_items){
    int shmid = shmget(getpid(), sizeof(int32_t) * number_of_items, IPC_CREAT | 0666);
    if(shmid < 0){
        printf("error creating shared memory");
        return NULL;
    }

    void * mem = shmat(shmid, NULL, 0);
    if (mem == (void *)-1) {
        printf("error attaching shared memory");
        shmctl(shmid, IPC_RMID, NULL);  // Destroy the shared memory segment
        return NULL;
    }

    int32_t  * numbers = (int32_t *) mem;

    return numbers;
}

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


int count_numbers_from_file(const char * filename){
    FILE * file = fopen(filename, "r");
    if(file == NULL){
        perror("Unable to open file");
        return -1;
    }

    int count = 0;
    int temp = 0;

    while (fscanf(file, "%d\n", &temp) == 1) {
        //printf("%d", temp);
        count++;
    }
    return count;
}


int32_t * read_numbers_from_file(const char* filename, int max_numbers){
    FILE* file = fopen(filename, "r");
    if (file == NULL){
        perror("Unable to open file");
        return NULL;
    }

    //malloc
    int32_t * numbers = (int32_t * )malloc(sizeof(int32_t) * max_numbers);
    if(numbers == NULL){
        perror("Unable to allocate memory");
        return NULL;
    }

    int count = 0;
    while (count < max_numbers && fscanf(file, "%d", numbers + count) == 1) {
        count++;
    }

    fclose(file);

    return numbers;  // return the number of integers read
}

int main(int argc, char *argv[]){
    if(argc != 3){
        printf("Wrong number of arguments!\n");
        return 1;
    }

    char * filename = argv[1];
    int num = count_numbers_from_file(filename);
    /// now we know how much data has to be created

    ///read the file
    int32_t * numbers = read_numbers_from_file(filename, num);
    if(numbers == NULL){
        printf("error while reading numbers from file");
        return -1;
    }
   // bubbleSort(numbers, num);


    Server * server;
    int memory_id;
    server = connect_to_shared_memory(&memory_id);
    if(server == NULL){
        printf("Shared memory wasnt created yet");
        return 1;
    }

    if(kill(server->server_pid, 0) == -1){
        printf("Brak procesu serwera");
        return -1;
    }

    ///allocate another shared memory for numbers
    int32_t  * all = create_and_attach_shared_memory_for_numbers(server, num);

    for(int i = 0; i < num; i++) {
        printf("%d ", *(numbers + i));
    }

    ///after allocation of the numbers copy them into shared memory and determine the order
    if(strcmp(argv[2], "des") == 0){
        server->asc_desc = true;
    }
    memcpy(all, numbers, sizeof (int32_t) * num);
    server->shm_key = getpid();
    server->number_of_elems = num;
    printf("%d", server->number_of_elems);


    ///check if already one connected
    int busy = sem_trywait(&server->server_busy);
    if(busy != 0){
        printf("Server is busy");
        //TODO: czysczenie itp
        return -1;
    }
    printf("przed kalkulacji");

    /// if not connected then you can continue with client stuff
    server->connected = true;
    if (sem_post(&server->sem) == -1) {
        printf("sem_wait");
        if (shmdt(server) == -1) {
            perror("shmdt");
        }
        return -1;
    }

    if (sem_wait(&server->calculations_finished) == -1) {
        printf("sem_wait");
        if (shmdt(server) == -1) {
            perror("shmdt");
        }
        return -1;
    }
    printf("po kalkulacji");

    //-------------------------------------------------------------

        for(int i = 0; i < num; i++) {
            printf("%d ", *(all + i));
        }
        server->connected = false;
        server->asc_desc = false;
        server->number_of_elems = 0;
    ///------------------------------------

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


























































