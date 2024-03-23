import sys
import numpy as np
import threading
import queue
from child_process import compute_sum_of_squares_pipe, compute_sum_of_squares_shm
from threading import Lock


def main(file_name, num_threads, ipc_method):
    try:
        with open(file_name, 'r') as f:
            numbers = np.array([float(x) for x in f.read().split()], dtype=np.float64)
    except FileNotFoundError:
        print("Unable to open the file")
        return
    except ValueError:
        print("The file must contain real numbers.")
        return

    if len(numbers) < 2:
        print("The file must contain at least 2 numbers.")
        return

    M = len(numbers)
    N = num_threads
    sizes = [M // N] * N
    for i in range(M % N):
        sizes[i] += 1

    starts = [sum(sizes[:i]) for i in range(N)]
    ends = [start + size for start, size in zip(starts, sizes)]

    threads = []
    results_queue = queue.Queue()
    if ipc_method == 'pipe':
        for i in range(N):
            data_segment = numbers[starts[i]:ends[i]]
            thread = threading.Thread(target=compute_sum_of_squares_pipe, args=(data_segment, results_queue))
            threads.append(thread)
            thread.start()

        total_sum = sum(results_queue.get() for _ in range(N))
    elif ipc_method == 'shm':
        global_sum = [0]
        lock = Lock()
        for i in range(N):
            data_segment = numbers[starts[i]:ends[i]]
            thread = threading.Thread(target=compute_sum_of_squares_shm, args=(data_segment, global_sum, lock))
            threads.append(thread)
            thread.start()

        for thread in threads:
            thread.join()
        total_sum = global_sum[0]
    else:
        print("Invalid IPC method. Please use 'shm' for shared memory or 'pipe' for pipes.")
        return

    print(f"Total sum of squares: {total_sum}")


if __name__ == "__main__":
    if len(sys.argv) != 4:
        print("Incorrect usage. Expected format: python parent_process.py <filename> <number_of_children> <ipc_method>")
        sys.exit(1)
    filename = sys.argv[1]
    n_threads = int(sys.argv[2])
    ipc = sys.argv[3]
    main(filename, n_threads, ipc)
