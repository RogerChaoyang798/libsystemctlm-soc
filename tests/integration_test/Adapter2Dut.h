#ifndef GRACE_NTTP_TEST_ADAPTER2DUT_H_
#define GRACE_NTTP_TEST_ADAPTER2DUT_H_

#include <array>
#include <sstream>

#include "systemc"

#include "tlm-bridges/tlm2axilite-bridge.h"
#include "tlm-bridges/axilite2tlm-bridge.h"
// AXI-Lite signal bundle used to connect the two bridges
#include "test-modules/signals-axilite.h"
// HW bridge type used in bind helpers
#include "rtl-bridges/pcie-host/axi/tlm/tlm2axi-hw-bridge.h"

// A small helper that owns per-initiator AXI-Lite host bridges (t2axilite)
// and per-initiator DUT SW bridges (axilite2tlm), and exposes them as arrays
// so external code can bind sockets succinctly.
//
// Types/parameters are chosen to match existing tests using AXI-Lite host
// (tlm2axilite_bridge<64,32>) and DUT side axilite2tlm_bridge<64,64>.
// If you need different widths, consider templating those as well.

template<int NR_OF_INITIATORS>
class Adapter2Dut {
public:
  std::array<tlm2axilite_bridge<64,32>*, NR_OF_INITIATORS> bridge_axil_arr;
  std::array<axilite2tlm_bridge<64,32>*, NR_OF_INITIATORS> bridge_dut_arr;
  std::array<AXILiteSignals<64,32>*, NR_OF_INITIATORS> axil_signals_arr;

  Adapter2Dut() {
    for (int i = 0; i < NR_OF_INITIATORS; ++i) {
      std::ostringstream os1, os2;
      os1 << "adapter_bridge_tlm2axilite_" << i;
      os2 << "adapter_bridge_dut_axilite2tlm_" << i;
      bridge_axil_arr[i] = new tlm2axilite_bridge<64,32>(os1.str().c_str());
      bridge_dut_arr[i] = new axilite2tlm_bridge<64,32>(os2.str().c_str());

      // Create and connect AXI-Lite signal bundle between the two bridges
      std::ostringstream oss;
      oss << "adapter_axil_signals_" << i;
      axil_signals_arr[i] = new AXILiteSignals<64,32>(oss.str().c_str());
      axil_signals_arr[i]->connect(*bridge_axil_arr[i]);
      axil_signals_arr[i]->connect(*bridge_dut_arr[i]);
    }
  }

  ~Adapter2Dut() {
    for (int i = 0; i < NR_OF_INITIATORS; ++i) {
      delete axil_signals_arr[i];
      delete bridge_axil_arr[i];
      delete bridge_dut_arr[i];
    }
  }

  // Connect tlm2axi_hw_bridge -> tlm2axilite_bridge target socket
  inline void bind_hw_host(int idx, tlm2axi_hw_bridge& tlm_hw) {
    tlm_hw.bridge_socket(bridge_axil_arr[idx]->tgt_socket);
  }

  // Wire RTL AXI-Lite slave s_axi_* ports to our AXI-Lite signals and optionally a checker
  template<typename RtlHW, typename AXILiteChecker>
  inline void bind_axil_host(int idx, RtlHW& rtl_hw, AXILiteChecker* checker = nullptr) {
    auto& sig = *axil_signals_arr[idx];
    sig.connect(*bridge_axil_arr[idx]);
    if (checker) { sig.connect(*checker); }

    rtl_hw.s_axi_awvalid(sig.awvalid);
    rtl_hw.s_axi_awready(sig.awready);
    rtl_hw.s_axi_awaddr(sig.awaddr);
    rtl_hw.s_axi_awprot(sig.awprot);
    rtl_hw.s_axi_wvalid(sig.wvalid);
    rtl_hw.s_axi_wready(sig.wready);
    rtl_hw.s_axi_wdata(sig.wdata);
    rtl_hw.s_axi_wstrb(sig.wstrb);
    rtl_hw.s_axi_bvalid(sig.bvalid);
    rtl_hw.s_axi_bready(sig.bready);
    rtl_hw.s_axi_bresp(sig.bresp);
    rtl_hw.s_axi_arvalid(sig.arvalid);
    rtl_hw.s_axi_arready(sig.arready);
    rtl_hw.s_axi_araddr(sig.araddr);
    rtl_hw.s_axi_arprot(sig.arprot);
    rtl_hw.s_axi_rvalid(sig.rvalid);
    rtl_hw.s_axi_rready(sig.rready);
    rtl_hw.s_axi_rdata(sig.rdata);
    rtl_hw.s_axi_rresp(sig.rresp);
  }

  // Wire RTL DMA master m_axi_* ports to provided AXI full signals
  template<typename RtlHW, typename AXISignalsT>
  inline void bind_dma(int /*idx*/, RtlHW& rtl_hw, AXISignalsT& dma) {
    rtl_hw.m_axi_awvalid(dma.awvalid);
    rtl_hw.m_axi_awready(dma.awready);
    rtl_hw.m_axi_awaddr(dma.awaddr);
    rtl_hw.m_axi_awprot(dma.awprot);
    rtl_hw.m_axi_awuser(dma.awuser);
    rtl_hw.m_axi_awregion(dma.awregion);
    rtl_hw.m_axi_awqos(dma.awqos);
    rtl_hw.m_axi_awcache(dma.awcache);
    rtl_hw.m_axi_awburst(dma.awburst);
    rtl_hw.m_axi_awsize(dma.awsize);
    rtl_hw.m_axi_awlen(dma.awlen);
    rtl_hw.m_axi_awid(dma.awid);
    rtl_hw.m_axi_awlock(dma.awlock);
    rtl_hw.m_axi_wvalid(dma.wvalid);
    rtl_hw.m_axi_wready(dma.wready);
    rtl_hw.m_axi_wdata(dma.wdata);
    rtl_hw.m_axi_wstrb(dma.wstrb);
    rtl_hw.m_axi_wuser(dma.wuser);
    rtl_hw.m_axi_wlast(dma.wlast);
    rtl_hw.m_axi_bvalid(dma.bvalid);
    rtl_hw.m_axi_bready(dma.bready);
    rtl_hw.m_axi_bresp(dma.bresp);
    rtl_hw.m_axi_buser(dma.buser);
    rtl_hw.m_axi_bid(dma.bid);
    rtl_hw.m_axi_arvalid(dma.arvalid);
    rtl_hw.m_axi_arready(dma.arready);
    rtl_hw.m_axi_araddr(dma.araddr);
    rtl_hw.m_axi_arprot(dma.arprot);
    rtl_hw.m_axi_aruser(dma.aruser);
    rtl_hw.m_axi_arregion(dma.arregion);
    rtl_hw.m_axi_arqos(dma.arqos);
    rtl_hw.m_axi_arcache(dma.arcache);
    rtl_hw.m_axi_arburst(dma.arburst);
    rtl_hw.m_axi_arsize(dma.arsize);
    rtl_hw.m_axi_arlen(dma.arlen);
    rtl_hw.m_axi_arid(dma.arid);
    rtl_hw.m_axi_arlock(dma.arlock);
    rtl_hw.m_axi_rvalid(dma.rvalid);
    rtl_hw.m_axi_rready(dma.rready);
    rtl_hw.m_axi_rdata(dma.rdata);
    rtl_hw.m_axi_rresp(dma.rresp);
    rtl_hw.m_axi_ruser(dma.ruser);
    rtl_hw.m_axi_rid(dma.rid);
    rtl_hw.m_axi_rlast(dma.rlast);
  }

  // Wire RTL user master m_axi_usr_* to provided DUT signals; also connect DUT signals to SW bridge and optional checker
  template<typename RtlHW, typename DutSignalsT, typename DutCheckerT = void>
  inline void bind_usr(int idx, RtlHW& rtl_hw, DutSignalsT& dut, DutCheckerT* checker = nullptr) {
    rtl_hw.m_axi_usr_awvalid(dut.awvalid);
    rtl_hw.m_axi_usr_awready(dut.awready);
    rtl_hw.m_axi_usr_awaddr(dut.awaddr);
    rtl_hw.m_axi_usr_awprot(dut.awprot);
    rtl_hw.m_axi_usr_awuser(dut.awuser);
    rtl_hw.m_axi_usr_awregion(dut.awregion);
    rtl_hw.m_axi_usr_awqos(dut.awqos);
    rtl_hw.m_axi_usr_awcache(dut.awcache);
    rtl_hw.m_axi_usr_awburst(dut.awburst);
    rtl_hw.m_axi_usr_awsize(dut.awsize);
    rtl_hw.m_axi_usr_awlen(dut.awlen);
    rtl_hw.m_axi_usr_awid(dut.awid);
    rtl_hw.m_axi_usr_awlock(dut.awlock);
    rtl_hw.m_axi_usr_wvalid(dut.wvalid);
    rtl_hw.m_axi_usr_wready(dut.wready);
    rtl_hw.m_axi_usr_wdata(dut.wdata);
    rtl_hw.m_axi_usr_wstrb(dut.wstrb);
    rtl_hw.m_axi_usr_wuser(dut.wuser);
    rtl_hw.m_axi_usr_wlast(dut.wlast);
    rtl_hw.m_axi_usr_bvalid(dut.bvalid);
    rtl_hw.m_axi_usr_bready(dut.bready);
    rtl_hw.m_axi_usr_bresp(dut.bresp);
    rtl_hw.m_axi_usr_buser(dut.buser);
    rtl_hw.m_axi_usr_bid(dut.bid);
    rtl_hw.m_axi_usr_arvalid(dut.arvalid);
    rtl_hw.m_axi_usr_arready(dut.arready);
    rtl_hw.m_axi_usr_araddr(dut.araddr);
    rtl_hw.m_axi_usr_arprot(dut.arprot);
    rtl_hw.m_axi_usr_aruser(dut.aruser);
    rtl_hw.m_axi_usr_arregion(dut.arregion);
    rtl_hw.m_axi_usr_arqos(dut.arqos);
    rtl_hw.m_axi_usr_arcache(dut.arcache);
    rtl_hw.m_axi_usr_arburst(dut.arburst);
    rtl_hw.m_axi_usr_arsize(dut.arsize);
    rtl_hw.m_axi_usr_arlen(dut.arlen);
    rtl_hw.m_axi_usr_arid(dut.arid);
    rtl_hw.m_axi_usr_arlock(dut.arlock);
    rtl_hw.m_axi_usr_rvalid(dut.rvalid);
    rtl_hw.m_axi_usr_rready(dut.rready);
    rtl_hw.m_axi_usr_rdata(dut.rdata);
    rtl_hw.m_axi_usr_rresp(dut.rresp);
    rtl_hw.m_axi_usr_ruser(dut.ruser);
    rtl_hw.m_axi_usr_rid(dut.rid);
    rtl_hw.m_axi_usr_rlast(dut.rlast);

    dut.connect(*bridge_dut_arr[idx]);
    if (checker) { dut.connect(*checker); }
  }

  // Misc signals: IRQ/GPIO/reset and IRQ handshake with tlm_hw
  template<typename RtlHW>
  inline void bind_misc(
      int /*idx*/, RtlHW& rtl_hw,
      sc_core::sc_signal<sc_dt::sc_bv<128>>& h2c_intr_out,
      sc_core::sc_signal<sc_dt::sc_bv<256>>& h2c_gpio_out,
      sc_core::sc_signal<sc_dt::sc_bv<64>>& h2c_pulse_out,
      sc_core::sc_signal<sc_dt::sc_bv<64>>& c2h_intr,
      sc_core::sc_signal<sc_dt::sc_bv<256>>& c2h_gpio_in,
      sc_core::sc_signal<sc_dt::sc_bv<4>>& usr_resetn,
      sc_core::sc_signal<bool>& irq_out,
      sc_core::sc_signal<bool>& irq_ack,
      tlm2axi_hw_bridge& tlm_hw,
      sc_core::sc_vector< sc_core::sc_signal<bool> >& h2c_intr_write)
  {
    rtl_hw.h2c_intr_out(h2c_intr_out);
    rtl_hw.h2c_gpio_out(h2c_gpio_out);
    rtl_hw.c2h_intr_in(c2h_intr);
    rtl_hw.c2h_gpio_in(h2c_gpio_out); // loopback as in TopMulti
    rtl_hw.h2c_pulse_out(h2c_pulse_out);
    rtl_hw.usr_resetn(usr_resetn);

    rtl_hw.irq_out(irq_out);
    rtl_hw.irq_ack(irq_ack);
    tlm_hw.irq(irq_out);
    tlm_hw.h2c_irq(h2c_intr_write);
  }

  // Convenience: bind one complete instance using all helpers
  template<
    typename RtlHW,
    typename AXILiteChecker,
    typename AXISignalsT,
    typename DutSignalsT,
    typename DutCheckerT
  >
  inline void bind_instance(
      int idx,
      tlm2axi_hw_bridge& tlm_hw,
      RtlHW& rtl_hw,
      AXILiteChecker* axil_checker,
      AXISignalsT& dma,
      DutSignalsT& dut,
      DutCheckerT* dut_checker,
      sc_core::sc_signal<sc_dt::sc_bv<128>>& h2c_intr_out,
      sc_core::sc_signal<sc_dt::sc_bv<256>>& h2c_gpio_out,
      sc_core::sc_signal<sc_dt::sc_bv<64>>& h2c_pulse_out,
      sc_core::sc_signal<sc_dt::sc_bv<64>>& c2h_intr,
      sc_core::sc_signal<sc_dt::sc_bv<256>>& c2h_gpio_in,
      sc_core::sc_signal<sc_dt::sc_bv<4>>& usr_resetn,
      sc_core::sc_signal<bool>& irq_out,
      sc_core::sc_signal<bool>& irq_ack,
      sc_core::sc_vector< sc_core::sc_signal<bool> >& h2c_intr_write)
  {
    bind_hw_host(idx, tlm_hw);
    bind_axil_host(idx, rtl_hw, axil_checker);
    bind_dma(idx, rtl_hw, dma);
    bind_usr(idx, rtl_hw, dut, dut_checker);
    bind_misc(idx, rtl_hw,
              h2c_intr_out, h2c_gpio_out, h2c_pulse_out,
              c2h_intr, c2h_gpio_in, usr_resetn,
              irq_out, irq_ack,
              tlm_hw, h2c_intr_write);
  }
};

#endif // GRACE_NTTP_TEST_ADAPTER2DUT_H_
