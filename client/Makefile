all: client.out

client.out: client.o
	gcc -o client.out client.o

client.o: fileup_clnt.c
	gcc -c -o client.o fileup_clnt.c

clean:
	rm *.out *.o