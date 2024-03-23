#!/bin/bash

gcc -o child_process child_process.c -lrt
gcc -o parent_process parent_process.c -lrt