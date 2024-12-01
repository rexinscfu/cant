#ifndef ROUTE_TABLE_H
#define ROUTE_TABLE_H

#include <stdint.h>
#include <stdbool.h>

#define MAX_ROUTE_ENTRIES 32
#define ROUTE_HASH_SIZE 16

typedef struct {
    uint8_t source;
    uint8_t target;
    uint16_t service;
    uint8_t next;
} RouteEntry;

typedef struct {
    RouteEntry entries[MAX_ROUTE_ENTRIES];
    uint8_t hash_table[ROUTE_HASH_SIZE];
    uint32_t entry_count;
} RouteTable;

void RouteTable_Init(RouteTable* table);
bool RouteTable_Add(RouteTable* table, uint8_t source, uint8_t target, uint16_t service);
bool RouteTable_Remove(RouteTable* table, uint8_t source, uint8_t target);
RouteEntry* RouteTable_Find(RouteTable* table, uint8_t source, uint8_t target);

#endif 