module FpuMultDiv(
	clk,
	stall,
	ivalid,
	opcode,
	a,
	b,
	o,
	finish);

	parameter N = 32;
	parameter L = 4;
	
	parameter LATENCY_FDIV = 20;
	parameter LATENCY_FMULT = 14;

	parameter FMUL = 6'h37;
	parameter FDIV = 6'h38;
	
	parameter WIDTH = $clog2(LATENCY_FDIV+1);//must be set to the largest latency
	
	input clk;
	input stall;
	input ivalid;
	input [5:0] opcode;
	input [N*L-1:0] a;
	input [N*L-1:0] b;
	output [N*L-1:0] o;
	output finish;
	
	wire [N*L-1:0] o;
	
	reg [WIDTH-1:0] count;
	
	wire [WIDTH-1:0] latency = ((opcode==FMUL)?LATENCY_FMULT:LATENCY_FDIV);
	
	
	wire finish = (count == latency);
	wire clk_enable = (ivalid || count != {(WIDTH-1){1'b0}});
	
	wire clk_en_mul = clk_enable && (opcode==FMUL);
	wire clk_en_div = clk_enable && (opcode==FDIV);
	
	wire [N*L-1:0] res_mul, res_div;
	
	FP_Mult #(.L(L)) fmult_all(clk_en_mul, clk, a, b, res_mul);
	FP_Div #(.L(L)) fdiv_all(clk_en_div, clk, a, b, res_div);
	
	
	assign o = ((opcode==FMUL)?res_mul:res_div);
	
	
	always @ (posedge clk) begin
		if(!ivalid)
			count = {(WIDTH-1){1'b0}};
		else if(!finish)
			count = count + 1'b1;
		else if(!stall)
			count = {(WIDTH-1){1'b0}};
		else
			count = count;
	end
endmodule
