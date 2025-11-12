#include <cstddef>
#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <limits>
#include <iomanip>
#include <cstdlib>
#include <regex>

using namespace std;

size_t const static NUM_REGS = 8;
size_t const static MEM_SIZE = 1<<13;
size_t const static REG_SIZE = 1<<16;

void load_machine_code(ifstream &f, unsigned short mem[]) {
    regex machine_code_re("^ram\\[(\\d+)\\] = 16'b(\\d+);.*$");
    size_t expectedaddr = 0; 
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

unsigned signExtender7B(unsigned imm){
    if ((imm & 64) == 64){
        return imm | 65408; 
    }
    return imm; 
}

/*
    Prints out the correctly-formatted configuration of a cache.

    @param cache_name The name of the cache. "L1" or "L2"

    @param size The total size of the cache, measured in memory cells.
        Excludes metadata

    @param assoc The associativity of the cache. One of [1,2,4,8,16]

    @param blocksize The blocksize of the cache. One of [1,2,4,8,16,32,64])

    @param num_rows The number of rows in the given cache.
*/
void print_cache_config(const string &cache_name, int size, int assoc, int blocksize, int num_rows) {
    cout << "Cache " << cache_name << " has size " << size <<
        ", associativity " << assoc << ", blocksize " << blocksize <<
        ", rows " << num_rows << endl;
}

/*
    Prints out a correctly-formatted log entry.

    @param cache_name The name of the cache where the event
        occurred. "L1" or "L2"

    @param status The kind of cache event. "SW", "HIT", or
        "MISS"

    @param pc The program counter of the memory
        access instruction

    @param addr The memory address being accessed.

    @param row The cache row or set number where the data
        is stored.
*/
void print_log_entry(const string &cache_name, const string &status, int pc, int addr, int row) {
    cout << left << setw(8) << cache_name + " " + status <<  right <<
        " pc:" << setw(5) << pc <<
        "\taddr:" << setw(5) << addr <<
        "\trow:" << setw(4) << row << endl;
}

/**
    Main function
    Takes command-line args as documented below
*/
int main(int argc, char *argv[]) {
    /*
        Parse the command-line arguments
    */
    char *filename = nullptr;
    bool do_help = false;
    bool arg_error = false;
    string cache_config;
    for (int i=1; i<argc; i++) {
        string arg(argv[i]);
        if (arg.rfind("-",0)==0) {
            if (arg== "-h" || arg == "--help")
                do_help = true;
            else if (arg=="--cache") {
                i++;
                if (i>=argc)
                    arg_error = true;
                else
                    cache_config = argv[i];
            }
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
        cerr << "usage " << argv[0] << " [-h] [--cache CACHE] filename" << endl << endl;
        cerr << "Simulate E20 cache" << endl << endl;
        cerr << "positional arguments:" << endl;
        cerr << "  filename    The file containing machine code, typically with .bin suffix" << endl<<endl;
        cerr << "optional arguments:"<<endl;
        cerr << "  -h, --help  show this help message and exit"<<endl;
        cerr << "  --cache CACHE  Cache configuration: size,associativity,blocksize (for one"<<endl;
        cerr << "                 cache) or"<<endl;
        cerr << "                 size,associativity,blocksize,size,associativity,blocksize"<<endl;
        cerr << "                 (for two caches)"<<endl;
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

    /* parse cache config */
    if (cache_config.size() > 0) {
        vector<int> parts;
        size_t pos;
        size_t lastpos = 0;

        //Variables for one cache 
        int c1L1size; //rows 
        int c1L1assoc;//blocks per row 
        int c1L1blocksize; //blocksize 
        int c1L1rows; 
        vector<vector<int>> cache; 

        //Variables for two caches 
        int c2L1size;
        int c2L1assoc;
        int c2L1blocksize;
        int c2L1rows;
        int c2L2size;
        int c2L2assoc;
        int c2L2blocksize;
        int c2L2rows;
        vector<vector<int>> L1cache; 
        vector<vector<int>> L2cache; 

        while ((pos = cache_config.find(",", lastpos)) != string::npos) {
            parts.push_back(stoi(cache_config.substr(lastpos,pos)));
            lastpos = pos + 1;
        }
        parts.push_back(stoi(cache_config.substr(lastpos)));
        if (parts.size() == 3) {
            c1L1size = parts[0]; 
            c1L1assoc = parts[1]; //blocks per row 
            c1L1blocksize = parts[2]; //blocksize 
            c1L1rows = c1L1size / (c1L1assoc * c1L1blocksize);
            cache = vector<vector<int>>(c1L1rows, vector<int>(c1L1assoc, -1)); 
            print_cache_config("L1", c1L1size, c1L1assoc, c1L1blocksize, c1L1rows);
            // TODO: execute E20 program and simulate one cache here
        } else if (parts.size() == 6) {
            c2L1size = parts[0];
            c2L1assoc = parts[1];
            c2L1blocksize = parts[2];
            c2L1rows = c2L1size / (c2L1assoc * c2L1blocksize);
            c2L2size = parts[3];
            c2L2assoc = parts[4];
            c2L2blocksize = parts[5];
            c2L2rows = c2L2size / (c2L2assoc * c2L2blocksize);
            L1cache = vector<vector<int>>(c2L1rows, vector<int>(c2L1assoc, -1)); 
            L2cache = vector<vector<int>>(c2L2rows, vector<int>(c2L2assoc, -1));
            print_cache_config("L1", c2L1size, c2L1assoc, c2L1blocksize, c2L1rows); 
            print_cache_config("L2", c2L2size, c2L2assoc, c2L2blocksize, c2L2rows);
            // TODO: execute E20 program and simulate two caches here
        } else {
            cerr << "Invalid cache config"  << endl;
            return 1;
        }

        //E20 Processor 

        while (!halt && pc < 8192){ 
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
                        //add
                    }
                    else if ((curr_instruction & 15) == 1){
                        unsigned short result = registers[srcA] - registers[srcB]; 
                        registers[dst] = result; 
                        //sub
                    }
                    else if ((curr_instruction & 15) == 2){
                        unsigned short result = registers[srcA] | registers[srcB]; 
                        registers[dst] = result; 
                        //or
                    }
                    else if ((curr_instruction & 15) == 3){
                        unsigned short result = registers[srcA] & registers[srcB]; 
                        registers[dst] = result; 
                        //and 
                    }
                    else if ((curr_instruction & 15) == 4){
                        if (registers[srcA] < registers[srcB]){
                            registers[dst] = 1; 
                        }
                        else{
                            registers[dst] = 0; 
                        }
                        //slt
                    }
                }
                pc += 1;
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
                unsigned short address = registers[srcA] + imm;
                if (dst != 0){
                    registers[dst] = memory[address]; 
                }
                //lw

                //1 Cache 
                if (parts.size() == 3){
                    int blockid = address / c1L1blocksize; 
                    int index = blockid % c1L1rows; 
                    int curr_tag = blockid / c1L1rows; 
                    bool evictLRU = true; 
                    bool tagFound = false; 

                    for (int i = cache[index].size()-1; i >= 0 ; i--){  //Traverse through the row to check for current tag 
                        if (cache[index][i] == curr_tag){
                            for (int j = i+1; j < cache[index].size(); j++){ //If current tag is found, move current tag to front and shift all other tags back 
                                cache[index][j-1] = cache[index][j]; 
                            }
                            cache[index][cache[index].size() - 1] = curr_tag;
                            tagFound = true; 
                            break; 
                        }
                        else if (cache[index][i] == -1){
                            for (int i = 1; i < cache[index].size(); i++){ //If an empty space in the row is available, move current tag to front and shift other tags/empty spaces back 
                                cache[index][i-1] = cache[index][i]; 
                            }
                            cache[index][cache[index].size() - 1] = curr_tag; 
                            evictLRU = false; 
                            break; 
                        }
                    }

                    if (evictLRU && !tagFound){  
                        for (int i = 1; i < cache[index].size(); i++){  //If an eviction is necessary, shift all elements back to remove the least recently used tag, and then insert the current tag at the front
                            cache[index][i-1] = cache[index][i]; 
                        }
                        cache[index][cache[index].size() - 1] = curr_tag;
                    }

                    if (tagFound){
                        print_log_entry("L1","HIT",pc,address,index);                     
                    }
                    else{
                        print_log_entry("L1","MISS",pc,address,index);                     
                    }
                }
                //2 Caches 
                else if (parts.size() == 6){
                    int L1blockid = address / c2L1blocksize; 
                    int L1index = L1blockid % c2L1rows;
                    int L1curr_tag = L1blockid / c2L1rows; 
                    bool L1evictLRU = true; 
                    bool L1tagFound = false; 

                    int L2blockid = address / c2L2blocksize; 
                    int L2index = L2blockid % c2L2rows;
                    int L2curr_tag = L2blockid / c2L2rows; 
                    bool L2evictLRU = true; 
                    bool L2tagFound = false; 

                    for (int i = L1cache[L1index].size()-1; i >= 0 ; i--){ 
                        if (L1cache[L1index][i] == L1curr_tag){
                            for (int j = i+1; j < L1cache[L1index].size(); j++){ 
                                L1cache[L1index][j-1] = L1cache[L1index][j]; 
                            }
                            L1cache[L1index][L1cache[L1index].size() - 1] = L1curr_tag;
                            L1tagFound = true; 
                            break; 
                        }
                        else if (L1cache[L1index][i] == -1){
                            for (int i = 1; i < L1cache[L1index].size(); i++){
                                L1cache[L1index][i-1] = L1cache[L1index][i]; 
                            }
                            L1cache[L1index][L1cache[L1index].size() - 1] = L1curr_tag;  
                            L1evictLRU = false; 
                            break; 
                        }
                    }

                    if (L1evictLRU){
                        for (int i = 1; i < L1cache[L1index].size(); i++){
                            L1cache[L1index][i-1] = L1cache[L1index][i]; 
                        }
                        L1cache[L1index][L1cache[L1index].size() - 1] = L1curr_tag; 
                    }

                    if(L1tagFound){
                        print_log_entry("L1","HIT",pc,address,L1index); 
                    }
                    else{
                        print_log_entry("L1","MISS",pc,address,L1index);  //Only if the L1 Cache is a miss do we consult the L2 cache 

                        for (int i = L2cache[L2index].size()-1; i >= 0 ; i--){ 
                            if (L2cache[L2index][i] == L2curr_tag){
                                for (int j = i+1; j < L2cache[L2index].size(); j++){ 
                                    L2cache[L2index][j-1] = L2cache[L2index][j]; 
                                }
                                L2cache[L2index][L2cache[L2index].size() - 1] = L2curr_tag;
                                L2tagFound = true; 
                                break; 
                            }
                            else if (L2cache[L2index][i] == -1){
                                for (int i = 1; i < L2cache[L2index].size(); i++){
                                    L2cache[L2index][i-1] = L2cache[L2index][i]; 
                                }
                                L2cache[L2index][L2cache[L2index].size() - 1] = L2curr_tag; 
                                L2evictLRU = false; 
                                break; 
                            }
                        }

                        if (L2evictLRU && !L2tagFound){
                            for (int i = 1; i < L2cache[L2index].size(); i++){
                                L2cache[L2index][i-1] = L2cache[L2index][i]; 
                            }
                            L2cache[L2index][L2cache[L2index].size() - 1] = L2curr_tag; 
                        }
                        
                        if (L2tagFound){
                            print_log_entry("L2","HIT",pc,address,L2index); 
                        }
                        else{
                            print_log_entry("L2","MISS",pc,address,L2index); 
                        }
                    }
                }
                pc += 1; 
            }
            else if (opcode == 5){
                srcA = curr_instruction>>10 & 7;
                srcB = curr_instruction>>7 & 7; 
                imm = signExtender7B(curr_instruction & 127); 
                unsigned short address = registers[srcA] + imm; 
                memory[address] = registers[srcB]; 
                //sw 

                //1 Cache 
                if (parts.size() == 3){
                    int blockid = address / c1L1blocksize; 
                    int index = blockid % c1L1rows;
                    int curr_tag = blockid / c1L1rows; 
                    bool evictLRU = true; 
                    bool tagFound = false; 

                    for (int i = cache[index].size()-1; i >= 0 ; i--){ 
                        if (cache[index][i] == curr_tag){
                            for (int j = i+1; j < cache[index].size(); j++){ 
                                cache[index][j-1] = cache[index][j]; 
                            }
                            cache[index][cache[index].size() - 1] = curr_tag;
                            tagFound = true; 
                            break; 
                        }
                        else if (cache[index][i] == -1){
                            for (int i = 1; i < cache[index].size(); i++){
                                cache[index][i-1] = cache[index][i]; 
                            }
                            cache[index][cache[index].size() - 1] = curr_tag; 
                            evictLRU = false; 
                            break; 
                        }
                    }

                    if (evictLRU){
                        for (int i = 1; i < cache[index].size(); i++){
                            cache[index][i-1] = cache[index][i]; 
                        }
                        cache[index][cache[index].size() - 1] = curr_tag; 
                    }
                    print_log_entry("L1","SW",pc,address,index);                     
                }
                //2 Caches 
                else if (parts.size() == 6){
                    int L1blockid = address / c2L1blocksize; 
                    int L1index = L1blockid % c2L1rows;
                    int L1curr_tag = L1blockid / c2L1rows; 
                    bool L1evictLRU = true; 
                    bool L1tagFound = false; 

                    int L2blockid = address / c2L2blocksize; 
                    int L2index = L2blockid % c2L2rows;
                    int L2curr_tag = L2blockid / c2L2rows; 
                    bool L2evictLRU = true; 
                    bool L2tagFound = false; 

                    for (int i = L1cache[L1index].size()-1; i >= 0 ; i--){ 
                        if (L1cache[L1index][i] == L1curr_tag){
                            for (int j = i+1; j < L1cache[L1index].size(); j++){ 
                                L1cache[L1index][j-1] = L1cache[L1index][j]; 
                            }
                            L1cache[L1index][L1cache[L1index].size() - 1] = L1curr_tag;
                            L1tagFound = true; 
                            break; 
                        }
                        else if (L1cache[L1index][i] == -1){
                            for (int i = 1; i < L1cache[L1index].size(); i++){
                                L1cache[L1index][i-1] = L1cache[L1index][i]; 
                            }
                            L1cache[L1index][L1cache[L1index].size() - 1] = L1curr_tag;  
                            L1evictLRU = false; 
                            break; 
                        }
                    }

                    if (L1evictLRU && !L1tagFound){
                        for (int i = 1; i < L1cache[L1index].size(); i++){
                            L1cache[L1index][i-1] = L1cache[L1index][i]; 
                        }
                        L1cache[L1index][L1cache[L1index].size() - 1] = L1curr_tag; 
                    }
                    print_log_entry("L1","SW",pc,address,L1index); 

                    for (int i = L2cache[L2index].size()-1; i >= 0 ; i--){ 
                            if (L2cache[L2index][i] == L2curr_tag){
                                for (int j = i+1; j < L2cache[L2index].size(); j++){ 
                                    L2cache[L2index][j-1] = L2cache[L2index][j]; 
                                }
                                L2cache[L2index][L2cache[L2index].size() - 1] = L2curr_tag;
                                L2tagFound = true; 
                                break; 
                            }
                            else if (L2cache[L2index][i] == -1){
                                for (int i = 1; i < L2cache[L2index].size(); i++){
                                    L2cache[L2index][i-1] = L2cache[L2index][i]; 
                                }
                                L2cache[L2index][L2cache[L2index].size() - 1] = L2curr_tag; 
                                L2evictLRU = false; 
                                break; 
                            }
                    }

                    if (L2evictLRU && !L2tagFound){
                        for (int i = 1; i < L2cache[L2index].size(); i++){
                            L2cache[L2index][i-1] = L2cache[L2index][i]; 
                        }
                        L2cache[L2index][L2cache[L2index].size() - 1] = L2curr_tag; 
                    }
                    print_log_entry("L2","SW",pc,address,L2index); 
                }
                pc += 1; 
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
        }    
    }

    f.close(); 

    return 0;
}


//ra0Eequ6ucie6Jei0koh6phishohm9