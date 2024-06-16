#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <sys/time.h>
#include <pthread.h>
#include <errno.h>
#include "client_list.h"
#include "lb.h"

int server_count = 0;
int count = 0;
int sock = 0;       // raw socket
int recv_size = 0;
struct sockaddr_in saddr;

ClientList * client_list;
void extract_ip_header(char* buffer);
void extract_tcp_packet(char* buffer);

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
		printf("Usage: %s <Source IP> <Source Port> <LB_ALGORITHM>\n", argv[0]);
		return 1;
	}

    lb_addr = inet_addr(argv[1]);
    lb_port = htons(atoi(argv[2]));
    int algo = (argv[3][0] - '0');

    sock = socket(AF_INET, SOCK_RAW, IPPROTO_TCP);
	if (sock == -1) {
		perror("socket");
        exit(EXIT_FAILURE);
	}

    // Source IP
	saddr.sin_family = AF_INET;
	saddr.sin_port = htons(atoi(argv[2]));
	if (inet_pton(AF_INET, argv[1], &saddr.sin_addr) != 1) {
		perror("Source IP configuration failed\n");
		exit(EXIT_FAILURE);
	}
    
    set_servers();

    // three_way_handshaking, socket option 설정 (headers are included in the packet)
	int one = 1;
	const int *val = &one;
	if (setsockopt(sock, IPPROTO_IP, IP_HDRINCL, val, sizeof(one)) == -1) {
		perror("setsockopt(IP_HDRINCL, 1)");
		exit(EXIT_FAILURE);
	}

    // 서버들과 연결
    connect_with_servers();

    // 서버의 리소스 정보를 받아오는 쓰레드 생성
    int resource_based = 0;
    pthread_t thread_id;
    if (algo == 2) {
        resource_based = 1;
    }
    // pthread_create(&thread_id, NULL, &get_resource, (void *)&resource_based);

    // 서버로 데이터를 전송하는 쓰레드 생성
    // pthread_t send_thread;
    // pthread_create(&send_thread, NULL, send_data_to_server, NULL);
    
    char datagram[BUF_SIZE];
    while (1) {
        recv_size = recvfrom(sock, datagram, BUF_SIZE, 0, NULL, NULL);
        
        // IP 헤더와 TCP 헤더 추출
        struct iphdr *iph = (struct iphdr *)datagram;
        struct tcphdr *tcph = (struct tcphdr *)(datagram + sizeof(struct iphdr));

        // LB로 오는 데이터가 아니라면 continue
        if (iph->daddr != lb_addr && tcph->dest != lb_port)
            continue;

        int server_index = 0;

        // syn이면 클라이언트의 3way handshaking 요청
        if (tcph->syn && !tcph->ack && !tcph->fin) {
            printf("start three-way-handshaking\n");
            server_index = select_server(algo);
            three_way_handshaking_client(sock, server_list[server_index].saddr, server_index, datagram);
        }

        // data request면 server로 전송
        // if (tcph->psh && tcph->ack) {
        //     printf("send data request\n");
        //     send_data_to_server(sock, server_list[server_index].saddr, server_index, datagram);
        // }
        
    //     // fin이면 클라이언트의 4way handshaking 요청
    //     else if (tcph->fin == 1 && !tcph->ack) {
    //         int server_index = select_server(algo);
    //         pthread_t thread_id;
    //         four_way_handshaking_client(server_list[server_index], server_index, datagram);
    //     }
    }

    return 0;
}

void set_servers()
{
    printf("set_servers\n");
    char *ip = "127.0.0.1";

    for (int i = 0; i < SERVER_NUM; i++) {
        server_list[i].server_index = i;
        server_list[i].client_count = 0;
        server_list[i].sock = -1;

        server_list[i].saddr.sin_family = AF_INET;
        server_list[i].saddr.sin_port = htons(8888);
        if (inet_pton(AF_INET, ip, &server_list[i].saddr.sin_addr.s_addr) != 1) {
            perror("Source IP configuration failed\n");
            exit(EXIT_FAILURE);
        }
    }
}

void connect_with_servers()
{
    while (server_count < SERVER_NUM) {
        int tcp_sock;
        ServInfo * cur_svr = &server_list[server_count];
        
        // 소켓 생성 및 초기화 (TCP 소켓으로 변경)
        tcp_sock = socket(AF_INET, SOCK_STREAM, 0); // TCP 소켓
        if (tcp_sock < 0) {
            perror("Socket creation failed");
            exit(EXIT_FAILURE);
        }

        // 서버 주소 설정
        struct sockaddr_in server_addr;
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = cur_svr->saddr.sin_addr.s_addr;
        server_addr.sin_port = cur_svr->saddr.sin_port; // 포트 번호 설정

        // 서버에 연결 시도
        if (connect(tcp_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
            perror("Connection failed");
            exit(EXIT_FAILURE);
        }

        // 서버 정보 설정
        cur_svr->sock = tcp_sock;
        cur_svr->client_count++;
        printf("connected with servers ... %d\n", tcp_sock);

        server_count++;
    }
}


// void remove_from_server_list(uint32_t server_index) {
//     for (int i = 0; i < SERVER_NUM; i++) {
//         if (server_list[i].server_index == server_index) {
//             server_list[i].saddr.sin_addr.s_addr = 0;
//             server_list[i].saddr.sin_port = 0;
//             server_list[i].client_count = 0;
//             break;
//         }
//     }
// }

static void *get_resource(void * arg)
{
    int resource_based = *(int*)arg;
    char datagram[BUF_SIZE];
    int str_len;
    printf("create get_resource thread ...\n");
    printf("resource_based %d \n", resource_based);

    // 모든 서버에게 resource based인지 flag 전송
    for (int i = 0; i < SERVER_NUM; i++) {
        printf("write resource based(%d): %d\n", server_list[i].sock, resource_based);
        if ((str_len = write(server_list[i].sock, &resource_based, sizeof(int))) == 0) {
            perror("write failed in get resource");
        }
    }
    printf("complete write resource based\n");


    while (resource_based == 1) {
        // raw socket에서 resource 정보 받기
        recvfrom(sock, datagram, sizeof(datagram), 0, NULL, NULL);
        struct iphdr *ip = (struct iphdr *)datagram;
        struct tcphdr *tcp = (struct tcphdr *)(datagram + sizeof(struct iphdr));
        
        // 서버에서 온 socket이면
        if (is_in_server_list(ip->saddr) && tcp->psh == 1 && tcp->ack) {
            // 현재 시간에 비해 마지막으로 온 패킷이 얼마나 오래되었는지 측정
            int serv_index = get_server_index(ip->saddr);
            struct timeval now;
            gettimeofday(&now, NULL);
            resource_list[serv_index].prv_time = now;

            if (resource_based == 1) {
                // get_resource_data(datagram);
                double cpu_usage, ram_usage;
                read(server_list[serv_index].sock, &cpu_usage, sizeof(double));
                read(server_list[serv_index].sock, &ram_usage, sizeof(double));

                printf("\n------- server resource -------\n");
                printf("cpu: %.2f%%\n", cpu_usage);
		        printf("mem: %.2f%%\n", ram_usage);
            }
        }
    }

    return NULL;
}

int is_in_server_list(uint32_t addr)
{
    for (int i = 0; i < SERVER_NUM; i++) {
        if (server_list[i].saddr.sin_addr.s_addr == addr) {
            return 1;
        }
    }
    return 0;
}

int get_server_index(uint32_t addr)
{
    for (int i = 0; i < SERVER_NUM; i++) {
        if (server_list[i].saddr.sin_addr.s_addr == addr) {
            return server_list[i].server_index;
        }
    }
    return -1;
}

/* return server index */
int select_server(int algo)
{
    int selected_index = 0;
    
    if (algo == ROUND_ROBIN) {
        selected_index = count % server_count;
        count++;
    } else if (algo == LEAST_CONNECTION) {
        int min_conn = server_list[0].client_count;
        int min_index = 0;
        for (int i = 1; i < server_count; i++) {
            if (server_list[i].client_count < min_conn) {
                min_conn = server_list[i].client_count;
                min_index = i;
            }
        }
        selected_index = min_index;
    } else if (algo == RESOURCE_BASED) {
        int min_resource = resource_list[0].cpu * 1 + resource_list[0].memory * 1;
        int min_index = 0;
        for (int i = 1; i < server_count; i++) {
            int current_resource = resource_list[i].cpu * 1 + resource_list[i].memory * 1;
            if (current_resource < min_resource) {
                min_resource = current_resource;
                min_index = i;
            }
        }
        selected_index = min_index;
    }
    
    return selected_index;
}

void change_header(char *datagram, int server_index)
{
    struct iphdr *ip = (struct iphdr *)datagram;
    struct tcphdr *tcp = (struct tcphdr *)(datagram + (ip->ihl * 4));
    struct sockaddr_in serv_adr = server_list[server_index].saddr;

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
	ip->saddr = ip->saddr;
	ip->daddr = serv_adr.sin_addr.s_addr;

	tcp->source = tcp->source;
	tcp->dest = serv_adr.sin_port;
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

void change_header_ack(char *datagram, int server_index)
{
    struct iphdr *ip = (struct iphdr *)datagram;
    struct tcphdr *tcp = (struct tcphdr *)(datagram + (ip->ihl * 4));
    struct sockaddr_in serv_adr = server_list[server_index].saddr;

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
	ip->saddr = ip->saddr;
	ip->daddr = serv_adr.sin_addr.s_addr;

	tcp->source = tcp->source;
	tcp->dest = serv_adr.sin_port;
	tcp->seq = tcp->seq;
	tcp->ack_seq = tcp->ack_seq;

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

void change_header_data(char *datagram, int server_index)
{
    struct iphdr *ip = (struct iphdr *)datagram;
    struct tcphdr *tcp = (struct tcphdr *)(datagram + (ip->ihl * 4));
    struct sockaddr_in serv_adr = server_list[server_index].saddr;

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
	ip->saddr = ip->saddr;
	ip->daddr = serv_adr.sin_addr.s_addr;

	tcp->source = tcp->source;
	tcp->dest = serv_adr.sin_port;
	tcp->seq = tcp->seq;
	tcp->ack_seq = tcp->ack_seq;

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

	pseudo->saddr = ip->saddr;
	pseudo->daddr = ip->daddr;
	pseudo->placeholder = 0;
	pseudo->protocol = IPPROTO_TCP;
	pseudo->tcplen = htons(recv_size);

    tcp->check = 0;
    ip->check = 0;

	memcpy(pseudo_datagram, pseudo, sizeof(Pseudo));
	memcpy(pseudo_datagram + sizeof(Pseudo), tcp, recv_size);
	
    tcp->check = checksum((__u_short *)pseudo_datagram, sizeof(Pseudo) + recv_size);
	ip->check = checksum((__u_short *)datagram, ip->tot_len);

	// set TCP options
	free(pseudo);
	free(pseudo_datagram);
}

void three_way_handshaking_client(int sock, struct sockaddr_in server_addr, int server_index, char *datagram)
{
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(struct sockaddr_in);

    // SYN
    change_header(datagram, server_index);          // SYN 패킷을 설정하는 사용자 정의 함수
    printf("LB에서 SYN 헤더 바꾼 것\n");
    extract_ip_header(datagram);
	struct iphdr *ip = (struct iphdr*)datagram;
    
    int size;
    if (sendto(sock, datagram, recv_size, 0, (struct sockaddr *)&server_addr, sizeof(struct sockaddr))< 0) {
        perror("sendto failed");
    }
    printf("send size = %d %d\n", size, errno);
    sleep(5);

    // ACK이면
    while (1) {
        if ((recv_size = recvfrom(sock, datagram, BUF_SIZE, 0, NULL, NULL)) < 0) {
            perror("recvfrom failed");
        }
        struct iphdr *ip = (struct iphdr *)datagram;
        struct tcphdr *tcp = (struct tcphdr *)(datagram + sizeof(struct iphdr));

        if (ip->daddr != lb_addr && tcp->dest != lb_port)
            continue;

        if (tcp->syn != 1 && tcp->ack == 1 && tcp->fin != 1 && tcp->rst != 1) {
            change_header_ack(datagram, 0); // ACK 패킷을 설정하는 사용자 정의 함수
            printf("LB에서 ACK 헤더 바꾼 것\n");
            if ((size = sendto(sock, datagram, recv_size, 0, (struct sockaddr *)&server_addr, sizeof(struct sockaddr))) < 0) {
                perror("sendto failed");
            }
            printf("send size = %d %d\n", size, errno);
            break;
        }
   }

//     ClientNode * newnode =  InitNodeInfo(server_index, server_list[server_index].addr, server_list[server_index].port);
//     InsertNode(newnode);
}

// void four_way_handshaking_client(struct server_info server, int server_index, char *datagram) {
//     struct sockaddr_in client_addr;
//     socklen_t addr_len = sizeof(struct sockaddr_in);
//     int client = 0; // 클라이언트 식별자 설정

//     // FIN
//     change_header(datagram); // FIN 패킷 설정
//     if (sendto(server_list[server_index].sock, datagram, strlen(datagram), 0, (struct sockaddr *)&server.addr.saddr.sin_addr.s_addr, sizeof(server.addr.saddr.sin_addr.s_addr)) < 0) {
//         perror("sendto failed");
//     }

//     // ACK
//     while (1) {
//         if (recvfrom(server_list[server_index], datagram, BUF_SIZE, 0, (struct sockaddr *)&client_addr, &addr_len) < 0) {
//             perror("recvfrom failed");
//         }
//         struct iphdr *ip = (struct iphdr *)datagram;
//         struct tcphdr *tcp = (struct tcphdr *)(datagram + sizeof(struct iphdr));

//         if (ip->saddr == server.addr.saddr.sin_addr.s_addr.sin_addr.s_addr && tcp->ack == 1) {
//             change_header(datagram); // ACK 패킷 설정
//             if (sendto(server_list[server_index], datagram, strlen(datagram), 0, (struct sockaddr *)&server.addr.saddr.sin_addr.s_addr, sizeof(server.addr.saddr.sin_addr.s_addr)) < 0) {
//                 perror("sendto failed");
//             }
//             break;
//         }
//     }

//     RemoveNode(client); // 클라이언트 노드 제거 함수 호출
// }

void send_data_to_server(int sock, struct sockaddr_in server_addr, int server_index, char *datagram)
{
    printf("send_data_to_server thread start ...\n");
    // struct sockaddr_in from_addr;
    // socklen_t from_len = sizeof(from_addr);

    // data
    change_header_data(datagram, server_index);
    printf("LB에서 data 헤더 바꾼 것\n");
    extract_ip_header(datagram);
	struct iphdr *ip = (struct iphdr*)datagram;
    
    int size;
    if (sendto(sock, datagram, recv_size, 0, (struct sockaddr *)&server_addr, sizeof(struct sockaddr))< 0) {
        perror("sendto failed");
    }
    printf("send size = %d %d\n", size, errno);

    // 클라이언트 리스트에 소스 IP가 있는지 확인
    // if (Search(list, htos(iph->saddr))) {
    //     change_header(datagram); // 헤더 변경 함수, 구현 필요
        
    //     // 데이터를 매칭되는 서버로 전송
    //     if (sendto(sock, datagram, sizeof(datagram), 0, (struct sockaddr *)server_addr, sizeof(*server_addr)) < 0) {
    //         perror("sendto failed");
    //         return;
    //     }
    // }
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