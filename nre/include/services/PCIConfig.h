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
#include <ipc/PtClientSession.h>
#include <utcb/UtcbFrame.h>
#include <util/BDF.h>
#include <Exception.h>
#include <CPU.h>

namespace nre {

/**
 * Types for the PCI configuration service
 */
class PCIConfig {
public:
    typedef uint32_t value_type;

    /**
     * The available commands
     */
    enum Command {
        READ,
        WRITE,
        ADDR,
        REBOOT,
        SEARCH_DEVICE,
        SEARCH_BRIDGE
    };

private:
    PCIConfig();
};

/**
 * Represents a session at the PCI configuration service
 */
class PCIConfigSession : public PtClientSession {
    typedef PCIConfig::value_type value_type;

public:
    /**
     * Creates a new session at given service
     *
     * @param service the service name
     */
    explicit PCIConfigSession(const char *service) : PtClientSession(service) {
    }

    /**
     * Reads a value from given bdf and offset
     *
     * @param bdf the bus-device-function triple
     * @param offset the offset
     * @return the value
     * @throws Exception if not found
     */
    value_type read(BDF bdf, size_t offset) const {
        UtcbFrame uf;
        uf << PCIConfig::READ << bdf << offset;
        pt().call(uf);
        uf.check_reply();
        uint32_t value;
        uf >> value;
        return value;
    }

    /**
     * Writes the given value to given bdf and offset
     *
     * @param bdf the bus-device-function triple
     * @param offset the offset
     * @param value the value to write
     * @throws Exception if not found
     */
    void write(BDF bdf, size_t offset, value_type value) {
        UtcbFrame uf;
        uf << PCIConfig::WRITE << bdf << offset << value;
        pt().call(uf);
        uf.check_reply();
    }

    /**
     * Determines the address of given bdf and offset
     *
     * @param bdf the bus-device-function triple
     * @param offset the offset
     * @return the address
     * @throws Exception if not found
     */
    uintptr_t addr(BDF bdf, size_t offset) const {
        UtcbFrame uf;
        uf << PCIConfig::ADDR << bdf << offset;
        pt().call(uf);
        uf.check_reply();
        uintptr_t addr;
        uf >> addr;
        return addr;
    }

    /**
     * Searches for the <inst>'th device that has the given class and/or subclass.
     *
     * @param theclass the class of the device (~0U = ignore)
     * @param subclass the subclass of the device (~0U = ignore)
     * @param inst the instance of the device (~0U = ignore)
     * @return the bus-device-function triple if found
     * @throws Exception if the device was not found
     */
    BDF search_device(value_type theclass = ~0U, value_type subclass = ~0U,
                           uint inst = ~0U) const {
        UtcbFrame uf;
        uf << PCIConfig::SEARCH_DEVICE << theclass << subclass << inst;
        pt().call(uf);
        uf.check_reply();
        BDF bdf;
        uf >> bdf;
        return bdf;
    }

    /**
     * Searches for the bridge with given id
     *
     * @param dst the bridge id
     * @return the bus-device-function triple if found
     * @throws Exception if the bridge was not found
     */
    BDF search_bridge(BDF dst) const {
        UtcbFrame uf;
        uf << PCIConfig::SEARCH_BRIDGE << dst;
        pt().call(uf);
        uf.check_reply();
        BDF bdf;
        uf >> bdf;
        return bdf;
    }

    /**
     * Tries to reboot the PCI via the PCI configuration space
     */
    void reboot() {
        UtcbFrame uf;
        uf << PCIConfig::REBOOT;
        pt().call(uf);
        uf.check_reply();
    }
};

}
