# -*- coding: utf-8 -*-
"""
Created on Mon Sep 26 19:53:56 2016

Reading hex file and sending to Arduino for PIC
programming

@author: brian
"""
from __future__ import print_function

import struct
import serial
import sys
import datetime
import time

class DataBlock:
    def __init__(self, block_first_addr = 0):
        self.block_first_addr = block_first_addr
        self.block_last_addr = block_first_addr + (64-1) * 2
        self.size = 0
        self.instructions = [0xFFFFFFFF]*64
        self.crc = 0
        
"""
******************************************************************************
Function definitions
******************************************************************************
"""
def send_data(data, pack_format, ser):
    ser.write(struct.pack('>' + pack_format, data)) #pack it as big endian
        
def rcv_ack(place, ser):
    ack = ''       
    ack = ser.read()
    if len(ack) < 1:
        ser.close()
        sys.exit("No ack: " + place)
        #print("bad ack")
    else:
        print('{} : {}'.format(place, ack))

def send_instructions(db, ser):
    ser.write('i')
    rcv_ack('Block {} Instr Control'.format(db.block_first_addr), ser)
    send_data(db.block_first_addr, 'I', ser)
    rcv_ack('Block {} Instr Addr'.format(db.block_first_addr), ser)
    #64 instructions sent as 4-64 byte burts w/ acks in between
    for i in range(len(db.instructions)):
        send_data(db.instructions[i], 'I', ser)
    rcv_ack('Block {} Instr Block'.format(db.block_first_addr), ser)
    
def send_config_regs(cw1, cw1_addr, cw2, cw2_addr, ser):
    ser.write('c')
    rcv_ack('Config Write Control', ser)
    
    send_data(cw2_addr, 'I', ser)
    rcv_ack('Config Write Addr', ser)
    
    send_data(cw1, 'H', ser)
    send_data(cw2, 'H', ser)
    rcv_ack('Config Write Data', ser)
    
def print_block_to_file(db, out_file):
    out_file.write(': {}\n'.format(db.block_first_addr))
    for i in range(64):
        out_file.write('{0:#x}\n'.format(db.instructions[i]))
    out_file.write('\n')
    
def print_config_to_file(cw1, cw2, out_file):
    out_file.write('CW1: {0:#x}\n'.format(cw1))
    out_file.write('CW2: {0:#x}\n'.format(cw2))
    out_file.write('\n')
    
def getSerial(name):
    try:
        return serial.Serial(port = name, 
                            baudrate = 9600,
                            bytesize = 8,
                            parity = serial.PARITY_NONE,
                            stopbits = 2,
                            timeout = 40)
    except:
        return None
        
"""
******************************************************************************
Variable definitions
******************************************************************************
"""

filename = 'dist/default/production/pic24f_programming.X.production.hex'
f = open(filename, 'r')

today = datetime.datetime.now()
out_filename = 'python_debug/data_chunks_' + today.strftime("%H:%M:%S")
f_out = open(out_filename, 'w')

data_offset = 9 #const used for convenience. Data will always be the 9th char
line_num = 0 #used for debugging bad syntax only (line missing ':')
base_addr = 0 #used to calculated physical addr. most sig 16 bits of 32 bit addr
cw1_addr = 0xABFE
cw2_addr = 0xABFC
send_config = 0

prev_addr = -2 #used with prev_count to see if a new DataBlock should be made
                #initialized to -2 for the first instruction to pass the if-statement
running_addr = 0 #used to keep track of actual addresses within a single line

curr_block = DataBlock()
block_list = []

instr_start = 0 #helper index to loop through instructions (4 byte chunks)
instr_end = 0

#the following have hard coded locations in each line of the HEX file.
byte_count = 0 #actual number in HEX file, indicates # of bytes in data field
offset_addr = 0 #addr in the HEX file. This is least sig 16 bits of 32 bit addr
data_type = 0 #three types allowed: 1 = EOF, 4 = update base_addr, 0 = data
crc = 0 #crc in HEX file, used to compare on Arduino

first_line = True
eof_reached = False

cw1_value = 0x7FFF;
cw2_value = 0xFFFF;
        
"""
******************************************************************************
Script start
******************************************************************************
"""
for line in f:
    line = line[:len(line)-1]
    if (line[0] != ':'):
        print('File syntax error: {}'.format(line_num))
        break
    else:
        #Line information
        byte_count = int(line[1:3], 16) 
        offset_addr = int(line[3:7], 16)
        data_type = int(line[7:9], 16)
        crc = int(line[len(line)-2:len(line)], 16)
        
        #EOF has been reached
        if (data_type == 1):
            block_list.append(curr_block)
            eof_reached = True
#            if(arduino_online):
#                ser.write('e')
#                rcv_ack("EOF Control", ser)
            
        #Updating the base address
        elif (data_type == 4):
            #The base address is the MSB 2 bytes of the 4 byte address
            temp = int(line[data_offset:data_offset+4], 16) 
            base_addr = (temp << 16)
            
        #Data to be written through ICSP
        elif (data_type == 0):
            line_actual_addr = (base_addr + offset_addr) / 2
            
            #Looping through total number of instructions in the data block
            #instructions are four bytes each
            for i in range(byte_count / 4):
                
                instr_actual_addr = line_actual_addr + (i * 2) #Word address. i is instruction number. Each instruction is 2 words                
                
                #multiples of 8 because line[] is indexed by char count! 8 chars per word
                instr_start = data_offset + i * 8
                instr_end = instr_start + 8                
                
                if first_line:
                    #Create a new DataBlock for the first instruction
                    curr_block = DataBlock(instr_actual_addr)
                    curr_block.instructions[0] = int(line[instr_start:instr_end], 16)
                    first_line = False
                    curr_block.size += 1
                
                elif (instr_actual_addr == cw1_addr):
                    cw1_value = int(line[instr_start:instr_start + 4], 16) #only 16 bits
#                    send_config = send_config + 1
#                    if(send_config == 2 and arduino_online):                       
#                        send_instructions(curr_block, ser)
#                        send_config_regs(cw1_value, cw1_addr, cw2_value, cw2_addr, ser)
                elif (instr_actual_addr == cw2_addr):
                    cw2_value = int(line[instr_start:instr_start + 4], 16) #only 16 bits
#                    send_config = send_config + 1
#                    if(send_config == 2 and arduino_online):
#                        #Assumption being that configuration words come after all instructions                        
#                        send_instructions(curr_block, ser)
#                        send_config_regs(cw1_value, cw1_addr, cw2_value, cw2_addr, ser)
                # next instruction is still part of the sequence
                elif ((instr_actual_addr <= curr_block.block_last_addr) and (curr_block.size < 64)):
                    #use the line_actual_addr as the 0 element. This way if there's a jump in memeory
                    #but still within the 64 instruction range it will get added to the right block
                    block_index = (instr_actual_addr - curr_block.block_first_addr) / 2
                    curr_block.instructions[block_index] = int(line[instr_start:instr_end], 16)
                    curr_block.size += 1
                    
                #either filled up 64 instructions or skipped addr locations. either way send to Arduino
                #clear the DataBlock and its index
                else:
#                    if(arduino_online):
#                        send_instructions(curr_block, ser)
                    block_list.append(curr_block)    
#                    print_block_to_file(curr_block, f_out)
                    print('Making new DB at {}'.format(curr_block.block_last_addr + 2))
                    #Create a new DataBlock
                    curr_block = DataBlock(curr_block.block_last_addr + 2)
                    block_index = (instr_actual_addr - curr_block.block_first_addr) / 2
                    curr_block.instructions[block_index] = int(line[instr_start:instr_end], 16)
                    curr_block.size += 1                  
        
        #unknown type of bad formatting
        else:
            print('Bad type')
            break


for i in range(len(block_list)):
    print_block_to_file(block_list[i], f_out)           
print_config_to_file(cw1_value, cw2_value, f_out)

ser = getSerial('/dev/ttyUSB0')
time.sleep(2)
if (ser == None):
    print('arduino offline')
    arduino_online = False
else:
    arduino_online = True

#wake up the arduino
if(arduino_online):
    ser.write('A')
    rcv_ack("Initial wakeup", ser)
    
    for i in range(len(block_list)):
        print('Sending block {}'.format(i))
        send_instructions(block_list[i], ser)

    print('Sending config words')
    send_config_regs(cw1_value, cw1_addr, cw2_value, cw2_addr, ser)
    
    ser.close()
    
f.close()
f_out.close()


