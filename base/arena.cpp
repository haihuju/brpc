// Copyright (c) 2015 Baidu.com, Inc. All Rights Reserved
//
// Do small memory allocations on continuous blocks.
//
// Author: Ge,Jun (gejun@baidu.com)
// Date: Fri Jun  5 18:25:40 CST 2015

#include <stdlib.h>
#include <algorithm>
#include "base/arena.h"

namespace base {

ArenaOptions::ArenaOptions()
    : initial_block_size(64)
    , max_block_size(8192)
{}

Arena::Arena(const ArenaOptions& options)
    : _cur_block(NULL)
    , _isolated_blocks(NULL)
    , _block_size(options.initial_block_size)
    , _options(options) {
}

Arena::~Arena() {
    while (_cur_block != NULL) {
        Block* const saved_next = _cur_block->next;
        free(_cur_block);
        _cur_block = saved_next;
    }
    while (_isolated_blocks != NULL) {
        Block* const saved_next = _isolated_blocks->next;
        free(_isolated_blocks);
        _isolated_blocks = saved_next;
    }
}

void Arena::swap(Arena& other) {
    std::swap(_cur_block, other._cur_block);
    std::swap(_isolated_blocks, other._isolated_blocks);
    std::swap(_block_size, other._block_size);
    const ArenaOptions tmp = _options;
    _options = other._options;
    other._options = tmp;
}

void Arena::clear() {
    // TODO(gejun): Reuse memory
    Arena a;
    swap(a);
}

void* Arena::allocate_new_block(size_t n) {
    Block* b = (Block*)malloc(offsetof(Block, data) + n);
    b->next = _isolated_blocks;
    b->alloc_size = n;
    b->size = n;
    _isolated_blocks = b;
    return b->data;
}

void* Arena::allocate_in_other_blocks(size_t n) {
    if (n > _block_size / 4) { // put outlier on separate blocks.
        return allocate_new_block(n);
    }
    // Waste the left space. At most 1/4 of allocated spaces are wasted.

    // Grow the block size gradually.
    if (_cur_block != NULL) {
        _block_size = std::min(2 * _block_size, _options.max_block_size);
    }
    size_t new_size = _block_size;
    if (new_size < n) {
        new_size = n;
    }
    Block* b = (Block*)malloc(offsetof(Block, data) + new_size);
    if (NULL == b) {
        return NULL;
    }
    b->next = NULL;
    b->alloc_size = n;
    b->size = new_size;
    if (_cur_block) {
        _cur_block->next = _isolated_blocks;
        _isolated_blocks = _cur_block;
    }
    _cur_block = b;
    return b->data;
}

}  // namespace base