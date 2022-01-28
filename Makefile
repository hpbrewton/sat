main:
	gcc -g -o main main.c
	./main ex4.cnf
	rm main

debug:
	gcc -g -o main main.c
	lldb main