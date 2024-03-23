Запуск

ahae@linux-virtual:~/hse/task6$ ./linux numbers.txt 7

Скомпилить

ahae@linux-virtual:~/hse/task6$ gcc -o linux linux.c -lpthread


ahae@linux-virtual:~/hse/task6$ gcc -o sp-linux linux.c -lpthread
ahae@linux-virtual:~/hse/task6$ ./sp-linux numbers.txt 893465763298456348756
Invalid number of threads.
ahae@linux-virtual:~/hse/task6$ ./sp-linux numbers-kek.txt 89
Error opening file: No such file or directory
ahae@linux-virtual:~/hse/task6$ cat numbers-broken.txt 
3 2.5 -1.0 4.0ahae@linux-virtual:~/hse/task6$ ./sp-linux numbers-broken.txt 89
Warning: Number of threads is greater than number of numbers. Reducing threads to 4.
Final sum of squares: 32.25
ahae@linux-virtual:~/hse/task6$ cat numbers-with-text.txt 
3.5 kek -1.0 4.0ahae@linux-virtual:~/hse/task6$ ./sp-linux numbers-with-text.txt 89
File must contain at least two numbers
ahae@linux-virtual:~/hse/task6$ ./sp-linux numbers.txt 7
Warning: Number of threads is greater than number of numbers. Reducing threads to 4.
Final sum of squares: 35.50


