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
#include <services/PCIConfig.h>
#include <mem/DataSpace.h>
#include <utcb/UtcbFrame.h>
#include <Exception.h>
#include <CPU.h>

namespace nre {

/**
 * Types for the ACPI service
 */
class ACPI {
public:
    /**
     * The available commands
     */
    enum Command {
        FIND_TABLE,
        IRQ_TO_GSI,
        GET_GSI,
    };

    /**
     * Root system descriptor table
     */
    struct RSDT {
        char signature[4];
        uint32_t length;
        uint8_t revision;
        uint8_t checksum;
        char oemId[6];
        char oemTableId[8];
        uint32_t oemRevision;
        char creatorId[4];
        uint32_t creatorRevision;
    } PACKED;

private:
    ACPI();
};

/**
 * Represents a session at the ACPI service
 */
class ACPISession : public PtClientSession {
public:
    /**
     * Creates a new session at given service
     *
     * @param service the service name
     */
    explicit ACPISession(const char *service) : PtClientSession(service) {
    }

    /**
     * Finds the ACPI table with given name
     *
     * @param name the name of the table
     * @param instance the instance that is encountered (0 = the first one, 1 = the second, ...)
     * @return a dataspace that contains the table
     * @throws Exception if the table doesn't exist
     */
    DataSpace find_table(const String &name, uint instance = 0) const {
        ScopedCapSels cap;
        UtcbFrame uf;
        uf.delegation_window(Crd(cap.get(), 0, Crd::OBJ_ALL));
        uf << ACPI::FIND_TABLE << name << instance;
        pt().call(uf);
        uf.check_reply();
        return DataSpace(cap.release());
    }

    /**
     * Determines the GSI that corresponds to the given ISA IRQ. If the MADT is present, it will
     * be searched for an interrupt source override entry for that IRQ. If not found or MADT
     * is not present, it will be assumed that the IRQ is identity mapped to the GSI.
     *
     * @param irq the ISA IRQ
     * @return the GSI
     */
    uint irq_to_gsi(uint irq) const {
        UtcbFrame uf;
        uf << ACPI::IRQ_TO_GSI << irq;
        pt().call(uf);

        uf.check_reply();
        uint gsi;
        uf >> gsi;
        return gsi;
    }

    /**
     * Search for the GSI that is triggered by the given device, specified by <bdf>.
     *
     * @param bdf the bus-device-function triple of the device
     * @param pin the IRQ pin of the device
     * @param parent_bdf the bus-device-function triple of the parent device (e.g. bridge)
     * @return the GSI
     */
    uint get_gsi(BDF bdf, uint8_t pin, BDF parent_bdf) const {
        UtcbFrame uf;
        uf << ACPI::GET_GSI << bdf << pin << parent_bdf;
        pt().call(uf);

        uf.check_reply();
        uint gsi;
        uf >> gsi;
        return gsi;
    }
};

}
