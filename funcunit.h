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

#include "fpu.h"

#define MAXCWIDTH	10

static const unsigned IDLEN(6);
static const unsigned FPU_E(8), FPU_M(23);


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
template <unsigned N, unsigned R, unsigned SIZE>
	class SramLsu : public FuncUnit<N, R> {
public:
	std::vector<unsigned> get_opcodes() {
		std::vector<unsigned> ops;

		ops.push_back(0x23);
		ops.push_back(0x24);

		return ops;
	}

	virtual fuOutput<N, R> generate(fuInput<N, R> in, chdl::node valid) {
		const unsigned L2WORDS(CLOG2(SIZE/(N/8)));

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
