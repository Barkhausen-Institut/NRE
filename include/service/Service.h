/*
 * TODO comment me
 *
 * Copyright (C) 2012, Nils Asmussen <nils@os.inf.tu-dresden.de>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of NUL.
 *
 * NUL is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * NUL is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 2 for more details.
 */

#pragma once

#include <kobj/LocalEc.h>
#include <kobj/Pt.h>
#include <kobj/Sm.h>
#include <kobj/UserSm.h>
#include <service/ServiceInstance.h>
#include <mem/DataSpace.h>
#include <utcb/UtcbFrame.h>
#include <Exception.h>
#include <ScopedPtr.h>
#include <BitField.h>
#include <CPU.h>

namespace nul {

class Service;

template<class T>
class SessionIterator;

class ServiceException : public Exception {
public:
	explicit ServiceException(ErrorCode code) throw() : Exception(code) {
	}
};

class SessionData {
	friend class ServiceInstance;

public:
	SessionData(Service *s,capsel_t pts,Pt::portal_func func);
	virtual ~SessionData() {
		for(uint i = 0; i < Hip::MAX_CPUS; ++i) {
			if(_pts[i])
				delete _pts[i];
		}
		delete _ds;
	}

	capsel_t caps() const {
		return _caps;
	}
	const DataSpace *ds() const {
		return _ds;
	}

protected:
	virtual void set_ds(DataSpace *ds) {
		_ds = ds;
	}

private:
	capsel_t _caps;
	Pt *_pts[Hip::MAX_CPUS];
	DataSpace *_ds;
};

class Service {
	friend class ServiceInstance;
	template<class T>
	friend class SessionIterator;

public:
	enum Command {
		OPEN_SESSION,
		SHARE_DATASPACE
	};

	enum {
		MAX_SESSIONS		= 32
	};

	Service(const char *name,Pt::portal_func portal)
		: _regcaps(CapSpace::get().allocate(Hip::MAX_CPUS,Hip::MAX_CPUS)),
		  _caps(CapSpace::get().allocate(MAX_SESSIONS * Hip::MAX_CPUS,MAX_SESSIONS * Hip::MAX_CPUS)),
		  _sm(), _name(name), _func(portal), _insts(), _sessions() {
	}
	virtual ~Service() {
		for(size_t i = 0; i < MAX_SESSIONS; ++i)
			delete _sessions[i];
		for(size_t i = 0; i < Hip::MAX_CPUS; ++i)
			delete _insts[i];
		CapSpace::get().free(_caps,MAX_SESSIONS * Hip::MAX_CPUS);
		CapSpace::get().free(_regcaps,Hip::MAX_CPUS);
	}

	const char *name() const {
		return _name;
	}
	Pt::portal_func portal() const {
		return _func;
	}

	template<class T>
	SessionIterator<T> sessions_begin();
	template<class T>
	SessionIterator<T> sessions_end();

	template<class T>
	T *get_session(capsel_t pid) {
		T *sess = static_cast<T*>(_sessions[(pid - _caps) / Hip::MAX_CPUS]);
		if(!sess)
			throw ServiceException(E_ARGS_INVALID);
		return sess;
	}

	void provide_on(cpu_t cpu) {
		assert(_insts[cpu] == 0);
		_insts[cpu] = new ServiceInstance(this,_regcaps + cpu,cpu);
		_bf.set(cpu);
	}
	LocalEc *get_ec(cpu_t cpu) const {
		return _insts[cpu] != 0 ? &_insts[cpu]->ec() : 0;
	}
	void reg() {
		UtcbFrame uf;
		uf.delegate(CapRange(_regcaps,Util::nextpow2<size_t>(Hip::MAX_CPUS),DESC_CAP_ALL));
		uf << String(_name) << _bf;
		CPU::current().reg_pt->call(uf);
		ErrorCode res;
		uf >> res;
		if(res != E_SUCCESS)
			throw ServiceException(res);
	}

	// TODO wrong place?
	void wait() {
		Sm sm(0);
		sm.down();
	}

private:
	virtual SessionData *create_session(capsel_t pts,Pt::portal_func func) {
		return new SessionData(this,pts,func);
	}

	SessionData *new_session() {
		ScopedLock<UserSm> guard(&_sm);
		for(size_t i = 0; i < MAX_SESSIONS; ++i) {
			if(_sessions[i] == 0) {
				_sessions[i] = create_session(_caps + i * Hip::MAX_CPUS,_func);
				return _sessions[i];
			}
		}
		throw ServiceException(E_CAPACITY);
	}

	Service(const Service&);
	Service& operator=(const Service&);

	capsel_t _regcaps;
	capsel_t _caps;
	UserSm _sm;
	const char *_name;
	Pt::portal_func _func;
	ServiceInstance *_insts[Hip::MAX_CPUS];
	BitField<Hip::MAX_CPUS> _bf;
	SessionData *_sessions[MAX_SESSIONS];
};

template<class T>
class SessionIterator {
	friend class Service;

	SessionIterator(Service *s,size_t pos = 0) : _s(s), _pos(pos) {
	}

public:
	~SessionIterator() {
	}

	T& operator *() const {
		return *static_cast<T*>(_s->_sessions[_pos]);
	}
	T* operator ->() const {
		return &(operator*());
	}
	SessionIterator& operator ++() {
		while(_pos < Service::MAX_SESSIONS - 1 && !_s->_sessions[++_pos])
			;
		return *this;
	}
	SessionIterator operator ++(int) {
		SessionIterator<T> tmp(*this);
		operator++();
		return tmp;
	}
	SessionIterator& operator --() {
		while(_pos > 0 && !_s->_sessions[--_pos])
			;
		return *this;
	}
	SessionIterator operator --(int) {
		SessionIterator<T> tmp(*this);
		operator--();
		return tmp;
	}
	bool operator ==(const SessionIterator<T>& rhs) {
		return _pos == rhs._pos;
	}
	bool operator !=(const SessionIterator<T>& rhs) {
		return _pos != rhs._pos;
	}

private:
	Service* _s;
	size_t _pos;
};


template<class T>
SessionIterator<T> Service::sessions_begin() {
	return SessionIterator<T>(this);
}

template<class T>
SessionIterator<T> Service::sessions_end() {
	return SessionIterator<T>(this,MAX_SESSIONS);
}

}
