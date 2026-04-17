// commit.c — Commit creation and history traversal

#include "commit.h"
#include "index.h"
#include "tree.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>

// Forward declarations (from object.c)
int object_write(ObjectType type, const void *data, size_t len, ObjectID *id_out);
int object_read(const ObjectID *id, ObjectType *type_out, void **data_out, size_t *len_out);


// ───────────────────────────────────────────────────────────────
// PARSE COMMIT
// ───────────────────────────────────────────────────────────────
int commit_parse(const void *data, size_t len, Commit *commit_out) {
    (void)len;
    const char *p = (const char *)data;
    char hex[HASH_HEX_SIZE + 1];

    // tree
    if (sscanf(p, "tree %64s\n", hex) != 1) return -1;
    if (hex_to_hash(hex, &commit_out->tree) != 0) return -1;
    p = strchr(p, '\n') + 1;

    // parent (optional)
    if (strncmp(p, "parent ", 7) == 0) {
        if (sscanf(p, "parent %64s\n", hex) != 1) return -1;
        if (hex_to_hash(hex, &commit_out->parent) != 0) return -1;
        commit_out->has_parent = 1;
        p = strchr(p, '\n') + 1;
    } else {
        commit_out->has_parent = 0;
    }

    // author
    char author_buf[COMMIT_AUTHOR_MAX];
    uint64_t ts;

    if (sscanf(p, "author %255[^\n]\n", author_buf) != 1) return -1;

    char *last_space = strrchr(author_buf, ' ');
    if (!last_space) return -1;

    ts = (uint64_t)strtoull(last_space + 1, NULL, 10);
    *last_space = '\0';

    snprintf(commit_out->author, COMMIT_AUTHOR_MAX, "%s", author_buf);
    commit_out->timestamp = ts;

    // skip author, committer, blank line
    p = strchr(p, '\n') + 1;
    p = strchr(p, '\n') + 1;
    p = strchr(p, '\n') + 1;

    // message
    snprintf(commit_out->message, COMMIT_MESSAGE_MAX, "%s", p);

    return 0;
}


// ───────────────────────────────────────────────────────────────
// SERIALIZE COMMIT
// ───────────────────────────────────────────────────────────────
int commit_serialize(const Commit *commit, void **data_out, size_t *len_out) {
    char tree_hex[HASH_HEX_SIZE + 1];
    char parent_hex[HASH_HEX_SIZE + 1];

    hash_to_hex(&commit->tree, tree_hex);

    char buf[8192];
    int n = 0;

    n += snprintf(buf + n, sizeof(buf) - n, "tree %s\n", tree_hex);

    if (commit->has_parent) {
        hash_to_hex(&commit->parent, parent_hex);
        n += snprintf(buf + n, sizeof(buf) - n, "parent %s\n", parent_hex);
    }

    n += snprintf(buf + n, sizeof(buf) - n,
                  "author %s %" PRIu64 "\n"
                  "committer %s %" PRIu64 "\n"
                  "\n"
                  "%s",
                  commit->author, commit->timestamp,
                  commit->author, commit->timestamp,
                  commit->message);

    *data_out = malloc(n + 1);
    if (!*data_out) return -1;

    memcpy(*data_out, buf, n + 1);
    *len_out = (size_t)n;

    return 0;
}


// ───────────────────────────────────────────────────────────────
// WALK COMMITS
// ───────────────────────────────────────────────────────────────
int commit_walk(commit_walk_fn callback, void *ctx) {
    ObjectID id;

    if (head_read(&id) != 0) return -1;

    while (1) {
        ObjectType type;
        void *raw;
        size_t len;

        if (object_read(&id, &type, &raw, &len) != 0) return -1;

        Commit c;
        if (commit_parse(raw, len, &c) != 0) {
            free(raw);
            return -1;
        }

        free(raw);

        callback(&id, &c, ctx);

        if (!c.has_parent) break;
        id = c.parent;
    }

    return 0;
}


// ───────────────────────────────────────────────────────────────
// READ HEAD
// ───────────────────────────────────────────────────────────────
int head_read(ObjectID *id_out) {
    FILE *f = fopen(HEAD_FILE, "r");
    if (!f) return -1;

    char line[512];

    if (!fgets(line, sizeof(line), f)) {
        fclose(f);
        return -1;
    }

    fclose(f);
    line[strcspn(line, "\r\n")] = '\0';

    char ref_path[512];

    if (strncmp(line, "ref: ", 5) == 0) {
        snprintf(ref_path, sizeof(ref_path), "%s/%s", PES_DIR, line + 5);

        f = fopen(ref_path, "r");
        if (!f) return -1;

        if (!fgets(line, sizeof(line), f)) {
            fclose(f);
            return -1;
        }

        fclose(f);
        line[strcspn(line, "\r\n")] = '\0';
    }

    return hex_to_hash(line, id_out);
}


// ───────────────────────────────────────────────────────────────
// UPDATE HEAD
// ───────────────────────────────────────────────────────────────
int head_update(const ObjectID *new_commit) {
    FILE *f = fopen(HEAD_FILE, "r");
    if (!f) return -1;

    char line[512];

    if (!fgets(line, sizeof(line), f)) {
        fclose(f);
        return -1;
    }

    fclose(f);
    line[strcspn(line, "\r\n")] = '\0';

    char target[520];

    if (strncmp(line, "ref: ", 5) == 0) {
        snprintf(target, sizeof(target), "%s/%s", PES_DIR, line + 5);
    } else {
        snprintf(target, sizeof(target), "%s", HEAD_FILE);
    }

    char tmp[528];
    snprintf(tmp, sizeof(tmp), "%s.tmp", target);

    f = fopen(tmp, "w");
    if (!f) return -1;

    char hex[HASH_HEX_SIZE + 1];
    hash_to_hex(new_commit, hex);

    fprintf(f, "%s\n", hex);

    fflush(f);
    fsync(fileno(f));
    fclose(f);

    return rename(tmp, target);
}


// ───────────────────────────────────────────────────────────────
// CREATE COMMIT (MAIN LOGIC)
// ───────────────────────────────────────────────────────────────
int commit_create(const char *message, ObjectID *commit_id_out) {
    Commit commit;
    memset(&commit, 0, sizeof(Commit));

    // 1. Create tree
    if (tree_from_index(&commit.tree) != 0) return -1;

    // 2. Parent
    ObjectID parent;
    if (head_read(&parent) == 0) {
        commit.parent = parent;
        commit.has_parent = 1;
    } else {
        commit.has_parent = 0;
    }

    // 3. Author + time
    const char *author = pes_author();
    if (!author) return -1;

    snprintf(commit.author, COMMIT_AUTHOR_MAX, "%s", author);
    commit.timestamp = (uint64_t)time(NULL);

    // 4. Message
    if (!message) return -1;
    snprintf(commit.message, COMMIT_MESSAGE_MAX, "%s", message);

    // 5. Serialize
    void *data = NULL;
    size_t len = 0;

    if (commit_serialize(&commit, &data, &len) != 0) return -1;

    // 6. Store
    ObjectID id;
    if (object_write(OBJ_COMMIT, data, len, &id) != 0) {
        free(data);
        return -1;
    }

    free(data);

    // 7. Update HEAD
    if (head_update(&id) != 0) return -1;

    // 8. Return ID
    if (commit_id_out) *commit_id_out = id;

    return 0;
}
