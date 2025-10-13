/*
 * Dual-bridge test: instantiate two tlm2axi_hw_bridge and drive them concurrently.
 *
 * Based on tests/rtl-bridges/axi/test-pcie-master-axi.cc
 */

#include <sstream>
#include <array>

#define SC_INCLUDE_DYNAMIC_PROCESSES

#include "systemc"
using namespace sc_core;
using namespace sc_dt;
using namespace std;

#include "tlm.h"
#include "tlm_utils/simple_initiator_socket.h"
#include "tlm_utils/simple_target_socket.h"

#include "tlm-modules/tlm-splitter.h"
#include "tlm-bridges/tlm2axi-bridge.h"
#include "tlm-bridges/tlm2axilite-bridge.h"
#include "tlm-bridges/axi2tlm-bridge.h"
#include "tlm-bridges/axilite2tlm-bridge.h"
#include "rtl-bridges/pcie-host/axi/tlm/tlm2axi-hw-bridge.h"
#include "traffic-generators/tg-tlm.h"
#include "traffic-generators/random-traffic.h"
#include "checkers/pc-axi.h"
#include "checkers/pc-axilite.h"

#include "test-modules/memory.h"
#include "test-modules/signals-axi.h"
#include "test-modules/signals-axilite.h"
#include "test-modules/utils.h"
#include "trace/trace.h"

#include <verilated_vcd_sc.h>
#include "Vaxi3_master.h"
#include "Vaxi4_master.h"
#include "Vaxi4_lite_master.h"

#ifdef __AXI_VERSION_AXI3__
#define DUT_SW_BRIDGE_DECL axi2tlm_bridge<64, 128, 16, 4, 2, 0, 0, 0, 0, 0 >
#define DUT_HW_BRIDGE_TYPE Vaxi3_master
#define DUT_SIGNALS_DECL   AXISignals<64, 128, 16, 4, 2, 0, 0, 0, 0, 0 >
#define DUT_CHECKER_DECL   AXIProtocolChecker<64, 128, 16, 4, 2, 0, 0, 0, 0, 0 >
#define DUT_CHECKER_CFG    AXIPCConfig::all_enabled()

#elif defined(__AXI_VERSION_AXILITE__)
#define DUT_SW_BRIDGE_DECL axilite2tlm_bridge<64, 64 >
#define DUT_HW_BRIDGE_TYPE Vaxi4_lite_master
#define DUT_SIGNALS_DECL   AXILiteSignals<64, 64 >
#define DUT_CHECKER_DECL   AXILiteProtocolChecker<64, 64 >
#define DUT_CHECKER_CFG    AXILitePCConfig::all_enabled()

#else
#define DUT_SW_BRIDGE_DECL axi2tlm_bridge<64, 128, 16, 8, 1, 32, 32, 32, 32, 32 >
#define DUT_HW_BRIDGE_TYPE Vaxi4_master
#define DUT_SIGNALS_DECL   AXISignals<64, 128, 16, 8, 1, 32, 32, 32, 32, 32 >
#define DUT_CHECKER_DECL   AXIProtocolChecker<64, 128, 16, 8, 1, 32, 32, 32, 32, 32 >
#define DUT_CHECKER_CFG    AXIPCConfig::all_enabled()
#endif

using namespace utils;

// Top module with two independent bridge stacks.
SC_MODULE(TopMulti)
{
	// Global clock/reset
	sc_clock clk;
	sc_signal<bool> rst;     // Active high.
	sc_signal<bool> rst_n;   // Active low.

	// Instance 0 signals and components
	AXILiteSignals<64, 32 > signals_host0;
	AXISignals<64, 128, 4, 8, 1, 32, 32, 32, 32, 32 > signals_host_dma0;
	DUT_SIGNALS_DECL signals_dut0;
	sc_signal<sc_bv<128> > signals_h2c_intr_out0;
	sc_vector<sc_signal<bool> > signals_h2c_intr_write0;
	sc_signal<sc_bv<256> > signals_h2c_gpio_out0;
	sc_signal<sc_bv<64> > signals_h2c_pulse_out0;
	sc_signal<sc_bv<64> > signals_c2h_intr0;
	sc_signal<sc_bv<256> > signals_c2h_gpio_in0;
	sc_signal<sc_bv<4> > signals_usr_resetn0;
	sc_signal<bool> signals_irq_out0;
	sc_signal<bool> signals_irq_ack0;

	tlm2axilite_bridge<64, 32 > bridge_tlm2axilite0;	// tlm2axilite_bridge mimics PCIe Core & AXI-MM Bridge
	DUT_SW_BRIDGE_DECL bridge_dut0;
	AXILiteProtocolChecker<64, 32 > checker_axilite0;
	DUT_CHECKER_DECL checker_dut0;

	tlm2axi_hw_bridge tlm_hw_bridge0;
	DUT_HW_BRIDGE_TYPE rtl_hw_bridge0;

	RandomTraffic rand_xfers0;
	TLMTrafficGenerator tg0;
	memory ram0;

	// Instance 1 signals and components
	AXILiteSignals<64, 32 > signals_host1;
	AXISignals<64, 128, 4, 8, 1, 32, 32, 32, 32, 32 > signals_host_dma1;
	DUT_SIGNALS_DECL signals_dut1;
	sc_signal<sc_bv<128> > signals_h2c_intr_out1;
	sc_vector<sc_signal<bool> > signals_h2c_intr_write1;
	sc_signal<sc_bv<256> > signals_h2c_gpio_out1;
	sc_signal<sc_bv<64> > signals_h2c_pulse_out1;
	sc_signal<sc_bv<64> > signals_c2h_intr1;
	sc_signal<sc_bv<256> > signals_c2h_gpio_in1;
	sc_signal<sc_bv<4> > signals_usr_resetn1;
	sc_signal<bool> signals_irq_out1;
	sc_signal<bool> signals_irq_ack1;

	tlm2axilite_bridge<64, 32 > bridge_tlm2axilite1;
	DUT_SW_BRIDGE_DECL bridge_dut1;
	AXILiteProtocolChecker<64, 32 > checker_axilite1;
	DUT_CHECKER_DECL checker_dut1;

	tlm2axi_hw_bridge tlm_hw_bridge1;
	DUT_HW_BRIDGE_TYPE rtl_hw_bridge1;

	RandomTraffic rand_xfers1;
	TLMTrafficGenerator tg1;
	memory ram1;

	SC_HAS_PROCESS(TopMulti);

	void gen_rst_n(void) { rst_n.write(!rst.read()); }

	TopMulti(sc_module_name name, unsigned int ram_size = 8 * 1024)
	: clk("clk", sc_time(1, SC_NS))
	, rst("rst")
	, rst_n("rst_n")
	// 0
	, signals_host0("signals_host0")
	, signals_host_dma0("signals_host_dma0")
	, signals_dut0("signals_dut0")
	, signals_h2c_intr_out0("h2c_intr0")
	, signals_h2c_intr_write0("h2c_intr_write0", 128)
	, signals_h2c_pulse_out0("h2c_pulse_out0")
	, signals_c2h_intr0("c2h_intr0")
	, signals_c2h_gpio_in0("c2h_gpio_in0")
	, bridge_tlm2axilite0("bridge_tlm2axilite0")
	, bridge_dut0("bridge_dut0")
	, checker_axilite0("checker_axilite0", AXILitePCConfig::all_enabled())
	, checker_dut0("checker_dut0", DUT_CHECKER_CFG)
	, tlm_hw_bridge0("tlm_hw_bridge0", /*base_addr*/0x00000, /*base_offset*/0x0)
	, rtl_hw_bridge0("rtl_hw_bridge0")
	, rand_xfers0(0, ram_size - 4, UINT64_MAX, 1, ram_size, ram_size, 10, 1)
	, tg0("tg0", 1)
	, ram0("ram0", sc_time(1, SC_NS), ram_size)
	// 1
	, signals_host1("signals_host1")
	, signals_host_dma1("signals_host_dma1")
	, signals_dut1("signals_dut1")
	, signals_h2c_intr_out1("h2c_intr1")
	, signals_h2c_intr_write1("h2c_intr_write1", 128)
	, signals_h2c_pulse_out1("h2c_pulse_out1")
	, signals_c2h_intr1("c2h_intr1")
	, signals_c2h_gpio_in1("c2h_gpio_in1")
	, bridge_tlm2axilite1("bridge_tlm2axilite1")
	, bridge_dut1("bridge_dut1")
	, checker_axilite1("checker_axilite1", AXILitePCConfig::all_enabled())
	, checker_dut1("checker_dut1", DUT_CHECKER_CFG)
	, tlm_hw_bridge1("tlm_hw_bridge1", /*base_addr*/0x10000, /*base_offset*/0x0)
	, rtl_hw_bridge1("rtl_hw_bridge1")
	, rand_xfers1(0, ram_size - 4, UINT64_MAX, 1, ram_size, ram_size, 10, 1)
	, tg1("tg1", 1)
	, ram1("ram1", sc_time(1, SC_NS), ram_size)
	{
		tlm_hw_bridge0.set_debuglevel(1);
		tlm_hw_bridge1.set_debuglevel(1);
		SC_METHOD(gen_rst_n);
		sensitive << rst;

		// Compact wiring for both instances using arrays and a loop
		auto tlm_hw_arr = std::array<tlm2axi_hw_bridge*, 2>{ &tlm_hw_bridge0, &tlm_hw_bridge1 };
		auto rtl_hw_arr = std::array<DUT_HW_BRIDGE_TYPE*, 2>{ &rtl_hw_bridge0, &rtl_hw_bridge1 };
		auto bridge_axil_arr = std::array<tlm2axilite_bridge<64,32>*, 2>{ &bridge_tlm2axilite0, &bridge_tlm2axilite1 };
		auto bridge_dut_arr = std::array<DUT_SW_BRIDGE_DECL*, 2>{ &bridge_dut0, &bridge_dut1 };
		auto checker_axil_arr = std::array<AXILiteProtocolChecker<64,32>*, 2>{ &checker_axilite0, &checker_axilite1 };
		auto checker_dut_arr = std::array<DUT_CHECKER_DECL*, 2>{ &checker_dut0, &checker_dut1 };
		auto tg_arr = std::array<TLMTrafficGenerator*, 2>{ &tg0, &tg1 };
		auto rand_arr = std::array<RandomTraffic*, 2>{ &rand_xfers0, &rand_xfers1 };
		auto ram_arr = std::array<memory*, 2>{ &ram0, &ram1 };

		for (int i = 0; i < 2; ++i) {
			// Clock and reset wiring
			bridge_axil_arr[i]->clk(clk); bridge_axil_arr[i]->resetn(rst_n);
			bridge_dut_arr[i]->clk(clk); bridge_dut_arr[i]->resetn(rst_n);
			checker_axil_arr[i]->clk(clk); checker_axil_arr[i]->resetn(rst_n);
			checker_dut_arr[i]->clk(clk); checker_dut_arr[i]->resetn(rst_n);
			rtl_hw_arr[i]->axi_aclk(clk); rtl_hw_arr[i]->axi_aresetn(rst_n);
			tlm_hw_arr[i]->rst(rst);

			// Traffic configuration, start both at the same time
			tg_arr[i]->enableDebug();
			tg_arr[i]->addTransfers(*rand_arr[i], 0);
			tg_arr[i]->setStartDelay(sc_time(15, SC_US));

			// Bind TGs directly to each HW bridge target socket
			tg_arr[i]->socket.bind(tlm_hw_arr[i]->tgt_socket);

			// Back-end: connect each HW bridge to its own AXI-Lite host bridge
			tlm_hw_arr[i]->bridge_socket(bridge_axil_arr[i]->tgt_socket);

			// DUT side: user master paths to TLM RAMs
			bridge_dut_arr[i]->socket(ram_arr[i]->socket);
		}


		// Signals arrays to finish wiring inside the loop
		auto sig_host_arr = std::array<AXILiteSignals<64,32>*, 2>{ &signals_host0, &signals_host1 };
		auto sig_host_dma_arr = std::array<AXISignals<64,128,4,8,1,32,32,32,32,32>*, 2>{ &signals_host_dma0, &signals_host_dma1 };
		auto sig_dut_arr = std::array<DUT_SIGNALS_DECL*, 2>{ &signals_dut0, &signals_dut1 };
		auto sig_h2c_intr_out_arr = std::array<sc_signal<sc_bv<128>>*, 2>{ &signals_h2c_intr_out0, &signals_h2c_intr_out1 };
		auto sig_h2c_gpio_out_arr = std::array<sc_signal<sc_bv<256>>*, 2>{ &signals_h2c_gpio_out0, &signals_h2c_gpio_out1 };
		auto sig_h2c_pulse_out_arr = std::array<sc_signal<sc_bv<64>>*, 2>{ &signals_h2c_pulse_out0, &signals_h2c_pulse_out1 };
		auto sig_c2h_intr_arr = std::array<sc_signal<sc_bv<64>>*, 2>{ &signals_c2h_intr0, &signals_c2h_intr1 };
		auto sig_c2h_gpio_in_arr = std::array<sc_signal<sc_bv<256>>*, 2>{ &signals_c2h_gpio_in0, &signals_c2h_gpio_in1 };
		auto sig_usr_resetn_arr = std::array<sc_signal<sc_bv<4>>*, 2>{ &signals_usr_resetn0, &signals_usr_resetn1 };
		auto sig_irq_out_arr = std::array<sc_signal<bool>*, 2>{ &signals_irq_out0, &signals_irq_out1 };
		auto sig_irq_ack_arr = std::array<sc_signal<bool>*, 2>{ &signals_irq_ack0, &signals_irq_ack1 };
		auto sig_h2c_intr_write_arr = std::array<sc_vector<sc_signal<bool>>*, 2>{ &signals_h2c_intr_write0, &signals_h2c_intr_write1 };

		// Bind all remaining per-instance signals in the existing loop
		for (int i = 0; i < 2; ++i) {
			// AXI-Lite host
			sig_host_arr[i]->connect(*bridge_axil_arr[i]);
			sig_host_arr[i]->connect(*checker_axil_arr[i]);
			rtl_hw_arr[i]->s_axi_awvalid(sig_host_arr[i]->awvalid);
			rtl_hw_arr[i]->s_axi_awready(sig_host_arr[i]->awready);
			rtl_hw_arr[i]->s_axi_awaddr(sig_host_arr[i]->awaddr);
			rtl_hw_arr[i]->s_axi_awprot(sig_host_arr[i]->awprot);
			rtl_hw_arr[i]->s_axi_wvalid(sig_host_arr[i]->wvalid);
			rtl_hw_arr[i]->s_axi_wready(sig_host_arr[i]->wready);
			rtl_hw_arr[i]->s_axi_wdata(sig_host_arr[i]->wdata);
			rtl_hw_arr[i]->s_axi_wstrb(sig_host_arr[i]->wstrb);
			rtl_hw_arr[i]->s_axi_bvalid(sig_host_arr[i]->bvalid);
			rtl_hw_arr[i]->s_axi_bready(sig_host_arr[i]->bready);
			rtl_hw_arr[i]->s_axi_bresp(sig_host_arr[i]->bresp);
			rtl_hw_arr[i]->s_axi_arvalid(sig_host_arr[i]->arvalid);
			rtl_hw_arr[i]->s_axi_arready(sig_host_arr[i]->arready);
			rtl_hw_arr[i]->s_axi_araddr(sig_host_arr[i]->araddr);
			rtl_hw_arr[i]->s_axi_arprot(sig_host_arr[i]->arprot);
			rtl_hw_arr[i]->s_axi_rvalid(sig_host_arr[i]->rvalid);
			rtl_hw_arr[i]->s_axi_rready(sig_host_arr[i]->rready);
			rtl_hw_arr[i]->s_axi_rdata(sig_host_arr[i]->rdata);
			rtl_hw_arr[i]->s_axi_rresp(sig_host_arr[i]->rresp);

			// AXI host DMA (m_axi_*)
			rtl_hw_arr[i]->m_axi_awvalid(sig_host_dma_arr[i]->awvalid);
			rtl_hw_arr[i]->m_axi_awready(sig_host_dma_arr[i]->awready);
			rtl_hw_arr[i]->m_axi_awaddr(sig_host_dma_arr[i]->awaddr);
			rtl_hw_arr[i]->m_axi_awprot(sig_host_dma_arr[i]->awprot);
			rtl_hw_arr[i]->m_axi_awuser(sig_host_dma_arr[i]->awuser);
			rtl_hw_arr[i]->m_axi_awregion(sig_host_dma_arr[i]->awregion);
			rtl_hw_arr[i]->m_axi_awqos(sig_host_dma_arr[i]->awqos);
			rtl_hw_arr[i]->m_axi_awcache(sig_host_dma_arr[i]->awcache);
			rtl_hw_arr[i]->m_axi_awburst(sig_host_dma_arr[i]->awburst);
			rtl_hw_arr[i]->m_axi_awsize(sig_host_dma_arr[i]->awsize);
			rtl_hw_arr[i]->m_axi_awlen(sig_host_dma_arr[i]->awlen);
			rtl_hw_arr[i]->m_axi_awid(sig_host_dma_arr[i]->awid);
			rtl_hw_arr[i]->m_axi_awlock(sig_host_dma_arr[i]->awlock);
			rtl_hw_arr[i]->m_axi_wvalid(sig_host_dma_arr[i]->wvalid);
			rtl_hw_arr[i]->m_axi_wready(sig_host_dma_arr[i]->wready);
			rtl_hw_arr[i]->m_axi_wdata(sig_host_dma_arr[i]->wdata);
			rtl_hw_arr[i]->m_axi_wstrb(sig_host_dma_arr[i]->wstrb);
			rtl_hw_arr[i]->m_axi_wuser(sig_host_dma_arr[i]->wuser);
			rtl_hw_arr[i]->m_axi_wlast(sig_host_dma_arr[i]->wlast);
			rtl_hw_arr[i]->m_axi_bvalid(sig_host_dma_arr[i]->bvalid);
			rtl_hw_arr[i]->m_axi_bready(sig_host_dma_arr[i]->bready);
			rtl_hw_arr[i]->m_axi_bresp(sig_host_dma_arr[i]->bresp);
			rtl_hw_arr[i]->m_axi_buser(sig_host_dma_arr[i]->buser);
			rtl_hw_arr[i]->m_axi_bid(sig_host_dma_arr[i]->bid);
			rtl_hw_arr[i]->m_axi_arvalid(sig_host_dma_arr[i]->arvalid);
			rtl_hw_arr[i]->m_axi_arready(sig_host_dma_arr[i]->arready);
			rtl_hw_arr[i]->m_axi_araddr(sig_host_dma_arr[i]->araddr);
			rtl_hw_arr[i]->m_axi_arprot(sig_host_dma_arr[i]->arprot);
			rtl_hw_arr[i]->m_axi_aruser(sig_host_dma_arr[i]->aruser);
			rtl_hw_arr[i]->m_axi_arregion(sig_host_dma_arr[i]->arregion);
			rtl_hw_arr[i]->m_axi_arqos(sig_host_dma_arr[i]->arqos);
			rtl_hw_arr[i]->m_axi_arcache(sig_host_dma_arr[i]->arcache);
			rtl_hw_arr[i]->m_axi_arburst(sig_host_dma_arr[i]->arburst);
			rtl_hw_arr[i]->m_axi_arsize(sig_host_dma_arr[i]->arsize);
			rtl_hw_arr[i]->m_axi_arlen(sig_host_dma_arr[i]->arlen);
			rtl_hw_arr[i]->m_axi_arid(sig_host_dma_arr[i]->arid);
			rtl_hw_arr[i]->m_axi_arlock(sig_host_dma_arr[i]->arlock);
			rtl_hw_arr[i]->m_axi_rvalid(sig_host_dma_arr[i]->rvalid);
			rtl_hw_arr[i]->m_axi_rready(sig_host_dma_arr[i]->rready);
			rtl_hw_arr[i]->m_axi_rdata(sig_host_dma_arr[i]->rdata);
			rtl_hw_arr[i]->m_axi_rresp(sig_host_dma_arr[i]->rresp);
			rtl_hw_arr[i]->m_axi_ruser(sig_host_dma_arr[i]->ruser);
			rtl_hw_arr[i]->m_axi_rid(sig_host_dma_arr[i]->rid);
			rtl_hw_arr[i]->m_axi_rlast(sig_host_dma_arr[i]->rlast);

			// User master AXI (m_axi_usr_*)
			rtl_hw_arr[i]->m_axi_usr_awvalid(sig_dut_arr[i]->awvalid);
			rtl_hw_arr[i]->m_axi_usr_awready(sig_dut_arr[i]->awready);
			rtl_hw_arr[i]->m_axi_usr_awaddr(sig_dut_arr[i]->awaddr);
			rtl_hw_arr[i]->m_axi_usr_awprot(sig_dut_arr[i]->awprot);
			rtl_hw_arr[i]->m_axi_usr_awuser(sig_dut_arr[i]->awuser);
			rtl_hw_arr[i]->m_axi_usr_awregion(sig_dut_arr[i]->awregion);
			rtl_hw_arr[i]->m_axi_usr_awqos(sig_dut_arr[i]->awqos);
			rtl_hw_arr[i]->m_axi_usr_awcache(sig_dut_arr[i]->awcache);
			rtl_hw_arr[i]->m_axi_usr_awburst(sig_dut_arr[i]->awburst);
			rtl_hw_arr[i]->m_axi_usr_awsize(sig_dut_arr[i]->awsize);
			rtl_hw_arr[i]->m_axi_usr_awlen(sig_dut_arr[i]->awlen);
			rtl_hw_arr[i]->m_axi_usr_awid(sig_dut_arr[i]->awid);
			rtl_hw_arr[i]->m_axi_usr_awlock(sig_dut_arr[i]->awlock);
			rtl_hw_arr[i]->m_axi_usr_wvalid(sig_dut_arr[i]->wvalid);
			rtl_hw_arr[i]->m_axi_usr_wready(sig_dut_arr[i]->wready);
			rtl_hw_arr[i]->m_axi_usr_wdata(sig_dut_arr[i]->wdata);
			rtl_hw_arr[i]->m_axi_usr_wstrb(sig_dut_arr[i]->wstrb);
			rtl_hw_arr[i]->m_axi_usr_wuser(sig_dut_arr[i]->wuser);
			rtl_hw_arr[i]->m_axi_usr_wlast(sig_dut_arr[i]->wlast);
			rtl_hw_arr[i]->m_axi_usr_bvalid(sig_dut_arr[i]->bvalid);
			rtl_hw_arr[i]->m_axi_usr_bready(sig_dut_arr[i]->bready);
			rtl_hw_arr[i]->m_axi_usr_bresp(sig_dut_arr[i]->bresp);
			rtl_hw_arr[i]->m_axi_usr_buser(sig_dut_arr[i]->buser);
			rtl_hw_arr[i]->m_axi_usr_bid(sig_dut_arr[i]->bid);
			rtl_hw_arr[i]->m_axi_usr_arvalid(sig_dut_arr[i]->arvalid);
			rtl_hw_arr[i]->m_axi_usr_arready(sig_dut_arr[i]->arready);
			rtl_hw_arr[i]->m_axi_usr_araddr(sig_dut_arr[i]->araddr);
			rtl_hw_arr[i]->m_axi_usr_arprot(sig_dut_arr[i]->arprot);
			rtl_hw_arr[i]->m_axi_usr_aruser(sig_dut_arr[i]->aruser);
			rtl_hw_arr[i]->m_axi_usr_arregion(sig_dut_arr[i]->arregion);
			rtl_hw_arr[i]->m_axi_usr_arqos(sig_dut_arr[i]->arqos);
			rtl_hw_arr[i]->m_axi_usr_arcache(sig_dut_arr[i]->arcache);
			rtl_hw_arr[i]->m_axi_usr_arburst(sig_dut_arr[i]->arburst);
			rtl_hw_arr[i]->m_axi_usr_arsize(sig_dut_arr[i]->arsize);
			rtl_hw_arr[i]->m_axi_usr_arlen(sig_dut_arr[i]->arlen);
			rtl_hw_arr[i]->m_axi_usr_arid(sig_dut_arr[i]->arid);
			rtl_hw_arr[i]->m_axi_usr_arlock(sig_dut_arr[i]->arlock);
			rtl_hw_arr[i]->m_axi_usr_rvalid(sig_dut_arr[i]->rvalid);
			rtl_hw_arr[i]->m_axi_usr_rready(sig_dut_arr[i]->rready);
			rtl_hw_arr[i]->m_axi_usr_rdata(sig_dut_arr[i]->rdata);
			rtl_hw_arr[i]->m_axi_usr_rresp(sig_dut_arr[i]->rresp);
			rtl_hw_arr[i]->m_axi_usr_ruser(sig_dut_arr[i]->ruser);
			rtl_hw_arr[i]->m_axi_usr_rid(sig_dut_arr[i]->rid);
			rtl_hw_arr[i]->m_axi_usr_rlast(sig_dut_arr[i]->rlast);

			// Misc wires
			rtl_hw_arr[i]->h2c_intr_out(*sig_h2c_intr_out_arr[i]);
			rtl_hw_arr[i]->h2c_gpio_out(*sig_h2c_gpio_out_arr[i]);
			rtl_hw_arr[i]->c2h_intr_in(*sig_c2h_intr_arr[i]);
			// Preserve original loopback behavior for c2h_gpio_in
			rtl_hw_arr[i]->c2h_gpio_in(*sig_h2c_gpio_out_arr[i]);
			rtl_hw_arr[i]->h2c_pulse_out(*sig_h2c_pulse_out_arr[i]);
			rtl_hw_arr[i]->usr_resetn(*sig_usr_resetn_arr[i]);

			// DUT user masters to checkers and SW bridges
			sig_dut_arr[i]->connect(*bridge_dut_arr[i]);
			sig_dut_arr[i]->connect(*checker_dut_arr[i]);

			// IRQ wiring per instance
			rtl_hw_arr[i]->irq_out(*sig_irq_out_arr[i]);
			rtl_hw_arr[i]->irq_ack(*sig_irq_ack_arr[i]);
			tlm_hw_arr[i]->irq(*sig_irq_out_arr[i]);
			sig_irq_ack_arr[i]->write(1);
			tlm_hw_arr[i]->h2c_irq(*sig_h2c_intr_write_arr[i]);
		}
	}
};

int sc_main(int argc, char *argv[])
{
	Verilated::commandArgs(argc, argv);
	TopMulti top("TopMulti");

	sc_trace_file *trace_fp = sc_create_vcd_trace_file(argv[0]);
	trace(trace_fp, top, "top_dual");

	// Emit reset
	top.rst.write(true);
	sc_start(4, SC_US);
	top.rst.write(false);

	// Run long enough for both TGs to overlap and complete a large number of transfers
	// sc_start(2, SC_MS);
	sc_start(20, SC_US);

	if (trace_fp) {
		sc_close_vcd_trace_file(trace_fp);
	}
	return 0;
}
