#include "assert.h"
#include "Tree.h"
#include "path_utils.h"
#include "HashMap.h"
#include "readers-writers.h"
#include "err.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#define ERELATED -1; //Target exists in a subtree


struct Tree {
    HashMap *folder;
    ReadWrite *readwrite;
};

static bool lastNodeInPath(const char *path) {
    return strcmp(path, "/") == 0;
}

//Wait for all threads waiting and working inside a tree
static void block_tree(Tree *tree) {
    HashMapIterator iterator = hmap_iterator(tree->folder);
    const char *key;
    Tree *tempTree;

    lowPriorityStart(tree->readwrite);

    while (hmap_next(tree->folder, &iterator, &key, (void **) &tempTree)) {
        block_tree(tempTree);
    }
}

Tree *tree_new() {
    Tree *newTree = malloc(sizeof(Tree));
    if(!newTree) {
        fatal("Malloc failed.");
    }

    newTree->folder = hmap_new();
    newTree->readwrite = readwrite_new();
    return newTree;
}

void tree_free(Tree *tree) {
    HashMapIterator iterator = hmap_iterator(tree->folder);
    const char *key;
    Tree *tempTree;
    while (hmap_next(tree->folder, &iterator, &key, (void **) &tempTree)) {
        tree_free(tempTree);
    }
    hmap_free(tree->folder);
    readwrite_destroy(tree->readwrite);
    free(tree);
}

char *tree_list(Tree *tree, const char *path) {
    if (!is_path_valid(path)) {
        return NULL;
    }

    char component[MAX_FOLDER_NAME_LENGTH + 1];
    const char *subPath = path;
    Tree *subTree = tree;
    Tree *fatherTree = tree;

    readerStart(subTree->readwrite);

    while ((subPath = split_path(subPath, component))) {
        if (!(subTree = hmap_get(subTree->folder, component))) {
            readerEnd(fatherTree->readwrite);
            return NULL;
        }
        readerStart(subTree->readwrite);
        readerEnd(fatherTree->readwrite);
        fatherTree = subTree;
    }
    char *result = make_map_contents_string(subTree->folder);

    readerEnd(subTree->readwrite);
    return result;
}

int tree_create(Tree *tree, const char *path) {
    if (!is_path_valid(path)) {
        return EINVAL;
    }
    if (strcmp(path, "/") == 0) {
        return EEXIST;
    }

    char destination[MAX_FOLDER_NAME_LENGTH + 1];
    char component[MAX_FOLDER_NAME_LENGTH + 1];
    const char *subPath = path;
    Tree *subTree = tree;
    Tree *fatherTree = tree;

    char *pathToParent = make_path_to_parent(subPath, destination);
    subPath = pathToParent;


    if (lastNodeInPath(subPath)) {
        writerStart(subTree->readwrite);
    } else {
        readerStart(subTree->readwrite);
    }

    while ((subPath = split_path(subPath, component))) {
        if (!(subTree = hmap_get(subTree->folder, component))) {
            readerEnd(fatherTree->readwrite);
            free(pathToParent);
            return ENOENT;
        }
        if (lastNodeInPath(subPath)) {
            writerStart(subTree->readwrite);
            assert(subTree->readwrite->writersCount == 1);
        } else {
            readerStart(subTree->readwrite);
        }
        readerEnd(fatherTree->readwrite);
        fatherTree = subTree;
    }

    free(pathToParent);

    if (hmap_get(subTree->folder, destination)) {
        writerEnd(subTree->readwrite);
        return EEXIST;
    }
    Tree *newTree = tree_new();
    hmap_insert(subTree->folder, destination, newTree);

    writerEnd(subTree->readwrite);
    return 0;
}

int tree_remove(Tree *tree, const char *path) {
    if (!is_path_valid(path)) {
        return EINVAL;
    }
    if (strcmp(path, "/") == 0) {
        return EBUSY;
    }

    char destination[MAX_FOLDER_NAME_LENGTH + 1];
    char component[MAX_FOLDER_NAME_LENGTH + 1];
    const char *subPath = path;
    Tree *subTree = tree;
    Tree *fatherTree = tree;

    char *pathToParent = make_path_to_parent(subPath, destination);
    subPath = pathToParent;

    if (lastNodeInPath(subPath)) {
        writerStart(subTree->readwrite);
    } else {
        readerStart(subTree->readwrite);
    }

    while ((subPath = split_path(subPath, component))) {
        if (!(subTree = hmap_get(subTree->folder, component))) {
            readerEnd(fatherTree->readwrite);
            free(pathToParent);
            return ENOENT;
        }
        if (lastNodeInPath(subPath)) {
            writerStart(subTree->readwrite);
        } else {
            readerStart(subTree->readwrite);
        }
        readerEnd(fatherTree->readwrite);
        fatherTree = subTree;
    }
    free(pathToParent);

    Tree *destinationTree;
    if (!(destinationTree = hmap_get(subTree->folder, destination))) {
        writerEnd(subTree->readwrite);
        return ENOENT;
    } else if (hmap_size(destinationTree->folder) != 0) {
        writerEnd(subTree->readwrite);
        return ENOTEMPTY;
    }

    block_tree(destinationTree);
    tree_free(destinationTree);
    hmap_remove(subTree->folder, destination);

    writerEnd(subTree->readwrite);
    return 0;

}

int tree_move(Tree *tree, const char *source, const char *target) {
    if (!is_path_valid(source) || !is_path_valid(target)) {
        return EINVAL;
    }
    if (strcmp(source, "/") == 0) {
        return EBUSY;
    }
    if (strcmp(target, "/") == 0) {
        return EEXIST;
    }
    if (paths_related(source, target)) {
        return ERELATED;
    }

    Tree *subTreeLCA = tree;
    Tree *fatherTree = tree;

    char sourceChar[MAX_FOLDER_NAME_LENGTH + 1];
    char targetChar[MAX_FOLDER_NAME_LENGTH + 1];
    char component[MAX_FOLDER_NAME_LENGTH + 1];

    char *pathLCATemp = find_LCA(source, target);
    const char *subPathLCA = pathLCATemp;

    if (lastNodeInPath(subPathLCA)) {
        writerStart(tree->readwrite);
    } else {
        readerStart(tree->readwrite);
    }

    while ((subPathLCA = split_path(subPathLCA, component))) {
        if (!(subTreeLCA = hmap_get(subTreeLCA->folder, component))) {
            readerEnd(fatherTree->readwrite);
            free(pathLCATemp);
            return ENOENT;
        }
        if (lastNodeInPath(subPathLCA)) {
            writerStart(subTreeLCA->readwrite);
        } else {
            readerStart(subTreeLCA->readwrite);
        }
        readerEnd(fatherTree->readwrite);
        fatherTree = subTreeLCA;
    }

    Tree *subTreeSource = subTreeLCA;
    Tree *subTreeTarget = subTreeLCA;
    fatherTree = subTreeLCA;
    const char *subPathSource = source + strlen(pathLCATemp) - 1;
    const char *subPathTarget = target + strlen(pathLCATemp) - 1;
    bool firstStepSource = true;
    bool firstStepTarget = true;

    free(pathLCATemp);

    if (strcmp(target, source) == 0) {
        writerEnd(subTreeLCA->readwrite);
        return 0;
    }

    char *subPathSourceTemp = make_path_to_parent(subPathSource, sourceChar);
    subPathSource = subPathSourceTemp;

    while ((subPathSource = split_path(subPathSource, component))) {
        if (!(subTreeSource = hmap_get(subTreeSource->folder, component))) {
            if (!firstStepSource) {
                readerEnd(fatherTree->readwrite);
            }
            writerEnd(subTreeLCA->readwrite);
            free(subPathSourceTemp);
            return ENOENT;
        }
        if (lastNodeInPath(subPathSource)) {
            writerStart(subTreeSource->readwrite);
        } else {
            readerStart(subTreeSource->readwrite);
        }
        if (!firstStepSource) {
            readerEnd(fatherTree->readwrite);
        }
        fatherTree = subTreeSource;
        firstStepSource = false;
    }

    Tree *treeSource;
    if (!(treeSource = hmap_get(subTreeSource->folder, sourceChar))) {
        if (!firstStepSource) {
            writerEnd(subTreeSource->readwrite);
        }
        free(subPathSourceTemp);
        writerEnd(subTreeLCA->readwrite);
        return ENOENT;
    }

    block_tree(treeSource);
    if (!firstStepSource) {
        writerEnd(subTreeSource->readwrite);
    }

    char *subPathTargetTemp = make_path_to_parent(subPathTarget, targetChar);
    subPathTarget = subPathTargetTemp;

    if(!subPathTarget) {
        free(subPathSourceTemp);
        writerEnd(subTreeLCA->readwrite);
        return EEXIST;
    }

    free(subPathSourceTemp);

    while ((subPathTarget = split_path(subPathTarget, component))) {
        if (!(subTreeTarget = hmap_get(subTreeTarget->folder, component))) {
            free(subPathTargetTemp);
            writerEnd(subTreeLCA->readwrite);
            if (!firstStepTarget) {
                readerEnd(fatherTree->readwrite);
            }
            return ENOENT;
        }
        if (lastNodeInPath(subPathTarget)) {
            writerStart(subTreeTarget->readwrite);
        } else {
            readerStart(subTreeTarget->readwrite);
        }
        if (!firstStepTarget) {
            readerEnd(fatherTree->readwrite);
        }
        fatherTree = subTreeTarget;
        firstStepTarget = false;
    }

    if (hmap_get(subTreeTarget->folder, targetChar)) {
        free(subPathTargetTemp);
        if (!firstStepTarget) {
            writerEnd(subTreeTarget->readwrite);
        }
        writerEnd(subTreeLCA->readwrite);
        return EEXIST;
    }
    free(subPathTargetTemp);

    hmap_remove(subTreeSource->folder, sourceChar);
    hmap_insert(subTreeTarget->folder, targetChar, treeSource);

    writerEnd(subTreeLCA->readwrite);

    if (!firstStepTarget) {
        writerEnd(subTreeTarget->readwrite);
    }

    return 0;
}