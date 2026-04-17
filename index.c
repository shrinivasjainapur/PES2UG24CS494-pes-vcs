// commit.h — Commit object interface
//
// Defines the structure and operations for commit objects.
// A commit represents a snapshot of the project state along with
// metadata and a reference to its parent commit.

#ifndef COMMIT_H
#define COMMIT_H

#include "pes.h"
#include <stdint.h>
#include <stddef.h>   // for size_t

// ───────────────────────────────────────────────────────────────
// Constants
// ───────────────────────────────────────────────────────────────

#define COMMIT_AUTHOR_MAX   256
#define COMMIT_MESSAGE_MAX  4096


// ───────────────────────────────────────────────────────────────
// Commit Object Structure
// ───────────────────────────────────────────────────────────────
typedef struct {
    ObjectID tree;          // Root tree hash (project snapshot)

    ObjectID parent;        // Parent commit hash
    int has_parent;         // 0 = initial commit, 1 = has parent

    char author[COMMIT_AUTHOR_MAX];     // Author name
    uint64_t timestamp;                // Unix timestamp

    char message[COMMIT_MESSAGE_MAX];  // Commit message
} Commit;


// ───────────────────────────────────────────────────────────────
// Core Commit Operations
// ───────────────────────────────────────────────────────────────

// Create a new commit from the current index.
// Returns 0 on success, -1 on failure.
int commit_create(const char *message, ObjectID *commit_id_out);


// Parse raw commit data into a Commit structure.
int commit_parse(const void *data, size_t len, Commit *commit_out);


// Serialize Commit into raw format for storage.
// Caller must free(*data_out).
int commit_serialize(const Commit *commit, void **data_out, size_t *len_out);


// ───────────────────────────────────────────────────────────────
// Commit History Traversal
// ───────────────────────────────────────────────────────────────

// Callback used during commit traversal
typedef void (*commit_walk_fn)(
    const ObjectID *id,
    const Commit *commit,
    void *ctx
);


// Traverse commits from HEAD → root.
int commit_walk(commit_walk_fn callback, void *ctx);


// ───────────────────────────────────────────────────────────────
// HEAD Reference Helpers
// ───────────────────────────────────────────────────────────────

// Read current HEAD commit
// Returns 0 on success, -1 if no commits exist
int head_read(ObjectID *id_out);


// Update HEAD (atomic operation required)
int head_update(const ObjectID *new_commit);


#endif // COMMIT_H
