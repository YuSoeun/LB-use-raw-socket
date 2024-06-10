#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define BUF_SIZE 1024
void error_handling(char *message);

int main(int argc, char *argv[])
{
	int serv_sock, lb_sock;
	char message[BUF_SIZE];
	int str_len, i;
	int option = 1;
	
	struct sockaddr_in serv_adr;
	struct sockaddr_in lb_adr;
	socklen_t lb_adr_sz;
	
	if (argc != 2) {
		printf("Usage : %s <port>\n", argv[0]);
		exit(1);
	}
	
	serv_sock = socket(PF_INET, SOCK_STREAM, 0);   
	if (serv_sock == -1)
		error_handling("socket() error");

	setsockopt(serv_sock, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option) );
	
	memset(&serv_adr, 0, sizeof(serv_adr));
	serv_adr.sin_family = AF_INET;
	serv_adr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_adr.sin_port = htons(atoi(argv[1]));

	if (bind(serv_sock, (struct sockaddr*)&serv_adr, sizeof(serv_adr)) == -1)
		error_handling("bind() error");
	
	if (listen(serv_sock, 5) == -1)
		error_handling("listen() error");
	
	lb_adr_sz = sizeof(lb_adr);

	lb_sock = accept(serv_sock, (struct sockaddr*)&lb_adr, &lb_adr_sz);
	if (lb_sock == -1)
		error_handling("accept() error");
	else
		printf("Connected client %d \n", i+1);
	
	// get resource 알고리즘을 사용하는 LB인지 받기
	int is_get_resource = 0;
	if ((str_len = read(lb_sock, is_get_resource, sizeof(int))) == 0) {
		perror("read failed in get resource");
	}
	
	while (1) {
		// resource 정보 보내기
		write(1, message, str_len);
		sleep(1000);

		if (is_get_resource) {
			write(1, message, str_len);
		}
	}

	close(lb_sock);

	close(serv_sock);
	return 0;
}

void error_handling(char *message)
{
	fputs(message, stderr);
	fputc('\n', stderr);
	exit(1);
}