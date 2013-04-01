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
#include <chdl/hierarchy.h>

#include <chdl/statemachine.h>

#include "fpu.h"



#define PROTOTYPE 1
#define blah

#ifdef DEBUG
#define DBGTAP(x) do {TAP(x); } while(0)
#else
#define DBGTAP(x) do {} while(0)
#endif

#define MAXCWIDTH	10

static const unsigned FPU_E(8), FPU_M(23);


static const unsigned IDLEN(6);
static const unsigned Q(4);
static const unsigned DELAY(5);



template <unsigned N, unsigned R, unsigned L> struct fuInput {
	chdl::bvec<N> imm, pc;
	chdl::vec<L, chdl::bvec<N>> r0, r1, r2;
	chdl::bvec<L> p0, p1;
	chdl::node hasimm, stall, pdest;
	
	chdl::bvec<6> op;
	chdl::bvec<IDLEN> iid;
	chdl::bvec<CLOG2(R)> didx;
	
	chdl::bvec<L> wb;
};

template <unsigned N, unsigned R, unsigned L> struct fuOutput {
	chdl::vec<L, chdl::bvec<N>> out;
	chdl::bvec<L> wb;
	chdl::bvec<IDLEN> iid;
	chdl::node valid, pdest;
	chdl::bvec<CLOG2(R)> didx;
};

template <unsigned N, unsigned R, unsigned L> class FuncUnit {
	public:
	FuncUnit(bool getExposed = 0) {
		if(getExposed){
			this->outportID = this->outportNum++;
		}
		else
			this->outportID = -1;
	}
	
	virtual std::vector<unsigned> get_opcodes() = 0;
	
	virtual fuOutput<N, R, L> generate(fuInput<N, R, L> in, chdl::node valid) = 0;
	virtual chdl::node ready() { return chdl::Lit(1); } // Output is ready.
	
	unsigned getLatency(unsigned x) {
		return latency[x];
	}
	static int outportNum;
	
protected:
	std::map<unsigned, unsigned> latency;
	int outportID;
};

template <unsigned N, unsigned R, unsigned L> int FuncUnit<N, R, L>::outportNum = 0;




// Functional unit with 1-cycle latency supporting all common arithmetic/logic
// instructions.
template <unsigned N, unsigned R, unsigned L>
class BasicAlu : public FuncUnit<N, R, L>{
public:
	std::vector<unsigned> get_opcodes() {
		std::vector<unsigned> ops;
		
		for (unsigned i = 0x05; i <= 0x0c; ++i) ops.push_back(i); // 0x05 - 0x0c
		for (unsigned i = 0x0f; i <= 0x16; ++i) ops.push_back(i); // 0x0f - 0x16
		for (unsigned i = 0x19; i <= 0x1c; ++i) ops.push_back(i); // 0x19 - 0x1c
		ops.push_back(0x25);
		return ops;
	}
	
	virtual fuOutput<N, R, L> generate(fuInput<N, R, L> in, chdl::node valid) {
		using namespace std;
		using namespace chdl;
		
		hierarchy_enter("BasicAlu");
		
		tap("valid_alu", valid);
		tap("stall_alu", in.stall);
		
		fuOutput<N, R, L> o;
		node w(!in.stall);
		
		for (unsigned i = 0; i < L; ++i) {
			bvec<N> a(in.r0[i]), b(Mux(in.hasimm, in.r1[i], in.imm));
			
			bvec<N> sum(Adder(a, Mux(in.op[0], b, ~b), in.op[0]));
			bvec<N> prod(a * b);
			
			vec<64, bvec<N>> mux_in;
			mux_in[0x05] = -a;
			mux_in[0x06] = ~a;
			mux_in[0x07] = a & b;
			mux_in[0x08] = a | b;
			mux_in[0x09] = a ^ b;
			mux_in[0x0a] = sum;
			mux_in[0x0b] = sum;
			mux_in[0x0c] = prod;
			mux_in[0x0f] = a << Zext<CLOG2(N)>(b);
			mux_in[0x10] = a >> Zext<CLOG2(N)>(b);
			mux_in[0x11] = mux_in[0x07];
			mux_in[0x12] = mux_in[0x08];
			mux_in[0x13] = mux_in[0x09];
			mux_in[0x14] = sum;
			mux_in[0x15] = sum;
			mux_in[0x16] = prod;
			mux_in[0x19] = mux_in[0x0f];
			mux_in[0x1a] = mux_in[0x10];
			mux_in[0x1b] = in.pc;
			mux_in[0x1c] = in.pc;
			mux_in[0x25] = b;
			
			o.out[i] = Wreg(w, Mux(in.op, mux_in));
		}
		
		o.valid = Wreg(w, valid);
		o.iid = Wreg(w, in.iid);
		o.didx = Wreg(w, in.didx);
		o.pdest = Wreg(w, in.pdest);
		o.wb = Wreg(w, in.wb);
		
		isReady = w;
		
		hierarchy_exit();
		
		return o;
	}
	
	virtual chdl::node ready() { return isReady; }
private:
	
	chdl::node isReady;
	
};

template <unsigned N, unsigned R, unsigned L>
class PredLu : public FuncUnit<N, R, L>{
public:
	std::vector<unsigned> get_opcodes() {
		std::vector<unsigned> ops;
		for (unsigned i = 0x26; i <= 0x2c; ++i){
			ops.push_back(i); // 0x26 - 0x2c
		}
		
		return ops;
	}
	
	virtual fuOutput<N, R, L> generate(fuInput<N, R, L> in, chdl::node valid) {
		using namespace std;
		using namespace chdl;
		
		hierarchy_enter("PredLu");
		
		fuOutput<N, R, L> o;
		
		node w(!in.stall);
		
		tap("valid_plu", valid);
		tap("stall_plu", in.stall);
		
		for (unsigned i = 0; i < L; ++i) {
			bvec<N> r0(in.r0[i]);
			node p0(in.p0[i]), p1(in.p1[i]);
			
			bvec<64> mux_in;
			mux_in[0x26] = OrN(r0);
			mux_in[0x27] = p0 && p1;
			mux_in[0x28] = p0 || p1;
			mux_in[0x29] = p0 != p1;
			mux_in[0x2a] = !p0;
			mux_in[0x2b] = r0[N-1];
			mux_in[0x2c] = !OrN(r0);
			
			o.out[i] = Zext<N>(bvec<1>(Wreg(w, Mux(in.op, mux_in))));
		}
		
		o.valid = Wreg(w, valid);
		o.iid = Wreg(w, in.iid);
		o.didx = Wreg(w, in.didx);
		o.pdest = Wreg(w, in.pdest);
		o.wb = Wreg(w, in.wb);
		isReady = w;
		
		hierarchy_exit();
		
		return o;
	}
	
	chdl::node ready() { return isReady; }
private:
	chdl::node isReady;
};

// Integrated SRAM load/store unit with no MMU, per-lane RAM
template <unsigned N, unsigned R, unsigned L, unsigned SIZE>
class SramLsu : public FuncUnit<N, R, L>{
public:
	std::vector<unsigned> get_opcodes() {
    std::vector<unsigned> ops;

    ops.push_back(0x23);
    ops.push_back(0x24);

    return ops;
  }

  virtual fuOutput<N, R, L> generate(fuInput<N, R, L> in, chdl::node valid) {
    const unsigned L2WORDS(CLOG2(SIZE/(N/8)));

    using namespace std;
    using namespace chdl;

    hierarchy_enter("SramLsu");

    fuOutput<N, R, L> o;

    node w(!in.stall);

    bvec<6> op(in.op);
    bvec<N> imm(in.imm);

    for (unsigned i = 0; i < L; ++i) {
      bvec<N> r0(in.r0[i]), r1(in.r1[i]), imm(in.imm),
              addr(imm + Mux(op[0], r1, r0));
      bvec<L2WORDS> memaddr(Zext<L2WORDS>(addr[range<CLOG2(N/8), N-1>()]));
      bvec<CLOG2(N)> memshift(Lit<CLOG2(N)>(8) *
                                Zext<CLOG2(N)>(addr[range<0, CLOG2(N/8)-1>()]));

      bvec<N> sramout = Syncmem(memaddr, r0, valid && !op[0], "rom.hex");

      o.out[i] = sramout >> Reg(memshift);

      // Simple character output from lane 0.
      if (i == 0) {
        node io(addr[N-1]), io_wr(valid && !op[0]),
             char_out(io_wr && addr[range<0, N-2>()] == Lit<N-1>(0));
        bvec<7> char_out_val(r0[range<0, 6>()]);
        TAP(char_out);
        TAP(char_out_val);
      }
    }

    o.valid = Wreg(w, valid && op[0]);
    o.iid = Wreg(w, in.iid);
    o.didx = Wreg(w, in.didx);
    o.pdest = Wreg(w, in.pdest);
    o.wb = Wreg(w, in.wb);

    hierarchy_exit();

    return o;
  }
 private:

};

template <unsigned N, bool D>
chdl::bvec<N> Shiftreg(
	chdl::bvec<N> in, chdl::node load, chdl::node shift, chdl::node shin
)
{
	using namespace chdl;
	using namespace std;
	
	HIERARCHY_ENTER();  
	
	bvec<N+1> val;
	val[D?N:0] = shin;
	
	if (D) {
		for (int i = N-1; i >= 0; --i)
			val[i] = Reg(Mux(load, Mux(shift, val[i], val[i+1]), in[i]));
		HIERARCHY_EXIT();
		return val[range<0, N-1>()];
	} else {
		for (unsigned i = 1; i < N; ++i)
			val[i] = Reg(Mux(load, Mux(shift, val[i], val[i-1]), in[i-1]));
		HIERARCHY_EXIT();
		return val[range<1, N>()];
	}
}

template <unsigned N>
chdl::bvec<N> Rshiftreg(
	chdl::bvec<N> in, chdl::node load, chdl::node shift,
	chdl::node shin = chdl::Lit(0)
)
{ return Shiftreg<N, true>(in, load, shift, shin); }

template <unsigned N>
chdl::bvec<N> Lshiftreg(
	chdl::bvec<N> in, chdl::node load, chdl::node shift,
	chdl::node shin = chdl::Lit(0)
)
{ return Shiftreg<N, false>(in, load, shift, shin); }

template <unsigned N>
void Serdiv(
	chdl::bvec<N> &q, chdl::bvec<N> &r, chdl::node &ready, chdl::node &waiting,
	chdl::bvec<N> n, chdl::bvec<N> d, chdl::node v, chdl::node stall
)
{
	using namespace std;
	using namespace chdl;
	
	// The controller
	Statemachine<N+3> sm;
	bvec<CLOG2(N+3)> state(sm);
	sm.edge(0, 0, !v);
	sm.edge(0, 1, v);
	for (unsigned i = 1; i < N+2; ++i)
		sm.edge(i, i+1, Lit(1));
	sm.edge(N+2, 0, !stall);
	sm.generate();
	static bool copy = false;
	if (!copy) {
		copy = true;
		tap("div_n", Wreg(v, n));
		tap("div_d", Wreg(v, d));
		tap("div_state", state);
	}
	ready = (bvec<CLOG2(N+3)>(sm) == Lit<CLOG2(N+3)>(N+2));
	waiting = (bvec<CLOG2(N+3)>(sm) == Lit<CLOG2(N+3)>(0));
	
	// The data path
	bvec<2*N> s(Rshiftreg(Cat(d, Lit<N>(0)), v, Lit(1)));
	node qbit(Cat(Lit<N>(0), r) >= s);
	r = Reg(Mux(v, Mux(qbit, r, r - s[range<0, N-1>()]), n));
	q = Lshiftreg(Lit<N>(0), v, !ready, qbit);
}

template <unsigned N, unsigned R, unsigned L>
  class SerialDivider : public FuncUnit<N, R, L>
{
public:
  std::vector<unsigned> get_opcodes() {
    std::vector<unsigned> ops;

    ops.push_back(0x0d);
    ops.push_back(0x0e);
    ops.push_back(0x17);
    ops.push_back(0x18);

    return ops;
  }

  virtual fuOutput<N, R, L> generate(fuInput<N, R, L> in, chdl::node valid) {
    using namespace std;
    using namespace chdl;

    hierarchy_enter("SerialDivider");

    tap("div_valid", valid);
    tap("div_stall", in.stall);

    fuOutput<N, R, L> o;
    node outputReady, issue(valid && isReady);

    for (unsigned i = 0; i < L; ++i) {
      bvec<N> n(in.r0[i]), d(Mux(in.hasimm, in.r1[i], in.imm)), q, r;
      Serdiv(q, r, outputReady, isReady, n, d, issue, in.stall);
      o.out[i] = Mux(Wreg(issue, in.op[0]), r, q);
    }

    o.valid = outputReady;
    o.iid = Wreg(issue, in.iid);
    o.didx = Wreg(issue, in.didx);
    o.pdest = Lit(0);
    o.wb = Wreg(issue, in.wb);
  
    hierarchy_exit();

    return o;
  }
  virtual chdl::node ready() { return isReady; }
  
private:
	chdl::node isReady;
};


// Basic FP unit with multi-cycle latencies
template <unsigned N, unsigned R, unsigned L>
class BasicFpu : public FuncUnit<N, R, L> {
public:
	BasicFpu(bool getExposed = 0) : FuncUnit<N, R, L> (getExposed) {
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
	
	virtual fuOutput<N, R, L> generate(fuInput<N, R, L> in, chdl::node valid) {
		using namespace std;
		using namespace chdl;
		
		
		
		
		fuOutput<N, R, L> o;
		
		//port name
		ostringstream str;
		str << "fp" << "Basic";
		
		//
		node finish, go;
		//whether res can be delivered to next stage
		
		
#ifdef DEBUG
		//if(this->outportID == -1){
			//built-in functions, allow debugging signals
			
			//counter, and latency mux that dynamially selects max counter value for counter
			//hierarchy_enter("BasicFpu");
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
			
			
			bvec<MAXCWIDTH> count;
			count = Reg(Mux(valid, Lit<MAXCWIDTH>(0),
							Mux(count==latency, count+Lit<MAXCWIDTH>(1),
								Mux(!in.stall, count, Lit<MAXCWIDTH>(0)))));
			
			/*count = Reg(Mux(count!=Lit<MAXCWIDTH>(0), Mux(issue, Lit<MAXCWIDTH>(0), Lit<MAXCWIDTH>(1)),
							Mux(count==latency, count+Lit<MAXCWIDTH>(1),
								Mux(in.stall, Lit<MAXCWIDTH>(0), count))));
			*/
			finish = (count==latency);
			go = finish && !in.stall;
			tap(str.str()+"count", count);
			tap(str.str()+"go", go);
			tap(str.str()+"finish", finish);
			tap(str.str()+"isReady", isReady);
			tap(str.str()+"ivalid", valid);
			tap(str.str()+"in.stall", in.stall);
			tap(str.str()+"ovalid", o.valid);
			
			for(unsigned i = 0; i < L; ++i){
				bvec<N> a(in.r0[i]);
				bvec<N> b(Mux(in.hasimm, in.r1[i], in.imm));
				
				
				floatnum <FPU_E, FPU_M> itof (Itof<FPU_E, FPU_M, N> (a));
				bvec<N> ftoi (Ftoi<FPU_E, FPU_M, N> (a));
				floatnum <FPU_E, FPU_M> fadd (Fadd<FPU_E, FPU_M> (a, b));
				floatnum <FPU_E, FPU_M> fsub (Fadd<FPU_E, FPU_M> (a, b));
				bvec<N> fneg (Fneg<N>(a));
				
				vec<64, bvec<N>> mux_in;
				mux_in[0x33] = (itof);
				mux_in[0x34] = ftoi;
				mux_in[0x35] = (fadd);
				mux_in[0x36] = (fsub);
				mux_in[0x39] = (fneg);
				
				o.out[i] = Wreg(go, Mux(in.op, mux_in));
			}
			//hierarchy_exit();
		//}
#else
		//else{
			//this module is exposed to other verilog module
			//inport: result, finish(whether counter reaches max value) 
			bvec<N*L> vitof = Input<N*L> (str.str()+"itof");
			bvec<N*L> vftoi = Input<N*L> (str.str()+"ftoi");;
			bvec<N*L> vfadd = Input<N*L> (str.str()+"fadd");
			bvec<N*L> vfsub = Input<N*L> (str.str()+"fsub");
			bvec<N*L> vfneg = Input<N*L> (str.str()+"fneg");;
			bvec<3> exceptions = Input<3> (str.str()+"excp");
			
			
			bvec<N*L> a;
			bvec<N*L> b;
			bvec<N*L> tmpb;
			for(unsigned i = 0; i < L; ++i){
				a[i*N, (i+1)*N-1] = (in.r0[i])[0, N-1];
				
				tmpb[i*N, (i+1)*N-1] = in.r1[i][0, N-1];
			}
			b = tmpb;
			
			for(unsigned i = 0; i < L; ++i){
				vec<64, bvec<N>> mux_in;
				mux_in[0x33] = vitof[i*N, (i+1)*N-1];
				mux_in[0x34] = vftoi[i*N, (i+1)*N-1];
				mux_in[0x35] = vfadd[i*N, (i+1)*N-1];
				mux_in[0x36] = vfsub[i*N, (i+1)*N-1];
				mux_in[0x39] = vfneg[i*N, (i+1)*N-1];
				o.out[i] = Wreg(issue, Mux(in.op, mux_in));
			}
			
			finish = Input(str.str()+"finish");
			//outport: ivalid (input valid bit), stall(pipeline stall)
			tap(str.str()+"a", a);
			tap(str.str()+"b", b); 
			tap(str.str()+"ivalid", valid);
			tap(str.str()+"stall", in.stall);
			tap(str.str()+"sub", in.op==Lit<IDLEN>(0x36));
		//}
#endif
			
		
		
		o.valid = Reg(Mux(go, 0, valid));
		o.iid = Wreg(go, in.iid);
		o.didx = Wreg(go, in.didx);
		o.pdest = Wreg(go, in.pdest);
		o.wb = Wreg(go, in.wb);
		isReady = valid&&finish;
		
		return o;
	}
	
	virtual chdl::node ready() { return isReady; }
	
private:
	chdl::node isReady;
};


#endif
