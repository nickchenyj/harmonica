module FP_Add_Sub(
	issub,
	clk_en,
	clk,
	a,
	b,
	o);

	parameter N = 32;
	parameter L = 4;

	input issub;
	input clk_en;
	input clk;
	input [N*L-1:0] a;
	input [N*L-1:0] b;
	output [N*L-1:0] o;

	
	genvar i;
	generate 
		for(i = 0; i < L; i = i + 1) begin : start
			altfp_add_sub_ex fp_add_sub(issub, clk_en, clk, a[(i+1)*N-1: i*N], b[(i+1)*N-1: i*N], o[(i+1)*N-1: i*N]);
		end
	endgenerate

endmodule