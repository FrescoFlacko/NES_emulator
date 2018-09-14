#!usr/bin/python

address_modes = {0 : "ZERO_PAGE", 1 : "IND_ZERO_PAGE_X", 2: "IND_ZERO_PAGE_Y", 3 : "ABSOLUTE", 4 : "IND_ABSOLUTE_X", 5 : "IND_ABSOLUTE_Y", 6 : "INDIRECT", 7 : "RELATIVE", 8 : "INDEXED_INDIRECT_X", 9 : "INDEXED_INDIRECT_Y", 10 : "IMMEDIATE" }

f = open("opcode", "r")
str = "void perform_instruction(uint8_t opcode, uint16_t address)\n{\n\tswitch (opcode) {\n";
opcode = 0
for line in f:
    if (line[0] == '#'):
        continue;
    tokens = line.split(',')
    str += "\t\tcase " + format(opcode, '#04X') + ":\n"
    str += "\t\t\t" + tokens[0] + "("
    if (tokens[1] == '0'):
        str += address_modes[int(tokens[2])] + "(address)"
    elif (tokens[1] == '1'):
        str += "address, " + address_modes[int(tokens[2])]
    elif (tokens[1] == '2'):
        str += "address"
    elif (tokens[1] == '3'):
        str += address_modes[int(tokens[2])] + "(address), address, 1"

    str += ");\n"
    str += "\t\t\t/* size = " + tokens[3] + "; */\n"
    str += "\t\t\tcycles = " + tokens[4][:1] + ";\n"
    str += "\t\t\tbreak;\n"
    opcode += 1

str += "\t\tdefault:\n\t\t\tbreak;\n"
str += "\t}\n}"

print(str)
