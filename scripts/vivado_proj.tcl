
create_project anyseq_128_sysarr_hw /home/fpga/anydsl/anyseq/build/vivado/anyseq_128_sysarr_hw -part xczu7ev-ffvc1156-2-e
set_property board_part xilinx.com:zcu104:part0:1.1 [current_project]
set_property  ip_repo_paths  /home/fpga/anydsl/anyseq/build/anyseq_align_local_128/align_128/impl/ip [current_project]
update_ip_catalog
create_bd_design "sysarr_design"
update_compile_order -fileset sources_1
startgroup
create_bd_cell -type ip -vlnv xilinx.com:ip:zynq_ultra_ps_e:3.2 zynq_ultra_ps_e_0
endgroup
apply_bd_automation -rule xilinx.com:bd_rule:zynq_ultra_ps_e -config {apply_board_preset "1" }  [get_bd_cells zynq_ultra_ps_e_0]
set_property -dict [list CONFIG.PSU__USE__M_AXI_GP1 {0} CONFIG.PSU__USE__S_AXI_GP0 {1}] [get_bd_cells zynq_ultra_ps_e_0]
startgroup
create_bd_cell -type ip -vlnv xilinx.com:hls:align:1.0 align_0
endgroup
startgroup
create_bd_cell -type ip -vlnv xilinx.com:hls:align:1.0 align_0
endgroup
startgroup
create_bd_cell -type ip -vlnv xilinx.com:ip:xlconcat:2.1 xlconcat_0
endgroup
startgroup
create_bd_cell -type ip -vlnv xilinx.com:ip:axi_dma:7.1 axi_dma_0
endgroup
set_property -dict [list CONFIG.NUM_PORTS {3}] [get_bd_cells xlconcat_0]
connect_bd_net [get_bd_pins align_0/interrupt] [get_bd_pins xlconcat_0/In0]
connect_bd_net [get_bd_pins axi_dma_0/mm2s_introut] [get_bd_pins xlconcat_0/In1]
connect_bd_net [get_bd_pins axi_dma_0/s2mm_introut] [get_bd_pins xlconcat_0/In2]
connect_bd_net [get_bd_pins xlconcat_0/dout] [get_bd_pins zynq_ultra_ps_e_0/pl_ps_irq0]
startgroup
set_property -dict [list CONFIG.c_include_sg {0} CONFIG.c_sg_length_width {16} CONFIG.c_sg_include_stscntrl_strm {0} CONFIG.c_include_mm2s_dre {1} CONFIG.c_include_s2mm_dre {1}] [get_bd_cells axi_dma_0]
endgroup
connect_bd_intf_net [get_bd_intf_pins align_0/wbuff] [get_bd_intf_pins axi_dma_0/S_AXIS_S2MM]
connect_bd_intf_net [get_bd_intf_pins align_0/sub] [get_bd_intf_pins axi_dma_0/M_AXIS_MM2S]
startgroup
apply_bd_automation -rule xilinx.com:bd_rule:axi4 -config { Clk_master {Auto} Clk_slave {Auto} Clk_xbar {Auto} Master {/axi_dma_0/M_AXI_MM2S} Slave {/zynq_ultra_ps_e_0/S_AXI_HPC0_FPD} intc_ip {Auto} master_apm {0}}  [get_bd_intf_pins zynq_ultra_ps_e_0/S_AXI_HPC0_FPD]
apply_bd_automation -rule xilinx.com:bd_rule:axi4 -config { Clk_master {Auto} Clk_slave {Auto} Clk_xbar {Auto} Master {/zynq_ultra_ps_e_0/M_AXI_HPM0_FPD} Slave {/align_0/s_axi_CTRL} intc_ip {New AXI Interconnect} master_apm {0}}  [get_bd_intf_pins align_0/s_axi_CTRL]
apply_bd_automation -rule xilinx.com:bd_rule:axi4 -config { Clk_master {Auto} Clk_slave {Auto} Clk_xbar {Auto} Master {/zynq_ultra_ps_e_0/M_AXI_HPM0_FPD} Slave {/axi_dma_0/S_AXI_LITE} intc_ip {New AXI Interconnect} master_apm {0}}  [get_bd_intf_pins axi_dma_0/S_AXI_LITE]
endgroup
apply_bd_automation -rule xilinx.com:bd_rule:axi4 -config { Clk_master {Auto} Clk_slave {/zynq_ultra_ps_e_0/pl_clk0 (100 MHz)} Clk_xbar {/zynq_ultra_ps_e_0/pl_clk0 (100 MHz)} Master {/axi_dma_0/M_AXI_S2MM} Slave {/zynq_ultra_ps_e_0/S_AXI_HPC0_FPD} intc_ip {/axi_smc} master_apm {0}}  [get_bd_intf_pins axi_dma_0/M_AXI_S2MM]
set_property location {3 1014 25} [get_bd_cells xlconcat_0]
set_property location {4 1556 240} [get_bd_cells align_0]
set_property location {5 2079 238} [get_bd_cells axi_smc]
set_property location {5 2020 46} [get_bd_cells axi_dma_0]
set_property location {2 918 610} [get_bd_cells rst_ps8_0_100M]
set_property location {1 984 302} [get_bd_cells rst_ps8_0_100M]
set_property location {2 1511 619} [get_bd_cells rst_ps8_0_100M]
set_property location {2 1404 32} [get_bd_cells xlconcat_0]
set_property location {0.5 -264 443} [get_bd_cells rst_ps8_0_100M]
set_property location {2 790 453} [get_bd_cells align_0]
regenerate_bd_layout -routing
include_bd_addr_seg [get_bd_addr_segs -excluded axi_dma_0/Data_MM2S/SEG_zynq_ultra_ps_e_0_HPC0_LPS_OCM]
include_bd_addr_seg [get_bd_addr_segs -excluded axi_dma_0/Data_S2MM/SEG_zynq_ultra_ps_e_0_HPC0_LPS_OCM]
make_wrapper -files [get_files /home/fpga/anydsl/anyseq/build/vivado/anyseq_128_sysarr_hw/anyseq_128_sysarr_hw.srcs/sources_1/bd/sysarr_design/sysarr_design.bd] -top
add_files -norecurse /home/fpga/anydsl/anyseq/build/vivado/anyseq_128_sysarr_hw/anyseq_128_sysarr_hw.srcs/sources_1/bd/sysarr_design/hdl/sysarr_design_wrapper.v
launch_runs impl_1 -to_step write_bitstream -jobs 4
file mkdir /home/fpga/anydsl/anyseq/build/vivado/anyseq_128_sysarr_hw/anyseq_128_sysarr_hw.sdk
file copy -force /home/fpga/anydsl/anyseq/build/vivado/anyseq_128_sysarr_hw/anyseq_128_sysarr_hw.runs/impl_1/sysarr_design_wrapper.sysdef /home/fpga/anydsl/anyseq/build/vivado/anyseq_128_sysarr_hw/anyseq_128_sysarr_hw.sdk/sysarr_design_wrapper.hdf
launch_sdk -workspace /home/fpga/anydsl/anyseq/build/vivado/anyseq_128_sysarr_hw/anyseq_128_sysarr_hw.sdk -hwspec /home/fpga/anydsl/anyseq/build/vivado/anyseq_128_sysarr_hw/anyseq_128_sysarr_hw.sdk/sysarr_design_wrapper.hdf
