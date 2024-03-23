#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

float calculateSumOfSquares(float *numbers, int count) {
    float sum = 0.0;
    for (int i = 0; i < count; ++i) {
        sum += numbers[i] * numbers[i];
    }
    return sum;
}

int main(int argc, char *argv[]) {
    if (argc != 5 && argc != 2) {
        fprintf(stderr, "Invalid number of arguments\n");
        return 1;
    }

    if (strcmp(argv[1], "shm") == 0) {
        int startIndex = atoi(argv[2]);
        int partSize = atoi(argv[3]);
        int resultIndex = atoi(argv[4]);

        HANDLE shmNumbers = OpenFileMapping(FILE_MAP_READ, FALSE, "SharedMemoryForNumbers");
        HANDLE shmResults = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, "SharedMemoryForResults");
        HANDLE mutexHandle = OpenMutex(MUTEX_ALL_ACCESS, FALSE, "MutexForResults");

        if (!shmNumbers || !shmResults || !mutexHandle) {
            fprintf(stderr, "Cannot open shared resources. Error: %d\n", GetLastError());
            return 1;
        }

        float *numbers = (float *)MapViewOfFile(shmNumbers, FILE_MAP_READ, 0, 0, 0);
        float *results = (float *)MapViewOfFile(shmResults, FILE_MAP_ALL_ACCESS, 0, 0, 0);
        if (!numbers || !results) {
            fprintf(stderr, "Cannot map view of file. Error: %d\n", GetLastError());
            return 1;
        }

        float sum = calculateSumOfSquares(numbers + startIndex, partSize);

        WaitForSingleObject(mutexHandle, INFINITE);
        results[resultIndex] = sum;
        ReleaseMutex(mutexHandle);
        //Sleep(10000);
        UnmapViewOfFile(numbers);
        UnmapViewOfFile(results);
        CloseHandle(shmNumbers);
        CloseHandle(shmResults);
        CloseHandle(mutexHandle);
    } else if (strcmp(argv[1], "pipe") == 0) {
        float sum = 0.0;
        float number;
        while (scanf("%f", &number) == 1) {
            sum += number * number;
        }
        printf("%f\n", sum);
        //Sleep(10000);
        return 0;
    } else {
        fprintf(stderr, "Invalid method\n");
        return 1;
    }

    return 0;
}
