// commit.h — Commit object interface
//
// A commit represents a snapshot of the project at a point in time.
// It links a tree (snapshot of files), optional parent commit,
// and metadata such as author, timestamp, and message.

#ifndef COMMIT_H
#define COMMIT_H

#include "pes.h"
#include <stdint.h>

// ───────────────────────────────────────────────────────────────
// Commit Object Structure
// ───────────────────────────────────────────────────────────────
typedef struct {
    ObjectID tree;          // Root tree hash (snapshot of project)
    ObjectID parent;        // Parent commit hash (if exists)
    int has_parent;         // 0 = initial commit, 1 = has parent

    char author[256];       // Author name (from PES_AUTHOR env variable)
    uint64_t timestamp;     // Commit creation time (Unix timestamp)

    char message[4096];     // Commit message (description of changes)
} Commit;


// ───────────────────────────────────────────────────────────────
// Core Commit Operations
// ───────────────────────────────────────────────────────────────

// Create a new commit from the current index.
//
// Steps:
//   1. Build a tree object from the index (tree_from_index)
//   2. Read current HEAD to determine parent commit (if any)
//   3. Create and serialize the commit object
//   4. Store the commit in the object database
//   5. Update HEAD (or current branch) to the new commit
//
// Returns:
//   0  → success
//  -1  → failure
int commit_create(const char *message, ObjectID *commit_id_out);


// Parse raw commit object data into a structured Commit object.
//
// Parameters:
//   data        → raw commit object bytes
//   len         → size of data
//   commit_out  → output parsed commit
//
// Returns:
//   0 on success, -1 on failure
int commit_parse(const void *data, size_t len, Commit *commit_out);


// Serialize a Commit struct into raw bytes for storage.
//
// Output buffer (*data_out) must be freed by caller.
//
// Returns:
//   0 on success, -1 on failure
int commit_serialize(const Commit *commit, void **data_out, size_t *len_out);


// ───────────────────────────────────────────────────────────────
// Commit History Traversal
// ───────────────────────────────────────────────────────────────

// Callback function type for commit traversal
typedef void (*commit_walk_fn)(const ObjectID *id,
                               const Commit *commit,
                               void *ctx);


// Walk commit history starting from HEAD.
//
// Traverses commits from newest → oldest using parent links.
// Stops when the root commit (no parent) is reached.
//
// Parameters:
//   callback → function called for each commit
//   ctx      → user-defined context
//
// Returns:
//   0 on success, -1 on failure
int commit_walk(commit_walk_fn callback, void *ctx);


// ───────────────────────────────────────────────────────────────
// HEAD Reference Helpers
// ───────────────────────────────────────────────────────────────

// Read the commit ID currently pointed to by HEAD.
//
// Supports symbolic references:
//   Example: HEAD → "ref: refs/heads/main"
//
// Returns:
//   0  → success
//  -1  → no commits yet / error
int head_read(ObjectID *id_out);


// Update HEAD (or current branch) to a new commit.
//
// Must use atomic write (write to temp file → rename)
//
// Returns:
//   0 on success, -1 on failure
int head_update(const ObjectID *new_commit);


#endif // COMMIT_H
