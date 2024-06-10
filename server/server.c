#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/sysinfo.h>
#include <sys/times.h>
#include "cpu.h"

#define BUF_SIZE 1024
void error_handling(char *message);

int main(int argc, char *argv[])
{
	int serv_sock, lb_sock;
	char message[BUF_SIZE] = "정보를 보내겠다.";
	int str_len, i;
	int option = 1;
	
	struct sockaddr_in serv_adr;
	struct sockaddr_in lb_adr;
	socklen_t lb_adr_sz;
	struct sysinfo info;

	CpuUsage prev, curr;
	
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
	if ((str_len = read(lb_sock, &is_get_resource, sizeof(int))) == 0) {
		perror("read failed in get resource");
	}
	printf("is_get_resource %d \n", is_get_resource);
	
	while (1) {
		// resource 정보 보내기
		sysinfo(&info);

		get_cpu_usage(&prev);
		sleep(1); // 1초 대기 후 다시 측정
		get_cpu_usage(&curr);

		double cpu_usage = calculate_cpu_usage(&prev, &curr);
		double ram_usage = (double)(info.totalram-info.freeram)/(double)info.totalram * 100.0;
        printf("\n------- server resource -------\n");
		printf("cpu: %.2f%%\n", cpu_usage);
		printf("mem: %.2f%%\n", ram_usage);
		
		if (is_get_resource) {
			write(lb_sock, &cpu_usage, sizeof(double));
			write(lb_sock, &ram_usage, sizeof(double));
		}
		sleep(100);
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