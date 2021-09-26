# C Compiler
This simple compiler processes C code through a Finite State Machine, and translates it into MIPS assembly. `states.txt` represents the FSM for which the driver code in `MIPSgen.cc` constructs a tree where every node is a token from the given C code.
