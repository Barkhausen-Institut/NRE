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

#include <kobj/ObjCap.h>
#include <kobj/GlobalThread.h>
#include <kobj/VCpu.h>
#include <kobj/Pd.h>
#include <String.h>

namespace nre {

/**
 * Represents a scheduling context. You can't create an instance of this class, because this is
 * done by GlobalThread or VCpu.
 */
class Sc : public ObjCap {
    friend class GlobalThread;
    friend class VCpu;

public:
    enum Command {
        ALLOC,
        CREATE,
        JOIN,
        DESTROY
    };

    /**
     * @return the ec it is bound to
     */
    Ec *ec() {
        return _ec;
    }
    /**
     * @return the quantum-priority descriptor (might be changed by start())
     */
    Qpd qpd() const {
        return _qpd;
    }

private:
    /**
     * Binds this object to the given sc-selector for thread <gt>. This is intended for the main
     * thread of each application.
     *
     * @param gt the global thread
     * @param sel the selector
     */
    explicit Sc(GlobalThread *gt, capsel_t sel)
        : ObjCap(sel, ObjCap::KEEP_SEL_BIT | ObjCap::KEEP_CAP_BIT), _ec(gt), _qpd() {
    }
    /**
     * Creates a new Sc that is bound to the given GlobalThread. Note that it does NOT start it. Please
     * call start() afterwards.
     *
     * @param ec the GlobalThread to bind it to
     * @param qpd the quantum-priority descriptor for the Sc
     */
    explicit Sc(GlobalThread *ec, Qpd qpd) : ObjCap(), _ec(ec), _qpd(qpd) {
        // don't create the Sc here, because then we have no chance to store the created object
        // somewhere to make it accessible for the just started Thread
    }
    /**
     * Creates a new Sc that is bound to the given VCPU. Note that it does NOT start it. Please
     * call start() afterwards.
     *
     * @param vcpu the VCPU to bind it to
     * @param qpd the quantum-priority descriptor for the Sc
     */
    explicit Sc(VCpu *vcpu, Qpd qpd) : ObjCap(), _ec(vcpu), _qpd(qpd) {
    }

    /**
     * Starts the Sc, i.e. the attached GlobalThread.
     *
     * @param name the name
     * @param id the thread id (0 = not joinable)
     */
    void start(const String &name, ulong id);

    Sc(const Sc&);
    Sc& operator=(const Sc&);

    Ec *_ec;
    Qpd _qpd;
};

}
