#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include "client.h"

// Before run this code, execute the command below 
// $ sudo iptables -A OUTPUT -p tcp --tcp-flags RST RST -j DROP

struct sockaddr_in saddr;

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
	if (argc != 4) {
		printf("Usage: %s <Source IP> <Destination IP> <Destination Port>\n", argv[0]);
		return 1;
	}

	srand(time(NULL));

	int sock = socket(AF_INET, SOCK_RAW, IPPROTO_TCP);
	if (sock == -1) {
		perror("socket");
        exit(EXIT_FAILURE);
	}

	// Source IP
	saddr.sin_family = AF_INET;
	saddr.sin_port = htons(rand() % 65535); // random client port
	if (inet_pton(AF_INET, argv[1], &saddr.sin_addr) != 1) {
		perror("Source IP configuration failed\n");
		exit(EXIT_FAILURE);
	}

	// Destination IP and Port 
	struct sockaddr_in daddr;
	daddr.sin_family = AF_INET;
	daddr.sin_port = htons(atoi(argv[3]));
	if (inet_pton(AF_INET, argv[2], &daddr.sin_addr) != 1) {
		perror("Destination IP and Port configuration failed");
		exit(EXIT_FAILURE);
	}

	// three_way_handshaking, socket option 설정 (headers are included in the packet)
	int one = 1;
	const int *val = &one;
	if (setsockopt(sock, IPPROTO_IP, IP_HDRINCL, val, sizeof(one)) == -1) {
		perror("setsockopt(IP_HDRINCL, 1)");
		exit(EXIT_FAILURE);
	}
	struct tcphdr *tcp = (struct tcphdr *)malloc(sizeof(struct tcphdr));
	memcpy(tcp, three_way_handshaking(sock, saddr, daddr), sizeof(struct tcphdr));
	char message[BUF_SIZE];
	// Data transfer 
	uint32_t next_seq = tcp->seq;
	uint32_t next_ack = tcp->ack_seq;
	while (1)
	{
		memset(message, 0, BUF_SIZE);
		fputs("Input message(Q to quit): ", stdout);
		fgets(message, BUF_SIZE, stdin);
		message[strlen(message)] = '\0';
		
		if (!strcmp(message,"q\n") || !strcmp(message,"Q\n"))
			break;

		data_transfer(sock, saddr, daddr, message, &next_seq, &next_ack);
	}

	close(sock);
	return 0;
}

void* three_way_handshaking(int sock, struct sockaddr_in saddr, struct sockaddr_in daddr)
{
	// TCP Three-way Handshaking
	// Step 1. Send SYN
	printf("Step 1. Send SYN (no need to use TCP options)\n");

	char *datagram = (char *)calloc(sizeof(Pseudo) + sizeof(struct tcphdr) + sizeof(struct iphdr), sizeof(char));
	struct iphdr *ip = (struct iphdr*)datagram;
	struct tcphdr *tcp = (struct tcphdr *)(datagram + sizeof(struct iphdr));

	set_syn_packet(ip, tcp, saddr, daddr, datagram);
	
	int size;
	socklen_t addr_size = sizeof(struct sockaddr);
	if ((size = sendto(sock, datagram, ip->tot_len, 0, (struct sockaddr*)&daddr, addr_size)) < 0) {
		perror("Could not send SYN data");
	}
	extract_ip_header(datagram);
	printf("send size: %d\n", size);

	// Step 2. Receive SYN-ACK
	printf("\nStep 2. Receive SYN-ACK\n");
	char message[BUF_SIZE];
	struct tcphdr *recv_tcp = (struct tcphdr *)malloc(sizeof(struct tcphdr));
	while(1) {
		if ((size = recvfrom(sock, message, BUF_SIZE, 0, NULL, NULL)) < 0) {
			perror("Receive error");
		}

		if (ip->daddr != saddr.sin_addr.s_addr)
			continue;

		recv_tcp = (struct tcphdr*)(message + sizeof(struct iphdr));
		if (tcp->source == recv_tcp->th_dport && recv_tcp->syn && recv_tcp->ack && !recv_tcp->psh)
			break;
	}
	extract_ip_header(message);
	printf("receive size: %d\n", size);

	// Step 3. Send ACK
	printf("\nStep 3. Send ACK \n");
	uint32_t seq = tcp->seq;
	uint16_t id = ip->id;
	datagram = memset(datagram, 0, sizeof(Pseudo) + sizeof(struct tcphdr) + sizeof(struct iphdr));
	set_ack_packet(ip, tcp, daddr, datagram, seq, recv_tcp->th_seq, id);

	if ((size = sendto(sock, datagram, ip->tot_len, 0, (struct sockaddr*)&daddr, addr_size)) < 0) {
		perror("Could not send ACK data");
	}
	extract_ip_header(datagram);
	printf("send size: %d\n", size);
	
	tcp = (struct tcphdr *)(datagram + sizeof(struct iphdr));
	return (void *)tcp;
}

void data_transfer(int sock, struct sockaddr_in saddr, struct sockaddr_in daddr, char *message, uint32_t *next_seq, uint32_t *next_ack)
{
		// Step 4. Send an application message (with PSH and ACK flag)! 
		printf("\nStep 4. Send an application message (with PSH and ACK flag)!\n");
		
		socklen_t addr_size = sizeof(struct sockaddr);
		char *datagram = (char *)calloc(sizeof(Pseudo) + sizeof(struct tcphdr) + sizeof(struct iphdr) + sizeof(message), sizeof(char));
		struct iphdr *ip = (struct iphdr*)datagram;
		struct tcphdr *tcp = (struct tcphdr *)(datagram + sizeof(struct iphdr));
		struct iphdr *recv_ip = (struct iphdr *)malloc(sizeof(struct iphdr));
		struct tcphdr *recv_tcp = (struct tcphdr *)malloc(sizeof(struct tcphdr));
		uint16_t id = ip->id;

		set_data_packet(ip, tcp, saddr, daddr, datagram, message, next_seq, next_ack, id);
		extract_ip_header(datagram);
		
		int size = 0;
		if ((size = sendto(sock, datagram, ip->tot_len, 0, (struct sockaddr*)&daddr, addr_size)) < 0) {
			perror("Could not send ACK data");
		}
		
		// Step 5. Receive ACK
		printf("\nStep 5. Receive ACK\n");
		recv_tcp = memset(recv_tcp, 0, sizeof(struct tcphdr));
		recv_ip = memset(recv_ip, 0, sizeof(struct iphdr));
		while(1) {
			if ((size = recvfrom(sock, message, BUF_SIZE, 0, NULL, NULL)) < 0) {
				perror("Receive error");
			}

			recv_ip = (struct iphdr*)datagram;
			recv_tcp = (struct tcphdr*)(message + sizeof(struct iphdr));

			if (ip->daddr != saddr.sin_addr.s_addr || tcp->source != recv_tcp->th_dport)
				continue;

			if (recv_tcp->ack && !recv_tcp->syn && !recv_tcp->psh) {
				extract_ip_header(message);
				break;
			}
		}

		// char data[BUF_SIZE];
		// strcpy(data, message + sizeof(struct iphdr) + sizeof(struct tcphdr));

		extract_ip_header(message);
		// printf("receive size: %d\n", size);
		// printf("receive msg: %s\n", data);

		*next_seq = recv_tcp->ack_seq;
		*next_ack = recv_tcp->seq;
}


void set_syn_packet(struct iphdr *ip, struct tcphdr *tcp, struct sockaddr_in saddr, struct sockaddr_in daddr, char *datagram)
{
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

	ip->id = htons(rand() % 65535);			// 이것도 알아보기 -> 하나씩 증가하는걸로 구현
	ip->frag_off = htons(1 << 14);
	ip->ttl = 64;
	ip->protocol = IPPROTO_TCP;
	ip->saddr = saddr.sin_addr.s_addr;
	ip->daddr = daddr.sin_addr.s_addr;

	tcp->source = saddr.sin_port;
	tcp->dest = daddr.sin_port;
	tcp->seq = htonl(rand() % 4294967295);	// ISN 설정 가능 범위
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

	pseudo->saddr = saddr.sin_addr.s_addr;
	pseudo->daddr = daddr.sin_addr.s_addr;
	pseudo->placeholder = 0;
	pseudo->protocol = IPPROTO_TCP;
	pseudo->tcplen = htons(sizeof(struct tcphdr) + OPT_SIZE);

	memcpy(pseudo_datagram, pseudo, sizeof(Pseudo));
	memcpy(pseudo_datagram + sizeof(Pseudo), tcp, sizeof(struct tcphdr) + OPT_SIZE);
	tcp->check = checksum((__u_short *)pseudo_datagram, sizeof(Pseudo) + sizeof(struct tcphdr) + OPT_SIZE);
	ip->check = checksum((__u_short *)datagram, ip->tot_len);

	// set TCP options
	free(pseudo);
	free(pseudo_datagram);
}

void set_ack_packet(struct iphdr *ip, struct tcphdr *tcp, struct sockaddr_in daddr, char *datagram, uint32_t seq, uint32_t ack_seq, uint16_t id)
{
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

	ip->id = htons(ntohs(id) + 1);			// 하나씩 증가하는걸로 구현
	ip->frag_off = htons(1 << 14);
	ip->ttl = 64;
	ip->protocol = IPPROTO_TCP;
	ip->daddr = daddr.sin_addr.s_addr;
	ip->saddr = saddr.sin_addr.s_addr;

	tcp->dest = daddr.sin_port;
	tcp->source = saddr.sin_port;
	tcp->seq = htonl(ntohl(seq) + 1);
	tcp->ack_seq = htonl(ntohl(ack_seq) + 1);

	tcp->doff = 5;
	tcp->res1 = 0;
	tcp->urg = 0;
	tcp->ack = 1;
	tcp->psh = 0;
	tcp->rst = 0;
	tcp->syn = 0;
	tcp->fin = 0;

	tcp->window = htons(5840);		// window size
	tcp->urg_ptr = 0;

	pseudo->saddr = saddr.sin_addr.s_addr;
	pseudo->daddr = daddr.sin_addr.s_addr;
	pseudo->placeholder = 0;
	pseudo->protocol = IPPROTO_TCP;
	pseudo->tcplen = htons(sizeof(struct tcphdr) + OPT_SIZE);

	tcp->check = 0;
	ip->check = 0;

	memcpy(pseudo_datagram, pseudo, sizeof(Pseudo));
	memcpy(pseudo_datagram + sizeof(Pseudo), tcp, sizeof(struct tcphdr));
	tcp->check = checksum((__u_short *)pseudo_datagram, sizeof(Pseudo) + sizeof(struct tcphdr) + OPT_SIZE);
	ip->check = checksum((__u_short *)datagram, ip->tot_len);

	free(pseudo);
	free(pseudo_datagram);
}

void set_data_packet(struct iphdr *ip, struct tcphdr *tcp, struct sockaddr_in saddr, struct sockaddr_in daddr, char *datagram, char* msg, uint32_t* seq, uint32_t* ack_seq, uint16_t id)
{
	int data_len = strlen(msg)+1;
	printf("data_len: %d\n", data_len);
	memcpy(datagram + sizeof(struct iphdr) + sizeof(struct tcphdr) + OPT_SIZE, msg, data_len);
	Pseudo *pseudo = (Pseudo *)calloc(sizeof(Pseudo), sizeof(char));
	char *pseudo_datagram = (char *)malloc(sizeof(Pseudo) + sizeof(struct tcphdr) + OPT_SIZE + data_len);
	if (pseudo == NULL) {
		perror("malloc: ");
		exit(EXIT_FAILURE);
	}

	ip->version = 4;
	ip->ihl = 5;
	ip->tos = 0;
	ip->tot_len = sizeof(struct iphdr) + sizeof(struct tcphdr) + OPT_SIZE + data_len;

	ip->id = htons(ntohs(id) + 1);			// 하나씩 증가하는걸로 구현
	ip->frag_off = htons(1 << 14);
	ip->ttl = 64;
	ip->protocol = IPPROTO_TCP;
	ip->saddr = saddr.sin_addr.s_addr;
	ip->daddr = daddr.sin_addr.s_addr;

	tcp->source = saddr.sin_port;
	tcp->dest = daddr.sin_port;
	tcp->seq = *seq;
	tcp->ack_seq = *ack_seq;

	tcp->doff = 5;
	tcp->res1 = 0;
	tcp->urg = 0;
	tcp->ack = 1;
	tcp->psh = 1;
	tcp->rst = 0;
	tcp->syn = 0;
	tcp->fin = 0;

	tcp->window = htons(5840);		// window size
	tcp->urg_ptr = 0;

	pseudo->saddr = saddr.sin_addr.s_addr;
	pseudo->daddr = daddr.sin_addr.s_addr;
	pseudo->placeholder = 0;
	pseudo->protocol = IPPROTO_TCP;
	pseudo->tcplen = htons(sizeof(struct tcphdr) + OPT_SIZE + data_len);

	memcpy(pseudo_datagram, pseudo, sizeof(Pseudo));
	memcpy(pseudo_datagram + sizeof(Pseudo), tcp, sizeof(struct tcphdr) + OPT_SIZE + data_len);
	tcp->check = checksum((__u_short *)pseudo_datagram, sizeof(Pseudo) + sizeof(struct tcphdr) + OPT_SIZE + data_len);
	ip->check = checksum((__u_short *)datagram, ip->tot_len);

	free(pseudo);
	free(pseudo_datagram);
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