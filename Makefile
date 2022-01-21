main:
	gcc -g -o main random_graph.c
	./main -s -n 32

solver:
	gcc -g -o main main.c
	./main
	rm main