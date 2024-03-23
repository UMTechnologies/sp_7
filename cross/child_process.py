import numpy as np


def compute_sum_of_squares_pipe(data_segment, results_queue):
    result = sum(x**2 for x in data_segment)
    results_queue.put(result)


def compute_sum_of_squares_shm(data_segment, global_sum, lock):
    result = sum(x ** 2 for x in data_segment)
    with lock:
        global_sum[0] += result
