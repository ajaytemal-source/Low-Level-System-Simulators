#include <cstddef>
#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <iomanip>
#include <regex>
#include <cstdlib>

using namespace std;

size_t const static NUM_REGS = 8;
size_t const static MEM_SIZE = 1<<13;
size_t const static REG_SIZE = 1<<16;

/*
    Loads an E20 machine code file into the list
    provided by mem. We assume that mem is
    large enough to hold the values in the machine
    code file.

    @param f Open file to read from
    @param mem Array represetnting memory into which to read program
*/
void load_machine_code(ifstream &f, unsigned short mem[]) {
    regex machine_code_re("^ram\\[(\\d+)\\] = 16'b(\\d+);.*$");
    size_t expectedaddr = 0; // IF WE CAN CHANGE THE CODE, MAKE THIS GLOBAL TO THE CODE 
    string line;
    while (getline(f, line)) {
        smatch sm;
        if (!regex_match(line, sm, machine_code_re)) {
            cerr << "Can't parse line: " << line << endl;
            exit(1);
        }
        size_t addr = stoi(sm[1], nullptr, 10);
        unsigned instr = stoi(sm[2], nullptr, 2);
        if (addr != expectedaddr) {
            cerr << "Memory addresses encountered out of sequence: " << addr << endl;
            exit(1);
        }
        if (addr >= MEM_SIZE) {
            cerr << "Program too big for memory" << endl;
            exit(1);
        }
        expectedaddr ++;
        mem[addr] = instr;
    }
}

/*
    Prints the current state of the simulator, including
    the current program counter, the current register values,
    and the first memquantity elements of memory.

    @param pc The final value of the program counter
    @param regs Final value of all registers
    @param memory Final value of memory
    @param memquantity How many words of memory to dump
*/
void print_state(unsigned pc, unsigned short regs[], unsigned short memory[], size_t memquantity) {
    cout << setfill(' ');
    cout << "Final state:" << endl;
    cout << "\tpc=" <<setw(5)<< pc << endl;

    for (size_t reg=0; reg<NUM_REGS; reg++)
        cout << "\t$" << reg << "="<<setw(5)<<regs[reg]<<endl;

    cout << setfill('0');
    bool cr = false;
    for (size_t count=0; count<memquantity; count++) {
        cout << hex << setw(4) << memory[count] << " ";
        cr = true;
        if (count % 8 == 7) {
            cout << endl;
            cr = false;
        }
    }
    if (cr)
        cout << endl;
}

unsigned signExtender7B(unsigned imm){
    if ((imm & 64) == 64){
        return imm | 65408; 
    }
    return imm; 
}

/*
    Main function
    Takes command-line args as documented below
*/
int main(int argc, char *argv[]) {

    //In command line, main takes the .bin file you add after the program 
    char *filename = nullptr;
    bool do_help = false;
    bool arg_error = false;
    for (int i=1; i<argc; i++) {
        string arg(argv[i]);
        if (arg.rfind("-",0)==0) {
            if (arg== "-h" || arg == "--help")
                do_help = true;
            else
                arg_error = true;
        } else {
            if (filename == nullptr)
                filename = argv[i];
            else
                arg_error = true;
        }
    }

    /* Display error message if appropriate */
    if (arg_error || do_help || filename == nullptr) {
        cerr << "usage " << argv[0] << " [-h] filename" << endl << endl;
        cerr << "Simulate E20 machine" << endl << endl;
        cerr << "positional arguments:" << endl;
        cerr << "  filename    The file containing machine code, typically with .bin suffix" << endl<<endl;
        cerr << "optional arguments:"<<endl;
        cerr << "  -h, --help  show this help message and exit"<<endl;
        return 1;
    }

    ifstream f(filename);
    if (!f.is_open()) {
        cerr << "Can't open file "<<filename<<endl;
        return 1;
    }

    
    unsigned short memory[8192] = {0}; 
    unsigned short pc = 0; 
    unsigned short registers[8] = {0}; 
    
    load_machine_code(f, memory); 

    unsigned short dst; 
    unsigned short srcA; 
    unsigned short srcB; 
    unsigned short imm; 
    unsigned short opcode; 
    bool halt = false; 

    while (!halt){ 
        unsigned short curr_instruction = memory[pc]; 
        opcode = curr_instruction>>13;  
        if (opcode == 0){
            srcA = curr_instruction>>10 & 7;
            srcB = curr_instruction>>7 & 7; 
            dst = curr_instruction>>4 & 7;
            if (dst != 0){ //the following directions all modify the dst register
                if ((curr_instruction & 15) == 0){
                    unsigned short result = registers[srcA] + registers[srcB]; 
                    registers[dst] = result;  
                    pc += 1; 
                    //add
                }
                else if ((curr_instruction & 15) == 1){
                    unsigned short result = registers[srcA] - registers[srcB]; 
                    registers[dst] = result; 
                    pc += 1; 
                    //sub
                }
                else if ((curr_instruction & 15) == 2){
                    unsigned short result = registers[srcA] | registers[srcB]; 
                    registers[dst] = result; 
                    pc += 1; 
                    //or
                }
                else if ((curr_instruction & 15) == 3){
                    unsigned short result = registers[srcA] & registers[srcB]; 
                    registers[dst] = result; 
                    pc += 1; 
                    //and 
                }
                else if ((curr_instruction & 15) == 4){
                    if (registers[srcA] < registers[srcB]){
                        registers[dst] = 1; 
                    }
                    else{
                        registers[dst] = 0; 
                    }
                    pc += 1; 
                    //slt
                }
            }
            if ((curr_instruction & 15) == 8){
                pc = registers[srcA];
                //jr 
            }
        }
        else if (opcode == 7){
            srcA = curr_instruction>>10 & 7;
            dst = curr_instruction>>7 & 7; 
            imm = signExtender7B(curr_instruction & 127);  
            if (dst != 0){
                if (registers[srcA] < imm){
                    registers[dst] = 1; 
                }
                else{
                    registers[dst] = 0; 
                }
            }
            pc += 1; 
            //slti
        }
        else if (opcode == 4){
            srcA = curr_instruction>>10 & 7;
            dst = curr_instruction>>7 & 7; 
            imm = signExtender7B(curr_instruction & 127); 
            if (dst != 0){
                unsigned short result = registers[srcA] + imm;
                registers[dst] = memory[result]; 
            }
            pc += 1; 
            //lw
        }
        else if (opcode == 5){
            srcA = curr_instruction>>10 & 7;
            srcB = curr_instruction>>7 & 7; 
            imm = signExtender7B(curr_instruction & 127); 
            unsigned short address = registers[srcA] + imm; 
            memory[address] = registers[srcB]; 
            pc += 1; 
            //sw 
        }
        else if (opcode == 6){
            srcA = curr_instruction>>10 & 7;
            srcB = curr_instruction>>7 & 7; 
            imm = signExtender7B(curr_instruction & 127); 
            if (registers[srcA] == registers[srcB]){
                pc += imm;  
            }
            pc += 1; 
            //jeq 
        }
        else if (opcode == 1){
            srcA = curr_instruction>>10 & 7; 
            dst = curr_instruction>>7 & 7;  
            imm = signExtender7B(curr_instruction & 127); 
            if (dst != 0){
                unsigned short result = registers[srcA] + imm;
                registers[dst] = result; 
            }
            pc += 1; 
            //addi
        }
        else if (opcode == 2){
            imm = curr_instruction & 8191;
            if (pc == imm){
                halt = true; 
            }
            else{
                pc = imm;
            } 
            //j
        }
        else if (opcode == 3){
            imm = curr_instruction & 8191;
            registers[7] = pc+1; 
            pc = imm; 
            //jal
        }
        pc %= 8192; 
    }

    print_state(pc, registers, memory, 128); 

    f.close(); 

    return 0;
}
