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
#include <arch/ExecEnv.h>
#include <kobj/ObjCap.h>
#include <mem/DataSpaceDesc.h>
#include <collection/SortedSList.h>
#include <stream/OStringStream.h>
#include <bits/MaskField.h>
#include <util/Math.h>
#include <Exception.h>

namespace nre {

class ChildMemoryException : public Exception {
public:
    explicit ChildMemoryException(ErrorCode code = E_FAILURE, const String &msg = String()) throw()
        : Exception(code, msg) {
    }
};

class OStream;

/**
 * Manages the virtual memory of a child process
 */
class ChildMemory {
public:
    enum Flags {
        R   = DataSpaceDesc::R,
        W   = DataSpaceDesc::W,
        X   = DataSpaceDesc::X,
        RW  = R | W,
        RX  = R | X,
        RWX = R | W | X,
        // indicates that the memory has been requested by us, i.e. we haven't just joined the DS
        OWN = 1 << 4,
    };

    /**
     * A dataspace in the address space of the child including administrative information.
     */
    class DS : public SListItem {
    public:
        /**
         * Creates the dataspace with given descriptor and cap
         */
        explicit DS(const DataSpaceDesc &desc, capsel_t cap)
            : SListItem(), _desc(desc), _cap(cap),
              _perms(Math::blockcount<size_t>(desc.size(), ExecEnv::PAGE_SIZE) * 4) {
        }

        /**
         * @return the permission masks for all pages
         */
        const MaskField<4> &perms() const {
            return _perms;
        }
        /**
         * @return the dataspace descriptor
         */
        DataSpaceDesc &desc() {
            return _desc;
        }
        /**
         * @return the dataspace descriptor
         */
        const DataSpaceDesc &desc() const {
            return _desc;
        }
        /**
         * @return the dataspace (unmap) capability
         */
        capsel_t cap() const {
            return _cap;
        }
        /**
         * @param addr the virtual address (is expected to be in this dataspace)
         * @return the origin for the given address
         */
        uintptr_t origin(uintptr_t addr) const {
            return _desc.origin() + (addr - _desc.virt());
        }
        /**
         * @param addr the virtual address (is expected to be in this dataspace)
         * @return the permissions of the given page
         */
        uint page_perms(uintptr_t addr) const {
            return _perms.get((addr - _desc.virt()) / ExecEnv::PAGE_SIZE);
        }
        /**
         * Sets the permissions of the given page range to <perms>. It sets all pages until it
         * encounters a page that already has the given permissions. Additionally, it makes sure
         * not to leave this dataspace.
         *
         * @param addr the virtual address where to start
         * @param pages the number of pages
         * @param perms the permissions to set
         * @return the actual number of pages that have been changed
         */
        size_t page_perms(uintptr_t addr, size_t pages, uint perms) {
            uintptr_t off = addr - _desc.virt();
            pages = Math::min<size_t>(pages, (_desc.size() - off) / ExecEnv::PAGE_SIZE);
            for(size_t i = 0, o = off / ExecEnv::PAGE_SIZE; i < pages; ++i, ++o) {
                uint oldperms = _perms.get(o);
                if(oldperms == perms)
                    return i;
                _perms.set(o, perms);
            }
            return pages;
        }
        /**
         * Sets the permissions of all pages to <perms>
         *
         * @param perms the new permissions
         */
        void all_perms(uint perms) {
            _perms.set_all(perms);
        }

        /**
         * Swaps the backend of this and <ds>.
         *
         * @param ds the other dataspace
         */
        void swap_backend(DS *ds) {
            uintptr_t org = _desc.origin();
            switch_to(ds->_desc.origin());
            ds->switch_to(org);
        }

        /**
         * Sets the given origin as backend
         *
         * @param origin the backend origin
         */
        void switch_to(uintptr_t origin) {
            _desc.origin(origin);
            all_perms(0);
        }

    private:
        DataSpaceDesc _desc;
        capsel_t _cap;
        MaskField<4> _perms;
    };

    typedef SList<DS>::const_iterator iterator;

    /**
     * Constructor
     */
    explicit ChildMemory() : _list(isless) {
    }
    /**
     * Destructor
     */
    ~ChildMemory() {
        for(iterator it = begin(); it != end(); ) {
            iterator cur = it++;
            delete &*cur;
        }
    }

    /**
     * Determines the memory usage
     *
     * @param virt will be set to the total amount of virtual memory that is in use
     * @param phys will be set to the total amount of physical memory that this child has aquired
     */
    void memusage(size_t &virt, size_t &phys) const {
        virt = 0;
        phys = 0;
        for(iterator it = begin(); it != end(); ++it) {
            virt += it->desc().size();
            if(it->desc().type() != DataSpaceDesc::VIRTUAL && (it->desc().flags() & OWN))
                phys += it->desc().size();
        }
    }

    /**
     * @return the first dataspace
     */
    iterator begin() const {
        return _list.cbegin();
    }
    /**
     * @return end of dataspaces
     */
    iterator end() const {
        return _list.cend();
    }

    /**
     * Finds the dataspace with given selector
     *
     * @param sel the selector
     * @return the dataspace or nullptr if not found
     */
    DS *find(capsel_t sel) {
        return get(sel);
    }
    /**
     * Finds the dataspace with given address
     *
     * @param addr the virtual address
     * @return the dataspace or nullptr if not found
     */
    DS *find_by_addr(uintptr_t addr) {
        for(auto it = _list.begin(); it != _list.end(); ++it) {
            if(addr >= it->desc().virt() && addr < it->desc().virt() + it->desc().size())
                return &*it;
        }
        return nullptr;
    }

    /**
     * Finds a free position in the address space to put in <size> bytes.
     *
     * @param size the number of bytes to map
     * @param align the alignment (has to be a power of 2)
     * @return the address where it can be mapped
     * @throw ChildMemoryException if there is not enough space
     */
    uintptr_t find_free(size_t size, size_t align = 1) const {
        // the list is sorted, so use the last ds
        uintptr_t e = _list.length() > 0 ? _list.ctail()->desc().virt() + _list.ctail()->desc().size() : 0;
        // leave one page space (earlier error detection)
        e = (e + ExecEnv::PAGE_SIZE * 2 - 1) & ~(ExecEnv::PAGE_SIZE - 1);
        // align it
        e = (e + align - 1) & ~(align - 1);
        // check if the size fits below the kernel
        if(e + size < e || e + size > ExecEnv::KERNEL_START) {
            VTHROW(ChildMemoryException, E_CAPACITY,
                   "Unable to allocate " << size << " bytes in childs address space");
        }
        return e;
    }

    /**
     * Adds the given dataspace to the address space
     *
     * @param desc the dataspace descriptor (desc.virt() is expected to contain the address where
     *  the memory is located in the parent (=us))
     * @param addr the virtual address where to map it to in the child
     * @param flags the flags to use (desc.flags() is ignored)
     * @param sel the selector for the dataspace
     */
    void add(const DataSpaceDesc& desc, uintptr_t addr, uint flags, capsel_t sel = ObjCap::INVALID) {
        DS *ds = new DS(DataSpaceDesc(desc.size(), desc.type(), flags, desc.phys(), addr,
                                      desc.virt()), sel);
        _list.insert(ds);
    }

    /**
     * Removes the dataspace with given selector
     *
     * @param sel the selector
     * @return the descriptor
     * @throws ChildMemoryException if not found
     */
    DataSpaceDesc remove(capsel_t sel) {
        return remove(get(sel), nullptr);
    }

    /**
     * Removes the dataspace that contains the given address
     *
     * @param addr the virtual address
     * @param sel will be set to the descriptor, if not nullptr
     * @return the descriptor
     * @throws ChildMemoryException if not found
     */
    DataSpaceDesc remove_by_addr(uintptr_t addr, capsel_t *sel = nullptr) {
        return remove(find_by_addr(addr), sel);
    }

private:
    DS *get(capsel_t sel) {
        for(auto it = _list.begin(); it != _list.end(); ++it) {
            if(it->cap() == sel)
                return &*it;
        }
        return nullptr;
    }
    DataSpaceDesc remove(DS *ds, capsel_t *sel) {
        DataSpaceDesc desc;
        if(!ds)
            throw ChildMemoryException(E_NOT_FOUND, "Dataspace not found");
        _list.remove(ds);
        if(sel)
            *sel = ds->cap();
        desc = ds->desc();
        delete ds;
        return desc;
    }

    static bool isless(const DS &a, const DS &b) {
        return a.desc().virt() < b.desc().virt();
    }

    SortedSList<DS> _list;
};

OStream &operator<<(OStream &os, const ChildMemory &cm);

}
