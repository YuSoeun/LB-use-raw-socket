#ifndef __CLIENT_H__
#define __CLIENT_H__

#define BUF_SIZE 4096
#define OPT_SIZE 0

// 	가상 헤더는 TCP 헤더와 함께 사용되며, 주로 송신 및 수신 IP 주소, 
// 	프로토콜 번호(IPPROTO_TCP), 그리고 TCP 세그먼트의 길이를 포함
typedef struct pseudo_header
{
	u_int32_t saddr;
	u_int32_t daddr;
	u_int8_t placeholder;
	u_int8_t  protocol;
	u_int16_t tcplen;
} Pseudo;

unsigned short checksum(__u_short *addr, int len);
void extract_ip_header(char* buffer);
void extract_tcp_packet(char* buffer);
void set_syn_option(char * datagram);
void set_ack_option(char * datagram);

void set_syn_packet(struct iphdr *ip, struct tcphdr *tcp, struct sockaddr_in saddr, struct sockaddr_in daddr, char *datagram);
void set_ack_packet(struct iphdr *ip, struct tcphdr *tcp, struct sockaddr_in daddr, char *datagram, uint32_t seq, uint32_t ack_seq, uint16_t id);
void set_data_packet(struct iphdr *ip, struct tcphdr *tcp, struct sockaddr_in saddr, struct sockaddr_in daddr, char *datagram, char* msg, uint32_t seq, uint32_t ack_seq, uint16_t id);

void three_way_handshaking(int sock, struct sockaddr_in saddr, struct sockaddr_in daddr);
void data_transfer(int sock, struct sockaddr_in saddr, struct sockaddr_in daddr, char *message, uint32_t* next_seq, uint32_t* next_ack);
#endif