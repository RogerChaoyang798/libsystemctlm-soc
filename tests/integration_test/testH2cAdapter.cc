//
// TbH2cAdapter: inherit from TbXbar and rewire sockets through XbarH2cAdapter
//

#include <sstream>
#define SC_INCLUDE_DYNAMIC_PROCESSES
#define DISABLE_CLUSTER_V3
#include "systemc"
using namespace sc_core;

#include "tlm.h"
// tlm-aligner.h (pulled by tlm2axi-hw-bridge.h) uses tlm_utils sockets
// but does not include their headers; include them here first.
#include "tlm_utils/simple_initiator_socket.h"
#include "tlm_utils/simple_target_socket.h"
#include "rtl-bridges/pcie-host/axi/tlm/tlm2axi-hw-bridge.h"

#include "soc/grace_noc_libs/grace/test/test_bench.h"
#include "nttp/XBarH2cAdapter.h"
// Helper that provides bridge_tlm2axilite and bridge_dut_arr arrays
#include "nttp/test/Adapter2Dut.h"
// Simple TLM RAM used in libsystemctlm-soc tests
#include "test-modules/memory.h"

// For this test, override the number of initiators/targets explicitly
static const int NR_OF_INITIATORS = 2;
static const int NR_OF_TARGETS = 2;
class TbH2cAdapter : public TbXbar {
public:
  // NI, NT follow TbXbar::NR_OF_INITIATORS, NR_OF_TARGETS
  typedef XbarH2cAdapter<NR_OF_INITIATORS, NR_OF_TARGETS> AdapterT;

  TbH2cAdapter(sc_core::sc_module_name name_, sc_core::sc_time period_, tlm2axi_hw_bridge* bridges[NR_OF_INITIATORS])
  : TbXbar(name_, period_, /*use_xbar_bindings=*/false)
  , m_adapter(nullptr)
  {
    // Instantiate adapter with provided HW bridges
    m_adapter = new AdapterT("xbarH2cAdapter", bridges);

    // Rewire bindings as requested
    for (unsigned i = 0; i < NR_OF_INITIATORS; ++i) {
      BIND(mst_tlm_tlm[i].tlm_init_skt, m_adapter->target_sockets[i]);
      m_adapter->m_hw_bridge[i]->bridge_socket(adapter2dut.bridge_axil_arr[i]->tgt_socket);
    }

  }

  ~TbH2cAdapter() {
    delete m_adapter;
  }

public:
  AdapterT* m_adapter;
  Adapter2Dut<NR_OF_INITIATORS> adapter2dut;
};


int sc_main(int argc, char* argv[])
{
  sc_core::sc_time period(1, sc_core::SC_NS);

  // Create hardware bridges for each master initiator
  tlm2axi_hw_bridge* bridges[NR_OF_INITIATORS];
  for (unsigned i = 0; i < NR_OF_INITIATORS; ++i) {
    std::ostringstream os;
    os << "hw_bridge_" << i;
    // Base address increases by 0x10000 per bridge
    uint64_t base_addr = 0x10000ull * i;
    bridges[i] = new tlm2axi_hw_bridge(os.str().c_str(), base_addr);
  }

  // Instantiate testbench with adapter wiring
  TbH2cAdapter tb("tb_h2c_adapter", period, bridges);

  // Create one RAM per initiator and bind to DUT-side axilite2tlm bridges
  const unsigned ram_size = 8 * 1024; // bytes
  memory* rams[NR_OF_INITIATORS];
  for (unsigned i = 0; i < NR_OF_INITIATORS; ++i) {
    std::ostringstream os;
    os << "ram_" << i;
    rams[i] = new memory(os.str().c_str(), sc_time(1, SC_NS), ram_size);
    tb.adapter2dut.bridge_dut_arr[i]->socket(rams[i]->socket);
  }

  // Run simulation
  sc_start(sc_core::sc_time(1, sc_core::SC_US));

  // Cleanup
  for (unsigned i = 0; i < NR_OF_INITIATORS; ++i) {
    delete bridges[i];
    delete rams[i];
  }

  return 0;
}

