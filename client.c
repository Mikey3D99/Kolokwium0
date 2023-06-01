#include <stdbool.h>
#include <sys/shm.h>
#include <semaphore.h>
#include <signal.h>
#include "stdio.h"
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>

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
    int number_of_elems;
    int stat;
    int stat_32t;
    int stat_16t;
    int stat_8t;
    bool connected;
    char shm_name[64];
    char datatype[20];
    int max_val;
    int min_val;


}Server;

#define SHM_NAME "shared_memory"
char shared_mem_name[64];

void * create_and_attach_shared_memory_for_numbers(int number_of_items, char datatype[]){
    // Generate a unique shared memory object name
    char shm_name[64];
    snprintf(shm_name, sizeof(shm_name), "%s_%d", SHM_NAME, getpid());

    int shm_fd = shm_open(shm_name, O_CREAT | O_RDWR, 0666);
    if(shm_fd == -1){
        perror("shm_open");
        return NULL;
    }

    size_t size;
    if(strcmp("uint8_t", datatype) == 0){
        printf("[u int 8]\n");
        size = sizeof (uint8_t) * number_of_items;
    }
    else if(strcmp("uint16_t", datatype) == 0){
        printf("[u int 16]\n");
        size = sizeof (uint16_t) * number_of_items;
    }
    else if(strcmp("uint32_t", datatype) == 0){
        printf("[u int 32]\n");
        size = sizeof (uint32_t) * number_of_items;
    }
    else{
        return NULL;
    }

    if (ftruncate(shm_fd, size) == -1) {
        perror("ftruncate");
        shm_unlink(shm_name);
        return NULL;
    }

    void * mem = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (mem == MAP_FAILED) {
        perror("mmap");
        shm_unlink(shm_name);
        return NULL;
    }

    void * numbers = mem;

    // Store the shared memory name for later usage (e.g., detaching and deleting it)
    strncpy(shared_mem_name, shm_name, 64);

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

int main(int argc, char *argv[]){
    if(argc != 5){
        printf("Wrong number of arguments!\n");
        return 1;
    }


    int number_of_elements = 0;
    int max_val = 0;
    int min_val = 0;
    char datatype[20];
    if(!sscanf(argv[2], "%d", &number_of_elements)){
        printf("Error scaning argument");
        return -1;
    }
    if(!sscanf(argv[1], "%s", datatype)){
        printf("Error scaning argument");
        return -1;
    }
    if(!sscanf(argv[3], "%d", &min_val)){
        printf("Error scaning argument");
        return -1;
    }
    if(!sscanf(argv[4], "%d", &max_val)){
        printf("Error scaning argument");
        return -1;
    }

    printf("Datatype [%s], min val[%d], max val[%d]", datatype, min_val, max_val);

    /// now we know how much data has to be created


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

    void  * all = create_and_attach_shared_memory_for_numbers( number_of_elements, datatype);

    strncpy(server->shm_name, shared_mem_name, 64);
    server->number_of_elems = number_of_elements;
    server->max_val = max_val;
    server->min_val = min_val;
    strcpy(server->datatype,datatype); // copy the data type to server
    printf("Number of elems: [%d]\n", server->number_of_elems);
    printf("Name of the shared mem: [%s]\n", server->shm_name);


    ///check if already one connected
    int busy = sem_trywait(&server->server_busy);
    if(busy != 0){
        printf("Server is busy");
        //TODO: czysczenie itp
        return -1;
    }

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

    if(strcmp("uint8_t", datatype) == 0){
        uint8_t * nums = (uint8_t *)all;
        for(int i = 0; i < number_of_elements; i++){
            printf("[%d]", *(nums + i));
        }

    }
    else if(strcmp("uint16_t", datatype) == 0){
        uint16_t * nums = (uint16_t *)all;
        for(int i = 0; i < number_of_elements; i++){
            printf("[%d]", *(nums + i));
        }
    }
    else if(strcmp("uint32_t", datatype) == 0){
        uint32_t * nums = (uint32_t *)all;
        for(int i = 0; i < number_of_elements; i++){
            printf("[%d]", *(nums + i));
        }
    }

    //-------------------------------------------------------------
        server->connected = false;
        server->number_of_elems = 0;
        server->min_val = 0;
        server->max_val = 0;
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


























































