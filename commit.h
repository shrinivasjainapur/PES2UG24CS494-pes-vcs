// commit.h — Commit object interface
//
// A commit represents a snapshot of the project at a point in time.
// It links a tree (snapshot of files), optional parent commit,
// and metadata such as author, timestamp, and message.

#ifndef COMMIT_H
#define COMMIT_H

#include "pes.h"
#include <stdint.h>
#include <stddef.h>   // for size_t

// ───────────────────────────────────────────────────────────────
// Commit Object Structure
// ───────────────────────────────────────────────────────────────
typedef struct {
    ObjectID tree;          // Root tree hash (snapshot of project)

    ObjectID parent;        // Parent commit hash (if exists)
    int has_parent;         // 0 = initial commit, 1 = has parent

    char author[256];       // Author name
    uint64_t timestamp;     // Unix timestamp

    char message[4096];     // Commit message
} Commit;


// ───────────────────────────────────────────────────────────────
// Core Commit Operations
// ───────────────────────────────────────────────────────────────

// Create a new commit from the current index.
//
// Steps:
//   1. Build tree from index (tree_from_index)
//   2. Read HEAD → get parent commit (if exists)
//   3. Fill Commit struct (author, time, message)
//   4. Serialize commit → raw format
//   5. Store using object_write()
//   6. Update HEAD to new commit
//
// Returns:
//   0  → success
//  -1  → failure
int commit_create(const char *message, ObjectID *commit_id_out);


// Parse raw commit data → Commit struct
int commit_parse(const void *data, size_t len, Commit *commit_out);


// Serialize Commit struct → raw bytes
// (*data_out must be freed by caller)
int commit_serialize(const Commit *commit, void **data_out, size_t *len_out);


// ───────────────────────────────────────────────────────────────
// Commit History Traversal
// ───────────────────────────────────────────────────────────────

// Callback type used in commit_walk
typedef void (*commit_walk_fn)(
    const ObjectID *id,
    const Commit *commit,
    void *ctx
);


// Traverse commit history (HEAD → root)
int commit_walk(commit_walk_fn callback, void *ctx);


// ───────────────────────────────────────────────────────────────
// HEAD Reference Helpers
// ───────────────────────────────────────────────────────────────

// Read current HEAD commit
int head_read(ObjectID *id_out);


// Update HEAD → new commit (atomic)
int head_update(const ObjectID *new_commit);


#endif // COMMIT_H
