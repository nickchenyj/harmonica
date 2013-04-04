module AluDiv(
	clk,
	stall,
	ivalid,
	a,
	b,
	q,
	r,
	finish);

	parameter N = 32;
	parameter L = 4;
	
	parameter LATENCY_IDIV = 32;

	parameter IDIV = 6'h38;
	
	parameter WIDTH = $clog2(LATENCY_IDIV+1);//must be set to the largest latency
	
	input clk;
	input stall;
	input ivalid;
	input [N*L-1:0] a;
	input [N*L-1:0] b;
	output [N*L-1:0] q, r;
	output finish;

	
	reg [WIDTH-1:0] count;
	
	wire [WIDTH-1:0] latency = LATENCY_IDIV;
	
	wire finish = (count == latency);
	wire clk_en = (ivalid || count != {(WIDTH-1){1'b0}});


	Int_Div #(.L(L)) idiv_all(clk_en, clk, b, a, q, r);
	
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
