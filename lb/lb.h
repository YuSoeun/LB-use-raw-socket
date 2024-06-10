#ifndef __LB_H__
#define __LB_H__

#define SERVER_NUM 1
#define BUF_SIZE 1024

typedef enum lb_algo {
    ROUND_ROBIN,
    LEAST_CONNECTION,
    RESOURCE_BASED
} LB_algo;

typedef struct server_info {
    uint32_t server_index;
    int sock;
    uint32_t addr;
    uint16_t port;
    uint32_t client_count;
} ServInfo;

typedef struct resource {
    uint32_t server_index;
    uint32_t cpu;
    uint32_t memory;
    struct timeval prv_time;
} Resrc;

struct server_info server_list[SERVER_NUM];
struct resource resource_list[SERVER_NUM];
uint32_t lb_addr;
uint16_t lb_port;

void set_servers();
void connect_with_servers();
void remove_from_server_list(uint32_t server_index);
struct server_info select_server(int algo);
void * send_data_to_server(void * arg);

static void *get_resource(void * arg);
// void get_resource_data(datagram);
int is_in_server_list(struct server_info server, struct sockaddr_in addr);

void three_way_handshaking_client(int sock, struct sockaddr_in server_addr, int server_index, char *datagram);
void four_way_handshaking_client(struct server_info server, int server_index, char *datagram);

#endif