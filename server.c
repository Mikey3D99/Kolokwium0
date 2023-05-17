//
// Created by michaubob on 17.05.23.
//
#include "stdio.h"
#include "semaphore.h"
#include <pthread.h>
#include <sys/shm.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
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

int initialize_semaphore_and_mutex(Server * server){
    if(server == NULL){
        return -1;
    }
    if(sem_init(&server->sem, 1, 1) == -1){
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
        (*server)->server_status = READY;
        (*server)->current_number_of_nums = 0;
        (*server)->sum = -1;
        (*server)->server_pid = getpid();
        (*server)->connected = false;
        (*server)->should_process = false;
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
    while(true){
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

        if(server->connected){
            server->stat++;
            if(server->should_process){
                // process calculations
                server->sum = 69;

                if(sem_post(&server->server_busy) == -1){
                    perror("sem_wait");
                    if (shmdt(server) == -1) {
                        perror("shmdt");
                    }
                    pthread_exit(NULL);
                }

                if(sem_post(&server->calculations_finished)== -1){
                    perror("sem_wait");
                    if (shmdt(server) == -1) {
                        perror("shmdt");
                    }
                    pthread_exit(NULL);
                }
            }
        }

        if (sem_post(&server->sem) == -1) {
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
    char command[100];
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

            if (sem_wait(&server->sem) == -1) {
                perror("sem_wait");
                if (shmdt(server) == -1) {
                    perror("shmdt");
                }
                pthread_exit(NULL);
            }

            server->server_status = CLOSED;

            if (sem_post(&server->sem) == -1) {
                perror("sem_wait");
                if (shmdt(server) == -1) {
                    perror("shmdt");
                }
                pthread_exit(NULL);
            }
            break;
        }
        else if(strcmp(command, "stat") == 0){
            if (sem_wait(&server->sem) == -1) {
                perror("sem_wait");
                if (shmdt(server) == -1) {
                    perror("shmdt");
                }
                pthread_exit(NULL);
            }

            printf("Stat: [%d]", server->stat);

            if (sem_post(&server->sem) == -1) {
                perror("sem_wait");
                if (shmdt(server) == -1) {
                    perror("shmdt");
                }
                pthread_exit(NULL);
            }
        }
        else {
            printf("Unknown command: %s\n", command);
        }
    }

    pthread_join(processing_thread, NULL);
    destroy_semaphore_and_mutex(server);
    if (shmdt(server) == -1) {
        perror("shmdt");
    }
    //TODO: free resources
    return 0;
}