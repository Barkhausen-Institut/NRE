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

#include <Logging.h>

#include "PhysicalMemory.h"
#include "VirtualMemory.h"
#include "Hypervisor.h"

using namespace nre;

PhysicalMemory::MemRegion *PhysicalMemory::MemRegion::_free = nullptr;
PhysicalMemory::RootDataSpace *PhysicalMemory::RootDataSpace::_free = nullptr;
size_t PhysicalMemory::_totalsize = 0;
PhysicalMemory::MemRegion PhysicalMemory::MemRegManager::_initial_regs[64];
bool PhysicalMemory::MemRegManager::_initial_added = false;
PhysicalMemory::MemRegManager PhysicalMemory::_mem INIT_PRIO_PMEM;
DataSpaceManager<PhysicalMemory::RootDataSpace> PhysicalMemory::_dsmng INIT_PRIO_PMEM;

void *PhysicalMemory::MemRegion::operator new(size_t) throw() {
    if(!MemRegManager::_initial_added) {
        for(size_t i = 0; i < ARRAY_SIZE(MemRegManager::_initial_regs); ++i) {
            MemRegManager::_initial_regs[i]._next = _free;
            _free = MemRegManager::_initial_regs + i;
        }
        MemRegManager::_initial_added = true;
    }

    if(_free == nullptr) {
        // if total size is still 0, we haven't mapped the physical memory yet, which means that
        // during the calculation of the usable memory, we ran out of regions.
        if(_totalsize == 0)
            throw Exception(E_CAPACITY, "Not enough initial regions");
        uintptr_t phys = PhysicalMemory::_mem.alloc_safe(ExecEnv::PAGE_SIZE);
        uintptr_t virt = VirtualMemory::phys_to_virt(phys);
        MemRegion *reg = reinterpret_cast<MemRegion*>(virt);
        for(size_t i = 0; i < ExecEnv::PAGE_SIZE / sizeof(MemRegion); ++i) {
            reg->_next = _free;
            _free = reg;
            reg++;
        }
    }

    MemRegion *res = _free;
    _free = _free->_next;
    return res;
}

void PhysicalMemory::MemRegion::operator delete(void *ptr) throw() {
    MemRegion *reg = static_cast<MemRegion*>(ptr);
    reg->_next = _free;
    _free = reg;
}

PhysicalMemory::RootDataSpace::RootDataSpace(const DataSpaceDesc &desc)
    : _desc(desc), _map(0), _unmap(0), _next() {
    _desc.size(Math::round_up<size_t>(_desc.size(), ExecEnv::PAGE_SIZE));

    uint flags = _desc.flags();
    if(_desc.phys() != 0) {
        _desc.phys(Math::round_dn<uintptr_t>(_desc.phys(), ExecEnv::PAGE_SIZE));
        if(!PhysicalMemory::can_map(_desc.phys(), _desc.size(), flags)) {
            VTHROW(DataSpaceException, E_ARGS_INVALID,
                   "Unable to map physical memory " << fmt(_desc.phys(), "p") << ".."
                                                    << fmt(_desc.phys() + _desc.size(), "p"));
        }

        _desc.virt(VirtualMemory::alloc(_desc.size()));
        _desc.origin(_desc.phys());
        Hypervisor::map_mem(_desc.phys(), _desc.virt(), _desc.size());
    }
    else {
        size_t align = 1UL << (_desc.align() + ExecEnv::PAGE_SHIFT);
        if(align < ExecEnv::BIG_PAGE_SIZE || _desc.size() < ExecEnv::BIG_PAGE_SIZE)
            flags &= ~DataSpaceDesc::BIGPAGES;

        _desc.phys(alloc(_desc.size(), align));
        _desc.origin(_desc.phys());
        _desc.virt(VirtualMemory::phys_to_virt(_desc.phys()));
    }
    _desc.flags(flags);
}

PhysicalMemory::RootDataSpace::RootDataSpace(capsel_t pid)
    : _desc(), _map(0, true), _unmap(0, true), _next() {
    // if we want to join a dataspace that does not exist in the root-task, its always an error
    VTHROW(DataSpaceException, E_NOT_FOUND, "Dataspace " << pid << " not found in root");
}

PhysicalMemory::RootDataSpace::~RootDataSpace() {
    // release memory
    uint flags = _desc.flags();
    bool isdev = PhysicalMemory::can_map(_desc.phys(), _desc.size(), flags);
    if(!isdev) {
        revoke_mem(_desc.virt(), _desc.size(), false);
        free(_desc.phys(), _desc.size());
    }
    else {
        revoke_mem(_desc.virt(), _desc.size(), true);
        VirtualMemory::free(_desc.virt(), _desc.size());
    }
}

void *PhysicalMemory::RootDataSpace::operator new(size_t) throw() {
    if(_free == nullptr) {
        uintptr_t phys = alloc(ExecEnv::PAGE_SIZE);
        uintptr_t virt = VirtualMemory::alloc(ExecEnv::PAGE_SIZE);
        Hypervisor::map_mem(phys, virt, ExecEnv::PAGE_SIZE);
        RootDataSpace *ds = reinterpret_cast<RootDataSpace*>(virt);
        for(size_t i = 0; i < ExecEnv::PAGE_SIZE / sizeof(RootDataSpace); ++i) {
            ds->_next = _free;
            _free = ds;
            ds++;
        }
    }

    RootDataSpace *res = _free;
    _free = _free->_next;
    return res;
}

void PhysicalMemory::RootDataSpace::operator delete(void *ptr) throw() {
    RootDataSpace *ds = static_cast<RootDataSpace*>(ptr);
    ds->_next = _free;
    _free = ds;
}

void PhysicalMemory::RootDataSpace::revoke_mem(uintptr_t addr, size_t size, bool self) {
    size_t count = size >> ExecEnv::PAGE_SHIFT;
    uintptr_t start = addr >> ExecEnv::PAGE_SHIFT;
    CapRange(start, count, Crd::MEM_ALL).revoke(self);
}

void PhysicalMemory::add(uintptr_t addr, size_t size) {
    if(VirtualMemory::alloc_ram(addr, size))
        free(addr, size);
}

void PhysicalMemory::remove(uintptr_t addr, size_t size) {
    _mem.alloc_at(addr, size, false);
}

void PhysicalMemory::map_all() {
    for(auto it = _mem.begin(); it != _mem.end(); ++it) {
        if(it->size)
            Hypervisor::map_mem(it->addr, VirtualMemory::phys_to_virt(it->addr), it->size);
    }
    _totalsize = _mem.total_count();
}

bool PhysicalMemory::can_map(uintptr_t phys, size_t size, uint &flags) {
    const Hip &hip = Hip::get();
    // check for overflow
    if(phys + size < phys)
        return false;

    // check if its a module
    for(auto it = hip.mem_begin(); it != hip.mem_end(); ++it) {
        if(it->type == HipMem::MB_MODULE) {
            uintptr_t end = Math::round_up<size_t>(it->addr + it->size, ExecEnv::PAGE_SIZE);
            if(phys >= it->addr && phys + size <= end) {
                // don't give the user write-access here
                flags = DataSpaceDesc::R;
                return true;
            }
        }
    }

    // check if the child wants to request none-device-memory
    for(auto it = hip.mem_begin(); it != hip.mem_end(); ++it) {
        // if addr is 0 its the BIOS area, which is ok
        if(it->addr != 0 && !it->reserved() && Math::overlapped(phys, size, it->addr, it->size))
            return false;
    }
    return true;
}

void PhysicalMemory::portal_dataspace(capsel_t) {
    UtcbFrameRef uf;
    try {
        capsel_t sel = 0;
        DataSpaceDesc desc;
        DataSpace::RequestType type;
        uf >> type;
        if(type == DataSpace::JOIN || type == DataSpace::DESTROY)
            sel = uf.get_translated(0).offset();
        if(type != DataSpace::JOIN)
            uf >> desc;
        uf.finish_input();

        switch(type) {
            case DataSpace::CREATE:
            case DataSpace::JOIN:
                if(type != DataSpace::JOIN && desc.type() == DataSpaceDesc::VIRTUAL) {
                    uintptr_t addr = VirtualMemory::alloc(desc.size());
                    desc = DataSpaceDesc(desc.size(), desc.type(), desc.flags(), 0, addr);
                    LOG(DATASPACES, "Root: Allocated virtual ds " << desc << "\n");
                    uf << E_SUCCESS << desc;
                }
                else {
                    const RootDataSpace &ds = type == DataSpace::JOIN
                                              ? _dsmng.join(sel)
                                              : _dsmng.create(desc);

                    if(type == DataSpace::CREATE) {
                        LOG(DATASPACES, "Root: Created " << ds << "\n");
                        uf.delegate(ds.sel(), 0);
                        uf.delegate(ds.unmapsel(), 1);
                    }
                    else {
                        LOG(DATASPACES, "Root: Joined " << ds << "\n");
                        uf.delegate(ds.unmapsel());
                    }
                    // pass back attributes so that the caller has the correct ones
                    uf << E_SUCCESS << ds.desc();
                }
                break;

            case DataSpace::DESTROY:
                if(desc.type() == DataSpaceDesc::VIRTUAL) {
                    LOG(DATASPACES, "Root: Destroyed virtual ds: " << desc << "\n");
                    VirtualMemory::free(desc.virt(), desc.size());
                }
                else {
                    LOG(DATASPACES, "Root: Destroyed ds " << sel << ": " << desc << "\n");
                    _dsmng.release(desc, sel);
                }
                uf << E_SUCCESS;
                break;

            case DataSpace::SWITCH_TO:
                assert(false);
                break;
        }
    }
    catch(const Exception& e) {
        uf.clear();
        uf << e;
    }
}
