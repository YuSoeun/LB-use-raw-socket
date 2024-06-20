#ifndef __LB_H__
#define __LB_H__

#define OPT_SIZE 0
#define SERVER_NUM 2
#define CLNT_NUM 10
#define BUF_SIZE 1024

uint32_t lb_addr;
uint16_t lb_port;

typedef enum lb_algo {
    ROUND_ROBIN,
    LEAST_CONNECTION,
    RESOURCE_BASED
} LB_algo;

typedef struct server_info {
    int sock;
    uint32_t server_index;
    struct sockaddr_in saddr;
    uint32_t client_count;
} ServInfo;

typedef struct clnt_info {
    uint32_t server_index;
    struct sockaddr_in saddr;
} ClntInfo;

typedef struct resource {
    uint32_t server_index;
    uint32_t cpu;               // CPU 사용률
    uint32_t memory;            // Memory 사용률
    struct timeval prv_time;
} Resrc;

typedef struct pseudo_header
{
	u_int32_t saddr;
	u_int32_t daddr;
	u_int8_t placeholder;
	u_int8_t  protocol;
	u_int16_t tcplen;
} Pseudo;

unsigned short checksum(__u_short *addr, int len);

struct server_info server_list[SERVER_NUM];
struct server_info clnt_match_serv[CLNT_NUM];
struct resource resource_list[SERVER_NUM];

void set_servers();
void connect_with_servers();
void remove_from_server_list(uint32_t server_index);
int select_server(int algo);
void send_data_to_server(int sock, struct sockaddr_in server_addr, int server_index, char *datagram);

void change_header(char *datagram, int server_index);
void change_header_ack(char *datagram, int server_index);
void change_header_data(char *datagram, int server_index);

static void *get_resource(void * arg);
int is_in_server_list(uint32_t addr);
int get_server_index(uint32_t addr);

void three_way_handshaking_client(int sock, struct sockaddr_in server_addr, int server_index, char *datagram);
void four_way_handshaking_client(int sock, struct sockaddr_in server_addr, int server_index, char *datagram);

#endif