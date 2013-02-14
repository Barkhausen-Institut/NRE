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

#include <services/Storage.h>
#include <services/PCIConfig.h>
#include <services/ACPI.h>
#include <util/PCI.h>

#include "Controller.h"

class ControllerMng {
    enum {
        CLASS_STORAGE_CTRL      = 0x1,
        SUBCLASS_IDE            = 0x1,
        SUBCLASS_SATA           = 0x6,
    };

public:
    explicit ControllerMng(bool idedma)
        : _idedma(idedma), _pcicfg("pcicfg"), _acpi("acpi"), _pci(_pcicfg, &_acpi), _count(0), _ctrls() {
        find_ahci_controller();
        find_ide_controller();
    }

    bool exists(size_t ctrl) const {
        return ctrl < nre::Storage::MAX_CONTROLLER && _ctrls[ctrl] != nullptr;
    }
    Controller *get(size_t ctrl) const {
        return _ctrls[ctrl];
    }

private:
    void find_ahci_controller();
    void find_ide_controller();

    bool _idedma;
    nre::PCIConfigSession _pcicfg;
    nre::ACPISession _acpi;
    nre::PCI _pci;
    size_t _count;
    Controller *_ctrls[nre::Storage::MAX_CONTROLLER];
};
