flowchart LR
  subgraph SC_TLM_World
    TG[TLM Traffic Generator tg]
    SPL[tlm_splitter<2> splitter]
    REF_RAM[Reference memory ref_ram]
    HWB[tlm2axi_hw_bridge tlm_hw_bridge]
    AXIL_BR[t lm2axilite_bridge bridge_tlm2axilite]
    RAM[memory ram]
    CHK_AXIL[AXI-Lite Protocol Checker]
    CHK_AXI[AXI Protocol Checker (DUT)]
  end

  subgraph RTL_World (Verilated)
    RTL[rtl_hw_bridge (AXI HW Bridge RTL)]
    DUT[Vaxi*_master (AXI DUT Master)]
  end

  %% TLM side connections
  TG -- TLM payloads --> SPL
  SPL -- path 0 --> REF_RAM
  SPL -- path 1 --> HWB

  %% Host control path (AXI-Lite)
  HWB -- bridge_socket --> AXIL_BR
  AXIL_BR -- s_axi_* pins --> RTL

  %% IRQ/GPIO
  RTL -- irq_out --> HWB
  HWB -- h2c_irq[128] -->|h2c intr write| RTL
  RTL -- h2c_gpio_out/h2c_pulse_out -->(exposed)
  RTL -- c2h_gpio_in/c2h_intr_in <--(driven in test)

  %% DUT user AXI master path
  DUT -- m_axi_usr_* --> RTL
  DUT === CHK_AXI

  %% DMA AXI master from RTL bridge (host side)
  RTL -- m_axi_* --> AXI_DMA_BUS[(AXI host DMA signals)]
  %% (No direct memory is bound here on TLM; rtl bridge handles DMA internally)

  %% DUT memory access via software bridge
  subgraph DUT_to_RAM_path
    SW_BR[axi2tlm_bridge / axilite2tlm_bridge (DUT_SW_BRIDGE_DECL)]
  end
  DUT -- m_axi_usr_* === SW_BR
  SW_BR -- TLM --> RAM

  %% AXI-Lite checker on host control bus
  AXIL_BR === CHK_AXIL

  %% Clocks/Reset
  CLK[(sc_clock clk)] --> AXIL_BR
  CLK --> RTL
  CLK --> DUT
  CLK --> CHK_AXIL
  CLK --> CHK_AXI
  RSTN[(rst_n)] --> AXIL_BR
  RSTN --> RTL
  RSTN --> DUT
  RST[(rst)] --> HWB