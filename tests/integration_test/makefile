CXX := g++
CXXFLAGS := -std=gnu++17 -O0 -g

INCLUDES := \
  -I. \
  -Isoc \
  -Isoc/grace_noc_libs \
  -Isoc/grace_noc_libs/nttp \
  -Isoc/grace_noc_libs/grace \
  -Isoc/grace_noc_libs/grace/src \
  -Isoc/grace_noc_libs/grace/test \
  -Isoc/grace_noc_libs/buses/ctrl_noc/include/ \
  -Isoc/grace_noc_libs/buses/cluster/clus_ctrl_noc \
  -Isoc/grace_noc_libs/crossbar \
  -Igrace_noc \
  -Igrace_noc/grace \
  -Igrace_noc/grace/src \
  -Igrace_noc/noc/src \
  -Isoc/pcie_ep/libsystemctlm-soc \
  -Isoc/pcie_ep/libsystemctlm-soc/utils \
  -Isoc/pcie_ep/libsystemctlm-soc/tlm-bridges \
  -Isoc/pcie_ep/libsystemctlm-soc/rtl-bridges \
  -Isoc/pcie_ep/libsystemctlm-soc/traffic-generators \
  -Isoc/pcie_ep/libsystemctlm-soc/tests \
  -Isoc/pcie_ep/libsystemctlm-soc/checkers \
  -Isoc/pcie_ep/libsystemctlm-soc/trace

LDFLAGS := -L${SYSTEMC_HOME}/lib-linux64
LIBS := -lsystemc -latomic -lpthread

SRCS := soc/grace_noc_libs/nttp/test/testH2cAdapter.cc
TARGET := build/test_h2c_adapter

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(SRCS)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $^ $(LDFLAGS) $(LIBS) -o $@

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)

# Original ad-hoc build command (kept for reference, now commented out)
# g++ -std=gnu++17 -O0 -g -DDISABLE_CLUSTER_V3 \
#   -I. \
#   -Isoc \
#   -Isoc/grace_noc_libs \
#   -Isoc/grace_noc_libs/nttp \
#   -Isoc/grace_noc_libs/grace \
#   -Isoc/grace_noc_libs/grace/src \
#   -Isoc/grace_noc_libs/grace/test \
#   -Isoc/grace_noc_libs/buses/ctrl_noc/include/ \
#   -Igrace_noc \
#   -Igrace_noc/grace \
#   -Igrace_noc/grace/src \
#   -Igrace_noc/noc/src \
#   -Isoc/pcie_ep/libsystemctlm-soc \
#   -Isoc/pcie_ep/libsystemctlm-soc/utils \
#   -Isoc/pcie_ep/libsystemctlm-soc/tlm-bridges \
#   -Isoc/pcie_ep/libsystemctlm-soc/rtl-bridges \
#   -Isoc/pcie_ep/libsystemctlm-soc/traffic-generators \
#   -Isoc/pcie_ep/libsystemctlm-soc/test-modules \
#   -Isoc/pcie_ep/libsystemctlm-soc/checkers \
#   -Isoc/pcie_ep/libsystemctlm-soc/trace \
#   soc/grace_noc_libs/nttp/test/testH2cAdapter.cc \
#   -L${SYSTEMC_HOME}/lib-linux64 -lsystemc -latomic -lpthread \
  -o build/test_h2c_adapter