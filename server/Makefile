all: server.out

server.out: server.o cpu.o
	gcc -o server.out server.o cpu.o

server.o: server.c
	gcc -c -o server.o server.c -lpthread

cpu.o: cpu.c cpu.h
	gcc -c -o cpu.o cpu.c -lpthread

clean:
	rm *.out *.o