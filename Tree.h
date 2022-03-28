#pragma once

typedef struct Tree Tree; // Let "Tree" mean the same as "struct Tree".

// Creates new tree with one empty directory "/"
Tree* tree_new();

// Frees all memory from Tree
void tree_free(Tree*);

// Returns all elements of directory (without going recursively) in any order
char* tree_list(Tree* tree, const char* path);

// Creates new subdirectory
int tree_create(Tree* tree, const char* path);

// Removes directory if it is non empty and exists
int tree_remove(Tree* tree, const char* path);

// Moves directory source with all content to directory target
int tree_move(Tree* tree, const char* source, const char* target);
