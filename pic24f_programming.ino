/*

Programming a PIC24F

*/

#define ICSP_MODE 0x4D434851
#define CW2_ADDR 0x00ABFC

//ICPS pin definitions
const int PGC = 9; //PORTB1
const int PGD = 8; //PORTB0
const int MCLR = 7;

// Used to to flag if the device has entered Program Memory Entry
// or not, ie has the first SIX command been sent or not
boolean icsp_mode_entered;

//Used to skip over icsp entry and memory block erase
boolean memory_erased;

boolean eof = false;

//four bytes for address
unsigned char addr[4];
//64 instructions * 4 bytes each
unsigned long instr_data[256];
unsigned char config_regs[4];

unsigned char control_byte;

//loop variables
int main_i;

void setup() {
 
  Serial.begin(9600);
  
  pinMode(MCLR, OUTPUT);
  digitalWrite(MCLR, LOW);
  
  icsp_mode_entered = false;
  memory_erased = false;
  
}


void loop() {
  
  //Wait for the start byte so erasing memory isn't done unnecessarily
  if(!memory_erased) {
    while(Serial.available() < 1);
    Serial.read();
    enter_icsp();
    block_erase_user_space();
    Serial.print('A');
    memory_erased = true;
  }
  
  //Start control byte loop
  while(Serial.available() < 1);
  control_byte = Serial.read();
  
  //instruction block
  if(control_byte == 'i') {
    
    Serial.print('B');
    
    //address
    for(main_i = 0; main_i < 4; main_i++) {
        
      while(Serial.available() < 1);
      addr[main_i] = Serial.read();
    }
    
    Serial.print('C');
    
    for(main_i = 0; main_i < 256; main_i++) {
        
      while(Serial.available() < 1);
      instr_data[main_i] = (unsigned long)Serial.read();      
      
    }
    
    //Write block to PIC
    write_code_memory(instr_data, four_char_to_ul(addr));
    //Serial.println(four_char_to_ul(addr));
    
    Serial.print('D');
  }
  //configuration words
  else if(control_byte == 'c') {
    
    Serial.print('E');
    
    //address
    for(main_i = 0; main_i < 4; main_i++) {
        
      while(Serial.available() < 1);
      addr[main_i] = Serial.read();
    }
    
    Serial.print('F');
    
    for(main_i = 0; main_i < 4; main_i++) {
        
      while(Serial.available() < 1);
      config_regs[main_i] = Serial.read();      
      
    }
    
    write_config_regs((unsigned long)((config_regs[1] << 8) + config_regs[0]),
                     (unsigned long)((config_regs[3] << 8) + config_regs[2]));
    
    Serial.print('G');
    
  }
  //eof
  else if(control_byte == 'e') {
    
    digitalWrite(MCLR, LOW);
    Serial.print('H');
    while(1);
    
  }
  //error
  else {
    
    digitalWrite(MCLR, LOW);
    Serial.print('I');
    while(1);
    
  }
  
  
}

/*
* Puts the PIC into ICSP mode. It then writes a NOP with the 5 extra clock cycles
* required for the first instruction. resets the PGD and PGC values to LOW
* before exiting
*/
void enter_icsp() {
 
  long temp_code = ICSP_MODE;
  int loc_i;
  
  digitalWrite(MCLR, HIGH);
  delay(1);
  digitalWrite(MCLR, LOW);
  delay(1);
  
  for(loc_i = 0; loc_i < 32; loc_i++) {
    
    write_bit((temp_code >> 31) & 0x00000001);
    temp_code = temp_code << 1;
    
  }
  
  PORTB = B00000000;
  
  delay(2);
  digitalWrite(MCLR, HIGH);
  delay(26);
  
}

/*
* This function reads the two configuration registers starting at the const CW2_Addr. 
* Must already be in ICSP mod, HEXe before calling this.
* parameters:
*    data  -  unsigned long array. data[0] = CW2, data[1] = CW1
* returns:
*    none
*/

void read_config_reg(unsigned long *data) {
 
  unsigned long pic_cw2_addr = CW2_ADDR;
  unsigned long temp;
  
  //Table 3-9 Reading All Configuration Memory
  //Step 1
  write_instruction(0);
  write_instruction(0x040200);
  write_instruction(0);
  
  //Step 2
  temp = 0x200000 | ((pic_cw2_addr & 0x00FF0000) >> 12);
  write_instruction(temp);
  write_instruction(0x880190);
  // !!!!! if it doesn't work try 0x200006. and if 0x200006 works then Table 3-9 has a typo
  temp = 0x200006 | ((pic_cw2_addr & 0x0000FFFF) << 4);
  write_instruction(temp);
  write_instruction(0x207847);
  write_instruction(0);
  
  //Step 3 (CW2)
  write_instruction(0xBA0BB6);
  write_instruction(0);
  write_instruction(0);
  data[0] = read_register();
  write_instruction(0);
  
  //Step 4 (CW1)
  write_instruction(0xBA0BB6);
  write_instruction(0);
  write_instruction(0);
  data[1] = read_register();
  write_instruction(0);
  
  //Step 5
  write_instruction(0x040200);
  write_instruction(0);
  
  PORTB = B00000000;
  
}


/*
* This function reads the two instructions starting from the input parameter addr
* parameters:
*    addr  -  the starting address to read configurations from
*    data  -  unsigned long array. data[0] = *addr, data[1] = *addr+1
* returns:
*    none
*/

void read_code_memory(unsigned long addr, unsigned long *data) {
 
  unsigned long temp_addr = addr, temp_instr, temp_data;
  
  //Table 3-8
  //Step 1
  write_instruction(0);
  write_instruction(0x040200);
  write_instruction(0);
  
  //Step 2
  temp_instr = 0x200000 | ((temp_addr & 0x00FF0000) >> 12);
  write_instruction(temp_instr);
  write_instruction(0x880190);
  temp_instr = 0x200006 | ((temp_addr & 0x0000FFFF) << 4);
  write_instruction(temp_instr);
  
  //Step 3
  write_instruction(0x207847);
  write_instruction(0);
  
  //Step 4
  write_instruction(0xBA0B96);
  write_instruction(0);
  write_instruction(0);
  data[0] = read_register();
  write_instruction(0);
  
  write_instruction(0xBADBB6);
  write_instruction(0);
  write_instruction(0);
  write_instruction(0xBAD3D6);
  write_instruction(0);
  write_instruction(0);
  temp_addr = read_register();
  write_instruction(0);
  
  write_instruction(0xBA0BB6);
  write_instruction(0);
  write_instruction(0);
  data[1] = read_register();
  write_instruction(0);
  
  //Step 5
  write_instruction(0x040200);
  write_instruction(0);
  
  PORTB = B00000000;
  
  data[0] += (temp_addr & 0x000000FF) << 16;
  data[1] += (temp_addr & 0x0000FF00) << 8;
  
}

/*
* This function will call the block erase command set for user space only.
* This prevents problems with configuration registers. And doing the block
* erase means the flash configuration registers will be reset back to all 1s
*/
void block_erase_user_space() {
  
  unsigned int wr_polling;
  
  //Table 3-4
  //Step 1
  write_instruction(0);
  write_instruction(0x040200);
  write_instruction(0);
  
  //Step 2
  write_instruction(0x2404FA);
  write_instruction(0x883B0A);
  
  //Step 3
  write_instruction(0x200000);
  write_instruction(0x880190);
  write_instruction(0x200000);
  write_instruction(0xBB0800);
  write_instruction(0);
  write_instruction(0);
  
  //Step 4
  write_instruction(0xA8E761);
  write_instruction(0);
  write_instruction(0);
  
  //Step 5
  do{
    write_instruction(0x040200);
    write_instruction(0);
    write_instruction(0x803B02);
    write_instruction(0x883C22);
    write_instruction(0);
    wr_polling = read_register();
    write_instruction(0);
  }while((wr_polling & 0x8000) == 0x8000);
  
}

/*
* This funciton writes 64 instructions starting at start_addr.
* parameters:
*   *instr_arr   -  pointer to a 256-element array holding the 64 instructions to be programmed, with
*                  instr_arr[0] being the first byte at start_addr
*    start_addr  -  starting address for the 64 instructions
* returns:
*    currently nothing but could be modified in the future to return an error code if the WR bit doesn't
*    get cleared after a certain amount of time. right now it just keeps polling. will try to do that after
*    something gets working. now, if it hangs, manual reset
*/
void write_code_memory(unsigned long *instr_arr, unsigned long start_addr) {
  
  unsigned long temp_data, temp_addr = start_addr;
  unsigned int wr_polling;
  int loc_i;
  
  //Table 3-5
  //Step 1
  write_instruction(0);
  write_instruction(0x040200);
  write_instruction(0);
  
  //Step 2
  write_instruction(0x24001A);
  write_instruction(0x883B0A);
  
  //Step 3
  temp_data = 0x200000 | ((temp_addr & 0x00FF0000) >> 12);
  write_instruction(temp_data);
  write_instruction(0x880190);
  temp_data = 0x200007 | ((temp_addr & 0x0000FFFF) << 4);
  write_instruction(temp_data);
  
  //Step 6 - loop through Steps 4 & 5 sixteen times
  for(loc_i = 0; loc_i < 16; loc_i ++) {
      //Step 4
     //LSW0
     temp_data = 0x200000 | ((instr_arr[(loc_i * 16) + 1] << 12) | instr_arr[(loc_i * 16) + 0] << 4);
     write_instruction(temp_data);
     //MSB1:MSB0
     temp_data = 0x200001 | ((instr_arr[(loc_i * 16) + 6] << 12) | instr_arr[(loc_i * 16) + 2] << 4);
     write_instruction(temp_data);
     //LSW1
     temp_data = 0x200002 | ((instr_arr[(loc_i * 16) + 5] << 12) | instr_arr[(loc_i * 16) + 4] << 4);
     write_instruction(temp_data);
     //LSW2
     temp_data = 0x200003 | ((instr_arr[(loc_i * 16) + 9] << 12) | instr_arr[(loc_i * 16) + 8] << 4);
     write_instruction(temp_data);
     //MSB3:MSB2
     temp_data = 0x200004 | ((instr_arr[(loc_i * 16) + 14] << 12) | instr_arr[(loc_i * 16) + 10] << 4);
     write_instruction(temp_data);
     //LSW3
     temp_data = 0x200005 | ((instr_arr[(loc_i * 16) + 13] << 12) | instr_arr[(loc_i * 16) + 12] << 4);
     write_instruction(temp_data);
     
     //Step 5
     write_instruction(0xEB0300);
     write_instruction(0); 
     write_instruction(0xBB0BB6);
     write_instruction(0); 
     write_instruction(0); 
     write_instruction(0xBBDBB6);
     write_instruction(0); 
     write_instruction(0); 
     write_instruction(0xBBEBB6);
     write_instruction(0); 
     write_instruction(0); 
     write_instruction(0xBB1BB6);
     write_instruction(0); 
     write_instruction(0); 
     write_instruction(0xBB0BB6);
     write_instruction(0); 
     write_instruction(0); 
     write_instruction(0xBBDBB6);
     write_instruction(0); 
     write_instruction(0); 
     write_instruction(0xBBEBB6);
     write_instruction(0); 
     write_instruction(0); 
     write_instruction(0xBB1BB6);
     write_instruction(0); 
     write_instruction(0);
  }
  
  //Step 7
  write_instruction(0xA8E761);
  write_instruction(0); 
  write_instruction(0);
  
  //Step 8
  do{
    write_instruction(0x040200);
    write_instruction(0);
    write_instruction(0x803B02);
    write_instruction(0x883C22);
    write_instruction(0);
    wr_polling = read_register();
    write_instruction(0);
  }while((wr_polling & 0x8000) == 0x8000);
  
  //Step 9
  write_instruction(0);
  write_instruction(0x040200);
  
}

/*
* This function will take as input CW1 and CW2 values and write to their appropriate address.
* The address is controlled through constants in case in the future this should support some
* other PIC.
* Had a problem merely looping Steps 5 - 9 and instead had to manually increment the address
* and redo all the steps for each word
* parameters:
*   cw1_val, cw2_val -  value of the CW1 and CW2 configuration registers
* returns:
*   nothing right now, but should be modified to return an error indicating if there was a timeout
*   while polling the WR bit
*/
void write_config_regs(unsigned long cw1_val, unsigned long cw2_val) {
  
  unsigned long cw2_addr = CW2_ADDR, temp;
  unsigned int wr_polling;
  int loc_i;
  
  /***************************************************
  *********************  CW2  ************************
  ***************************************************/
  //Step 1
  write_instruction(0);
  write_instruction(0x040200);
  write_instruction(0);
  
  //Step 2
  temp = 0x200007 | ((cw2_addr & 0x0000FFFF) << 4);
  write_instruction(temp);
  
  //Step 3
  write_instruction(0x24003A);
  write_instruction(0x883B0A);
  
  //Step 4
  temp = 0x200000 | ((cw2_addr & 0x00FF0000) >> 12);
  write_instruction(temp);
  write_instruction(0x880190);
  
  temp = 0x200006 | ((cw2_val & 0x0000FFFF) << 4);
  write_instruction(temp);
    
  //Step 6
  write_instruction(0);
  write_instruction(0xBB1B86);
  write_instruction(0);
  write_instruction(0);
  
  //Step 7
  write_instruction(0xA8E761);
  write_instruction(0);
  write_instruction(0);
  
  //Step 8
  do{
    write_instruction(0x040200);
    write_instruction(0);
    write_instruction(0x803B02);
    write_instruction(0x883C22);
    write_instruction(0);
    wr_polling = read_register();
    write_instruction(0);
  }while((wr_polling & 0x8000) == 0x8000);
  
  //Step 9
  write_instruction(0);
  write_instruction(0x040200);
  
  /***************************************************
  *********************  CW1  ************************
  ***************************************************/
  cw2_addr += 2;
  
  //Step 1
  write_instruction(0);
  write_instruction(0x040200);
  write_instruction(0);
  
  //Step 2
  temp = 0x200007 | ((cw2_addr & 0x0000FFFF) << 4);
  write_instruction(temp);
  
  //Step 3
  write_instruction(0x24003A);
  write_instruction(0x883B0A);
  
  //Step 4
  temp = 0x200000 | ((cw2_addr & 0x00FF0000) >> 12);
  write_instruction(temp);
  write_instruction(0x880190);
  
  temp = 0x200006 | ((cw1_val & 0x0000FFFF) << 4);
  write_instruction(temp);
    
  //Step 6
  write_instruction(0);
  write_instruction(0xBB1B86);
  write_instruction(0);
  write_instruction(0);
  
  //Step 7
  write_instruction(0xA8E761);
  write_instruction(0);
  write_instruction(0);
  
  //Step 8
  do{
    write_instruction(0x040200);
    write_instruction(0);
    write_instruction(0x803B02);
    write_instruction(0x883C22);
    write_instruction(0);
    wr_polling = read_register();
    write_instruction(0);
  }while((wr_polling & 0x8000) == 0x8000);
  
  //Step 9
  write_instruction(0);
  write_instruction(0x040200);
    
}


/*********************************************************************************
****************      I/O Helper Functions     ***********************************
*********************************************************************************/

/*
* This function will write the 24-bit input parameter.
* It also checks a global icsp_mode_entered flag. If this flag is true, the target chip
* has already entered Program Memory Entry mode. If it is false, it has not and this
* is the first SIX command, which requires an extra 5 clocks between command and instr
* It leaves PGD and PGC low when leaving and direction to INPUT
*/
void write_instruction(unsigned long data) {
 
  unsigned long temp_data = data;
  unsigned int command = 0, command_count;
  int loc_i;
  
  if(!icsp_mode_entered) {
    command_count = 9;
    icsp_mode_entered = true;
  }
  else {
    command_count = 4;
  }
  
  for(loc_i = 0; loc_i < command_count; loc_i++){
    write_bit(command);
    command = command >> 1;
  }
  
  DDRB = B00000011;
  delayMicroseconds(10);
  for(loc_i = 0; loc_i < 24; loc_i++) {
    write_bit(temp_data & 0x00000001);
    temp_data = temp_data >> 1;
  }
  
  DDRB = B00000011;
  PORTB = B00000000;
 
}

/*
*  This function sends the read command 0b0001 and then reads and returns
*  the 16-bit readout. It sets the direction of the pins and resets back to 
*  INPUT before leaving. It uses the read_bit & write_bit helper functions.
*  returns:
*    unsigned long representing the 16 bits read from the Data line
*/

unsigned int read_register() {
  
  unsigned int command = 1; // read command is always 1
  unsigned int return_data = 0;
  int loc_i;
  
  for(loc_i = 0; loc_i < 4; loc_i++){
    write_bit(command);
    command = command >> 1;
  }
  
  DDRB = B00000010;
  delayMicroseconds(10);
  for(loc_i = 0; loc_i < 24; loc_i++) {
    if (loc_i > 7)
      return_data = return_data | ((unsigned int)read_bit() << (loc_i - 8));
    else
      read_bit();
  }
  
  DDRB = B00000011;
  PORTB = B00000000;
  
  return return_data;
  
}

/*********************************************************************************
****************   Low Level Helper Functions  ***********************************
*********************************************************************************/

/*
* Pulses PGC as the passed in value (least sig bit) is set for PGD
*/
inline void write_bit(unsigned int val) {
  
  PORTB = B00000000 | (val & 0x0001);
  delay(1);
  PORTB = B00000010 | (val & 0x0001);
  delay(1);
  
}

/*
* Pulses PGC and returns what was read on PGD
*/
inline unsigned int read_bit() {
  
  PORTB = B00000000;
  delay(1);
  PORTB = B00000010;
  delay(1);
  return PINB & B00000001;
  
}


/*********************************************************************************
****************   Data Type Helper Functions  ***********************************
*********************************************************************************/


/*
* This function used to type cast char array to unsigned long array
* It's designed specifically to convert 16 elements of src array
* to unsigned long elements of the dst array
* Parameters:
*    src          - the 256 element array holding the 64 instruction words
*    src_offset   - the offset into the src array. should be # of 16 char blocks offset
*                  eg. if element 64 should be the starting element of src, src_offset = 4
*    dst          - a 16 element unsigned long array that will be populated 
*/
void cast_char_to_ul (unsigned char* src, int src_offset, unsigned long* dst) {

  int j;
  for(j = 0; j < 16; j++) {
    dst[j] = (unsigned long)src[src_offset * 16 + j];
  }
    
}

/*
* Packing 4 chars into an unsigned long
* parameters:
*  src          -  4 element char array
* returns:
*  unsigned long from the 4 chars in src
*/
unsigned long four_char_to_ul(unsigned char* src) {
 
   unsigned long ret_val = 0;
  
   ret_val = ((unsigned long)src[0] << 24) +
             ((unsigned long)src[1] << 16) +
             ((unsigned long)src[2] << 8) +
              (unsigned long)src[3];
              
   return ret_val;
}



