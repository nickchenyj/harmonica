#ifndef __FUNCUNIT_H
#define __FUNCUNIT_H

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <map>

#include <chdl/gateops.h>
#include <chdl/bvec-basic-op.h>

#include <chdl/adder.h>
#include <chdl/shifter.h>
#include <chdl/mux.h>
#include <chdl/enc.h>
#include <chdl/llmem.h>
#include <chdl/memory.h>

#include <chdl/mult.h>
#include <chdl/divider.h>

#include <chdl/opt.h>
#include <chdl/tap.h>
#include <chdl/sim.h>
#include <chdl/netlist.h>
#include <chdl/input.h>

<<<<<<< HEAD
#include "fpu.h"

#define MAXCWIDTH	10

static const unsigned IDLEN(6);
static const unsigned FPU_E(8), FPU_M(23);

=======
#define PROTOTYPE 1
#define blah

#ifdef DEBUG
#define DBGTAP(x) do {TAP(x); } while(0)
#else
#define DBGTAP(x) do {} while(0)
#endif

static const unsigned IDLEN(6);
static const unsigned Q(4);
static const unsigned DELAY(5);
>>>>>>> ad532910e7cba68535873e24e2631d195bf699a7

template <unsigned N, unsigned R> struct fuInput {
	chdl::bvec<N> r0, r1, r2, imm, pc;
	chdl::node p0, p1, hasimm, stall, pdest;
	chdl::bvec<6> op;
	chdl::bvec<IDLEN> iid;
	chdl::bvec<CLOG2(R)> didx;
};

template <unsigned N, unsigned R> struct fuOutput {
	chdl::bvec<N> out;
	chdl::bvec<IDLEN> iid;
	chdl::node valid, pdest;
	chdl::bvec<CLOG2(R)> didx;
};

template <unsigned N, unsigned R> class FuncUnit {
public:
	FuncUnit(bool getExposed = 0) {
		if(getExposed){
			this->outportID = this->outportNum++;
		}
		else
			this->outportID = -1;
	}

	virtual std::vector<unsigned> get_opcodes() = 0;

	virtual fuOutput<N, R> generate(fuInput<N, R> in, chdl::node valid) = 0;
	virtual chdl::node ready() { return chdl::Lit(1); } // Output is ready.
	
	unsigned getLatency(unsigned x) {
		return latency[x];
	}
	static int outportNum;
	
protected:
	std::map<unsigned, unsigned> latency;
	int outportID;
};

template <unsigned N, unsigned R> int FuncUnit<N, R>::outportNum = 0;


// Functional unit with 1-cycle latency supporting all common arithmetic/logic
// instructions.
template <unsigned N, unsigned R> class BasicAlu : public FuncUnit<N, R> {
public:
	std::vector<unsigned> get_opcodes() {
		std::vector<unsigned> ops;
			for (unsigned i = 0x05; i <= 0x0b; ++i) ops.push_back(i); // 0x05 - 0x0b
			for (unsigned i = 0x0f; i <= 0x15; ++i) ops.push_back(i); // 0x0f - 0x15
			for (unsigned i = 0x19; i <= 0x1c; ++i) ops.push_back(i); // 0x19 - 0x1c
			ops.push_back(0x25);

			return ops;
	}

	virtual fuOutput<N, R> generate(fuInput<N, R> in, chdl::node valid) {
		using namespace std;
		using namespace chdl;

		fuOutput<N, R> o;

		bvec<N> a(in.r0), b(Mux(in.hasimm, in.r1, in.imm));

			bvec<N> sum(Adder(a, Mux(in.op[0], b, ~b), in.op[0]));

		vec<64, bvec<N>> mux_in;
		mux_in[0x05] = -a;
		mux_in[0x06] = ~a;
		mux_in[0x07] = a & b;
		mux_in[0x08] = a | b;
		mux_in[0x09] = a ^ b;
		mux_in[0x0a] = sum;
		mux_in[0x0b] = sum;
		mux_in[0x0f] = a << Zext<CLOG2(N)>(b);
		mux_in[0x10] = a >> Zext<CLOG2(N)>(b);
		mux_in[0x11] = mux_in[0x07];
		mux_in[0x12] = mux_in[0x08];
		mux_in[0x13] = mux_in[0x09];
		mux_in[0x14] = sum;
		mux_in[0x15] = sum;
		mux_in[0x19] = mux_in[0x0f];
		mux_in[0x1a] = mux_in[0x10];
		mux_in[0x1b] = in.pc;
		mux_in[0x1c] = in.pc;
		mux_in[0x25] = b;

		node w(!in.stall);
		o.out = Wreg(w, Mux(in.op, mux_in));
		o.valid = Wreg(w, valid);
		o.iid = Wreg(w, in.iid);
		o.didx = Wreg(w, in.didx);
		o.pdest = Wreg(w, in.pdest);
			
		isReady = w;
		return o;
	}
	virtual chdl::node ready() { return isReady; }
	
private:
	chdl::node isReady;
};

// Predicate logic unit. All of the predicate/predicate and register/predicate
// instructions.
template <unsigned N, unsigned R> class PredLu : public FuncUnit<N, R> {
public:
	std::vector<unsigned> get_opcodes() {
		std::vector<unsigned> ops;
		for (unsigned i = 0x26; i <= 0x2c; ++i) ops.push_back(i); // 0x26 - 0x2c
		return ops;
	}

	virtual fuOutput<N, R> generate(fuInput<N, R> in, chdl::node valid) {
		using namespace std;
		using namespace chdl;

		fuOutput<N, R> o;

		bvec<N> r0(in.r0);
		node p0(in.p0), p1(in.p1);

		bvec<64> mux_in;
		mux_in[0x26] = OrN(r0);
		mux_in[0x27] = p0 && p1;
		mux_in[0x28] = p0 || p1;
		mux_in[0x29] = p0 != p1;
		mux_in[0x2a] = !p0;
		mux_in[0x2b] = r0[N-1];
		mux_in[0x2c] = !OrN(r0);

		node w(!in.stall);
		o.out = Zext<N>(bvec<1>(Wreg(w, Mux(in.op, mux_in))));
		o.valid = Wreg(w, valid);
		o.iid = Wreg(w, in.iid);
		o.didx = Wreg(w, in.didx);
		o.pdest = Wreg(w, in.pdest);
	
		isReady = w;

		return o;
	}
	chdl::node ready() { return isReady; }
	
private:
	chdl::node isReady;
};



// Functional unit with multi-cycle latency supporting integer multiplication
// instructions. 
template <unsigned N, unsigned R> class Multiplier : public FuncUnit<N, R> {
public:
	Multiplier(bool getExposed = 0) : FuncUnit<N, R> (getExposed) {
		//printf("Integer multiplier, id = %d\n", this->outportID);
		this->latency[0x0c] = 30;		//mul
		this->latency[0x16] = 30;		//muli
	}
	
	std::vector<unsigned> get_opcodes() {
		std::vector<unsigned> ops;
		ops.push_back(0x0c);
		ops.push_back(0x16);
		return ops;
	}

	virtual fuOutput<N, R> generate(fuInput<N, R> in, chdl::node valid) {
		using namespace std;
		using namespace chdl;

		fuOutput<N, R> o;
		
		node finish, go;
		bvec<N> a(in.r0), b(Mux(in.hasimm, in.r1, in.imm));
		bvec<N> res0;
		
		//port name
		ostringstream str;
		str << "int" << "Mul";
		
		//counter, and latency mux that dynamially selects max counter value for counter
		bvec<MAXCWIDTH> count;
		vec<64, bvec<MAXCWIDTH>> l_mux;
		std::vector<unsigned> op = get_opcodes();
		for(unsigned i = 0, s; i < op.size(); ++i){
			l_mux[op[i]] = Lit<MAXCWIDTH>(this->getLatency(op[i]));
		}
		//l_mux[0x0c] = Lit<MAXCWIDTH>(30);	//mul
		//l_mux[0x16] = l_mux[0x0c];			//muli
		
		//counter logic
		//if !valid				set counter to 0
		//else (valid, ongoing computation)
		//		if( counter < max value, not finished)
		//			counter++
		//		else (computation finished)
		//			if( !in.stall, deliver to next stage and reset counter)
		//				reset counter
		//			else
		//				maintain value
		
		bvec<MAXCWIDTH> latency(Mux(in.op, l_mux));
		
		count = Reg(Mux(valid, Lit<MAXCWIDTH>(0), 
			Mux(count==latency, count+Lit<MAXCWIDTH>(1),
				Mux(!in.stall, count, Lit<MAXCWIDTH>(0)))));
		
		//result mux
		vec<64, bvec<N>> mux_in;
		mux_in[0x0c] = res0;			//mul
		mux_in[0x16] = mux_in[0x0c];	//muli
		
		
		if(this->outportID == -1){
			//built-in functions, allow debugging signals
			res0 = Mult(a, b);
			
			finish = (count==latency);
			
			tap(str.str()+"count", count);
			tap(str.str()+"go", go);
			tap(str.str()+"finish", finish);
			tap(str.str()+"isReady", isReady);
			tap(str.str()+"ivalid", valid);
			tap(str.str()+"in.stall", in.stall);
			tap(str.str()+"ovalid", o.valid);
			
		}
		else{
			//this module is exposed to other verilog module
			//inport: result, finish(whether counter reaches max value) 
			res0 = Input<N> (str.str()+"res0");
			finish = Input(str.str()+"finish");
			
			//outport: ivalid (input valid bit), stall(pipeline stall)
			tap(str.str()+"ivalid", valid);
			tap(str.str()+"stall", in.stall);
		}

		//whether res can be delivered to next stage
		go = finish && (!in.stall);
		
		o.out = Wreg(go, Mux(in.op, mux_in));
		o.valid = Reg(Mux(go, 0, valid));
		o.iid = Wreg(go, in.iid);
		o.didx = Wreg(go, in.didx);
		o.pdest = Wreg(go, in.pdest);
		
		//issue logic issues instructions regardless of isReady bit
		//but input valid bit is put to low if isReady bit is set
		//suggesting previous computation is finished
		isReady = valid && go;
		
		return o;
	}

	virtual chdl::node ready() { return isReady; }

private:
	chdl::node isReady;
};


// Functional unit with multi-cycle latency supporting integer division
// instructions. 
template <unsigned N, unsigned R> class Divider : public FuncUnit<N, R> {
public:
	Divider(bool getExposed = 0) : FuncUnit<N, R> (getExposed) {
		this->latency[0x0d] = 50;		//div
		this->latency[0x0e] = 50;		//mod
		this->latency[0x17] = 50;		//divi
		this->latency[0x18] = 50;		//modi
	}
	
	std::vector<unsigned> get_opcodes() {
		std::vector<unsigned> ops;
		ops.push_back(0x0d);		//div
		ops.push_back(0x0e);		//mod
		ops.push_back(0x17);		//divi
		ops.push_back(0x18);		//modi
		return ops;
	}

	virtual fuOutput<N, R> generate(fuInput<N, R> in, chdl::node valid) {
		using namespace std;
		using namespace chdl;

		fuOutput<N, R> o;
		
		node finish, go;
		bvec<N> a(in.r0), b(Mux(in.hasimm, in.r1, in.imm));
		bvec<N> q;		//quotient
		bvec<N> r;		//remainder
		
		
		//port name
		ostringstream str;
		str << "int" << "Div";
		
		//counter, and latency mux that dynamially selects max counter value for counter
		bvec<MAXCWIDTH> count;
		vec<64, bvec<MAXCWIDTH>> l_mux;
		std::vector<unsigned> op = get_opcodes();
		for(unsigned i = 0, s; i < op.size(); ++i){
			l_mux[op[i]] = Lit<MAXCWIDTH>(this->getLatency(op[i]));
		}
		
		//counter logic
		//if !valid				set counter to 0
		//else (valid, ongoing computation)
		//		if( counter < max value, not finished)
		//			counter++
		//		else (computation finished)
		//			if( !in.stall, deliver to next stage and reset counter)
		//				reset counter
		//			else
		//				maintain value
		bvec<MAXCWIDTH> latency(Mux(in.op, l_mux));
		count = Reg(Mux(valid, Lit<MAXCWIDTH>(0), 
			Mux(count==latency, count+Lit<MAXCWIDTH>(1),
				Mux(!in.stall, count, Lit<MAXCWIDTH>(0)))));
		
		//result mux
		vec<64, bvec<N>> mux_in;
		mux_in[0x0d] = q;
		mux_in[0x0e] = r;
		mux_in[0x17] = mux_in[0x0d];
		mux_in[0x18] = mux_in[0x0e];
		
		
		if(this->outportID == -1){
			//built-in functions, allow debugging signals
			q = divider(a, b, r);
			finish = (count==latency);
			
			tap(str.str()+"count", count);
			tap(str.str()+"go", go);
			tap(str.str()+"finish", finish);
			tap(str.str()+"isReady", isReady);
			tap(str.str()+"ivalid", valid);
			tap(str.str()+"in.stall", in.stall);
			tap(str.str()+"ovalid", o.valid);
		}
		else{
			//this module is exposed to other verilog module
			//inport: result, finish(whether counter reaches max value) 
			q = Input<N> (str.str()+"q");
			r = Input<N> (str.str()+"r");
			finish = Input(str.str()+"finish");
			
			//outport: ivalid (input valid bit), stall(pipeline stall)
			tap(str.str()+"a", a);
			tap(str.str()+"b", b);
			tap(str.str()+"ivalid", valid);
			tap(str.str()+"stall", in.stall);
		}
		
		//whether res can be delivered to next stage
		go = finish && !in.stall;
		
		o.out = Wreg(go, Mux(in.op, mux_in));
		o.valid = Reg(Mux(go, 0, valid));
		o.iid = Wreg(go, in.iid);
		o.didx = Wreg(go, in.didx);
		o.pdest = Wreg(go, in.pdest);
		
		//issue logic issues instructions regardless of isReady bit
		//but input valid bit is put to low if isReady bit is set
		//suggesting previous computation is finished
		isReady = valid&&finish;
		
		return o;
	}

	virtual chdl::node ready() { return isReady; }

private:
	chdl::node isReady;
};



// Basic FP unit with multi-cycle latencies
template <unsigned N, unsigned R> class BasicFpu : public FuncUnit<N, R> {
public:
	BasicFpu(bool getExposed = 0) : FuncUnit<N, R> (getExposed) {
		this->latency[0x33] = 50;		//itof
		this->latency[0x34] = 50;		//ftoi
		this->latency[0x35] = 14;		//fadd
		this->latency[0x36] = 14;		//fsub
		this->latency[0x39] = 50;		//fneg
	}
	
	std::vector<unsigned> get_opcodes() {
		std::vector<unsigned> ops;
		ops.push_back(0x33);		//itof
		ops.push_back(0x34);		//ftoi
		ops.push_back(0x35);		//fadd
		ops.push_back(0x36);		//fsub
		ops.push_back(0x39);		//fneg
		return ops;
	}

	virtual fuOutput<N, R> generate(fuInput<N, R> in, chdl::node valid) {
		using namespace std;
		using namespace chdl;

		fuOutput<N, R> o;
		
		//port name
		ostringstream str;
		str << "fp" << "Basic";
		
		//counter, and latency mux that dynamially selects max counter value for counter
		bvec<MAXCWIDTH> count;
		vec<64, bvec<MAXCWIDTH>> l_mux;
		std::vector<unsigned> op = get_opcodes();
		for(unsigned i = 0, s; i < op.size(); ++i){
			l_mux[op[i]] = Lit<MAXCWIDTH>(this->getLatency(op[i]));
		}
		
		//counter logic
		//if !valid				set counter to 0
		//else (valid, ongoing computation)
		//		if( counter < max value, not finished)
		//			counter++
		//		else (computation finished)
		//			if( !in.stall, deliver to next stage and reset counter)
		//				reset counter
		//			else
		//				maintain value
		bvec<MAXCWIDTH> latency(Mux(in.op, l_mux));
		count = Reg(Mux(valid, Lit<MAXCWIDTH>(0), 
			Mux(count==latency, count+Lit<MAXCWIDTH>(1),
				Mux(!in.stall, count, Lit<MAXCWIDTH>(0)))));
		
		
		bvec<N> a(in.r0);
		bvec<N> b(Mux(in.hasimm, in.r1, in.imm));
		
		//result mux
		vec<64, bvec<N>> mux_in;
		
		
		node finish, go;
		
		if(this->outportID == -1){
			//built-in functions, allow debugging signals
			floatnum <FPU_E, FPU_M> itof (Itof<FPU_E, FPU_M, N> (a));
			bvec<N> ftoi (Ftoi<FPU_E, FPU_M, N> (a));
			floatnum <FPU_E, FPU_M> fadd (Fadd<FPU_E, FPU_M> (a, b));
			floatnum <FPU_E, FPU_M> fsub (Fadd<FPU_E, FPU_M> (a, b));
			bvec<N> fneg (Fneg<N>(a));
		
			
			mux_in[0x33] = (itof);
			mux_in[0x34] = ftoi;
			mux_in[0x35] = (fadd);
			mux_in[0x36] = (fsub);
			mux_in[0x39] = (fneg);
		
			finish = (count==latency);
			
			tap(str.str()+"count", count);
			tap(str.str()+"go", go);
			tap(str.str()+"finish", finish);
			tap(str.str()+"isReady", isReady);
			tap(str.str()+"ivalid", valid);
			tap(str.str()+"in.stall", in.stall);
			tap(str.str()+"ovalid", o.valid);
		}
		else{
			//this module is exposed to other verilog module
			//inport: result, finish(whether counter reaches max value) 
			bvec<N> vitof = Input<N> (str.str()+"itof");
			bvec<N> vftoi = Input<N> (str.str()+"ftoi");;
			bvec<N> vfadd = Input<N> (str.str()+"fadd");
			bvec<N> vfsub = Input<N> (str.str()+"fsub");
			bvec<N> vfneg = Input<N> (str.str()+"fneg");;
			bvec<3> exceptions = Input<3> (str.str()+"excp");
			mux_in[0x33] = vitof;
			mux_in[0x34] = vftoi;
			mux_in[0x35] = vfadd;
			mux_in[0x36] = vfsub;
			mux_in[0x39] = vfneg;
		
			finish = Input(str.str()+"finish");
			
			//outport: ivalid (input valid bit), stall(pipeline stall)
			tap(str.str()+"a", a);
			tap(str.str()+"b", b); 
			tap(str.str()+"ivalid", valid);
			tap(str.str()+"stall", in.stall);
			tap(str.str()+"sub", in.op==Lit<IDLEN>(0x36));
		}
		
		//whether res can be delivered to next stage
		go = finish && !in.stall;
		
		o.out = Wreg(go, Mux(in.op, mux_in));
		o.valid = Reg(Mux(go, 0, valid));
		o.iid = Wreg(go, in.iid);
		o.didx = Wreg(go, in.didx);
		o.pdest = Wreg(go, in.pdest);
		
		//issue logic issues instructions regardless of isReady bit
		//but input valid bit is put to low if isReady bit is set
		//suggesting previous computation is finished
		isReady = valid&&finish;
		
		return o;
	}

	virtual chdl::node ready() { return isReady; }

private:
	chdl::node isReady;
};



template <unsigned N, unsigned R> class FpMult : public FuncUnit<N, R> {
public:
	FpMult(bool getExposed = 0) : FuncUnit<N, R> (getExposed) {
		this->latency[0x37] = 11;		//imul
	}
	
	std::vector<unsigned> get_opcodes() {
		std::vector<unsigned> ops;
		ops.push_back(0x37);		//imul
		return ops;
	}

	virtual fuOutput<N, R> generate(fuInput<N, R> in, chdl::node valid) {
		using namespace std;
		using namespace chdl;

		fuOutput<N, R> o;
		
		//port name
		ostringstream str;
		str << "fp" << "Mult";
		
		//counter, and latency mux that dynamially selects max counter value for counter
		bvec<MAXCWIDTH> count;
		vec<64, bvec<MAXCWIDTH>> l_mux;
		std::vector<unsigned> op = get_opcodes();
		for(unsigned i = 0, s; i < op.size(); ++i){
			l_mux[op[i]] = Lit<MAXCWIDTH>(this->getLatency(op[i]));
		}
		
		//counter logic
		//if !valid				set counter to 0
		//else (valid, ongoing computation)
		//		if( counter < max value, not finished)
		//			counter++
		//		else (computation finished)
		//			if( !in.stall, deliver to next stage and reset counter)
		//				reset counter
		//			else
		//				maintain value
		bvec<MAXCWIDTH> latency(Mux(in.op, l_mux));
		count = Reg(Mux(valid, Lit<MAXCWIDTH>(0), 
			Mux(count==latency, count+Lit<MAXCWIDTH>(1),
				Mux(!in.stall, count, Lit<MAXCWIDTH>(0)))));
		
		
		bvec<N> a(in.r0);
		bvec<N> b(Mux(in.hasimm, in.r1, in.imm));
		
		//result mux
		vec<64, bvec<N>> mux_in;
		
		
		node finish, go;
		
		if(this->outportID == -1){
			//built-in functions, allow debugging signals
			floatnum <FPU_E, FPU_M> fmult (Fmul<FPU_E, FPU_M> (a, b));
				
			mux_in[0x37] = (fmult);
		
			finish = (count==latency);
			
			tap(str.str()+"count", count);
			tap(str.str()+"go", go);
			tap(str.str()+"finish", finish);
			tap(str.str()+"isReady", isReady);
			tap(str.str()+"ivalid", valid);
			tap(str.str()+"in.stall", in.stall);
			tap(str.str()+"ovalid", o.valid);
		}
		else{
			//this module is exposed to other verilog module
			//inport: result, finish(whether counter reaches max value) 
			bvec<N> vfmult = Input<N> (str.str()+"fneg");;
			
			//bvec<3> exceptions = Input<N> (str.str()+"excp");
			
			mux_in[0x37] = vfmult;
			finish = Input(str.str()+"finish");
			//outport: ivalid (input valid bit), stall(pipeline stall)
			tap(str.str()+"a", a);
			tap(str.str()+"b", b);
			tap(str.str()+"ivalid", valid);
			tap(str.str()+"stall", in.stall);
		}
		
		//whether res can be delivered to next stage
		go = finish && !in.stall;
		
		o.out = Wreg(go, Mux(in.op, mux_in));
		o.valid = Reg(Mux(go, 0, valid));
		o.iid = Wreg(go, in.iid);
		o.didx = Wreg(go, in.didx);
		o.pdest = Wreg(go, in.pdest);
		
		//issue logic issues instructions regardless of isReady bit
		//but input valid bit is put to low if isReady bit is set
		//suggesting previous computation is finished
		isReady = valid&&finish;
		
		return o;
	}

	virtual chdl::node ready() { return isReady; }

private:
	chdl::node isReady;
};




// Integrated SRAM load/store unit with no MMU
/*
   Load instruction:
   Cycle 1: insert request into loadqueue
   				Set pending flag, clear free flag
   				check for Load Store Forwarding (LSF)
					if LSF enabled, clear pending flag,
					set loaded flag, skip to cycle 4
				Bypass: push request to memory if ldq empty AND no LSF
					clear pending flag
   Cycle 2: Compete for memory if pending flag is set
   				Clear pending flag once sent to memory
   Cycle 3: Data returned from memory
   				Set loaded flag
				TODO: commit data if no other loaded data
					Clear loaded flag,
					Set free flag
   Cycle 4: Once loaded, compete for write back commit
   				If selected for commit, 
					Clear loaded flag
					Set free flag
   Store instruction:
   Cycle 1: insert request into store queue
				check for WAR hazard, set waiting flag
				and waitidx (queue id of dependent load)
   Cycle 2: if no WAR and at head of queue, compete for memory
   				if WAR, wait until pending flag of dependent
				load is cleared (1 cycle bubble for pending flag
				to propagate)

*/
template <unsigned N, unsigned R, unsigned SIZE>
<<<<<<< HEAD
	class SramLsu : public FuncUnit<N, R> {
public:
	std::vector<unsigned> get_opcodes() {
		std::vector<unsigned> ops;
=======
  class SramLsu : public FuncUnit<N, R>
{
 private:
  chdl::node isReady;
 public:
  virtual chdl::node ready() { return isReady; }
  std::vector<unsigned> get_opcodes() {
    std::vector<unsigned> ops;
>>>>>>> ad532910e7cba68535873e24e2631d195bf699a7

		ops.push_back(0x23);
		ops.push_back(0x24);

		return ops;
	}

<<<<<<< HEAD
	virtual fuOutput<N, R> generate(fuInput<N, R> in, chdl::node valid) {
		const unsigned L2WORDS(CLOG2(SIZE/(N/8)));
=======
  template <unsigned M> 
	  chdl::bvec <M> delay (chdl::bvec <M> in, unsigned cycles) {
	  if (cycles == 0) return in;
	  else return chdl::Reg<M>(delay(in, cycles-1));
  }
  chdl::node delay(chdl::node in, unsigned cycles) {
	  if (cycles == 0) return in;
	  else return chdl::Reg(delay(in, cycles-1));
  }

  virtual fuOutput<N, R> generate(fuInput<N, R> in, chdl::node valid) {
    const unsigned L2WORDS(CLOG2(SIZE/(N/8)));
 
    using namespace std;
    using namespace chdl;

	struct mshrld {
		bvec<CLOG2(R)> didx;
		bvec<IDLEN> iid;
		bvec<L2WORDS> memaddr;
		bvec<N> data;
		node free;
		node loaded;
		node pending;
	};

	struct mshrst {
		bvec<CLOG2(R)> didx;
		bvec<IDLEN> iid;
		bvec<L2WORDS> memaddr;
		bvec<N> data;
		node committed;
		node waiting;
		bvec<CLOG2(Q)> waitidx;
		node valid;
	};

	node resetReg;
	node reset = !resetReg;
	resetReg = Reg(Lit(1));

	vec<Q, mshrld> ldq;
	vec<Q, mshrst > stq;
    fuOutput<N, R> o;
    bvec<6> op(in.op);
    bvec<N> r0(in.r0), r1(in.r1), imm(in.imm),
            addr(imm + Mux(op[0], in.r1, in.r0));
    bvec<L2WORDS> memaddr(Zext<L2WORDS>(addr[range<CLOG2(N/8), N-1>()]));
    bvec<CLOG2(N)> memshift(Lit<CLOG2(N)>(8) *
                              Zext<CLOG2(N)>(addr[range<0, CLOG2(N/8)-1>()]));
	
	//insert to empty MSHR
	////////////////////////////////////////////////
	// load queue
	node memvalid;					//data from mem is valid 
	node readyToCommit; 			//have loaded ldq entry for commit
	node stqEnable, ldqEnable;  	//write enable for insertion
	bvec<CLOG2(Q)> freeldqidx, returnqidx; //qid of data from mem
	bvec<Q> ldqFree, ldqDone, ldqInsert, ldqPending, returnSelect, ldqLoaded;
	vec<Q, bvec<N> > ldqData;
	vec<Q, bvec<CLOG2(R)> > ldqdidx;
	vec<Q, bvec<IDLEN> > ldqiid;
    vec<Q, bvec<L2WORDS> > ldqMemaddr;
	bvec<L2WORDS> ldqMemaddrOut; 

	stqEnable = valid && !op[0] && isReady;
	ldqEnable = valid && op[0] && isReady;
	freeldqidx= Log2(ldqFree);
	node ldqAvailable = OrN(ldqFree);
	node ldqEmpty = AndN(ldqFree);
	ldqInsert = Decoder(freeldqidx, ldqEnable);
	returnSelect = Decoder(returnqidx, memvalid); 
	for(int i=0; i<Q; ++i)
	{
		ldqFree[i] = Wreg(reset || (ldqInsert[i]) || ldqDone[i] || returnSelect[i], 
						reset || ldqDone[i] || (returnSelect[i] && !readyToCommit) );
		ldqMemaddr[i] = Wreg<L2WORDS>(ldqInsert[i], memaddr);
		ldqiid[i] = Wreg(ldqInsert[i], in.iid);
		ldqdidx[i] = Wreg(ldqInsert[i], in.didx);

		ostringstream oss;
		oss << "ldq" << i << ".memaddr";
		tap(oss.str(), ldq[i].memaddr);
	}

	DBGTAP(reset); DBGTAP(resetReg);
	DBGTAP(ldqAvailable); DBGTAP(ldqEnable);
	DBGTAP(ldqInsert);
	DBGTAP(valid); DBGTAP(op);
	DBGTAP(ldqDone);
	DBGTAP(ldqFree);
	DBGTAP(memaddr);


	// store queue 
	//////////////////////////////////////////////////////////
	node dependentLd;
	node sendldreq, sendstreq;
	node validStqReq; 		//output to memory arbitration select signal
	bvec<Q> stqInsert, stqWaiting, stqValid, clearWait, stqTailSelect;
	vec<Q, bvec<N> > stqData;
	vec<Q, bvec<IDLEN> > stqiid;
	vec<Q, bvec<CLOG2(R)> > stqdidx;
	vec<Q, bvec<CLOG2(Q)> > stqWaitidx;
	vec< Q, bvec<L2WORDS> > stqMemaddr;
	bvec<CLOG2(Q)+1> stqSize;
	bvec<CLOG2(Q)> head, tail;

	//head/tail pointers into stq
	validStqReq = sendstreq && Mux(tail, stqValid);
	tail = Wreg(validStqReq, tail+Lit<CLOG2(Q)>(1) ); 
	head = Wreg(stqEnable, head+Lit<CLOG2(Q)>(1) ); 
	stqSize = Wreg( Xor(stqEnable, validStqReq), 
			Mux(validStqReq, stqSize+Lit<CLOG2(Q)+1>(1), 
			stqSize+Lit<CLOG2(Q)+1>(0xF)) ); // take stall into account?

	DBGTAP(stqSize); DBGTAP(tail); DBGTAP(head);
	DBGTAP(stqValid); DBGTAP(stqEnable); 
	DBGTAP(validStqReq);
	node stqValidReady = (Mux(tail, stqValid) );
	DBGTAP(stqValidReady);

	//Load Store Forwarding (LSF) and WAR hazard detection
	node enableLSF;
	bvec<Q> WARaddrMatch, LSFaddrMatch, ldqMatch, stqMatch;
	bvec<N> stqLSFData = Mux(Enc(LSFaddrMatch), stqData);
	
	for(int i=0; i<Q; ++i)
	{
		WARaddrMatch[i] = memaddr == ldq[i].memaddr;	
		ldqMatch[i] = ldq[i].pending && WARaddrMatch[i];
		
		LSFaddrMatch[i] = (memaddr == stq[i].memaddr);
		stqMatch[i] = LSFaddrMatch[i] && stq[i].valid;	
	}
	//WAR detection
	bvec<CLOG2(Q)> dependentLdidx = Enc(ldqMatch);
	dependentLd = OrN(WARaddrMatch) && ldqMemaddrOut != memaddr;
	stqInsert = Decoder(head, stqEnable);
	//LSF detection
	enableLSF = OrN(stqMatch);
	stqLSFData = Mux(Enc(LSFaddrMatch), stqData);

	for(int i=0; i<Q; ++i)
	{
		stqMemaddr[i] = Wreg<L2WORDS>(stqInsert[i], memaddr);
		stqData[i] = Wreg<N>(stqInsert[i], r0);
		stqiid[i] = Wreg(stqInsert[i], in.iid);
		clearWait[i] = Mux(stqWaitidx[i], ldqPending);
		stqWaiting[i] = Wreg(stqInsert[i] || !clearWait[i], dependentLd && stqInsert[i]); 
		stqWaitidx[i] = Wreg<CLOG2(Q)>(stqInsert[i], dependentLdidx);
		stqValid[i] = Wreg(stqInsert[i] || (stqTailSelect[i]), stqInsert[i]);//what clears it?

		TAP(stqData[i]);
	}

	DBGTAP(LSFaddrMatch); DBGTAP(stqMatch);
	DBGTAP(WARaddrMatch); DBGTAP(ldqMatch);
	DBGTAP(stqLSFData); 
	DBGTAP(stqTailSelect); DBGTAP(stqInsert);

	//send req to memory
	////////////////////////////////////////////////
	node memStall = Lit(0);
	node ldqPendingFlag = OrN(ldqPending);
	bvec<CLOG2(Q)> pendingldqidx = Mux(ldqPendingFlag, freeldqidx, Log2(ldqPending) );
	bvec<Q> ldqReq = Decoder(pendingldqidx, ldqPendingFlag);

	for(int i=0; i<Q; ++i)
	{
		//ldqPending[i] = 1 when: on insert 
		//ldqPending[i] = 0 when: sent to memory, bypass, or memStall
		// bypass occurs when ldqEmpty or no ldqPending
		ldqPending[i] = Wreg( (ldqInsert[i] ) || (ldqReq[i] && sendldreq && !memStall), (ldqInsert[i] && !enableLSF && !ldqPendingFlag && memStall) );
	}
	
	//loading data (from memory or st forwarding)
	////////////////////////////////////////////////
	node WARexists = Mux(tail, stqWaiting);
	sendldreq = (ldqEnable || !( ( (stqSize == Lit<CLOG2(Q)+1>(Q)) && !WARexists) || !ldqPendingFlag) ) ;//TODO should this be included? && !memStall;
	sendstreq = !sendldreq && !memStall;//should memstall be taken in to account?

	stqTailSelect = Decoder(tail, sendstreq);

	ldqMemaddrOut = Mux(!ldqPendingFlag && !enableLSF, Mux(pendingldqidx, ldqMemaddr), memaddr);
	bvec<L2WORDS> stqMemaddrOut = Mux(tail, stqMemaddr);
	bvec<L2WORDS> memaddrOut = Mux(sendldreq, stqMemaddrOut, ldqMemaddrOut);
	node memValidOut = ldqEnable || ldqPendingFlag || Mux(tail, stqValid); 
	node memWrite = Mux(tail, stqValid) && sendstreq;

	DBGTAP(ldqPending);
	DBGTAP(memaddrOut);
	DBGTAP(sendldreq); DBGTAP(sendstreq);
	DBGTAP(stqWaiting);
	
	//TESTING PURPOSE ONLY; MUST CHANGE WHEN INTERFACING WITH MEMORY
	//bvec<N> sramout = Syncmem(memaddr, r0, valid && !op[0]);
//*/
//TODO: must add latch to memory output
	bvec<N> sramout, memDataIn;
	bvec<Q> ldqReturn;	
#if PROTOTYPE
	sramout = Syncmem(memaddrOut, Mux(tail, stqData), memValidOut);
	memDataIn = delay(sramout, DELAY-1);
	returnqidx = delay(pendingldqidx, DELAY);
	memvalid = delay(memValidOut, DELAY);
	//memStall = sramout[8];

	ldqReturn = Decoder(returnqidx, memvalid);
#else
	sramout = Input<N>("memDataIn"); 
	returnqidx = Input<CLOG2(Q)>("memqidIn"); 
	memvalid = Input("memValidIn");
	TAP(memaddrOut); TAP(memWrite); TAP(memValidOut); TAP(pendingldqidx); 
#endif

	readyToCommit = OrN(ldqDone);
	
	for(int i=0; i<Q; ++i)
	{	
		ldqLoaded = Wreg( ldqInsert[i] || ldqDone[i] || (ldqReturn[i] && memvalid), (ldqReturn[i] && memvalid && readyToCommit) || (ldqInsert[i] && enableLSF) );
		ldqData = Wreg<N>( (ldqReturn[i] && memvalid) || ldqInsert[i], Mux(ldqInsert[i], memDataIn, stqLSFData) );	//loads from store at insertion regardless of LSF is enabled, is this a problem?
	}

	DBGTAP(memDataIn); DBGTAP(sramout);
	DBGTAP(pendingldqidx); DBGTAP(returnqidx);
	DBGTAP(memvalid); 
	DBGTAP(memValidOut); 
	DBGTAP(ldqPendingFlag);
	DBGTAP(ldqReturn);
	//commit 
	///////////////////////////////////////////////////
	for(int i=0; i<Q; ++i)
	{
		ldqLoaded[i] = ldq[i].loaded;
		ldqData[i] = ldq[i].data;
	}

	node regStall = Reg(in.stall);	
	bvec<CLOG2(Q)> loadedldqidx = Log2(ldqLoaded);
	node ldqLoadedFlag = OrN(ldqLoaded);
	bvec<Q> ldqCommit = Decoder(loadedldqidx, ldqLoadedFlag);

	for(int i=0; i<Q; ++i)
	{
		ldqDone[i] = ldqCommit[i] && !regStall;
	}

	isReady = ldqAvailable && stqSize != Lit<CLOG2(Q)+1>(Q); // && stqAvailable or stqSize > 0
	DBGTAP(isReady);

#if 1
    o.out = Mux(readyToCommit, memDataIn, Mux(loadedldqidx, ldqData));
	o.valid = readyToCommit || memvalid;
	o.iid = Mux(readyToCommit, Mux(returnqidx, ldqiid), Mux(loadedldqidx, ldqiid) );
	o.didx = Mux(readyToCommit, Mux(returnqidx, ldqdidx), Mux(loadedldqidx, ldqdidx) );
#else
    o.out = sramout >> memshift;
    o.valid = PipelineReg(3, valid && op[0]);
    o.iid = PipelineReg(3, in.iid);
    o.didx = PipelineReg(3, in.didx);
#endif 
	o.pdest = PipelineReg(3, in.pdest);

	//DBGTAP(ldqData);
	DBGTAP(o.out);
	DBGTAP(o.iid);
	DBGTAP(o.didx);
	DBGTAP(ldqLoadedFlag);
	DBGTAP(ldqLoaded);
	DBGTAP(regStall);
	DBGTAP(ldqCommit);
	return o;
  }
};

#if 0
  virtual fuOutput<N, R> generate(fuInput<N, R> in, chdl::node valid) {
    const unsigned L2WORDS(CLOG2(SIZE/(N/8)));
>>>>>>> ad532910e7cba68535873e24e2631d195bf699a7

		using namespace std;
		using namespace chdl;

		fuOutput<N, R> o;

		bvec<6> op(in.op);
		bvec<N> r0(in.r0), r1(in.r1), imm(in.imm),
						addr(imm + Mux(op[0], in.r1, in.r0));
		bvec<L2WORDS> memaddr(Zext<L2WORDS>(addr[range<CLOG2(N/8), N-1>()]));
		bvec<CLOG2(N)> memshift(Lit<CLOG2(N)>(8) *
															Zext<CLOG2(N)>(addr[range<0, CLOG2(N/8)-1>()]));

		bvec<N> sramout = Syncmem(memaddr, r0, valid && !op[0]);

		o.out = sramout >> memshift;
		o.valid = PipelineReg(3, valid && op[0]);
		o.iid = PipelineReg(3, in.iid);
		o.didx = PipelineReg(3, in.didx);
		o.pdest = PipelineReg(3, in.pdest);

		return o;
	}
private:
};
#endif

#endif

