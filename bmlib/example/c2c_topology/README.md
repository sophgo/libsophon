# C2C topology example

A self-contained sample + test for the libsophon C2C topology API
(`tpuRtSetupTopology` / `tpuRtGetTopologyV2` / `tpuRtGetTopology`,
`struct c2c_port_info_v2`), which is source/ABI compatible with tpuv7-runtime.

## What it models

A **4-chip C2C ring** (`chip0 -> chip1 -> chip2 -> chip3 -> chip0`). Each chip
has 4 C2C ports; this example uses 2 of them per chip:

- `pcie_0` -> next chip in the ring, over **cdma0** (send_port=recv_port=0)
- `pcie_1` -> previous chip in the ring, over **cdma1** (send_port=recv_port=1)
- `pcie_2`, `pcie_3` unused

`chip<N>.ini` is one file per device index `N`. `[system_if]` gives the chip's
identity (`slot_id`, `socket_id`); with the driver default
`C2C_CHIP_NUM_IN_CARD=1`, the global chip id is `slot_id*1 + socket_id`, so here
`socket_id == device index`. Each `[pcie_K]` describes one C2C port and the peer
it connects to (`peer_socketid`, `peer_pcieid`).

> Adjust the INIs to match your real board wiring. For fewer chips, provide only
> `chip0.ini .. chip{N-1}.ini`; missing files are treated as "no C2C links".

## Build

```sh
# against an installed libsophon (default prefix /opt/sophon):
make

# or against an in-tree build:
make INC=../../include LIB=/path/to/built/bmlib
```

## Run

The library reads the INI directory from `$TPU_C2C_INI_DIR`
(default `/etc/sophon/c2c`). Point it at this directory:

```sh
TPU_C2C_INI_DIR=$(pwd) ./test_c2c_topology
# if you built against an in-tree lib, also set:
#   LD_LIBRARY_PATH=/path/to/built/bmlib
```

## Expected output (4-chip ring)

```
device count = 4
tpuRtSetupTopology: ok
tpuRtGetTopologyV2: ok

         ->chip0   ->chip1   ->chip2   ->chip3
chip0      .       s0/r0 L1     .       s1/r1 L1
chip1   s1/r1 L1     .       s0/r0 L1     .
chip2      .       s1/r1 L1     .       s0/r0 L1
chip3   s0/r0 L1     .       s1/r1 L1     .

directed links:
  chip0.pcie0 -> chip1.pcie1   send=cdma0 recv=cdma0
  chip0.pcie1 -> chip3.pcie0   send=cdma1 recv=cdma1
  ...
```

Cell `[i][j]` describes the directed link chip `i` -> chip `j`; `.` means no
direct link. The matrix is also written back into each chip's SRAM at
`0x2fff8000` for the device side (AP/TP firmware, CDMA C2C routing) to consume.
