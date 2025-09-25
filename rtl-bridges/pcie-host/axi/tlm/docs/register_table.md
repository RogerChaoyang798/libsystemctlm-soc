tlm2axi-hw-bridge register table

1. 在 reset_thread() 里写的寄存器
这里主要是做 硬件复位和中断初始化：
    • 中断控制寄存器
        ○ INTR_ERROR_ENABLE_REG_ADDR_MASTER = 0x0
        ○ INTR_COMP_ENABLE_REG_ADDR_MASTER = 0x0
        ○ INTR_C2H_TOGGLE_ENABLE_0_REG_ADDR_MASTER = 0xFFFFFFFF
        ○ INTR_C2H_TOGGLE_ENABLE_1_REG_ADDR_MASTER = 0xFFFFFFFF
        ○ INTR_COMP_CLEAR_REG_ADDR_MASTER = 0xFFFF （清除 pending completion 中断）
        ○ INTR_COMP_ENABLE_REG_ADDR_MASTER = 0xFFFF （开启 completion 中断）
    • 描述符通用字段复位（循环 d=0..15，每个描述符槽位一组寄存器）：
        ○ DESC_0_AXADDR_2_REG_ADDR_MASTER = 0
        ○ DESC_0_AXADDR_3_REG_ADDR_MASTER = 0
        ○ DESC_0_AXID_0_REG_ADDR_MASTER = 0
        ○ DESC_0_AXID_1_REG_ADDR_MASTER = 0
        ○ DESC_0_AXID_2_REG_ADDR_MASTER = 0
        ○ DESC_0_AXID_3_REG_ADDR_MASTER = 0
👉 这一部分目的是：复位 DMA 引擎，屏蔽/使能中断，初始化 AXI 地址/ID。

2. 在 desc_access() 里写的寄存器
这里是 真正配置一次 AXI 事务描述符：
    • AXI 地址（64bit，低高 32bit 分开写）
        ○ DESC_0_AXADDR_0_REG_ADDR_MASTER = addr[31:0]
        ○ DESC_0_AXADDR_1_REG_ADDR_MASTER = addr[63:32]
    • 数据缓冲区偏移
        ○ DESC_0_DATA_OFFSET_REG_ADDR_MASTER = DRAM_OFFSET_WRITE_MASTER
    • 属性寄存器（burst 类型/secure/qos/region 等编码）
        ○ DESC_0_ATTR_REG_ADDR_MASTER = compute_attr(attr) | 1
    • 传输总大小（bytes）
        ○ DESC_0_SIZE_REG_ADDR_MASTER = total_size
    • 每 beat 大小（AXSIZE 编码，log2(bytes_per_beat)）
        ○ DESC_0_AXSIZE_REG_ADDR_MASTER = axsize
    • 事务类型
        ○ DESC_0_TXN_TYPE_REG_ADDR_MASTER = (is_write?0:1) | (need_wstrb?2:0)
    • Ownership flip 寄存器（提交 descriptor 给硬件执行）
        ○ OWNERSHIP_FLIP_REG_ADDR_MASTER = 1 << d
    • 完成后清除中断
        ○ INTR_COMP_CLEAR_REG_ADDR_MASTER = 1U << d
👉 这一部分就是在 配置和提交一个 AXI DMA 描述符。

3. 在 desc_setup_wstrb() 里写的寄存器
专门写 WSTRB 掩码：
    • DRAM_OFFSET_WSTRB_MASTER + n*4
        ○ 每个 32bit 值代表一段 4 字节的写 strobe 掩码。

总结表格
函数	写入寄存器类别	作用
reset_thread()	中断控制 / AXADDR / AXID	复位 DMA 引擎，初始化 AXI 地址和 ID，配置中断使能
desc_access()	AXADDR / SIZE / AXSIZE / ATTR / TXN_TYPE / OWNERSHIP	配置一次 DMA 描述符并提交给硬件执行
desc_setup_wstrb()	WSTRB buffer (DRAM 区)	配置写事务的字节掩码



下面总结 
axi2tlm_hw_bridge::process_desc_free()
 使用到的寄存器与固定地址（路径：
rtl-bridges/pcie-host/axi/tlm/axi2tlm-hw-bridge.h
），并给出其数值偏移。寄存器宏定义位于 
rtl-bridges/pcie-host/axi/tlm/private/user_slave_addr.h
，描述符基址计算位于 
rtl-bridges/pcie-host/axi/tlm/tlm-hw-bridge-base.h
。

注意：

所有寄存器偏移均相对于 USR_SLV_BASE_ADDR = 0x0。
设备访问时会再加上 base_addr（见 
tlm_hw_bridge_base::dev_access()
）。
每个描述符的寄存器实际地址 = USR_SLV_BASE_ADDR + 0x3000 + 0x200*d + <描述符内偏移>。
用到的描述符寄存器（按读取顺序）
AXI 地址高 32 位
名称：DESC_0_AXADDR_1_REG_ADDR_SLAVE
偏移（描述符内）：0x44
访问：
dev_read32(desc_addr + 0x44)
AXI 地址低 32 位
名称：DESC_0_AXADDR_0_REG_ADDR_SLAVE
偏移：0x40
访问：
dev_read32(desc_addr + 0x40)
事务类型
名称：DESC_0_TXN_TYPE_REG_ADDR_SLAVE
偏移：0x00
访问：
dev_read32(desc_addr + 0x00)
（bit0=读写，bit1=是否有BE）
AXSIZE（每拍字节数编码）
名称：DESC_0_AXSIZE_REG_ADDR_SLAVE
偏移：0x30
访问：
dev_read32(desc_addr + 0x30)
SIZE（AXI传输数据总长度，字节）
名称：DESC_0_SIZE_REG_ADDR_SLAVE
偏移：0x04
访问：
dev_read32(desc_addr + 0x04)
DATA_OFFSET（数据/BE在桥内RAM窗口中的偏移）
名称：DESC_0_DATA_OFFSET_REG_ADDR_SLAVE
偏移：0x08
访问：
dev_read32(desc_addr + 0x08)
AXID 低 32 位
名称：DESC_0_AXID_0_REG_ADDR_SLAVE
偏移：0x50
访问：
dev_read32(desc_addr + 0x50)
ATTR（AXI burst/lock/prot/qos 等属性）
名称：DESC_0_ATTR_REG_ADDR_SLAVE
偏移：0x34
访问：
dev_read32(desc_addr + 0x34)
说明：

desc_addr(d)
 固定计算公式在 
tlm-hw-bridge-base.h
：
固定地址：0x3000 + 0x200 * d（相对于 USR_SLV_BASE_ADDR）
上述“描述符内偏移”需与该固定基址相加，得到该描述符实例的实际寄存器地址。
用到的全局控制/状态寄存器（写）
响应状态寄存器
名称：STATUS_RESP_REG_ADDR_SLAVE
偏移：0x308
访问：
dev_write32_strong(0x308, r_resp)
响应顺序寄存器
名称：RESP_ORDER_REG_ADDR_SLAVE
偏移：0x32C
访问：
dev_write32_strong(0x32C, d | (1U << 31))
所有权翻转寄存器（将描述符归还硬件）
名称：OWNERSHIP_FLIP_REG_ADDR_SLAVE
偏移：0x304
访问：
dev_write32_strong(0x304, 1U << d)
用到的桥内 RAM 固定窗口（仅在特定条件下访问）
写数据窗口（Host->HW，非 mode1 时）
名称：DRAM_OFFSET_WRITE_SLAVE
偏移：0xC000
访问：
dev_copy_from(0xC000 + data_offset, data, size, ...)
读数据窗口（HW->Host，非 mode1 时）
名称：DRAM_OFFSET_READ_SLAVE
偏移：0x8000
访问：
dev_copy_to(0x8000 + data_offset, data, size)
写 strobe/byte-enable 窗口（当需字节使能且未直接提供 BE 指针时）
名称：DRAM_OFFSET_WSTRB_SLAVE
偏移：0x10000
访问：在 
process_be()
 中读取 
dev_read32(0x10000 + data_offset + i)
固定地址与计算汇总
描述符基址公式（相对 USR_SLV_BASE_ADDR）：0x3000 + 0x200 * d
描述符内关键字段偏移：
TXN_TYPE: 0x00
SIZE: 0x04
DATA_OFFSET: 0x08
AXSIZE: 0x30
ATTR: 0x34
AXADDR_0: 0x40
AXADDR_1: 0x44
AXID_0: 0x50
全局寄存器偏移（相对 USR_SLV_BASE_ADDR）：
STATUS_RESP: 0x308
RESP_ORDER: 0x32C
OWNERSHIP_FLIP: 0x304
桥内 DRAM 窗口（相对 USR_SLV_BASE_ADDR）：
写数据：0xC000
读数据：0x8000
写 strobe：0x10000