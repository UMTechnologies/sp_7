#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <string.h>
#include <limits.h>
#include <errno.h>

#define READ_END 0
#define WRITE_END 1

void executeWithSharedMemory(float *numbers, int count, int nChildren);
void executeWithPipes(float *numbers, int count, int nChildren);

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Incorrect usage. Expected format: %s <filename> <number_of_children> <ipc_method>\n", argv[0]);
        exit(EXIT_FAILURE);
    }


    if (strcmp(argv[3], "shm") != 0 && strcmp(argv[3], "pipe") != 0) {
        fprintf(stderr, "Invalid IPC method. Please use 'shm' for shared memory or 'pipe' for pipes.\n");
        exit(EXIT_FAILURE);
    }

    errno = 0; 
    char *end;
    long nChildrenLong = strtol(argv[2], &end, 10);
    if (end == argv[2] || *end != '\0' || errno == ERANGE) {
        if (errno == ERANGE)
            fprintf(stderr, "Error: The number of children is out of the allowed range.\n");
        else
            fprintf(stderr, "Error: The number of children must be a positive integer.\n");
        exit(EXIT_FAILURE);
    }
    if (nChildrenLong <= 0 || nChildrenLong > INT_MAX) {
        fprintf(stderr, "Error: The number of children must be a positive integer within the range of 1 to %d.\n", INT_MAX);
        exit(EXIT_FAILURE);
    }
    int nChildren = (int)nChildrenLong;

    FILE *file = fopen(argv[1], "r");
    if (!file) {
        perror("Unable to open the file");
        exit(EXIT_FAILURE);
    }

    float *numbers = malloc(sizeof(float));
    if (numbers == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(EXIT_FAILURE);
    }
    int count = 0, capacity = 1;
    float num;
    while (fscanf(file, "%f", &num) == 1) {
        if (count >= capacity) {
            capacity *= 2;
            numbers = realloc(numbers, capacity * sizeof(float));
            if (numbers == NULL) {
                fprintf(stderr, "Memory reallocation failed\n");
                fclose(file);
                exit(EXIT_FAILURE);
            }
        }
        numbers[count++] = num;
    }
    fclose(file);

    if (count < 2) {
        fprintf(stderr, "The file must contain at least 2 numbers.\n");
        free(numbers);
        exit(EXIT_FAILURE);
    }

    if (nChildren > count / 2) {
        nChildren = count / 2;
        printf("Warning: Number of child processes adjusted to %d to match input size constraints.\n", nChildren);
    }

    if (strcmp(argv[3], "shm") == 0) {
        executeWithSharedMemory(numbers, count, nChildren);
    } else if (strcmp(argv[3], "pipe") == 0) {
        executeWithPipes(numbers, count, nChildren);
    } else {
        fprintf(stderr, "Invalid IPC method. Please use 'shm' for shared memory or 'pipe' for pipes.\n");
        free(numbers);
        exit(EXIT_FAILURE);
    }

    free(numbers);
    return 0;
}


void executeWithSharedMemory(float *numbers, int count, int nChildren) {
    int shmID = shmget(IPC_PRIVATE, count * sizeof(float), IPC_CREAT | 0666);
    if (shmID == -1) {
        perror("shmget failed");
        exit(EXIT_FAILURE);
    }

    float *shmPtr = (float *)shmat(shmID, NULL, 0);
    if (shmPtr == (void *)-1) {
        perror("shmat failed");
        shmctl(shmID, IPC_RMID, NULL);
        exit(EXIT_FAILURE);
    }
    memcpy(shmPtr, numbers, count * sizeof(float));

    int shmIDResult = shmget(IPC_PRIVATE, nChildren * sizeof(float), IPC_CREAT | 0666);
    if (shmIDResult == -1) {
        perror("shmget failed for result");
        shmdt(shmPtr);
        shmctl(shmID, IPC_RMID, NULL);
        exit(EXIT_FAILURE);
    }

    float *resultPtr = (float *)shmat(shmIDResult, NULL, 0);
    if (resultPtr == (void *)-1) {
        perror("shmat failed for result");
        shmdt(shmPtr);
        shmctl(shmID, IPC_RMID, NULL);
        shmctl(shmIDResult, IPC_RMID, NULL);
        exit(EXIT_FAILURE);
    }

    sem_unlink("/semaphore");
    sem_t *sem = sem_open("/semaphore", O_CREAT | O_EXCL, 0644, 1);
    if (sem == SEM_FAILED) {
        perror("sem_open failed");
        shmdt(shmPtr);
        shmdt(resultPtr);
        shmctl(shmID, IPC_RMID, NULL);
        shmctl(shmIDResult, IPC_RMID, NULL);
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < nChildren; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            char shmIDStr[20], shmIDResultStr[20], childIndexStr[20], nChildrenStr[20], countStr[20];
            sprintf(shmIDStr, "%d", shmID);
            sprintf(shmIDResultStr, "%d", shmIDResult);
            sprintf(childIndexStr, "%d", i);
            sprintf(nChildrenStr, "%d", nChildren);
            sprintf(countStr, "%d", count);
            execl("./child_process", "child_process", "shm", shmIDStr, shmIDResultStr, childIndexStr, nChildrenStr, countStr, (char *)NULL);
            perror("execl failed");
            exit(EXIT_FAILURE);
        }
    }

    for (int i = 0; i < nChildren; i++) {
        wait(NULL);
    }

    double totalSum = 0;
    for (int i = 0; i < nChildren; i++) {
        totalSum += resultPtr[i];
    }
    printf("Total sum of squares: %f\n", totalSum);

    shmdt(shmPtr);
    shmdt(resultPtr);
    shmctl(shmID, IPC_RMID, NULL);
    shmctl(shmIDResult, IPC_RMID, NULL);
    sem_close(sem);
    sem_unlink("/semaphore");
}



void executeWithPipes(float *numbers, int count, int nChildren) {
    int segmentSize = count / nChildren;
    int remainder = count % nChildren;
    double totalSum = 0;

    int **pipes = malloc(nChildren * sizeof(int*));
    int **resultPipes = malloc(nChildren * sizeof(int*));
    for (int i = 0; i < nChildren; i++) {
        pipes[i] = malloc(2 * sizeof(int));
        resultPipes[i] = malloc(2 * sizeof(int));

        if (pipe(pipes[i]) != 0 || pipe(resultPipes[i]) != 0) {
            perror("pipe failed");
            for (int j = 0; j <= i; j++) {
                free(pipes[j]);
                free(resultPipes[j]);
            }
            free(pipes);
            free(resultPipes);
            exit(EXIT_FAILURE);
        }
    }

    for (int i = 0; i < nChildren; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            for (int j = 0; j < nChildren; j++) {
                if (j != i) {
                    close(pipes[j][READ_END]);
                    close(pipes[j][WRITE_END]);
                    close(resultPipes[j][READ_END]);
                    close(resultPipes[j][WRITE_END]);
                }
            }

            close(pipes[i][WRITE_END]);
            close(resultPipes[i][READ_END]);

            dup2(pipes[i][READ_END], STDIN_FILENO);
            dup2(resultPipes[i][WRITE_END], STDOUT_FILENO);

            close(pipes[i][READ_END]);
            close(resultPipes[i][WRITE_END]);

            char childIndexStr[10];
            sprintf(childIndexStr, "%d", i);
            execl("./child_process", "child_process", "pipe", childIndexStr, (char *)NULL);
            perror("execl failed");
            exit(EXIT_FAILURE);
        }
    }

    for (int i = 0; i < nChildren; i++) {
        close(pipes[i][READ_END]);
        close(resultPipes[i][WRITE_END]);

        int start = i * segmentSize + (i < remainder ? i : remainder);
        int end = start + segmentSize + (i < remainder ? 1 : 0);
        for (int j = start; j < end; j++) {
            write(pipes[i][WRITE_END], &numbers[j], sizeof(numbers[j]));
        }
        close(pipes[i][WRITE_END]);
    }

    for (int i = 0; i < nChildren; i++) {
        float result;
        read(resultPipes[i][READ_END], &result, sizeof(result));
        totalSum += result;
        close(resultPipes[i][READ_END]);
    }

    for (int i = 0; i < nChildren; i++) {
        wait(NULL);
    }

    printf("Total sum of squares: %f\n", totalSum);

    for (int i = 0; i < nChildren; i++) {
        free(pipes[i]);
        free(resultPipes[i]);
    }
    free(pipes);
    free(resultPipes);
}