module FpuBasic(
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
	
	parameter LATENCY_ADD_SUB = 15;
	parameter LATENCY_ITOF = 6;
	parameter LATENCY_FTOI = 6;
	parameter LATENCY_NEG = 50;
	
	parameter ITOF = 6'h33;
	parameter FTOI = 6'h34;
	parameter FADD = 6'h35;
	parameter FSUB = 6'h36;
	parameter FNEG = 6'h39;
	
	parameter WIDTH = $clog2(LATENCY_NEG+1);
	
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
	
	wire [WIDTH-1:0] latency = ((opcode==ITOF)?LATENCY_ITOF:
								((opcode==FTOI)?LATENCY_FTOI:
								((opcode==FADD)?LATENCY_ADD_SUB:
								((opcode==FSUB)?LATENCY_ADD_SUB:
								LATENCY_NEG
								)
								)
								)
								);
	
	
	wire finish = (count == latency);
	wire clk_enable = (ivalid || count != {(WIDTH-1){1'b0}});
	
	wire clk_en_itof = clk_enable && (opcode==ITOF);
	wire clk_en_ftoi = clk_enable && (opcode==FTOI);
	wire clk_en_add_sub = clk_enable && ((opcode==FADD) || (opcode==FSUB));
	wire clk_en_inv = clk_enable && (opcode==FNEG);
	
	wire addorsub = opcode[0];
	
	wire [N*L-1:0] res_itof, res_ftoi, res_add_sub, res_inv;
	
	FP_Conv_ItoF #(.L(L)) itof_all(clk_en_itof, clk, a, res_itof);
	FP_Conv_FtoI #(.L(L)) ftoi_all(clk_en_ftoi, clk, a, res_ftoi);
	FP_Add_Sub #(.L(L)) add_sub_all(addorsub, clk_en_add_sub, clk, a, b, res_add_sub);
	FP_Inv #(.L(L)) inv_all(clk_en_inv, clk, a, res_inv);
	
	
	assign o = ((opcode==ITOF)?res_itof:
	((opcode==FTOI)?res_ftoi:
	((opcode==FADD)?res_add_sub:
	((opcode==FSUB)?res_add_sub:
	res_inv
	)
	)
	)
	);
	
	
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
