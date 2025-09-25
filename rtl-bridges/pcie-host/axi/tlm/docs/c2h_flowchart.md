flowchart TD
  title["axi2tlm_hw_bridge::axi2tlm_hw_bridge(name, base_addr, offset, vdev)"]

  A[Start] --> B[Call tlm_hw_bridge_base ctor]
  B --> C[Init members: init_socket, desc_busy=0, we_init_socket, we_target_socket, wrap_expander, mm=NULL]
  C --> D[mode1 = true]
  D --> E{mode1?}
  E -- Yes --> F[mm = new tlm_mm_vfio_MAX_NR_DESCRIPTORS, AXI4_MAX_BURSTLENGTH*1024/8, vdev]
  E -- No --> G[mm remains NULL]
  F --> H
  G --> H
  H[we_target_socket注册we_b_transport] --> I[we_init_socket.bind-wrap_expander.target_socket]
  I --> J[wrap_expander.init_socket-we_target_socket]
  J --> K[SC_THREAD_reset_thread]
  K --> L[SC_THREAD_work_thread]
  L --> M[SC_THREAD_process_wires_ from base]
  M --> N[End]


flowchart TD
  title["axi2tlm_hw_bridge::reset_thread()"]

  A[Loop forever] --> B[wait(rst.negedge_event); wait(SC_ZERO_TIME)]
  B --> C[bridge_probe(); bridge_reset()]
  C --> D[Print mode and DESC_MASK]
  D --> E[dev_write32(MODE_SELECT, mode1); r_resp = 0]
  E --> F[r = dev_read32(INTR_TXN_AVAIL_STATUS)]
  F --> G{r != 0?}
  G -- Yes --> H[Print stale; dev_write32(STATUS_RESP_COMP, r)]
  G -- No --> I[Proceed]
  H --> I
  I --> J[r = dev_read32(STATUS_BUSY)]
  J --> K{r != 0?}
  K -- Yes --> L[Print recovery; dev_write32(STATUS_RESP_COMP, r)]
  L --> M[dev_write32(OWNERSHIP_FLIP, r)]
  M --> N[while(r) r=dev_read32(STATUS_BUSY)]
  N --> O[dev_write32(STATUS_RESP_COMP, 0)]
  K -- No --> P[Proceed]
  O --> P
  P --> Q[Enable TXN_AVAIL IRQs: dev_write32(INTR_TXN_AVAIL_ENABLE, DESC_MASK)]
  Q --> R[Enable C2H IRQ toggles (0/1) to 0xFFFFFFFF]
  R --> S[Ack stale completions: dev_write32(STATUS_RESP_COMP, 0)]
  S --> T[Hand all desc to HW: dev_write32(OWNERSHIP_FLIP, DESC_MASK)]
  T --> U{mm != NULL?}
  U -- Yes --> V[For i in [0..MAX_NR_DESCRIPTORS): program DESC_i data/wstrb host addrs using mm->to_dma()]
  U -- No --> W[Skip]
  V --> X[probed = true; probed_event.notify()]
  W --> X
  X --> A

flowchart TD
  title["axi2tlm_hw_bridge::process_be(data_offset, size, in32, out32) -> bool"]

  A[Start] --> B[assert(data_offset % 4 == 0)]
  B --> C[needed = false]
  C --> D[for i=0; i<size; i+=4]
  D --> E[bytes_to_handle = MIN(4, size - i)]
  E --> F{in32 != NULL?}
  F -- Yes --> G[be_byte = in32[i/4]]
  F -- No --> H[be_byte = dev_read32(DRAM_OFFSET_WSTRB_SLAVE + data_offset + i)]
  G --> I[out32[i/4] = be_byte]
  H --> I
  I --> J[needed = true  // unconditional due to code path]
  J --> D
  D -- loop end --> K[assert(needed)]
  K --> L[return needed (true)]


flowchart TD
  title["axi2tlm_hw_bridge::process_desc_free(d, r_avail)"]

  A[Start] --> B[desc_addr = this->desc_addr(d)]
  B --> C[Select gp: gp = mm? *mm->allocate(d) : this->gp]
  C --> D{mm != NULL?}
  D -- Yes --> E[gp.acquire(); data32 = gp.data_ptr(); be32 = gp.byte_enable_ptr(); desc_gp[d]=&gp; gp.get_extension(genattr)]
  D -- No --> F[data32 = this->data32; be32 = this->be32; genattr may be NULL]
  E --> G
  F --> G
  G{genattr == NULL?} -- Yes --> H[genattr = new(genattr_extension); gp.set_extension(genattr)]
  G -- No --> I[Keep existing extension]
  H --> I
  I --> J[Read AXI/TXN fields from DESC regs: axaddr, type, axsize, size, data_offset, axid, attr]
  J --> K[Derive: is_write=!(type&1), number_bytes=1<<axsize, axburst=attr&3, axlock=(attr>>2)&3, axprot=(attr>>8)&7, axqos=(attr>>11)&0xF]
  K --> L[axlen=(size-1)/data_bytewidth; burst_length=axlen+1]
  L --> M{is_axilite_slave()?}
  M -- Yes --> N[axburst=INCR; axlock=0]
  M -- No --> O{is_axi4_slave()?}
  O -- Yes --> P[axlock&=1; axqos=(attr>>11)&0xF]
  O -- No --> Q[No changes]
  N --> R
  P --> R
  Q --> R
  R[Set genattr: wrap(axburst==WRAP), exclusive(lock==EXCL), locked(lock==LOCKED), non_secure(axprot&NS), qos(axqos)] --> S[Compute offset = axaddr % data_bytewidth; tx_size = size]
  S --> T{is_write?}
  T -- Yes --> U{mode1?}
  U -- No --> V[dev_copy_from(DRAM_OFFSET_WRITE_SLAVE + data_offset, data32, size)]
  U -- Yes --> W[Skip dev_copy_from]
  V --> X
  W --> X
  X{type & 2 (has WSTRB)?} -- Yes --> Y[be_needed = process_be(data_offset, size, mode1?be32:NULL, be32)]
  X -- No --> Z[be_needed = false]
  Y --> AA
  Z --> AA
  AA{Narrow write? burst_length>1 && (axburst==FIXED || number_bytes < data_bytewidth)} -- Yes --> AB[Expand data/BE into wide beats; tx_size updated]
  AA -- No --> AC[No narrow handling]
  AB --> AD
  AC --> AD
  T -- No (READ) --> AE{Narrow read? burst_length>1 && number_bytes < data_bytewidth}
  AE -- Yes --> AF[tx_size = offset + number_bytes*burst_length - (axaddr & (number_bytes-1))]
  AE -- No --> AG[tx_size unchanged]
  AF --> AH
  AG --> AH
  AD --> AI
  AH --> AI
  AI[Setup gp: address=axaddr; data_ptr=data32+offset; data_len=tx_size-offset; streaming_width=tx_size-offset; byte_en_ptr=be_needed?be32+offset:NULL; be_len=be_needed?tx_size-offset:0; command=WRITE/READ] --> AJ{axburst==FIXED?}
  AJ -- Yes --> AK[Set streaming_width = number_bytes - (axaddr & (number_bytes-1))]
  AJ -- No --> AL[Keep streaming_width]
  AK --> AM
  AL --> AM
  AM{axburst==WRAP?} -- Yes --> AN[we_init_socket->b_transport(gp)]
  AM -- No --> AO[init_socket->b_transport(gp)]
  AN --> AP
  AO --> AP
  AP{READ path?} -- Yes --> AQ{Narrow read expansion needed?}
  AQ -- Yes --> AR[Use bounce buffer; expand into per-beat positions]
  AQ -- No --> AS[No expansion]
  AR --> AT
  AS --> AT
  AT{!mode1?} -- Yes --> AU[dev_copy_to(DRAM_OFFSET_READ_SLAVE + data_offset, data32, size)]
  AT -- No --> AV[Skip dev_copy_to]
  AU --> AW
  AV --> AW
  AW[axi_resp = tlm_gp_get_axi_resp(gp); update r_resp bits; write STATUS_RESP; RESP_ORDER(d|1<<31)] --> AX[dev_write32_strong(OWNERSHIP_FLIP, 1<<d)]
  AX --> AY[desc_state[d]=DESC_STATE_DATA; desc_busy|=1<<d]
  AY --> AZ{mm != NULL?}
  AZ -- Yes --> BA[Restore gp raw ptrs to base buffers (data32/be32)]
  AZ -- No --> BB[gp.release_extension(genattr)]
  BA --> BC[End]
  BB --> BC[End]

flowchart TD
  title["axi2tlm_hw_bridge::process(r_avail) -> unsigned int"]

  A[Start] --> B["dev_write32(INTR_TXN_AVAIL_CLEAR, r_avail)"]
  B --> C["busy = dev_read32(STATUS_BUSY)"]
  C --> D["own  = dev_read32(OWNERSHIP)"]
  D --> E["ack = (~busy) & (~own) & desc_busy & DESC_MASK"]
  E --> F{ack != 0?}
  F -- Yes --> G{"!busy && !own?"}
  G -- Yes --> H["dev_write32(OWNERSHIP_FLIP, ack)"]
  G -- No --> I[ack = 0]
  H --> J
  I --> J
  J --> K[for d in 0..MAX_NR_DESCRIPTORS]
  K --> L{"r_avail bit d set?"}
  L -- Yes --> M["process_desc_free(d, r_avail)"]
  L -- No --> N[No op]
  M --> O
  N --> O
  O{ack bit d set?} -- Yes --> P["desc_state[d]=FREE; desc_busy&=~(1<<d); if(mm) desc_gp[d]->release(); desc_gp[d]=NULL"]
  O -- No --> Q[No op]
  P --> R
  Q --> R
  R{"desc_state[d] != FREE?"} -- Yes --> S["num_pending++ ; num_pending_mask|=1<<d"]
  R -- No --> T[No op]
  S --> U
  T --> U
  U --> K
  K -- loop end --> V["if(debug) print status"]
  V --> W["return (desc_busy == 0xffff)"]

flowchart TD
  title["axi2tlm_hw_bridge::work_thread()"]

  A[Start] --> B{probed?}
  B -- No --> C[wait probed_event]
  B -- Yes --> D[Proceed]
  C --> D
  D --> E[Loop forever]
  E --> F{use_irq && !irq.read?}
  F -- Yes --> G[wait irq.posedge_event]
  F -- No --> H[wait 1 ns]
  G --> I
  H --> I
  I --> J[r_avail = dev_read32 INTR_TXN_AVAIL_STATUS]
  J --> K{r_avail == 0?}
  K -- Yes --> E
  K -- No --> L[dev_write32 INTR_TXN_AVAIL_ENABLE, 0 // mask]
  L --> M[num_loops=0; debug=false]
  M --> N["do { num_pending = process(r_avail); r_avail = dev_read32(INTR_TXN_AVAIL_STATUS); num_loops++; if(num_loops>10000) debug=true } while(r_avail || num_pending)"]
  N --> O["dev_write32(INTR_TXN_AVAIL_ENABLE, DESC_MASK) // re-enable"]
  O --> E

flowchart TD
  title["axi2tlm_hw_bridge::is_axilite_slave() -> bool"]

  A[Start] --> B{return bridge_type == TYPE_AXI4_LITE_SLAVE || bridge_type == TYPE_PCIE_AXI4_LITE_SLAVE}
  B --> C[End]

flowchart TD
  title["axi2tlm_hw_bridge::is_axi4_slave() -> bool"]

  A[Start] --> B{return bridge_type == TYPE_AXI4_SLAVE || bridge_type == TYPE_PCIE_AXI4_SLAVE}
  B --> C[End]

flowchart TD
  title["axi2tlm_hw_bridge::we_b_transport(gp, delay)"]

  A[Start] --> B[init_socket->b_transport(gp, delay)]
  B --> C[End]