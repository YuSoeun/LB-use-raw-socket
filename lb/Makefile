all: lb.out

lb.out: lb.o client_list.o
	gcc -o lb.out lb.o client_list.o

lb.o: lb.c lb.h
	gcc -c -o lb.o lb.c -lpthread

client_list.o: client_list.c client_list.h
	gcc -c -o client_list.o client_list.c

clean:
	rm *.out *.o