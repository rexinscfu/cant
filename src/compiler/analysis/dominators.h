#ifndef CANT_DOMINATORS_H
#define CANT_DOMINATORS_H

#include "cfg.h"
#include <stdbool.h>

// Dominator tree node
typedef struct DomNode {
    CFGNode* cfg_node;
    struct DomNode* idom;
    struct DomNode** children;
    uint32_t child_count;
    uint32_t pre_order;
    uint32_t post_order;
} DomNode;

// Dominator tree
typedef struct {
    DomNode* root;
    DomNode** nodes;
    uint32_t node_count;
    bool* dominance_matrix;
} DomTree;

// Dominator analysis functions
DomTree* dom_create_tree(CFG* cfg);
void dom_destroy_tree(DomTree* tree);
bool dom_compute_tree(DomTree* tree);
bool dom_update_tree(DomTree* tree, CFGNode* modified);

// Dominance queries
bool dom_dominates(DomTree* tree, CFGNode* a, CFGNode* b);
bool dom_strictly_dominates(DomTree* tree, CFGNode* a, CFGNode* b);
DomNode* dom_find_lca(DomTree* tree, DomNode* a, DomNode* b);
uint32_t dom_tree_depth(DomNode* node);

// Dominance frontier
typedef struct {
    CFGNode** nodes;
    uint32_t count;
} DomFrontier;

DomFrontier* dom_compute_frontier(DomTree* tree, CFGNode* node);
void dom_destroy_frontier(DomFrontier* frontier);
bool dom_is_in_frontier(DomFrontier* frontier, CFGNode* node);

// Utility functions
DomNode* dom_get_node(DomTree* tree, CFGNode* cfg_node);
bool dom_verify_tree(DomTree* tree);
void dom_print_tree(DomTree* tree);

#endif // CANT_DOMINATORS_H 