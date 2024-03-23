#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

void executeWithSharedMemory(float *numbers, int count, int nChildren);
void executeWithPipes(float *numbers, int count, int nChildren);

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <filename> <number_of_children> <ipc_method>\n", argv[0]);
        return EXIT_FAILURE;
    }

    if (strcmp(argv[3], "shm") != 0 && strcmp(argv[3], "pipe") != 0) {
        fprintf(stderr, "Invalid IPC method. Please use 'shm' for shared memory or 'pipe' for pipes. Ensure the correct order of parameters.\n");
        return EXIT_FAILURE;
    }

    char *endptr;
    errno = 0;
    long conv = strtol(argv[2], &endptr, 10);

    if ((errno == ERANGE && (conv == LONG_MAX || conv == LONG_MIN))
        || (errno != 0 && conv == 0)) {
        fprintf(stderr, "The number you've entered is too large. Please use a smaller number.\n");
        return EXIT_FAILURE;
    }
    if (endptr == argv[2]) {
        fprintf(stderr, "No digits were found. Please ensure you enter a number.\n");
        return EXIT_FAILURE;
    }
    if (*endptr != '\0') {
        fprintf(stderr, "Unexpected characters found after the number: %s. Please enter a valid number.\n", endptr);
        return EXIT_FAILURE;
    }

    int nChildren = (int)conv;
    if (nChildren <= 0) {
        fprintf(stderr, "The number of children must be a positive integer.\n");
        return EXIT_FAILURE;
    }

    FILE *file = fopen(argv[1], "r");
    if (!file) {
        perror("Unable to open file");
        return EXIT_FAILURE;
    }

    float *numbers = (float *)malloc(sizeof(float));
    if (!numbers) {
        fprintf(stderr, "Memory allocation failed\n");
        fclose(file);
        return EXIT_FAILURE;
    }

    int count = 0, capacity = 1;
    float num;
    while (fscanf(file, "%f", &num) == 1) {
        if (count >= capacity) {
            capacity *= 2;
            numbers = realloc(numbers, capacity * sizeof(float));
            if (!numbers) {
                fprintf(stderr, "Memory reallocation failed\n");
                fclose(file);
                return EXIT_FAILURE;
            }
        }
        numbers[count++] = num;
    }
    fclose(file);

    if (count < 2) {
        fprintf(stderr, "The file must contain at least 2 numbers.\n");
        free(numbers);
        return EXIT_FAILURE;
    }

    if (strcmp(argv[3], "shm") == 0) {
        executeWithSharedMemory(numbers, count, nChildren);
    } else if (strcmp(argv[3], "pipe") == 0) {
        executeWithPipes(numbers, count, nChildren);
    }

    free(numbers);
    return EXIT_SUCCESS;
}



void executeWithSharedMemory(float *numbers, int count, int nChildren) {
    if (nChildren > count / 2) {
        nChildren = count / 2;
        printf("Warning: Number of processes adjusted to %d due to insufficient data.\n", nChildren);
    }

    int basePartSize = count / nChildren;
    int remainder = count % nChildren;

    HANDLE shmHandleNumbers = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, count * sizeof(float), "SharedMemoryForNumbers");
    float *shmNumbers = (float *)MapViewOfFile(shmHandleNumbers, FILE_MAP_ALL_ACCESS, 0, 0, count * sizeof(float));
    memcpy(shmNumbers, numbers, count * sizeof(float));

    HANDLE shmHandleResults = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, nChildren * sizeof(float), "SharedMemoryForResults");
    float *shmResults = (float *)MapViewOfFile(shmHandleResults, FILE_MAP_ALL_ACCESS, 0, 0, nChildren * sizeof(float));

    HANDLE mutexHandle = CreateMutex(NULL, FALSE, "MutexForResults");

    PROCESS_INFORMATION *pi = (PROCESS_INFORMATION *)malloc(nChildren * sizeof(PROCESS_INFORMATION));
    STARTUPINFO si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);

    int startIndex = 0;
    for (int i = 0; i < nChildren; i++) {
        int partSize = basePartSize + (i < remainder ? 1 : 0);
        int resultIndex = i;

        char cmd[256];
        sprintf_s(cmd, 256, "child_process.exe shm %d %d %d", startIndex, partSize, resultIndex);

        if (!CreateProcess(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi[i])) {
            fprintf(stderr, "CreateProcess failed: %d\n", GetLastError());
            for (int j = 0; j <= i; j++) {
                CloseHandle(pi[j].hProcess);
                CloseHandle(pi[j].hThread);
            }
            free(pi);
            UnmapViewOfFile(shmNumbers);
            UnmapViewOfFile(shmResults);
            CloseHandle(shmHandleNumbers);
            CloseHandle(shmHandleResults);
            CloseHandle(mutexHandle);
            exit(EXIT_FAILURE);
        }
        startIndex += partSize;
    }

    for (int i = 0; i < nChildren; i++) {
        WaitForSingleObject(pi[i].hProcess, INFINITE);
        CloseHandle(pi[i].hProcess);
        CloseHandle(pi[i].hThread);
    }
    free(pi);

    float totalSum = 0;
    for (int i = 0; i < nChildren; i++) {
        totalSum += shmResults[i];
    }
    printf("Total sum of squares: %f\n", totalSum);

    UnmapViewOfFile(shmNumbers);
    UnmapViewOfFile(shmResults);
    CloseHandle(shmHandleNumbers);
    CloseHandle(shmHandleResults);
    CloseHandle(mutexHandle);
}


void CreateChildProcess(const char* cmdLine, HANDLE hChildStdIn, HANDLE hChildStdOut) {
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    si.hStdOutput = hChildStdOut;
    si.hStdInput = hChildStdIn;
    si.dwFlags |= STARTF_USESTDHANDLES;
    ZeroMemory(&pi, sizeof(pi));

    if (!CreateProcess(NULL, (LPSTR)cmdLine, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
        fprintf(stderr, "CreateProcess failed (%d).\n", GetLastError());
        exit(EXIT_FAILURE);
    }

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
}

void executeWithPipes(float *numbers, int count, int nChildren) {
    SECURITY_ATTRIBUTES saAttr;
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = NULL;

    HANDLE hChildStdInWr[nChildren], hChildStdOutRd[nChildren];
    DWORD written, read;
    char buffer[1024];
    int totalNumbers = count, number, i;

    if(nChildren > totalNumbers / 2) {
        nChildren = totalNumbers / 2;
        printf("Warning: Number of child processes reduced to %d due to insufficient data.\n", nChildren);
    }

    for (i = 0; i < nChildren; i++) {
        HANDLE hStdInRdTmp, hStdOutWrTmp;
        CreatePipe(&hStdInRdTmp, &hChildStdInWr[i], &saAttr, 0);
        CreatePipe(&hChildStdOutRd[i], &hStdOutWrTmp, &saAttr, 0);
        SetHandleInformation(hChildStdInWr[i], HANDLE_FLAG_INHERIT, 0);
        SetHandleInformation(hChildStdOutRd[i], HANDLE_FLAG_INHERIT, 0);

        char childCmdLine[] = "child_process.exe pipe";
        CreateChildProcess(childCmdLine, hStdInRdTmp, hStdOutWrTmp);

        CloseHandle(hStdInRdTmp);
        CloseHandle(hStdOutWrTmp);
    }

    int numsPerChild = totalNumbers / nChildren;
    int extraNumbers = totalNumbers % nChildren;
    int startIdx = 0;

    for (i = 0; i < nChildren; i++) {
        int numbersToSend = numsPerChild + (i < extraNumbers ? 1 : 0);
        for (int j = 0; j < numbersToSend; j++) {
            sprintf(buffer, "%f ", numbers[startIdx++]);
            WriteFile(hChildStdInWr[i], buffer, strlen(buffer), &written, NULL);
        }
        CloseHandle(hChildStdInWr[i]);
    }

    float totalSum = 0, childSum;
    for (i = 0; i < nChildren; i++) {
        while (ReadFile(hChildStdOutRd[i], buffer, sizeof(buffer), &read, NULL) && read != 0) {
            buffer[read] = '\0';
            sscanf(buffer, "%f", &childSum);
            totalSum += childSum;
        }
        CloseHandle(hChildStdOutRd[i]);
    }

    printf("Total sum of squares: %f\n", totalSum);
}
