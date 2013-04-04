module Int_Div(
	clk_en,
	clk,
	denom,
	numer,
	quotient,
	remainder);

	parameter N = 32;
	parameter L = 4;

	input clk_en;
	input clk;

	input [N*L-1:0] denom;
	input [N*L-1:0] numer;
	output wire [N*L-1:0] quotient;
	output wire [N*L-1:0] remainder;
	
	wire [N*L-1:0] q, r;
	wire [L-1:0] zero, en;
	genvar i;
	generate 
		for(i = 0; i < L; i = i + 1) begin : start
			assign zero[i] = (denom[(i+1)*N-1: i*N] == {(N){1'b0}});
			assign en[i] = clk_en && (!zero[i]); 
			lpm_divide_ex int_div_all (en[i], clk, denom[(i+1)*N-1: i*N], numer[(i+1)*N-1: i*N], quotient[(i+1)*N-1: i*N], remainder[(i+1)*N-1: i*N]);
			//assign quotient[(i+1)*N-1:i*N] = (zero[i]?q[(i+1)*N-1: i*N]:{(N){1'b0}});
			//assign remainer[(i+1)*N-1:i*N] = (zero[i]?r[(i+1)*N-1: i*N]:{(N){1'b0}});
		end
	endgenerate

endmodule