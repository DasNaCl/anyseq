# We assume align_postproc is already built for simplicity

string(CONCAT custom_tcl_script "open_project anyseq_align_${ALIGN_SCHEME}_${PE_COUNT}\n"
                                "set_top align\n"
                                "add_files utils_hls.cpp\n"
                                "add_files align.h\n"
                                "add_files -tb align_tb.cpp\n"
                                "open_solution align_${PE_COUNT}\n"
                                "set lower ${HLS_PART}\n"
                                "set_part ${HLS_PART}\n"
                                "create_clock -period 6 -name default\n"
                                "exit")
execute_process(COMMAND ${SOURCE_DIR}/scripts/hls_postproc/anyseq_hls_postproc ${BINARY_DIR}/utils_hls.cpp)
execute_process(COMMAND echo "${custom_tcl_script}" OUTPUT_FILE ${BINARY_DIR}/anyseq_align_${ALIGN_SCHEME}_${PE_COUNT}.tcl)
execute_process(COMMAND cp ${SOURCE_DIR}/src/hls/align_tb.cpp ${BINARY_DIR}/align_tb.cpp)
configure_file(${SOURCE_DIR}/src/hls/align.h.in ${BINARY_DIR}/align.h)

execute_process(COMMAND vivado_hls -f anyseq_align_${ALIGN_SCHEME}_${PE_COUNT}.tcl
        WORKING_DIRECTORY ${BINARY_DIR})
