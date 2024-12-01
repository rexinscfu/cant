#include "route_table.h"
#include <string.h>

static uint8_t calc_hash(uint8_t source, uint8_t target) {
    return ((source * 7) + (target * 13)) % ROUTE_HASH_SIZE;
}

void RouteTable_Init(RouteTable* table) {
    memset(table, 0, sizeof(RouteTable));
    memset(table->hash_table, 0xFF, sizeof(table->hash_table));
}

bool RouteTable_Add(RouteTable* table, uint8_t source, uint8_t target, uint16_t service) {
    if(table->entry_count >= MAX_ROUTE_ENTRIES) return false;
    
    uint8_t hash = calc_hash(source, target);
    uint32_t idx = table->entry_count;
    
    table->entries[idx].source = source;
    table->entries[idx].target = target;
    table->entries[idx].service = service;
    table->entries[idx].next = table->hash_table[hash];
    
    table->hash_table[hash] = idx;
    table->entry_count++;
    
    return true;
}

bool RouteTable_Remove(RouteTable* table, uint8_t source, uint8_t target) {
    uint8_t hash = calc_hash(source, target);
    uint8_t* link = &table->hash_table[hash];
    
    while(*link != 0xFF) {
        RouteEntry* entry = &table->entries[*link];
        if(entry->source == source && entry->target == target) {
            *link = entry->next;
            return true;
        }
        link = &entry->next;
    }
    
    return false;
}

RouteEntry* RouteTable_Find(RouteTable* table, uint8_t source, uint8_t target) {
    uint8_t hash = calc_hash(source, target);
    uint8_t idx = table->hash_table[hash];
    
    while(idx != 0xFF) {
        RouteEntry* entry = &table->entries[idx];
        if(entry->source == source && entry->target == target) {
            return entry;
        }
        idx = entry->next;
    }
    
    return NULL;
}

static uint32_t collision_count = 0;
static uint32_t lookup_count = 0; 