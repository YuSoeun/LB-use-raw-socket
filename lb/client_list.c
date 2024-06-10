/*
	linked-list.cpp: linked-list-related functions
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>

#include "client_list.h"

// #define DEBUG
#ifdef DEBUG
#define CHECK(fmt, ...) printf(fmt, ##__VA_ARGS__)
#else
#define CHECK(args...)
#endif

ClientList * InitList()
{
    ClientList * newlist = NULL;
    newlist = (ClientList *)malloc(sizeof(ClientList));

    newlist->head = NULL;
    newlist->size = 0;
    
    return newlist;
}

struct client_node * InitNode()
{  
    ClientNode * newnode = (ClientNode *)malloc(sizeof(ClientNode));
    
    newnode->server_index = 0;
    newnode->addr = 0;
    newnode->port = 0;
    newnode->next = NULL;

    return newnode;
}

struct client_node * InitNodeInfo(uint32_t server_index, uint32_t addr, uint16_t port)
{  
    ClientNode * newnode = newnode;
    newnode = (ClientNode *)malloc(sizeof(ClientNode));
    
    newnode->server_index = server_index;
    newnode->addr = addr;
    newnode->port = port;
    newnode->next = NULL;

    return newnode;
}

void Destroy(ClientList * list)
{
    while (list->size > 0) {
        Erase(list);
        CHECK("size - %d, ", list->size);
    }
    free(list);
}

void Traverse(ClientList * list)
{
    ClientNode * cur = list->head;
    int i = 0;

    printf("List Elements: \n");

    while (i < list->size) {
        printf("Addr: %d , Port: %d\n", cur->addr, cur->port);

        CHECK("w - %lld\n", (long long)cur);

        cur = cur->next;
        i++;
    }
}

void Insert(ClientList * list, uint32_t server_index, uint32_t addr, uint16_t port)
{
    ClientNode * newnode = NULL;
    ClientNode * cur = list->head;
    int i = 0;

    newnode = InitNode();
    
    newnode->server_index = server_index;
    newnode->addr = addr;
    newnode->port = port;

    if (list->size == 0) {
        list->head = newnode;
    } else {
        while (cur->next != NULL) {
            cur = cur->next;
        }
        cur->next = newnode;
    }
    CHECK("h - %lld\n", (long long)list->head);
    CHECK("%lld %lld\n", (long long)newnode, (long long)cur);

    list->size++;
}

void InsertNode(ClientList * list, struct client_node *newnode) {
    ClientNode * cur = list->head;

    if (list->size == 0) {
        list->head = newnode;
    } else {
        while (cur->next != NULL) {
            cur = cur->next;
        }
        cur->next = newnode;
    }

    CHECK("h - %lld\n", (long long)list->head);
    CHECK("%lld %lld\n", (long long)newnode, (long long)cur);

    list->size++;
}

void Erase(ClientList * list)
{
    ClientNode * pre_tail = list->head;
    ClientNode * tail = list->head;
    int i=0;

    CHECK("%d ", i);

    if (list->size == 0) {
    } else if (list->size == 1) {
        free(tail);
    } else {
        while (pre_tail->next->next != NULL) {
            pre_tail = pre_tail->next;
        }

        tail = pre_tail->next;

        CHECK("t - %lld %lld\n", (long long)pre_tail, (long long)tail);

        pre_tail->next = NULL;
        free(tail);
    }

    list->size--;
}

void remove_client_node(ClientList * list, uint32_t client_addr) {
    struct client_node *prev = NULL;
    struct client_node *cur = list->head;
    while (cur != NULL) {
        if (cur->addr == client_addr) {
            if (prev == NULL) {
                list->head = cur->next;
            } else {
                prev->next = cur->next;
            }
            free(cur);
            break;
        }
        prev = cur;
        cur = cur->next;
    }
}

ClientNode * Search(ClientList * list, uint32_t addr, uint16_t port)
{
    ClientNode * cur = list->head;

    while (cur->next != NULL) {
        if (cur->addr == addr && cur->port == port) {
            printf("%d %d is searched\n", addr, port);
            return cur;
        }
        cur = cur->next;
    }

    printf("Can't search %d %d \n", addr, port);
    return cur;
}

int is_in_client_list(ClientList * list, uint32_t addr)
{
    ClientNode * cur = list->head;

    while (cur->next != NULL) {
        if (cur->addr == addr) {
            printf("%d is searched\n", addr);
            return 1;
        }
        cur = cur->next;
    }

    printf("Can't search %d \n", addr);
    return 0;
}

int GetServerIndex(ClientList * list, uint32_t addr)
{
    ClientNode * cur = list->head;

    while (cur->next != NULL) {
        if (cur->addr == addr) {
            printf("%d is searched\n", addr);
            return cur->server_index;
        }
        cur = cur->next;
    }

    printf("Can't search %d \n", addr);
    return 0;
}