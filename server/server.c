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
#include <errno.h>
#include "cpu.h"

#define BUF_SIZE 1024

struct sockaddr_in serv_adr;

void error_handling(char *message);
void three_way_handshaking_client(int sock, struct sockaddr_in lb_adr, char *datagram);
static void *send_resource(void * arg);
void change_header(char *datagram, struct sockaddr_in lb_adr);
void extract_ip_header(char* buffer);
void extract_tcp_packet(char* buffer);

#define OPT_SIZE 0

unsigned short checksum(__u_short *addr, int len);
typedef struct pseudo_header
{
	u_int32_t saddr;
	u_int32_t daddr;
	u_int8_t placeholder;
	u_int8_t  protocol;
	u_int16_t tcplen;
} Pseudo;

// checksum function which returns unsigned short value 
unsigned short checksum(__u_short *addr, int len)
{
        int sum = 0;				// 체크섬 값 계산
        int left_len = len;			// 아직 처리되지 않은 데이터의 길이
        __u_short *cur = addr;		// 데이터의 현재 위치
        __u_short result = 0;		// 최종적으로 반환될 체크섬 값
 
        while (left_len > 1) {
			sum += *cur;  cur++;

			left_len -= 2;			// unsigned short여서 2빼기
        }
	
		// 데이터 길이가 홀수일 때 마지막 바이트 처리
        if (left_len == 1) {
			result = *(__u_char *)cur ;
			sum += result;
        }

		// 32비트 합산 값을 16비트로 축소 (오버플로우 처리를 위해)
       	sum = (sum >> 16) + (sum & 0xffff);       // masking
        result = ~sum;

        return(result);
}

int main(int argc, char *argv[])
{
	int serv_sock, lb_sock;
	char message[BUF_SIZE] = "정보를 보내겠다.";
	int str_len, i;
	int option = 1;
	
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

	// Source IP
	struct sockaddr_in saddr;
	saddr.sin_family = AF_INET;
	saddr.sin_port = htons(atoi(argv[1]));
	if (inet_pton(AF_INET, "127.0.0.1", &saddr.sin_addr) != 1) {
		perror("Source IP configuration failed\n");
		exit(EXIT_FAILURE);
	}

	// three_way_handshaking, socket option 설정 (headers are included in the packet)
	int one = 1;
	const int *val = &one;
	if (setsockopt(sock, IPPROTO_IP, IP_HDRINCL, val, sizeof(one)) == -1) {
		perror("setsockopt(IP_HDRINCL, 1)");
		exit(EXIT_FAILURE);
	}

    char datagram[BUF_SIZE];
	while (1) {
        int str_len = recvfrom(sock, datagram, BUF_SIZE, 0, NULL, NULL);
		
		// IP 헤더와 TCP 헤더 추출
        struct iphdr *ip = (struct iphdr *)datagram;
        struct tcphdr *tcp = (struct tcphdr *)(datagram + sizeof(struct iphdr));

		if (ip->saddr != lb_adr.sin_addr.s_addr && tcp->dest != serv_adr.sin_port)
			continue;

		// three way handshaking
		if (tcp->syn == 1 && !tcp->ack) {
			printf("datagram(%d): %s\n", str_len, datagram);
            three_way_handshaking_client(sock, lb_adr, datagram);
        }

		//     // fin이면 클라이언트의 4way handshaking 요청
		//     else if (tcp->fin == 1 && !tcp->ack) {
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
	struct iphdr *ip = (struct iphdr*)datagram;

    // SYNACK send
    change_header(datagram, lb_adr);
    if (sendto(sock, datagram, ip->tot_len, 0, (struct sockaddr *)&lb_adr, sizeof(lb_adr)) < 0) {
        perror("sendto failed");
    }

    // ACK
    while (1) {
        if (recvfrom(sock, datagram, BUF_SIZE, 0, (struct sockaddr *)&client_addr, &addr_len) < 0) {
            perror("recvfrom failed");
        }

		struct iphdr *ip = (struct iphdr *)datagram;
        struct tcphdr *tcp = (struct tcphdr *)(datagram + sizeof(struct iphdr));

		if (ip->saddr != lb_adr.sin_addr.s_addr && tcp->dest != serv_adr.sin_port)
			continue;

        // if (tcp->syn != 1 && tcp->ack == 1) {
    	// 	extract_ip_header(datagram);
        //     change_header(datagram, lb_adr); // ACK 패킷을 설정하는 사용자 정의 함수
        //     if (sendto(sock, datagram, strlen(datagram), 0, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        //         perror("sendto failed");
        //     }
        //     break;
        // }
   }

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
	struct iphdr *ip = (struct iphdr *)datagram;
    struct tcphdr *tcp = (struct tcphdr *)(datagram + (ip->ihl * 4));

    Pseudo *pseudo = (Pseudo *)calloc(sizeof(Pseudo), sizeof(char));
	char *pseudo_datagram = (char *)malloc(sizeof(Pseudo) + sizeof(struct tcphdr) + OPT_SIZE);
	if (pseudo == NULL) {
		perror("malloc: ");
		exit(EXIT_FAILURE);
	}

	ip->version = 4;
	ip->ihl = 5;
	ip->tos = 0;
	ip->tot_len = sizeof(struct iphdr) + sizeof(struct tcphdr) + OPT_SIZE;

	ip->id = ip->id;
	ip->frag_off = htons(1 << 14);
	ip->ttl = 64;
	ip->protocol = IPPROTO_TCP;
	ip->daddr = ip->saddr;
	ip->saddr = lb_adr.sin_addr.s_addr;

	tcp->dest = tcp->source;
	tcp->source = lb_adr.sin_port;
	tcp->seq = tcp->seq;
	tcp->ack_seq = htonl(0);

	tcp->doff = 5;
	tcp->res1 = 0;
	tcp->urg = 0;
	tcp->ack = 0;
	tcp->psh = 0;
	tcp->rst = 0;
	tcp->syn = 1;
	tcp->fin = 0;

	tcp->window = htons(5840);		// window size
	tcp->urg_ptr = 0;

	pseudo->saddr = ip->saddr;
	pseudo->daddr = ip->daddr;
	pseudo->placeholder = 0;
	pseudo->protocol = IPPROTO_TCP;
	pseudo->tcplen = htons(sizeof(struct tcphdr) + OPT_SIZE);

    tcp->check = 0;
    ip->check = 0;
    
	memcpy(pseudo_datagram, pseudo, sizeof(Pseudo));
	memcpy(pseudo_datagram + sizeof(Pseudo), tcp, sizeof(struct tcphdr) + OPT_SIZE);
	
    tcp->check = checksum((__u_short *)pseudo_datagram, sizeof(Pseudo) + sizeof(struct tcphdr) + OPT_SIZE);
	ip->check = checksum((__u_short *)datagram, ip->tot_len);

	// set TCP options
	free(pseudo);
	free(pseudo_datagram);
}

void error_handling(char *message)
{
	fputs(message, stderr);
	fputc('\n', stderr);
	exit(1);
}

// IP 헤더와 내용 추출
void extract_ip_header(char* buffer) {
    // 네트워크 주소 변환 사용해야함 (network byte 순서 -> host byte 순서)
    struct iphdr *ip = (struct iphdr*)(buffer);
    printf("-------------------------------------------- \n");
    printf("IP Header\n");
    printf("   |-Source IP               : %s\n", inet_ntoa(*(struct in_addr*)&ip->saddr));
    printf("   |-Destination IP          : %s\n", inet_ntoa(*(struct in_addr*)&ip->daddr));
    printf("   |-Protocol                : %d\n", ip->protocol);

    if (ip->protocol == IPPROTO_TCP) {
	extract_tcp_packet(buffer);
    } else {
        printf("Not TCP Packet\n");
    }
}

// TCP Packet과 내용 추출
void extract_tcp_packet(char* buffer) {
    struct tcphdr *tcp = (struct tcphdr*)(buffer + sizeof(struct iphdr));
    printf("-------------------------------------------- \n");
    printf("TCP Packet\n");
    printf("   |-Source Port             : %d\n", ntohs(*(uint16_t *)&tcp->th_sport));
    printf("   |-Destination Port        : %d\n", ntohs(*(uint16_t *)&tcp->th_dport));
    printf("   |-Sequence Number         : %d\n", ntohs(*(uint16_t *)&tcp->th_seq));
    printf("   |-Acknowledge Number      : %d\n", ntohs(*(uint16_t *)&tcp->th_ack));

    printf("   |-Flags                   :");
    if (tcp->fin)  printf(" FIN");
    if (tcp->syn)  printf(" SYN");
    if (tcp->rst)  printf(" RST");
    if (tcp->psh)  printf(" PSH");
    if (tcp->ack)  printf(" ACK");
    if (tcp->urg)  printf(" URG");
    printf("\n");
}