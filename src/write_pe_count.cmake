
file(REMOVE ../src/mapping_fpga_pe_count.impala)
file(WRITE ../src/mapping_fpga_pe_count.impala "static PE_COUNT = ${PE_COUNT};\n")

