#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/sysinfo.h>
#include <sys/times.h>
#include <pthread.h>
#include "cpu.h"

#define BUF_SIZE 1024
void error_handling(char *message);
void three_way_handshaking_client(int sock, struct sockaddr_in lb_adr, char *datagram);
static void *send_resource(void * arg);
void change_header(char *datagram, struct sockaddr_in lb_adr);

int main(int argc, char *argv[])
{
	int serv_sock, lb_sock;
	char message[BUF_SIZE] = "정보를 보내겠다.";
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
	// int is_get_resource = 0;
	// if ((str_len = read(lb_sock, &is_get_resource, sizeof(int))) == 0) {
	// 	perror("read failed in get resource");
	// }
	// printf("is_get_resource %d \n", is_get_resource);
	
	// resource 보내주는 thread 생성
	// if (is_get_resource) {
	// 	pthread_t thread_id;
    // 	pthread_create(&thread_id, NULL, &send_resource, (void *)&lb_sock);
	// }

	// raw socket listen
    int sock = socket(AF_INET, SOCK_RAW, IPPROTO_TCP);
	if (sock == -1) {
		perror("socket");
        exit(EXIT_FAILURE);
	}

    char datagram[BUF_SIZE];
	while (1) {
        int str_len = recvfrom(sock, datagram, BUF_SIZE, 0, (struct sockaddr *)&lb_adr, &lb_adr_sz);
		
		// IP 헤더와 TCP 헤더 추출
        struct iphdr *iph = (struct iphdr *)datagram;
        struct tcphdr *tcph = (struct tcphdr *)(datagram + sizeof(struct iphdr));

		if (iph->saddr != lb_adr.sin_addr.s_addr && tcph->dest != serv_adr.sin_port)
			continue;

		// three way handshaking
		if (tcph->syn == 1 && !tcph->ack) {
			printf("datagram(%d): %s\n", str_len, datagram);
            three_way_handshaking_client(sock, lb_adr, datagram);
        }

		//     // fin이면 클라이언트의 4way handshaking 요청
		//     else if (tcph->fin == 1 && !tcph->ack) {
		//         int server_index = select_server(algo);
		//         pthread_t thread_id;
		//         four_way_handshaking_client(server_list[server_index], server_index, datagram);
		//     }
	}

	close(lb_sock);
	close(serv_sock);
	return 0;
}

void three_way_handshaking_client(int sock, struct sockaddr_in lb_adr, char *datagram) {
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(struct sockaddr_in);

    // SYN
    change_header(datagram, lb_adr); // SYN 패킷을 설정하는 사용자 정의 함수
    if (sendto(sock, datagram, strlen(datagram), 0, (struct sockaddr *)&lb_adr, sizeof(lb_adr)) < 0) {
        perror("sendto failed");
    }

//     // ACK
//     while (1) {
//         if (recvfrom(sock, datagram, BUF_SIZE, 0, (struct sockaddr *)&client_addr, &addr_len) < 0) {
//             perror("recvfrom failed");
//         }
//         struct iphdr *ip = (struct iphdr *)datagram;
//         struct tcphdr *tcp = (struct tcphdr *)(datagram + sizeof(struct iphdr));

//         if (ip->saddr == server_addr.sin_addr.s_addr && tcp->syn == 1 && tcp->ack == 1) {
//             change_header(datagram, lb_adr); // ACK 패킷을 설정하는 사용자 정의 함수
//             if (sendto(sock, datagram, strlen(datagram), 0, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
//                 perror("sendto failed");
//             }
//             break;
//         }
//    }

//     ClientNode * newnode =  InitNodeInfo(server_index, server_list[server_index].addr, server_list[server_index].port);
//     InsertNode(newnode);
}

/* resource 정보(CPU와 RAM 사용률) 보내는 thread */
static void *send_resource(void * arg) {
	struct sysinfo info;
	CpuUsage prev, curr;
	int lb_sock = *(int*)arg;

	while (1) {
		sysinfo(&info);

		get_cpu_usage(&prev);
		sleep(50); // 50초 대기 후 다시 측정
		get_cpu_usage(&curr);

		double cpu_usage = calculate_cpu_usage(&prev, &curr);
		double ram_usage = (double)(info.totalram-info.freeram)/(double)info.totalram * 100.0;
        printf("\n------- server resource -------\n");
		printf("cpu: %.2f%%\n", cpu_usage);
		printf("mem: %.2f%%\n", ram_usage);
		
		write(lb_sock, &cpu_usage, sizeof(double));
		write(lb_sock, &ram_usage, sizeof(double));

		sleep(100);
	}
}

void change_header(char *datagram, struct sockaddr_in lb_adr)
{
    struct iphdr *iph = (struct iphdr *)datagram;
    struct tcphdr *tcph = (struct tcphdr *)(datagram + (iph->ihl * 4));
    
    iph->daddr = server.addr;
    tcph->dest = htons(server.port);
}

void error_handling(char *message)
{
	fputs(message, stderr);
	fputc('\n', stderr);
	exit(1);
}