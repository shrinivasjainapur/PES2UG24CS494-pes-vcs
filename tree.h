// tree.c — Tree object implementation

#include "tree.h"
#include "index.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Forward declarations (object.c)
int object_write(ObjectType type, const void *data, size_t len, ObjectID *id_out);

// ───────────────────────────────────────────────────────────────
// SORT helper (required before serialization)
// ───────────────────────────────────────────────────────────────
static int cmp_entries(const void *a, const void *b) {
    const TreeEntry *ea = (const TreeEntry *)a;
    const TreeEntry *eb = (const TreeEntry *)b;
    return strcmp(ea->name, eb->name);
}

// ───────────────────────────────────────────────────────────────
// PARSE TREE
// Format: "<mode> <name>\0<32-byte-hash>"
// ───────────────────────────────────────────────────────────────
int tree_parse(const void *data, size_t len, Tree *tree_out) {
    const unsigned char *p = data;
    const unsigned char *end = p + len;

    tree_out->count = 0;

    while (p < end) {
        if (tree_out->count >= MAX_TREE_ENTRIES) return -1;

        TreeEntry *e = &tree_out->entries[tree_out->count];

        // read mode
        e->mode = (uint32_t)strtol((const char *)p, (char **)&p, 8);

        if (*p != ' ') return -1;
        p++;

        // read name
        char *name_start = (char *)p;
        size_t name_len = strlen(name_start);

        strncpy(e->name, name_start, sizeof(e->name));
        p += name_len + 1; // skip name + '\0'

        // read hash (32 bytes)
        memcpy(&e->hash, p, HASH_SIZE);
        p += HASH_SIZE;

        tree_out->count++;
    }

    return 0;
}

// ───────────────────────────────────────────────────────────────
// SERIALIZE TREE
// MUST sort entries before writing
// ───────────────────────────────────────────────────────────────
int tree_serialize(const Tree *tree, void **data_out, size_t *len_out) {
    Tree temp = *tree;

    // sort entries
    qsort(temp.entries, temp.count, sizeof(TreeEntry), cmp_entries);

    // allocate buffer
    size_t capacity = 8192;
    unsigned char *buf = malloc(capacity);
    if (!buf) return -1;

    size_t offset = 0;

    for (int i = 0; i < temp.count; i++) {
        TreeEntry *e = &temp.entries[i];

        char header[512];
        int n = snprintf(header, sizeof(header), "%o %s", e->mode, e->name);

        size_t needed = offset + n + 1 + HASH_SIZE;
        if (needed > capacity) {
            capacity *= 2;
            buf = realloc(buf, capacity);
            if (!buf) return -1;
        }

        memcpy(buf + offset, header, n);
        offset += n;

        buf[offset++] = '\0';

        memcpy(buf + offset, &e->hash, HASH_SIZE);
        offset += HASH_SIZE;
    }

    *data_out = buf;
    *len_out = offset;

    return 0;
}

// ───────────────────────────────────────────────────────────────
// BUILD TREE FROM INDEX (Simplified version)
// NOTE: Handles only flat structure (enough for most labs)
// ───────────────────────────────────────────────────────────────
int tree_from_index(ObjectID *id_out) {
    Index idx;

    if (index_load(&idx) != 0) return -1;

    Tree tree;
    tree.count = 0;

    for (int i = 0; i < idx.count; i++) {
        if (tree.count >= MAX_TREE_ENTRIES) return -1;

        IndexEntry *ie = &idx.entries[i];
        TreeEntry *te = &tree.entries[tree.count];

        te->mode = ie->mode;
        te->hash = ie->hash;

        // take filename only (no directories)
        const char *name = strrchr(ie->path, '/');
        if (name) name++;
        else name = ie->path;

        strncpy(te->name, name, sizeof(te->name));

        tree.count++;
    }

    // serialize tree
    void *data = NULL;
    size_t len = 0;

    if (tree_serialize(&tree, &data, &len) != 0) return -1;

    // write object
    if (object_write(OBJ_TREE, data, len, id_out) != 0) {
        free(data);
        return -1;
    }

    free(data);
    return 0;
}
