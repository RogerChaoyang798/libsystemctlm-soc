```mermaid
flowchart TD

A[TLM 请求<br/>b_transport] --> B[加 base_offset<br/>检查 streaming_width]
B --> C{是否 probed?}
C -- 否 --> C1[等待 probed_event]
C --> D[mutex.lock]

D --> E{Write or Read?}
E -- Write --> F1[拷贝数据到<br/>DRAM_OFFSET_WRITE_MASTER]
E -- Read --> F2[无需预写数据]

F1 --> G[调用 desc_access]
F2 --> G[调用 desc_access]

subgraph desc_access 流程
    G --> H[计算 offset/nr_beats/axsize]
    H --> I{是否需要 WSTRB?}
    I -- 是 --> J[desc_setup_wstrb<br/>写 WSTRB buffer]
    I -- 否 --> K[跳过]

    J --> K
    K --> L[等待描述符所有权]
    L --> M[配置 AXI 地址寄存器]
    M --> N[配置 SIZE、AXSIZE、ATTR、TXN_TYPE]
    N --> O[写 OWNERSHIP_FLIP<br/>提交描述符]
    O --> P{use_irq?}
    P -- 是 --> P1[等待 IRQ 置位]
    P -- 否 --> Q[轮询 OWNERSHIP]
    P1 --> Q
    Q --> R[读取 RESP 状态]
end

R --> S{Read 请求?}
S -- 是且成功 --> T[从 DRAM_OFFSET_READ_MASTER<br/>拷贝数据到 trans.data_ptr]
S -- 否 --> U[跳过]

T --> V[设置 trans 响应状态<br/>tlm_gp_set_axi_resp]
U --> V

V --> W[mutex.unlock]
W --> X[notify process_wires_ev<br/>yield 一个 delta cycle]
X --> Y[完成 b_transport 返回]
