/*
	linked-list.h: declarations for linked-list-related functions
*/

#ifndef __LINKED_LIST_H__
#define __LINKED_LIST_H__

#define CLNT_NUM 10

typedef struct client_node {
    uint32_t server_index;
    uint32_t addr;
    uint16_t port;
    struct client_node* next;
} ClientNode;

typedef struct client_list
{
    struct client_node * head;
    int size;
} ClientList;

ClientList * InitList();                        // 리스트 초기화
struct client_node * InitNode();                // 노드 초기화
struct client_node * InitNodeInfo(uint32_t server_index, uint32_t addr, uint16_t port);                // 노드 초기화
void Destroy(ClientList * list);                // 리스트 삭제, 메모리 할당 해제

void Traverse(ClientList * list);                // 연결리스트 모든 원소 출력
void Insert(ClientList * list, uint32_t server_index, uint32_t addr, uint16_t port);      // 원소 추가
void InsertNode(ClientList * list, struct client_node *new_client);
ClientNode * Search(ClientList * list, uint32_t addr, uint16_t port);      // 원소 탐색
void RemoveNode(ClientList * list, uint32_t client_addr);          // 원소 탐색 후 제거
void Erase(ClientList * list);                   // 원소 제거

int is_in_client_list(ClientList * list, uint32_t addr);
int GetServerIndex(ClientList * list, uint32_t addr);

#endif