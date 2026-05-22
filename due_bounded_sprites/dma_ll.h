#pragma once
#include <SAM3XDUE.h>

#define END_OF_LIST 0

/*
	"linked list item"
	an lli has everything the dmac needs to send one command
*/
struct lli {
	uint32_t saddr = 0; //points to source of data
	uint32_t daddr = 0; //points to address of destination
	uint32_t ctrla = 0; //config
	uint32_t ctrlb = 0; //config
	uint32_t dscr = 0;	//points to address of next lli (or nullptr if end of list)

	lli() {}

	lli(const lli& o) {
		this->saddr = o.saddr;
		this->daddr = o.daddr;
		this->ctrla = o.ctrla;
		this->ctrlb = o.ctrlb;
		this->dscr = o.dscr;
	}

	lli(uint32_t saddr, uint32_t daddr, uint32_t ctrla, uint32_t ctrlb, uint32_t dscr) {
		this->saddr = saddr;
		this->daddr = daddr;
		this->ctrla = ctrla;
		this->ctrlb = ctrlb;
		this->dscr = dscr;
	}

	void operator=(volatile lli& o) volatile {
		this->saddr = o.saddr;
		this->daddr = o.daddr;
		this->ctrla = o.ctrla;
		this->ctrlb = o.ctrlb;
		this->dscr = o.dscr;
	}

	void operator=(const lli& o) volatile {
		this->saddr = o.saddr;
		this->daddr = o.daddr;
		this->ctrla = o.ctrla;
		this->ctrlb = o.ctrlb;
		this->dscr = o.dscr;
	}
	void operator=(const volatile lli& o) volatile {
		this->saddr = o.saddr;
		this->daddr = o.daddr;
		this->ctrla = o.ctrla;
		this->ctrlb = o.ctrlb;
		this->dscr = o.dscr;
	}
};