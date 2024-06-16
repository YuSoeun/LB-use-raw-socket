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
#include "client_list.h"
#include "lb.h"

int server_count = 0;
int count = 0;
int sock = 0;       // raw socket

ClientList * client_list;

int main(int argc, char *argv[])
{
    if (argc != 4) {
		printf("Usage: %s <Source IP> <Source Port> <LB_ALGORITHM>\n", argv[0]);
		return 1;
	}

    unsigned long lb_addr = inet_addr(argv[1]);
    unsigned short lb_port = htons(atoi(argv[2]));
    int algo = (argv[3][0] - '0');

    sock = socket(AF_INET, SOCK_RAW, IPPROTO_TCP);
	if (sock == -1) {
		perror("socket");
        exit(EXIT_FAILURE);
	}

    // Source IP
	struct sockaddr_in saddr;
	saddr.sin_family = AF_INET;
	saddr.sin_port = htons(rand() % 65535);
	if (inet_pton(AF_INET, argv[1], &saddr.sin_addr) != 1) {
		perror("Source IP configuration failed\n");
		exit(EXIT_FAILURE);
	}

    // 서버들과 연결
    set_servers();
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
        recvfrom(sock, datagram, BUF_SIZE, 0, NULL, NULL);
        
        // IP 헤더와 TCP 헤더 추출
        struct iphdr *iph = (struct iphdr *)datagram;
        struct tcphdr *tcph = (struct tcphdr *)(datagram + sizeof(struct iphdr));

        // LB로 오는 데이터가 아니라면 continue
        if (iph->daddr != lb_addr && tcph->dest != lb_port)
            continue;

        // syn이면 클라이언트의 3way handshaking 요청
        if (tcph->syn == 1 && !tcph->ack) {
            printf("start three-way-handshaking\n");
            int server_index = select_server(algo);
            three_way_handshaking_client(sock, server_list[server_index], server_index, datagram);
        }
        
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

    for (int i = 0; i < SERVER_NUM; i++) {
        server_list[i].server_index = i;
        server_list[i].client_count = 0;
        server_list[i].addr = inet_addr("127.0.0.1");
        server_list[i].port = htons(8888);
        server_list[i].sock = -1;
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
        server_addr.sin_addr.s_addr = cur_svr->addr;
        server_addr.sin_port = cur_svr->port; // 포트 번호 설정

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
//             server_list[i].addr = 0;
//             server_list[i].port = 0;
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
        if (server_list[i].addr == addr) {
            return 1;
        }
    }
    return 0;
}

int get_server_index(uint32_t addr)
{
    for (int i = 0; i < SERVER_NUM; i++) {
        if (server_list[i].addr == addr) {
            return server_list[i].server_index;
        }
    }
    return -1;
}

/* return server index */
int select_server(int algo)
{
    int selected_index;
    
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
    struct iphdr *iph = (struct iphdr *)datagram;
    struct tcphdr *tcph = (struct tcphdr *)(datagram + (iph->ihl * 4));
    
    struct server_info server = server_list[server_index];
    iph->daddr = server.addr;
    tcph->dest = htons(server.port);
}

void three_way_handshaking_client(int sock, struct server_info server_addr, int server_index, char *datagram)
{
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(struct sockaddr_in);

    // SYN
    change_header(datagram, server_index);          // SYN 패킷을 설정하는 사용자 정의 함수
    if (sendto(sock, datagram, strlen(datagram), 0, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
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
//             change_header(datagram); // ACK 패킷을 설정하는 사용자 정의 함수
//             if (sendto(sock, datagram, strlen(datagram), 0, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
//                 perror("sendto failed");
//             }
//             break;
//         }
 
//    }

//     ClientNode * newnode =  InitNodeInfo(server_index, server_list[server_index].addr, server_list[server_index].port);
//     InsertNode(newnode);
}

// void four_way_handshaking_client(struct server_info server, int server_index, char *datagram) {
//     struct sockaddr_in client_addr;
//     socklen_t addr_len = sizeof(struct sockaddr_in);
//     int client = 0; // 클라이언트 식별자 설정

//     // FIN
//     change_header(datagram); // FIN 패킷 설정
//     if (sendto(server_list[server_index].sock, datagram, strlen(datagram), 0, (struct sockaddr *)&server.addr, sizeof(server.addr)) < 0) {
//         perror("sendto failed");
//     }

//     // ACK
//     while (1) {
//         if (recvfrom(server_list[server_index], datagram, BUF_SIZE, 0, (struct sockaddr *)&client_addr, &addr_len) < 0) {
//             perror("recvfrom failed");
//         }
//         struct iphdr *ip = (struct iphdr *)datagram;
//         struct tcphdr *tcp = (struct tcphdr *)(datagram + sizeof(struct iphdr));

//         if (ip->saddr == server.addr.sin_addr.s_addr && tcp->ack == 1) {
//             change_header(datagram); // ACK 패킷 설정
//             if (sendto(server_list[server_index], datagram, strlen(datagram), 0, (struct sockaddr *)&server.addr, sizeof(server.addr)) < 0) {
//                 perror("sendto failed");
//             }
//             break;
//         }
//     }

//     RemoveNode(client); // 클라이언트 노드 제거 함수 호출
// }

void * send_data_to_server(void * arg)
{
    printf("send_data_to_server thread start ...\n");
    char datagram[4096];
    struct sockaddr_in from_addr;
    socklen_t from_len = sizeof(from_addr);

    // 데이터 수신
    // if (recvfrom(sock, datagram, sizeof(datagram), 0, (struct sockaddr *)&from_addr, &from_len) < 0) {
    //     perror("recvfrom failed");
    //     return;
    // }

    struct iphdr *iph = (struct iphdr *)datagram;

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