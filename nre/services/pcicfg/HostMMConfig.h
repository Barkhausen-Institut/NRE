/*
 * Copyright (C) 2012, Nils Asmussen <nils@os.inf.tu-dresden.de>
 * Copyright (C) 2010, Julian Stecklina <jsteckli@os.inf.tu-dresden.de>
 * Copyright (C) 2010, Bernhard Kauer <bk@vmmon.org>
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
#include <mem/DataSpace.h>
#include <collection/SList.h>
#include <util/Math.h>
#include <Compiler.h>

#include "Config.h"

class HostMMConfig : public Config {
    struct AcpiMCFG {
        uint32_t magic;
        uint32_t len;
        uint8_t rev;
        uint8_t checksum;
        uint8_t oem_id[6];
        uint8_t model_id[8];
        uint32_t oem_rev;
        uint32_t creator_vendor;
        uint32_t creator_utility;
        uint8_t _res[8];
        struct Entry {
            uint64_t base;
            uint16_t pci_seg;
            uint8_t pci_bus_start;
            uint8_t pci_bus_end;
            uint32_t _res;
        } PACKED entries[];
    } PACKED;

    class MMConfigRange : public nre::SListItem {
    public:
        explicit MMConfigRange(uintptr_t base, uintptr_t start, size_t size)
            : _start(start), _size(size),
              _ds(size * nre::ExecEnv::PAGE_SIZE, nre::DataSpaceDesc::LOCKED, nre::DataSpaceDesc::R,
                  base),
              _mmconfig(reinterpret_cast<value_type*>(_ds.virt())) {
        }

        uintptr_t addr(nre::BDF bdf, size_t offset) const {
            return _ds.phys() + field(bdf, offset) * sizeof(value_type);
        }
        bool contains(nre::BDF bdf, size_t offset) const {
            return offset < 0x1000 && nre::Math::in_range(bdf.value(), _start, _size);
        }
        value_type read(nre::BDF bdf, size_t offset) const {
            return _mmconfig[field(bdf, offset)];
        }
        void write(nre::BDF bdf, size_t offset, value_type value) {
            _mmconfig[field(bdf, offset)] = value;
        }

    private:
        size_t field(nre::BDF bdf, size_t offset) const {
            return (bdf.value() << 10) + ((offset >> 2) & 0x3FF);
        }

    private:
        uintptr_t _start;
        size_t _size;
        nre::DataSpace _ds;
        value_type *_mmconfig;
    };

public:
    explicit HostMMConfig();

    virtual const char *name() const {
        return "MMConfig";
    }
    virtual bool contains(nre::BDF bdf, size_t offset) const {
        for(auto it = _ranges.cbegin(); it != _ranges.cend(); ++it) {
            if(it->contains(bdf, offset))
                return true;
        }
        return false;
    }
    virtual uintptr_t addr(nre::BDF bdf, size_t offset) {
        MMConfigRange *range = find(bdf, offset);
        return range->addr(bdf, offset);
    }
    virtual value_type read(nre::BDF bdf, size_t offset) {
        MMConfigRange *range = find(bdf, offset);
        return range->read(bdf, offset);
    }
    virtual void write(nre::BDF bdf, size_t offset, value_type value) {
        MMConfigRange *range = find(bdf, offset);
        range->write(bdf, offset, value);
    }

private:
    MMConfigRange *find(nre::BDF bdf, size_t offset) {
        for(auto it = _ranges.begin(); it != _ranges.end(); ++it) {
            if(it->contains(bdf, offset))
                return &*it;
        }
        VTHROW(Exception, E_NOT_FOUND,
               "Unable to find " << bdf << "+" << nre::fmt(offset, "#x") << " in MMConfig");
    }

    nre::SList<MMConfigRange> _ranges;
};
