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

#include <kobj/Pt.h>
#include <kobj/UserSm.h>
#include <mem/DataSpaceManager.h>
#include <region/RegionManager.h>
#include <util/Bytes.h>

/**
 * Manages all physical memory. At the beginning, it is told what memory is available according
 * to the memory map in the Hip. Afterwards, you can allocate something from that and also free
 * it again. Note that all physical memory is directly mapped to VirtualMemory::RAM_BEGIN. Thus,
 * you can get the virtual address for a physical one by using VirtualMemory::phys_to_virt().
 */
class PhysicalMemory {
    class RootDataSpace;
    friend class RootDataSpace;

    /**
     * A special region for root to provide custom new and delete operators (this is necessary
     * because we're building dynamic memory with this stuff)
     */
    struct MemRegion : public nre::Region {
        static void *operator new(size_t size) throw();
        static void operator delete(void *ptr) throw();

    private:
        MemRegion *_next;
        static MemRegion *_free;
    };

    /**
     * Region manager for the physical memory
     */
    class MemRegManager : public nre::RegionManager<MemRegion> {
        friend struct MemRegion;

    public:
        explicit MemRegManager() : nre::RegionManager<MemRegion>() {
        }

        uintptr_t alloc_safe(size_t size) {
            for(auto it = _regs.begin(); it != _regs.end(); ++it) {
                // it has to be > because we can't free the region here
                if(it->size > size) {
                    it->addr += size;
                    it->size -= size;
                    return it->addr - size;
                }
            }
            VTHROW(RegionManagerException, E_CAPACITY, "Unable to allocate " << size << " bytes");
        }

        friend nre::OStream &operator<<(nre::OStream &os, const MemRegManager &rm) {
            for(auto it = rm.begin(); it != rm.end(); ++it) {
                os << "\t" << nre::fmt(it->addr, "p") << " .. " << nre::fmt(it->addr + it->size - 1, "p")
                   << " (" << nre::Bytes(it->size) << ")\n";
            }
            return os;
        }

    private:
        // we need a few regions at the beginning to be able to store available memory regions
        static MemRegion _initial_regs[];
        static bool _initial_added;
    };

    /**
     * The DataSpace equivalent for the root-task which works slightly different because it is
     * the end of the recursion :)
     */
    class RootDataSpace {
    public:
        explicit RootDataSpace() : _desc(), _map(0, true), _unmap(0, true), _next() {
        }
        RootDataSpace(const nre::DataSpaceDesc &desc);
        RootDataSpace(capsel_t);
        ~RootDataSpace();

        capsel_t sel() const {
            return _map.sel();
        }
        capsel_t unmapsel() const {
            return _unmap.sel();
        }
        const nre::DataSpaceDesc &desc() const {
            return _desc;
        }

        // we have to provide custom new and delete operators since we can't use dynamic memory for
        // building dynamic memory :)
        static void *operator new(size_t size) throw();
        static void operator delete(void *ptr) throw();

        friend nre::OStream & operator<<(nre::OStream &os, const RootDataSpace &ds) {
            os << "RootDataSpace[sel=" << nre::fmt(ds.sel(), "#x")
               << ", umsel=" << nre::fmt(ds.unmapsel(), "#x") << "]: " << ds.desc();
            return os;
        }

    private:
        static void revoke_mem(uintptr_t addr, size_t size, bool self = false);

        nre::DataSpaceDesc _desc;
        nre::Sm _map;
        nre::Sm _unmap;
        RootDataSpace *_next;
        static RootDataSpace *_free;
    };

public:
    /**
     * Allocates <size> bytes from the physical memory.
     *
     * @param size the number of bytes to allocate
     * @param align the alignment (in bytes; has to be a power of 2)
     */
    static uintptr_t alloc(size_t size, size_t align = 1) {
        return _mem.alloc(size, align);
    }
    /**
     * Free's the given physical memory
     *
     * @param phys the address
     * @param size the number of bytes
     */
    static void free(uintptr_t phys, size_t size) {
        _mem.free(phys, size);
    }

    /**
     * Only for the startup: Add the given memory to the available list
     */
    static void add(uintptr_t addr, size_t size);
    /**
     * Only for the startup: Remove the given memory from the available list
     */
    static void remove(uintptr_t addr, size_t size);
    /**
     * Only for the startup: Map all available memory. That is, use Hypervisor to delegate the
     * memory from the hypervisor Pd to our Pd.
     */
    static void map_all();

    /**
     * @return the total amount of available physical memory (this is constant after startup)
     */
    static size_t total_size() {
        return _totalsize;
    }
    /**
     * @return the amount of still free physical memory
     */
    static size_t free_size() {
        return _mem.total_count();
    }

    /**
     * @return the list of available physical memory regions
     */
    static const MemRegManager &regions() {
        return _mem;
    }

    /**
     * End-of-recursion service portal
     */
    PORTAL static void portal_dataspace(void*);

private:
    static bool can_map(uintptr_t phys, size_t size, uint &flags);

    PhysicalMemory();

    static size_t _totalsize;
    static MemRegManager _mem;
    static nre::DataSpaceManager<RootDataSpace> _dsmng;
};
