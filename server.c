#include "stdio.h"
#include "semaphore.h"
#include <pthread.h>
#include <sys/shm.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
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

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

int initialize_semaphore_and_mutex(Server * server){
    if(server == NULL){
        return -1;
    }
    if(sem_init(&server->sem, 1, 0) == -1){
        printf("Error with semaphore initialization");
        return -2;
    }
    if(sem_init(&server->calculations_finished, 1, 0) == -1){
        printf("Error with semaphore initialization");
        return -2;
    }
    if(sem_init(&server->server_busy, 1, 1) == -1){
        printf("Error with semaphore initialization");
        return -2;
    }

    return 0;
}

int destroy_semaphore_and_mutex(Server * server){
    if(server == NULL){
        return -1;
    }

    if(sem_destroy(&server->sem) == -1){
        printf("error destroying semaphore");
        return -2;
    }

    if(sem_destroy(&server->calculations_finished) == -1){
        printf("error destroying semaphore");
        return -2;
    }

    if(sem_destroy(&server->server_busy) == -1){
        printf("error destroying semaphore");
        return -2;
    }

    return 0;
}

int create_and_attach_shared_memory(Server** server){
    int shmid = shmget(SHM_KEY, SHM_SIZE, IPC_CREAT | 0666);
    if(shmid < 0){
        printf("error creating shared memory");
        return -1;
    }

    void* mem = shmat(shmid, NULL, 0);
    if (mem == (void *)-1) {
        printf("error attaching shared memory");
        shmctl(shmid, IPC_RMID, NULL);  // Destroy the shared memory segment
        return -1;
    }
    *server = (Server*) mem;

    return shmid;
}


int server_init(Server ** server){
    int shmid = create_and_attach_shared_memory(server);
    if(shmid != -1){
        initialize_semaphore_and_mutex(*server);
        (*server)->stat = 0;
        (*server)->stat_16t = 0;
        (*server)->stat_8t = 0;
        (*server)->stat_32t= 0;
        (*server)->server_status = READY;
        (*server)->server_pid = getpid();
        (*server)->connected = false;
        (*server)->number_of_elems = 0;
        return 0;
    }

    return -1;
}



//threaded function to process all server actions
void* process_array_calculations_thread(void* arg){
    Server * server = (Server*)arg;

    if(server == NULL){
        pthread_exit(NULL);
    }

    //check for client connection
    while(server->server_status != CLOSED){
        printf("test");
        fflush(stdout);
        if (sem_wait(&server->sem) == -1) {
            perror("sem_wait");
            if (shmdt(server) == -1) {
                perror("shmdt");
            }
            pthread_exit(NULL);
        }

        if(server->server_status == CLOSED){
            if (sem_post(&server->sem) == -1) {
                perror("sem_wait");
                if (shmdt(server) == -1) {
                    perror("shmdt");
                }
                pthread_exit(NULL);
            }
            break;
        }


        if(server->connected) {
            pthread_mutex_lock(&mutex);
            server->stat++;
            pthread_mutex_unlock(&mutex);

            // Open the shared memory object
            int shm_fd = shm_open(server->shm_name, O_RDWR, 0666);
            if(shm_fd == -1) {
                perror("shm_open");
                return NULL;
            }

            // Map the shared memory object into this process's memory
            void * numbers = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
            if(numbers == MAP_FAILED) {
                perror("mmap");
                return NULL;
            }

            ///PROCESSING THE DATA
            printf("[%s] - datatype", server->datatype);
            printf("[%d] - min val, [%d] - max val", server->min_val, server->max_val);
            if(strcmp("uint8_t", server->datatype) == 0){
                server->stat_8t++;
                uint8_t * nums = (uint8_t *)numbers;
                for(int i = 0; i < server->number_of_elems; i++){
                    *(nums + i) = (rand() %(server->max_val - server->min_val + 1)) + server->min_val;
                }

            }
            else if(strcmp("uint16_t", server->datatype) == 0){
                server->stat_16t++;
                uint16_t * nums = (uint16_t *)numbers;
                for(int i = 0; i < server->number_of_elems; i++){
                    *(nums + i) = (rand() %(server->max_val - server->min_val + 1)) + server->min_val;
                }
            }
            else if(strcmp("uint32_t", server->datatype) == 0){
                server->stat_32t++;
                uint32_t * nums = (uint32_t *)numbers;
                for(int i = 0; i < server->number_of_elems; i++){
                    *(nums + i) = (rand() %(server->max_val - server->min_val + 1)) + server->min_val;
                }
            }
            // detach from shared mem
            if(munmap(numbers, SHM_SIZE) == -1) {
                perror("munmap");
            }

            // Close the shared memory object
            if(close(shm_fd) == -1) {
                perror("close");
            }

            // Signal that calculations have finished
            if(sem_post(&server->calculations_finished) == -1) {
                perror("sem_post");
                pthread_exit(NULL);
            }
        }


        if (sem_wait(&server->sem) == -1) {
            perror("sem_wait");
            if (shmdt(server) == -1) {
                perror("shmdt");
            }
            pthread_exit(NULL);
        }

        if(sem_post(&server->server_busy) == -1){
            perror("sem_wait");
            if (shmdt(server) == -1) {
                perror("shmdt");
            }
            pthread_exit(NULL);
        }
    }

    pthread_exit(NULL);
}

int main(){
    Server * server;
    if(server_init(&server) == -1){
        printf("error with creating or attaching shared memory");
        return -1;
    }

    pthread_t processing_thread;
    if(pthread_create(&processing_thread, NULL, process_array_calculations_thread, (void*)server)){
        printf("error creating thread");
        //TODO: free resources
        return 1;
    }

    // Command loop
   /* char command[100];
    while (1) {
        printf("Enter a command: ");
        if (fgets(command, sizeof(command), stdin) == NULL) {
            printf("Error reading command\n");
            continue;
        }

        // Remove newline character at the end of the command
        command[strcspn(command, "\n")] = '\0';

        if (strcmp(command, "quit") == 0) {
            printf("Quitting...\n");
            pthread_mutex_lock(&mutex);
            if(!server->connected){
                sem_post(&server->sem);
            }
            server->server_status = CLOSED;
            pthread_mutex_unlock(&mutex);
            break;
        }
        else if(strcmp(command, "stat") == 0){

            pthread_mutex_lock(&mutex);
            printf("Stat for uint_8t: [%d]\n", server->stat_8t);
            printf("Stat for uint_16t: [%d]\n", server->stat_16t);
            printf("Stat for uint_32t: [%d]\n", server->stat_32t);
            pthread_mutex_unlock(&mutex);
        }
        else {
            printf("Unknown command: %s\n", command);
        }
    }*/

    pthread_join(processing_thread, NULL);
    destroy_semaphore_and_mutex(server);
    if (shmdt(server) == -1) {
        perror("shmdt");
    }
    pthread_mutex_destroy(&mutex);
    //TODO: free resources
    return 0;
}