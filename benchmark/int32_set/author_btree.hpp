// Original implementation: Haoqiang Fan (fanhqme2).
// https://gist.github.com/fanhqme2/df3d885ba303ec5e25ac5ffa519ca8b4
// Revision: 2ccbb7ca56cd071b3d83852ceaf090d2e558628b
// Adaptation: remove the benchmark driver/includes and add a namespace/header guard.
#pragma once
#include <climits>
#include <cstdint>
#include <vector>
#include <immintrin.h>

namespace nova_bench::author {
using std::vector;
static_assert(sizeof(int) == sizeof(int32_t));

// B-tree with order 64 (max 63 keys, max 64 children)
// keys[] is padded to 64 elements (BTREE_ORDER) for AVX2 alignment
const int BTREE_ORDER = 64;
const int MAX_KEYS = BTREE_ORDER - 1;  // 63
const int MIN_KEYS = (BTREE_ORDER - 1) / 2;  // 31

struct BTreeNode {
    int keys[BTREE_ORDER];  // Size 64 for AVX2, only first num_keys are valid
    int children[BTREE_ORDER];
    int num_keys; // 4
    bool is_leaf; // 1
    char padding[3];
};

// AVX2 removal from 64-element array using blend strategy
// Shifts elements to the left to remove element with value key, pads with INT_MAX
// Assumes array is sorted and padded with INT_MAX
inline void remove_64_avx2(int32_t keys[], int32_t key) {
    int32_t next_values[8];
    for (int chunk = 0; chunk < 7; chunk++) {
        next_values[chunk] = keys[chunk * 8 + 8];
    }
    next_values[7] = INT_MAX;
    __m256i vx = _mm256_set1_epi32(key);
    const __m256i perm_idx = _mm256_setr_epi32(1, 2, 3, 4, 5, 6, 7, 0);

    // Process each chunk with blend
    for (int chunk = 0; chunk < 8; chunk++) {
        int chunk_start = chunk * 8;

        __m256i va = _mm256_loadu_si256((__m256i const*)&keys[chunk_start]);
        __m256i v_inserted = _mm256_insert_epi32(va, next_values[chunk], 0);
        __m256i va_shifted = _mm256_permutevar8x32_epi32(v_inserted, perm_idx);

        __m256i cmp = _mm256_cmpgt_epi32(vx, va);
        // Blend: (A[i] < x) ? va : va_shifted
        __m256i result = _mm256_blendv_epi8(va_shifted, va, cmp);

        _mm256_storeu_si256((__m256i*)&keys[chunk_start], result);
    }
}

// AVX2 insertion into 64-element array using blend strategy
// Shifts elements to the right and inserts key at position pos
// Assumes array is sorted and padded with INT_MAX
inline void insert_64_avx2(int32_t keys[], int32_t key, int pos) {
    // Save boundary values before any stores (first chunk uses 0 as prev)
    int32_t prev_values[8];
    prev_values[0] = 0;
    for (int chunk = 1; chunk < 8; chunk++) {
        prev_values[chunk] = keys[chunk * 8 - 1];
    }

    __m256i vx = _mm256_set1_epi32(key);
    const __m256i perm_idx = _mm256_setr_epi32(7, 0, 1, 2, 3, 4, 5, 6);

    // Process each chunk with blend
    for (int chunk = 0; chunk < 8; chunk++) {
        int chunk_start = chunk * 8;

        __m256i va = _mm256_loadu_si256((__m256i const*)&keys[chunk_start]);
        __m256i v_inserted = _mm256_insert_epi32(va, prev_values[chunk], 7);
        __m256i va_shifted = _mm256_permutevar8x32_epi32(v_inserted, perm_idx);

        __m256i cmp = _mm256_cmpgt_epi32(vx, va);
        // Blend: (A[i] < x) ? va : va_shifted
        __m256i result = _mm256_blendv_epi8(va_shifted, va, cmp);

        _mm256_storeu_si256((__m256i*)&keys[chunk_start], result);
    }

    keys[pos] = key;
}

// AVX2 linear search - returns count of elements less than x
// Assumes array is sorted and padded with INT_MAX

inline int linear_search_avx2_64(const int32_t A[], int32_t x) {
    __m256i vx = _mm256_set1_epi32(x);
    int total_count = 0;

    // Process 8 elements at a time

    for (int i = 0; i < 64; i += 8) {
        __m256i va = _mm256_loadu_si256((__m256i const*)&A[i]);
        __m256i cmp = _mm256_cmpgt_epi32(vx, va);  // vx > va is equivalent to va < vx
        int mask = _mm256_movemask_ps(_mm256_castsi256_ps(cmp));
        total_count += __builtin_popcount(mask);
    }

    return total_count;
}

inline int linear_search_avx2(const int32_t A[], int32_t x) {
    __m256i vx = _mm256_set1_epi32(x);
    int total_count = 0;

    // Process 8 elements at a time
    int i0 = 0;

    if (A[31] < x){
        total_count += 32;
        i0 = 32;
    }

    for (int i = i0; i < i0 + 32; i += 8) {
        __m256i va = _mm256_loadu_si256((__m256i const*)&A[i]);
        __m256i cmp = _mm256_cmpgt_epi32(vx, va);  // vx > va is equivalent to va < vx
        int mask = _mm256_movemask_ps(_mm256_castsi256_ps(cmp));
        total_count += __builtin_popcount(mask);
    }

    return total_count;
}

struct BTree {
    vector<BTreeNode> pool;
    int root;
    int free_idx;

    BTree(int max_nodes) {
        pool.resize(max_nodes / MIN_KEYS + 3);
        root = -1;
        free_idx = 0;
    }

    void clear() {
        root = -1;
        free_idx = 0;
    }

    int new_node(bool is_leaf) {
        int idx = free_idx++;
        pool[idx].num_keys = 0;
        pool[idx].is_leaf = is_leaf;
        for (int i = 0; i < BTREE_ORDER; i++) {
            pool[idx].children[i] = -1;
            pool[idx].keys[i] = INT_MAX;  // Initialize keys with INT_MAX
        }
        return idx;
    }

    bool find(int key) {
        int cur = root;
        if (cur == -1){
            return false;
        }
        while (!pool[cur].is_leaf) {
            BTreeNode& node = pool[cur];
            int i = linear_search_avx2(node.keys, key);
            __builtin_prefetch(&pool[node.children[i]]);
            if (i < node.num_keys && key == node.keys[i]) {
                return true;
            }
            cur = node.children[i];
        }
        BTreeNode& node = pool[cur];
        int i;
        if (pool.size() <= 32768){
            i = linear_search_avx2(node.keys, key);
        } else {
            i = linear_search_avx2_64(node.keys, key);
        }
        //int i = linear_search_avx2_64(node.keys, key);
        if (i < node.num_keys && key == node.keys[i]) {
            return true;
        }
        return false;
    }

    void insert(int key) {
        if (root == -1) {
            root = new_node(true);
            pool[root].keys[0] = key;
            pool[root].num_keys = 1;
            return;
        }

        // If root is full, split it
        if (pool[root].num_keys == MAX_KEYS) {
            int new_root = new_node(false);
            pool[new_root].children[0] = root;
            split_child(new_root, 0);
            root = new_root;
        }

        insert_non_full(root, key);
    }

    // Split child i of node x
    void split_child(int x, int i) {
        BTreeNode& parent = pool[x];
        int y = parent.children[i];
        BTreeNode& full_child = pool[y];

        // Create new node z
        int z = new_node(full_child.is_leaf);
        BTreeNode& z_node = pool[z];

        // Median key index
        int mid = MAX_KEYS / 2;  // 31

        // Copy right half to z: keys[mid+1 ... MAX_KEYS-1]
        z_node.num_keys = MAX_KEYS - mid - 1;  // 31
        for (int j = 0; j < z_node.num_keys; j++) {
            z_node.keys[j] = full_child.keys[mid + 1 + j];
        }

        // Copy children if not leaf: children[mid+1 ... MAX_KEYS]
        if (!full_child.is_leaf) {
            for (int j = 0; j <= z_node.num_keys; j++) {
                z_node.children[j] = full_child.children[mid + 1 + j];
            }
        }

        // Save median key before modifying full_child
        int median_key = full_child.keys[mid];

        // Truncate full_child to left half: keys[0 ... mid-1]
        full_child.num_keys = mid;
        // Pad truncated positions with INT_MAX for AVX2
        for (int j = mid; j < BTREE_ORDER; j++) {
            full_child.keys[j] = INT_MAX;
        }

        // Insert median key into parent
        // Make room
        for (int j = parent.num_keys; j > i; j--) {
            parent.keys[j] = parent.keys[j - 1];
            parent.children[j + 1] = parent.children[j];
        }

        parent.children[i + 1] = z;
        parent.keys[i] = median_key;
        parent.num_keys++;
    }

    void insert_non_full(int x, int key) {

        while (!pool[x].is_leaf){
            BTreeNode& node = pool[x];
            // Use AVX2 to find child to insert into
            int pos = linear_search_avx2(node.keys, key);
            // Check for duplicate
            if (pos < node.num_keys && key == node.keys[pos]) {
                return;  // Duplicate
            }

            int child = node.children[pos];

            // Split child if full
            if (pool[child].num_keys == MAX_KEYS) {
                split_child(x, pos);

                // After split, decide which child to go to
                if (key > node.keys[pos]) {
                    pos++;
                } else if (key == node.keys[pos]) {
                    return;  // Duplicate
                }
            }
            x = node.children[pos];
        }

        BTreeNode& node = pool[x];

        // Use AVX2 to find insertion position
        int pos = linear_search_avx2(node.keys, key);
        // Check for duplicate
        if (pos < node.num_keys && key == node.keys[pos]) {
            return;  // Duplicate
        }
        // Use AVX2 blend to shift keys and insert
        insert_64_avx2(node.keys, key, pos);
        node.num_keys++;
    }

    // Remove a key from the B-tree
    bool remove(int key) {
        if (root == -1) return false;

        bool found = remove_rec(root, key);

        // Handle empty root
        if (root != -1 && pool[root].num_keys == 0) {
            if (pool[root].is_leaf) {
                root = -1;
            } else {
                root = pool[root].children[0];
            }
        }

        return found;
    }

    bool remove_rec(int x, int key) {
        BTreeNode& node = pool[x];
        // Use AVX2 to find key or child position
        int idx = linear_search_avx2(node.keys, key);

        if (idx < node.num_keys && key == node.keys[idx]) {
            // Key found in this node
            if (node.is_leaf) {
                // Use AVX2 blend to shift keys left and remove
                remove_64_avx2(node.keys, key);
                node.num_keys--;
                return true;
            } else {
                // Internal node - replace with predecessor
                int pred = get_predecessor(x, idx);
                node.keys[idx] = pred;

                // Ensure left child has enough keys
                int left_child = node.children[idx];
                if (pool[left_child].num_keys <= MIN_KEYS) {
                    fill_child(x, idx);
                    // Re-find position after potential changes
                    idx = linear_search_avx2(node.keys, pred);
                }

                return remove_rec(node.children[idx], pred);
            }
        } else if (!node.is_leaf) {
            // Key not in this node, recurse to child
            int child_pos = idx;
            int child = node.children[child_pos];

            if (child == -1) return false;

            // Ensure child has enough keys
            if (pool[child].num_keys <= MIN_KEYS) {
                fill_child(x, child_pos);
                // Re-check if key is now in this node
                idx = linear_search_avx2(node.keys, key);
                if (idx < node.num_keys && key == node.keys[idx]) {
                    return remove_rec(x, key);
                }
                child_pos = idx;
            }

            return remove_rec(node.children[child_pos], key);
        }

        return false;
    }

    int get_predecessor(int x, int idx) {
        int cur = pool[x].children[idx];
        while (!pool[cur].is_leaf) {
            cur = pool[cur].children[pool[cur].num_keys];
        }
        return pool[cur].keys[pool[cur].num_keys - 1];
    }

    void fill_child(int x, int child_pos) {
        BTreeNode& parent = pool[x];

        // Try to borrow from left sibling
        if (child_pos > 0) {
            int left = parent.children[child_pos - 1];
            if (pool[left].num_keys > MIN_KEYS) {
                borrow_from_left(x, child_pos);
                return;
            }
        }

        // Try to borrow from right sibling
        if (child_pos < parent.num_keys) {
            int right = parent.children[child_pos + 1];
            if (pool[right].num_keys > MIN_KEYS) {
                borrow_from_right(x, child_pos);
                return;
            }
        }

        // Must merge with a sibling
        if (child_pos < parent.num_keys) {
            merge_with_right(x, child_pos);
        } else if (child_pos > 0) {
            merge_with_right(x, child_pos - 1);
        }
    }

    void borrow_from_left(int x, int child_pos) {
        BTreeNode& parent = pool[x];
        int child = parent.children[child_pos];
        int left = parent.children[child_pos - 1];

        // Shift child's keys right
        for (int i = pool[child].num_keys; i > 0; i--) {
            pool[child].keys[i] = pool[child].keys[i - 1];
        }

        // Shift child's children right if not leaf
        if (!pool[child].is_leaf) {
            for (int i = pool[child].num_keys + 1; i > 0; i--) {
                pool[child].children[i] = pool[child].children[i - 1];
            }
        }

        // Move parent's separator key to child
        pool[child].keys[0] = parent.keys[child_pos - 1];
        pool[child].num_keys++;

        // Move left sibling's max key to parent
        parent.keys[child_pos - 1] = pool[left].keys[pool[left].num_keys - 1];

        // Move left sibling's rightmost child to child if not leaf
        if (!pool[child].is_leaf) {
            pool[child].children[0] = pool[left].children[pool[left].num_keys];
        }

        pool[left].keys[pool[left].num_keys - 1] = INT_MAX;  // Pad with INT_MAX
        pool[left].num_keys--;
    }

    void borrow_from_right(int x, int child_pos) {
        BTreeNode& parent = pool[x];
        int child = parent.children[child_pos];
        int right = parent.children[child_pos + 1];

        // Move parent's separator key to child
        pool[child].keys[pool[child].num_keys] = parent.keys[child_pos];
        pool[child].num_keys++;

        // Move right sibling's leftmost child to child if not leaf
        if (!pool[child].is_leaf) {
            pool[child].children[pool[child].num_keys] = pool[right].children[0];
        }

        // Move right sibling's min key to parent
        parent.keys[child_pos] = pool[right].keys[0];

        // Shift right sibling's keys left
        for (int i = 0; i < pool[right].num_keys - 1; i++) {
            pool[right].keys[i] = pool[right].keys[i + 1];
        }
        pool[right].keys[pool[right].num_keys - 1] = INT_MAX;  // Pad with INT_MAX

        // Shift right sibling's children left if not leaf
        if (!pool[right].is_leaf) {
            for (int i = 0; i < pool[right].num_keys; i++) {
                pool[right].children[i] = pool[right].children[i + 1];
            }
        }

        pool[right].num_keys--;
    }

    void merge_with_right(int x, int child_pos) {
        BTreeNode& parent = pool[x];
        int child = parent.children[child_pos];
        int right = parent.children[child_pos + 1];

        // Move parent's separator key to child
        pool[child].keys[pool[child].num_keys] = parent.keys[child_pos];
        pool[child].num_keys++;

        // Move all keys from right sibling
        for (int i = 0; i < pool[right].num_keys; i++) {
            pool[child].keys[pool[child].num_keys + i] = pool[right].keys[i];
        }

        // Move all children from right sibling if not leaf
        if (!pool[child].is_leaf) {
            for (int i = 0; i <= pool[right].num_keys; i++) {
                pool[child].children[pool[child].num_keys + i] = pool[right].children[i];
            }
        }

        pool[child].num_keys += pool[right].num_keys;

        // Remove separator from parent
        for (int i = child_pos; i < parent.num_keys - 1; i++) {
            parent.keys[i] = parent.keys[i + 1];
            parent.children[i + 1] = parent.children[i + 2];
        }
        parent.keys[parent.num_keys - 1] = INT_MAX;  // Pad with INT_MAX
        parent.num_keys--;
    }
};


}  // namespace nova_bench::author
