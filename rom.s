/* sieve of eratosthanes: Find some primes. */
.def SIZE 100

.align 4096
.perm x
.entry
<<<<<<< HEAD
entry:    ldi    %r0, #0
          ldi    %r1, #1
          ld     %r3, %r0, #0
loop:     shli   %r3, %r0, #2
          st     %r1, %r3, #0
          subi   %r2, %r1, #10
          jali   %r3, subroutine
          iszero @p1, %r2
    @p1 ? jmpi   sumthem
          jmpi   loop
sumthem:  ldi    %r0, #0
          ldi    %r1, #0
theloop:  ld     %r3, %r0, #0
          add    %r1, %r1, %r3
          addi   %r0, %r0, #4
          subi   %r3, %r0, #40
          rtop   @p0, %r3
    @p0 ? jmpi   theloop
    	  muli   %r2, %r1, #-3
    	  ldi    %r3, #-111
    	  divi   %r4, %r3, #5
    	  modi   %r5, %r4, #2
    	  itof	 %r6, %r5
    	  itof	 %r7, %r5
		  fadd	 %r7, %r6, %r7
finished: jmpi   finished

subroutine:
          addi   %r0, %r0, #1
          addi   %r1, %r1, #1
          jmpr   %r3
=======
.global
entry:
             ldi  %r0, #2; /* i = 2 */
loop1:       muli %r1, %r0, __WORD;
             st   %r0, %r1, array;
             addi %r0, %r0, #1;
             subi %r1, %r0, SIZE;
             rtop @p0, %r1;
       @p0 ? jmpi loop1;

             ldi  %r0, #1;    
loop2:       addi %r0, %r0, #1;
             muli %r1, %r0, __WORD;
             ld   %r1, %r1, array;
             iszero @p0, %r1;
       @p0 ? jmpi loop2;

             mul   %r2, %r1, %r1;
             subi  %r3, %r2, SIZE;
             neg   %r3, %r3
             isneg @p0, %r3;
       @p0 ? jmpi  end;

             ldi   %r3, #0;
loop3:       muli  %r4, %r2, __WORD;
             st    %r3, %r4, array;
             add   %r2, %r2, %r1;
             ldi   %r4, SIZE;
             sub   %r4, %r2, %r4;
             isneg @p0, %r4;
             notp  @p0, @p0;
       @p0 ? jmpi  loop2;
             jmpi  loop3;

end:         ldi   %r0, #0;
printloop:   ld    %r7, %r0, array;
             subi  %r2, %r0, (__WORD*SIZE);
             rtop  @p0, %r7;
             rtop  @p1, %r2;
             addi  %r0, %r0, __WORD;
             andp  @p2, @p0, @p1
       @p2 ? jali %r5, printdec;
       @p1 ? jmpi printloop;

finished:    jmpi  finished;

.perm rw
.global
array:     .space 100 /* SIZE words of space. */
digstack:  .space 10

printdec:    ldi  %r3, #1;
             shli %r3, %r3, (__WORD*8 - 1);
             and  %r6, %r3, %r7;
             rtop @p0, %r6;
       @p0 ? ldi  %r6, #0x2d;
       @p0 ? st   %r6, %r3, #0;
       @p0 ? neg  %r7, %r7;
             ldi  %r4, #0;
printdec_l1: modi %r6, %r7, #10;
             divi %r7, %r7, #10;
             addi %r6, %r6, #0x30;
             st   %r6, %r4, digstack;
             addi %r4, %r4, __WORD;
             rtop @p0, %r7;
       @p0 ? jmpi printdec_l1;
printdec_l2: subi %r4, %r4, __WORD;
             ld   %r6, %r4, digstack;
             st   %r6, %r3, #0;
             rtop @p0, %r4;
       @p0 ? jmpi printdec_l2;
             ldi  %r6, #0x0a;
             st   %r6, %r3, #0;
             jmpr %r5
>>>>>>> b66d8d266ad512e91fc50e3814f7524db65d634e
