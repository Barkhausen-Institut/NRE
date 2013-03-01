/*
 * Copyright (C) 2012, Nils Asmussen <nils@os.inf.tu-dresden.de>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of NRE (NOVA runtime environment).
 *
 * NRE is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * NRE is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 2 for more details.
 */

#pragma once

#include <arch/Types.h>

namespace nre {

template<class T>
class Treap;

/**
 * A node in the treap. You may create a subclass of this to add data to your nodes.
 */
template<typename KEY>
class TreapNode {
    template<typename T>
    friend class Treap;
public:
    typedef KEY key_t;

    /**
     * Constructor
     *
     * @param key the key of the node
     */
    explicit TreapNode(key_t key) : _key(key), _prio(), _left(), _right() {
    }

    /**
     * @return the key
     */
    key_t key() const {
        return _key;
    }
    /**
     * Sets the key. Note that changing the key while this node is already inserted in the tree
     * won't work in general.
     *
     * @param key the new key
     */
    void key(key_t key) {
        _key = key;
    }

private:
    key_t _key;
    int _prio;
    TreapNode *_left;
    TreapNode *_right;
};

/**
 * A treap is a combination of a binary tree and a heap. So the child-node on the left has a
 * smaller key (addr) than the parent and the child on the right has a bigger key.
 * Additionally the root-node has the smallest priority and it increases when walking towards the
 * leafs. The priority is "randomized" by fibonacci-hashing. This way, the tree is well balanced
 * in most cases.
 *
 * The idea and parts of the implementation are taken from the MMIX simulator, written by
 * Donald Knuth (http://mmix.cs.hm.edu/)
 */
template<class T>
class Treap {
public:
    typedef TreapNode<typename T::key_t> node_t;

    /**
     * Creates an empty treap
     */
    explicit Treap() : _prio(314159265), _root(0) {
    }

    /**
     * Finds the node with given key in the tree
     *
     * @param key the key
     * @return the node or nullptr if not found
     */
    T *find(typename T::key_t key) const {
        for(node_t *p = _root; p != nullptr; ) {
            if(p->_key == key)
                return static_cast<T*>(p);
            if(key < p->_key)
                p = p->_left;
            else
                p = p->_right;
        }
        return nullptr;
    }

    /**
     * Inserts the given node in the tree. Note that it is expected, that the key of the node is
     * already set.
     *
     * @param node the node to insert
     */
    void insert(node_t *node) {
        // we want to insert it by priority, so find the first node that has <= priority
        node_t **q, *p;
        for(p = _root, q = &_root; p && p->_prio < _prio; p = *q) {
            if(node->_key < p->_key)
                q = &p->_left;
            else
                q = &p->_right;
        }

        *q = node;
        // fibonacci hashing to spread the priorities very even in the 32-bit room
        node->_prio = _prio;
        _prio += 0x9e3779b9;    // floor(2^32 / phi), with phi = golden ratio

        // At this point we want to split the binary search tree p into two parts based on the
        // given key, forming the left and right subtrees of the new node q. The effect will be
        // as if key had been inserted before all of p’s nodes.
        node_t **l = &(*q)->_left, **r = &(*q)->_right;
        while(p) {
            if(node->_key < p->_key) {
                *r = p;
                r = &p->_left;
                p = *r;
            }
            else {
                *l = p;
                l = &p->_right;
                p = *l;
            }
        }
        *l = *r = nullptr;
    }

    /**
     * Removes the given node from the tree.
     *
     * @param node the node to remove (DOES have to be a valid pointer)
     */
    void remove(node_t *node) {
        node_t **p;
        // find the position where reg is stored
        for(p = &_root; *p && *p != node; ) {
            if(node->_key < (*p)->_key)
                p = &(*p)->_left;
            else
                p = &(*p)->_right;
        }
        remove_from(p, node);
    }

private:
    Treap(const Treap&);
    Treap& operator=(const Treap&);

    void remove_from(node_t **p, node_t *node) {
        // two childs
        if(node->_left && node->_right) {
            // rotate with left
            if(node->_left->_prio < node->_right->_prio) {
                node_t *t = node->_left;
                node->_left = t->_right;
                t->_right = node;
                *p = t;
                remove_from(&t->_right, node);
            }
            // rotate with right
            else {
                node_t *t = node->_right;
                node->_right = t->_left;
                t->_left = node;
                *p = t;
                remove_from(&t->_left, node);
            }
        }
        // one child: replace us with our child
        else if(node->_left)
            *p = node->_left;
        else if(node->_right)
            *p = node->_right;
        // no child: simply remove us from parent
        else
            *p = nullptr;
    }

    int _prio;
    node_t *_root;
};

}
