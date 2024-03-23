#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <semaphore.h>
#include <fcntl.h>
#include <string.h>

float calculateSumOfSquares(float *numbers, int startIdx, int endIdx);

int main(int argc, char *argv[]) {
    if (strcmp(argv[1], "shm") == 0) {
        int shmID = atoi(argv[2]);
        int shmIDResult = atoi(argv[3]);
        int childIndex = atoi(argv[4]);
        int nChildren = atoi(argv[5]);
        int count = atoi(argv[6]);

        float *numbers = (float *)shmat(shmID, NULL, SHM_RDONLY);
        float *resultPtr = (float *)shmat(shmIDResult, NULL, 0);
        if (numbers == (void *)-1 || resultPtr == (void *)-1) {
            fprintf(stderr, "shmat failed\n");
            exit(EXIT_FAILURE);
        }

        sem_t *sem = sem_open("/semaphore", 0);
        if (sem == SEM_FAILED) {
            perror("sem_open failed");
            exit(EXIT_FAILURE);
        }

        int segmentSize = count / nChildren;
        int remainder = count % nChildren;
        int startIdx = childIndex * segmentSize + (childIndex < remainder ? childIndex : remainder);
        int endIdx = startIdx + segmentSize + (childIndex < remainder ? 1 : 0);

        float sum = calculateSumOfSquares(numbers, startIdx, endIdx);

        sem_wait(sem);
        resultPtr[childIndex] = sum;
        sem_post(sem);

        shmdt(numbers);
        shmdt(resultPtr);
        sem_close(sem);

        //sleep(10); 
        exit(EXIT_SUCCESS);
    } else if (strcmp(argv[1], "pipe") == 0) {
        float sum = 0;
        float number;
        while (read(STDIN_FILENO, &number, sizeof(number)) > 0) {
            sum += (float)number * (float)number;
        }

        write(STDOUT_FILENO, &sum, sizeof(sum)); 

        //sleep(10);
        exit(EXIT_SUCCESS);
    } else {
        fprintf(stderr, "Invalid IPC method\n");
        exit(EXIT_FAILURE);
    }
}

float calculateSumOfSquares(float *numbers, int startIdx, int endIdx) {
    float sum = 0;
    for (int i = startIdx; i < endIdx; i++) {
        sum += (float)numbers[i] * (float)numbers[i];
    }
    return sum;
}
