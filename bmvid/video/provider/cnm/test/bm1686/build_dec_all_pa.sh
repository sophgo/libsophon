#!/bin/bash

function build_vpu()
{
    echo "SYSTEM ID         : $1"
    echo "CAHNNEL ID        : $2"
    echo "MMU PAGE SIZE     : $3"
    echo "ENABLE MMU        : $4"
    echo "MMU REMAP         : $5"
    echo "MMU MULTI-CHANNEL : $6"
    echo "TEST TYPE         : $7"
    echo "ALL PARAM         : $*"
    
    sy_id=$1
    ch_id=$2
    mmu_page=$3
    enable_mmu=$4
    remap=$5
    multch=$6
    type=$7
    
    core_id=$((${sy_id}*2+${ch_id}))
    echo "the current core is  = ${core_id}"

    find ./ -name libbmvideo.a | xargs rm -rf
    make TEST_CASE=wave_dec NPU_EN=0 RUN_ENV=DDR DEBUG=1 PLATFORM=ASIC  CHIP=BM1686 SYSTEM_ID=${sy_id} CHANNEL_ID=${ch_id} MMU_PAGE=${mmu_page} REMAP_MMU=${remap} MULTI_CHAN=${multch} ENABLE_MMU=${enable_mmu} SUBTYPE=palladium clean 
    make TEST_CASE=wave_dec NPU_EN=0 RUN_ENV=DDR DEBUG=1 PLATFORM=ASIC  CHIP=BM1686 SYSTEM_ID=${sy_id} CHANNEL_ID=${ch_id} MMU_PAGE=${mmu_page} REMAP_MMU=${remap} MULTI_CHAN=${multch} ENABLE_MMU=${enable_mmu} SUBTYPE=palladium MEM_START=0x410000000 MEM_SIZE=0x20000000

    (cp out/BM1686_wave_dec.elf run/BM1686_wave_dec_s${sy_id}c${ch_id}_${type}_nommu.elf)
    (cp out/target.ld run/BM1686_wave_dec_s${sy_id}c${ch_id}_${type}_nommu.ld)
    (cp out/BM1686_wave_dec.map run/BM1686_wave_dec_s${sy_id}c${ch_id}_${type}_nommu.map)
    (aarch64-elf-objdump -D run/BM1686_wave_dec_s${sy_id}c${ch_id}_${type}_nommu.elf > run/BM1684_wave_dec_s${sy_id}c${ch_id}_${type}_nommu.txt)
}

set -e
rm -rf ./run
mkdir -p ./run

# build disable mmu
#           sys  ch    p_s   en    re    mul     type
build_vpu   0    0     0     0     0     0       "dis"
### + ###      build_vpu   0    1     0     0     0     0       "dis"
### + ###      build_vpu   0    0     0     0     0     0       "dis"
### + ###      build_vpu   1    1     0     0     0     0       "dis"

### + ###      # build base mmu(4K)
### + ###      #           sys  ch    p_s   en    re    mul     type
### + ###      build_vpu   0    0     0     1     0     0       "base_4k"
### + ###      build_vpu   0    1     0     1     0     0       "base_4k"
### + ###      build_vpu   1    0     0     1     0     0       "base_4k"
### + ###      build_vpu   1    1     0     1     0     0       "base_4k"
### + ###      
### + ###      # build base mmu(32K)
### + ###      #           sys  ch    p_s   en    re    mul     type
# build_vpu   0    0     1     1     0     0       "base_32k"
### + ###      build_vpu   0    1     1     1     0     0       "base_32k"
### + ###      build_vpu   1    0     1     1     0     0       "base_32k"
### + ###      build_vpu   1    1     1     1     0     0       "base_32k"
### + ###      
### + ###      # build base mmu(64K)
### + ###      #           sys  ch    p_s   en    re    mul     type
### + ###      build_vpu   0    0     2     1     0     0       "base_64k"
### + ###      build_vpu   0    1     2     1     0     0       "base_64k"
### + ###      build_vpu   1    0     2     1     0     0       "base_64k"
### + ###      build_vpu   1    1     2     1     0     0       "base_64k"
### + ###      
### + ###      # build base mmu(128K)
### + ###      #           sys  ch    p_s   en    re    mul     type
### + ###      build_vpu   0    0     3     1     0     0       "base_128k"
### + ###      build_vpu   0    1     3     1     0     0       "base_128k"
### + ###      build_vpu   1    0     3     1     0     0       "base_128k"
### + ###      build_vpu   1    1     3     1     0     0       "base_128k"
### + ###      
### + ###      # remap mmu(4K)
### + ###      #           sys  ch    p_s   en    re    mul     type
### + ###      build_vpu   0    0     0     1     1     0       "remap_4k"
### + ###      build_vpu   0    1     0     1     1     0       "remap_4k"
### + ###      build_vpu   1    0     0     1     1     0       "remap_4k"
### + ###      build_vpu   1    1     0     1     1     0       "remap_4k"
### + ###      
### + ###      # remap mmu(32K)
### + ###      #           sys  ch    p_s   en    re    mul     type
### + ###      build_vpu   0    0     1     1     1     0       "remap_32k"
### + ###      build_vpu   0    1     1     1     1     0       "remap_32k"
### + ###      build_vpu   1    0     1     1     1     0       "remap_32k"
### + ###      build_vpu   1    1     1     1     1     0       "remap_32k"
### + ###      
### + ###      # remap mmu(64K)
### + ###      #           sys  ch    p_s   en    re    mul     type
### + ###      build_vpu   0    0     2     1     1     0       "remap_64k"
### + ###      build_vpu   0    1     2     1     1     0       "remap_64k"
### + ###      build_vpu   1    0     2     1     1     0       "remap_64k"
### + ###      build_vpu   1    1     2     1     1     0       "remap_64k"
### + ###      
### + ###      # remap mmu(128K)
### + ###      #           sys  ch    p_s   en    re    mul     type
### + ###      build_vpu   0    0     3     1     1     0       "remap_128k"
### + ###      build_vpu   0    1     3     1     1     0       "remap_128k"
### + ###      build_vpu   1    0     3     1     1     0       "remap_128k"
### + ###      build_vpu   1    1     3     1     1     0       "remap_128k"
### + ###      
### + ###      # multi channel mmu(4K)
### + ###      #           sys  ch    p_s   en    re    mul     type
### + ###      build_vpu   0    0     0     1     0     1       "mc_4k"
### + ###      build_vpu   0    1     0     1     0     1       "mc_4k"
### + ###      build_vpu   1    0     0     1     0     1       "mc_4k"
### + ###      build_vpu   1    1     0     1     0     1       "mc_4k"
### + ###      
### + ###      # multi channel mmu(32K)
### + ###      #           sys  ch    p_s   en    re    mul     type
### + ###      build_vpu   0    0     1     1     0     1       "mc_32k"
### + ###      build_vpu   0    1     1     1     0     1       "mc_32k"
### + ###      build_vpu   1    0     1     1     0     1       "mc_32k"
### + ###      build_vpu   1    1     1     1     0     1       "mc_32k"
### + ###      
### + ###      # multi channel mmu(64K)
### + ###      #           sys  ch    p_s   en    re    mul     type
### + ###      build_vpu   0    0     2     1     0     1       "mc_64k"
### + ###      build_vpu   0    1     2     1     0     1       "mc_64k"
### + ###      build_vpu   1    0     2     1     0     1       "mc_64k"
### + ###      build_vpu   1    1     2     1     0     1       "mc_64k"
### + ###      
### + ###      # multi channel mmu(128K)
### + ###      #           sys  ch    p_s   en    re    mul     type
### + ###      build_vpu   0    0     3     1     0     1       "mc_128k"
### + ###      build_vpu   0    1     3     1     0     1       "mc_128k"
### + ###      build_vpu   1    0     3     1     0     1       "mc_128k"
### + ###      build_vpu   1    1     3     1     0     1       "mc_128k"
