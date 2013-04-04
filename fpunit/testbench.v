module testbench(phi, togclk, reg00, reg01, reg02, reg03, reg04, reg05, reg06, reg07);
	parameter N = 32;
	parameter L = 4;
	
	output reg phi, togclk;
	output [N-1:0] reg00, reg01, reg02, reg03, reg04, reg05, reg06, reg07;
	wire FpuBasicStall, FpuMultStall, AluDivStall;
	wire FpuBasicValid, FpuMultValid, AluDivValid;
	wire FpuBasicFinish, FpuMultFinish, AluDivFinish;
	wire [5:0] FpuBasicOpcode, FpuMultOpcode;
	wire [N*L-1:0] FpuBasicA, FpuMultA, AluDivA;
	wire [N*L-1:0] FpuBasicB, FpuMultB, AluDivB;
	wire [N*L-1:0] FpuBasicRes, FpuMultRes, AluDivQ, AluDivR;
	
	
	
	
	FpuBasic #(.L(L)) fp_basic_0(phi, FpuBasicStall, FpuBasicValid, FpuBasicOpcode, FpuBasicA, FpuBasicB, FpuBasicRes, FpuBasicFinish);
	FpuMultDiv #(.L(L)) fp_mult_0(phi, FpuMultStall, FpuMultValid, FpuMultOpcode, FpuMultA, FpuMultB, FpuMultRes, FpuMultFinish);
	AluDiv #(.L(L)) int_div_0(phi, AluDivStall, AluDivValid, AluDivA, AluDivB, AluDivQ, AluDivR, AluDivFinish);

   harmonica harrr(phi, AluDivFinish, AluDivQ, AluDivR, FpuBasicFinish, FpuBasicRes, FpuMultFinish, FpuMultRes,
						 AluDivA, AluDivB, AluDivStall, AluDivValid, FpuBasicA, FpuBasicB, FpuBasicOpcode, FpuBasicStall,
						 FpuBasicValid, FpuMultA, FpuMultB, FpuMultOpcode, FpuMultStall, FpuMultValid, char_out, char_out_val,
						 reg00, reg01, reg02, reg03, reg04, reg05, reg06, reg07);
	
 
	initial begin
		phi = 0;
		togclk = 0;
		#1000 togclk = 1;
	end

   always begin
		#1000 phi = phi ^ togclk;
	end
	
endmodule // top
