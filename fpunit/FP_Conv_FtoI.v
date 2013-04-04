module FP_Conv_FtoI(
	clk_en,
	clk,
	a,
	o);

	parameter N = 32;
	parameter L = 4;

	input clk_en;
	input clk;

	input [N*L-1:0] a;

	output [N*L-1:0] o;
	
	genvar i;
	generate 
		for(i = 0; i < L; i = i + 1) begin : start
			altfp_convert_ftoi_ex fp_itof(clk_en, clk, a[(i+1)*N-1: i*N], o[(i+1)*N-1: i*N]);
		end
	endgenerate

endmodule