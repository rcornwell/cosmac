# Cosmac VIP emulator.

Emulate a CDP 1802 cpu as used with COSMAC VIP computer. VIP used a CDP 1861 video controller. This controller
displays a screen of 64 pixels wide and up to 128 pixels high. Each display line is 14 cycles long with 8 of
them being DMA copies from memory addressed by R0.

This emulator supports VIP, VP500, RCA Studio II and III console. The emulator supports Tape Read and Write. It also can operate with serial console.

The CDP 1802 is a very simple architecture consists of an accumulator (D) and carry flag (DF). It also has 16
index registers. One of these index registers is always used as the program counter. This value is set into
the (P) register. There is a default index register (X) used to access locations quickly. At interrupt the 
current P and X registers are saved in the (T) register. The P register is set to 1 and the X register is set
to 2.

To build the emulator either git clone it or extract the zip file from github. In the emulator directory do:

> mkdir build  
> cd build  
> cmake ..  
> make  

# Using

The COSMAC VIP had a single hex keyboard. The number pad is used for keyboard input. Keys 0-9, A(.), B(enter), C(+), D(-), E(*), F(/). For RCA Studio II and III emulation the second keypad uses 0(X), 1(A),2(S), 3(D), 4(Q), 5(W), 6(E), 7(1), 8(2), 9(3).


F1 Starts the system or stops it. This causes a reset.  
F2 Starts tape read.  
F3 Starts tape write.  
F4 Steps one instruction.  
F5 Starts the system, but no reset.
F6 Quit emulator.

Command line options are:  

 -b Loads a binary file starting at 0x0 or 0x200 if chip8 is loaded  
 -c Load CHIP8 into lower memory.  
 -d Read in a dump file.  
 -e Sets type of system to emulate.  
* vip: default, Cosmac VIP system.  
* vp: Cosmac VIP with color display.  
* studio2: RCA Studio II.  
* studio3: RCA Studio III.   
* 
 -h Print help.    
 -i Trace instruction execution to stderr.  
 -m Set memory size in K.  
 -r Load binary file to 0x400 for RCA Studio.  
 -s Enable serial console, switches to UT4 monitor.  
 -t File to read/write tape.  
 -1 to -9 scale display.  
 
 < filename >  : Load dump file.

