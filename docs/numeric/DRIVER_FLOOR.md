# The driver floor under Omega's GPU path

Ledger L101, 2026-10-19. Question (Drake): what do we still use that we did not write, and can it be replaced?

Short answer: there is no driver container and no CUDA library. Everything Omega runs on the GPU goes through our own C client (`nvrm/nvrm.c`, 810 lines, plus 379 lines in `m16/`). That client talks to **two** NVIDIA kernel modules, not one: `nvidia.ko` (the resource manager) and `nvidia-uvm.ko` (the unified memory driver). Behind `nvidia.ko` sits the **GSP firmware**, a program that runs on the GPU's own processor and does the real setup work. After setup, work submission is two plain memory stores from user space and the kernel is not involved.

Terms used below: RM = resource manager, the part of the NVIDIA driver that owns GPU objects. GSP = the GPU's own system processor. FSP = the security processor that starts the GSP. RPC = a message from the kernel module to the GSP firmware. GPFIFO = the command queue. QMD = the launch descriptor. UVM = NVIDIA's unified memory driver. Tags: OBSERVED (seen on this machine), DOCUMENTED (read in source, with file:line), INFERRED (reasoned from source), UNKNOWN.

Pins: our side = `aien-dev/physics` at 14273c5 (`nvrm/nvrm.c` is quoted by line). Their side = `NVIDIA/open-gpu-kernel-modules` tag 580.173.02 (HEAD 20e4e6e), cloned read-only at `~/workspace/_ext/open-gpu-kernel-modules-580.173.02`. Running module OBSERVED: `modinfo nvidia` = 580.173.02, Dual MIT/GPL; `/proc/driver/nvidia/gpus/*/information` shows "GPU Firmware: 580.173.02", so the GSP is running. Nothing was built, loaded or changed.

Paths in the "kernel path" column are relative to the clone root. "Lines" is the length of the named entry function only; the functions it calls are not counted, so real cost is higher. Where a count would be a guess it says "not counted".

## 1. What we call, and what the kernel does for it

The brief expected 8 ioctls (calls into the kernel). That is the `nvidia.ko` half. We also make 11 calls into `nvidia-uvm.ko`, listed in 1b, because the channel will not run without them (section 2).

### 1a. ioctls on /dev/nvidiactl and /dev/nvidia0 (nvidia.ko)

All enter at `nvidia_ioctl` (`kernel-open/nvidia/nv.c:2376`, 410 lines) and the RM escapes are handled in `src/nvidia/arch/nvalloc/unix/src/escape.c`.

| Call | What we use it for (nvrm.c) | Kernel path | Ends in GSP RPC? | Lines |
|---|---|---|---|---|
| NV_ESC_CARD_INFO | find the GPU id and device number, :235 | nv.c:2523 then `nvidia_read_card_info` (nv.c:2325) | No, kernel only | not counted |
| NV_ESC_REGISTER_FD | tie /dev/nvidia0 to our client, :156 | escape.c:844 | No, kernel only | not counted |
| NV_ESC_RM_ALLOC | create every object in 1c, :76 | escape.c:381 then `Nv04AllocWithSecInfo` then resource server | Depends on the class, see 1c | 51 here, constructors below |
| NV_ESC_RM_CONTROL | all controls in 1d, :138 | escape.c:716 then `Nv04ControlWithSecInfo` | Depends on the command, see 1d | 64 here |
| NV_ESC_RM_FREE | free any object, :96 | escape.c:432 | Yes for objects that were sent to the GSP: `rpcRmApiFree_GSP` (`rpc.c:11311`) | 23 here |
| NV_ESC_RM_MAP_MEMORY | map GPU registers (usermode page) and our buffers into the process, :172 | escape.c:507 then `rm_create_mmap_context` | No, kernel only (page-table setup in the Linux process) | 43 here |
| NV_ESC_RM_MAP_MEMORY_DMA | give a buffer its GPU address, :435 | escape.c:624 then `virtmemMapTo_IMPL` (`mem_mgr/virtual_mem.c:1206`) | Yes: `NV_RM_RPC_MAP_MEMORY_DMA` at virtual_mem.c:1551 (only when the memory was registered with the firmware, `bRpcAlloc`), RPC id MAP_MEMORY_DMA (14) | 16 here |
| NV_ESC_RM_UNMAP_MEMORY_DMA | undo it, :124 | escape.c:640 then virtual_mem.c:1892 | Yes: UNMAP_MEMORY_DMA (15) | 16 here |

### 1b. ioctls on /dev/nvidia-uvm (nvidia-uvm.ko, 140,022 lines in `kernel-open/nvidia-uvm`)

Dispatch table at `kernel-open/nvidia-uvm/uvm.c:1003-1029`. These are kernel-only on the host side; UVM then calls into `nvidia.ko` through an internal interface (for example `nvUvmInterfaceRetainChannel`, `uvm_user_channel.c:141`, and `nvUvmInterfaceBindChannelResources`, :467). Whether those interface calls reach the GSP: UNKNOWN (not traced).

| Call (nvrm.c line) | What we use it for |
|---|---|
| UVM_INITIALIZE (:227), UVM_MM_INITIALIZE (:231) | open a UVM session for the process |
| UVM_REGISTER_GPU (:313), UVM_REGISTER_GPU_VASPACE (:321) | tell UVM about the GPU and our address space |
| UVM_CREATE_EXTERNAL_RANGE (:423), UVM_MAP_EXTERNAL_ALLOCATION (:450) | tie each buffer to its GPU address, once per buffer |
| UVM_FREE (:112) | undo the two above |
| UVM_REGISTER_CHANNEL (:661) | bind the channel's context buffers; required before scheduling |
| UVM_UNREGISTER_CHANNEL (:682), UVM_UNREGISTER_GPU_VASPACE (:776), UVM_UNREGISTER_GPU (:783) | teardown |

### 1c. RM classes we allocate (through NV_ESC_RM_ALLOC)

How to read the "GSP" column: on this GPU the kernel module runs as a "GSP client" (`rpc_common.c:76`): the kernel keeps its own copy of each object and, for most, also asks the firmware to create the real one. Class table: `src/nvidia/src/kernel/rmapi/resource_list.h`.

| Class (value) | Use (nvrm.c) | Kernel class and path | GSP RPC | Lines |
|---|---|---|---|---|
| NV01_ROOT_CLIENT | our client handle, :223 | RmClientResource, resource_list.h:85 | No (kernel only) | not counted |
| NV01_DEVICE_0 (0x80) | the GPU as an object, :255 | Device, `gpu/device.c:65` | Yes: `NV_RM_RPC_ALLOC_SHARE_DEVICE` (device.c:185) becomes a GSP_RM_ALLOC (103) | 131 |
| NV20_SUBDEVICE_0 (0x2080) | the GPU's control surface, :258 | Subdevice, `gpu/subdevice/subdevice.c:51` | Yes: `NV_RM_RPC_ALLOC_SUBDEVICE` (subdevice.c:124, rpc.h:367) becomes GSP_RM_ALLOC | 81 |
| NV01_MEMORY_VIRTUAL (0x70) | the GPU address range buffers are mapped into, :262 | VirtualMemoryRange, `mem_mgr/virtual_mem.c:292`; RPC flag set at `virt_mem_range.c:134` | Yes: GSP_RM_ALLOC | not counted |
| NV01_MEMORY_SYSTEM (0x3E) | every buffer (GB10 has no separate video memory), :506 | SystemMemory, `mem_mgr/system_mem.c:144` | Yes: `NV_RM_RPC_ALLOC_MEMORY` (system_mem.c:490) sends the page list, RPC id ALLOC_MEMORY (4) | 382 |
| HOPPER_USERMODE_A (0xC661) | the doorbell page, :281 | UserModeApi, `gpu/fifo/usermode_api.c:33` | No (kernel only, hands out a register mapping) | 73 |
| FERMI_VASPACE_A (0x90F1) | the GPU address space, :300 | VaSpaceApi, `gpu/mem_mgr/vaspace_api.c:56` and `mem_mgr/gpu_vaspace.c:470` | Yes in the normal case (vaspace_api.c:227-245 sends unless a fabric condition skips it) | 383 + 326 |
| KEPLER_CHANNEL_GROUP_A (0xA06C) | the channel group, :620 | KernelChannelGroupApi, `gpu/fifo/kernel_channel_group_api.c:49` | Yes: `NV_RM_RPC_ALLOC_OBJECT` (kernel_channel_group_api.c:428) | 523 |
| FERMI_CONTEXT_SHARE_A (0x9067) | the sub-context, :625 | KernelCtxShareApi, resource_list.h:406 | Yes, generic: flag `RS_FLAGS_ALLOC_RPC_TO_ALL` (alloc_free.c:766,871) | not counted |
| BLACKWELL_CHANNEL_GPFIFO_A (0xC96F) | the command-queue channel, :642 | KernelChannel, `gpu/fifo/kernel_channel.c:133` | Yes: `NV_RM_RPC_ALLOC_CHANNEL` (kernel_channel.c:2838) | 1,011 |
| BLACKWELL_COMPUTE_B (0xCEC0) | the compute engine object bound to the channel, :643 | KernelGraphicsObject, `gpu/gr/kernel_graphics_object.c:286` | Yes, generic: `RS_FLAGS_CHANNEL_DESCENDANT_COMMON` includes RPC_TO_ALL (`resource_desc_flags.h:45`) | 76 |

### 1d. RM control commands (through NV_ESC_RM_CONTROL)

A control routed to the firmware carries the flag `RMCTRL_FLAGS_ROUTE_TO_PHYSICAL` (0x40, `src/nvidia/inc/kernel/rmapi/control.h:233`). The flags below come from the export tables in `src/nvidia/generated/g_*_nvoc.c`.

| Command | Use (nvrm.c) | Kernel function | Flags | GSP RPC | Lines |
|---|---|---|---|---|---|
| NV0000_CTRL_CMD_GPU_GET_ID_INFO_V2 | device number from GPU id, :248 | `cliresCtrlCmdGpuGetIdInfoV2_IMPL` `rmapi/client_resource.c:1405` | 0x109 | No | 10 |
| NV0080_CTRL_CMD_GPU_GET_CLASSLIST | which classes this GPU supports, :267/:271 | `deviceCtrlCmdGpuGetClasslist_IMPL` `gpu/device_ctrl.c:56` | 0x10b | No | 13 |
| NV2080_CTRL_CMD_GR_GET_INFO | SM version, :292 | `subdeviceCtrlCmdKGrGetInfo_IMPL` in `gpu/gr/kernel_graphics.c` | 0x118 | No by flags; internal use of static info not traced | not counted |
| NV2080_CTRL_CMD_GPU_GET_GID_INFO | GPU UUID, :306 | `subdeviceCtrlCmdGpuGetGidInfo_IMPL` `gpu/subdevice/subdevice_ctrl_gpu_kernel.c:3627` | 0x10118 | No | 30 |
| NVC36F_CTRL_CMD_GPFIFO_GET_WORK_SUBMIT_TOKEN | the doorbell value, :646 | `kchannelCtrlCmdGpfifoGetWorkSubmitToken_IMPL` `gpu/fifo/kernel_channel.c:3302` | 0x10008 | No on bare metal (RPC branch is for virtual GPUs only, kernel_channel.c:3319-3333) | 67 |
| NVA06C_CTRL_CMD_GPFIFO_SCHEDULE | put the channel on the GPU's run list, :668 | `kchangrpapiCtrlCmdGpFifoSchedule_IMPL` `gpu/fifo/kernel_channel_group_api.c:1065` | 0x10008 | **Yes**: `NV_RM_RPC_CONTROL` under `IS_GSP_CLIENT` (kernel_channel_group_api.c:1176-1190), sent as GSP_RM_CONTROL (76) | 142 |

The work-submit token itself is plain arithmetic: runlist id and channel id packed into one word (`kfifoGenerateWorkSubmitTokenHal_GB100`, `gpu/fifo/arch/blackwell/kernel_fifo_gb100.c:89`). INFERRED that GB10 uses this version; the dispatch was not traced.

## 2. The floor: what the GSP firmware does that nothing on the host can do

- **Which file.** The GB10 loads `gsp_ga10x.bin` (75,012,080 bytes, `/lib/firmware/nvidia/580.173.02/`). DOCUMENTED: `src/nvidia/arch/nvalloc/common/inc/nv-firmware.h:92-99` maps `NV_FIRMWARE_CHIP_FAMILY_GB10X` (and GB10Y, GB20X, Hopper, Ada, GA10X) to `gsp_ga10x`. `gsp_tu10x.bin` (30,471,256 bytes) is for Turing/GA100 and is not used here. OBSERVED that both files are present; UNKNOWN which one the running module actually opened (no trace taken; the source mapping says ga10x).
- **What it does.** DOCUMENTED by the call list in 1a-1d: the kernel module forwards creation of the device, subdevice, memory objects, address space, channel group, sub-context, channel and compute object to the firmware, and forwards the run-list scheduling command. That is: page tables for the GPU address space, memory registration, channel and engine context setup, and run-list scheduling. These live in firmware because the GPU's hardware only accepts those writes from the GSP. INFERRED from the design (kernel keeps a mirror object, firmware owns the hardware state); the exact register-level split is not documented in the open source, which contains the kernel half only. UNKNOWN: a line-by-line list of firmware duties, since the firmware is a binary.
- **Why it cannot be replaced.** The kernel does not verify the firmware. It finds the signature inside the firmware file (`kernel_gsp.c:4850-4880`, signature copied into a buffer at `kernel_gsp.c:4684-4716`), takes the boot image, its hash, its signature and public key from tables built into `nvidia.ko` (`gpu/fsp/arch/hopper/kern_fsp_gh100.c:1020-1065`), and sends them to the FSP (`kfspSendBootCommands_GH100`, kern_fsp_gh100.c:1512; flow at `gpu/gsp/arch/hopper/kernel_gsp_gh100.c:782-866`). The FSP and the GSP's RISC-V boot ROM do the check (the boot-ROM registers are programmed at kernel_gsp_gh100.c:707-754: image addresses and the "PKC" parameter block). DOCUMENTED that the kernel hands it over; "not found" for any signature check inside the kernel source (the only "verifies" comment is `kernel_gsp.c:4070`, about the older Booter path). Consequence, INFERRED: only NVIDIA-signed firmware starts, so the firmware cannot be replaced by us. The reason is hardware: the check runs in the chip's own boot logic, which no host code can change. The boot layout for this integrated GPU (no video memory, no VGA or PMU reservation) is `gpu/gsp/arch/blackwell/kernel_gsp_gb10b.c:60-90`.

## 3. The submit path after setup

Our code (`nvrm.c:720-745`): write one 8-byte entry into the GPFIFO ring in shared memory (:730), a store barrier (`dsb sy`), write the new put position into the channel's control block (:733), another barrier, then one 32-bit store of the work-submit token to the doorbell register page we mapped at setup (`NVC361_NOTIFY_CHANNEL_PENDING`, offset 0x90, `class/clc361.h:33`; the store is `nvrm_ring`, :743). Completion is detected by the host polling a marker word in uncached memory (`m16/m16_native.c:97-108`).

Between the doorbell store and the chip running our words, the steps are all in hardware: the GPU front end sees the token and picks the channel; its fetch unit reads the GPFIFO entry, then the command buffer; the compute engine decodes our methods, reads the QMD we built and runs our instruction words. The kernel is **not** involved. OBSERVED/DOCUMENTED evidence: (a) `nvrm_ring` is a plain store with no system call and no ioctl; (b) the doorbell page is a direct hardware mapping set up once by `NV_ESC_RM_MAP_MEMORY` (escape.c:507) and the Linux process then writes to it like memory; (c) the token is just numbers computed once (section 1d). We did not trace what the GPU hardware does internally after the doorbell; that part is described from the class headers and is INFERRED. One caveat: while the channel runs, the GSP firmware remains the owner of scheduling state; INFERRED that it is not on the per-launch path (no per-launch message exists in our code or in the call list).

## 4. Verdict per call, and what a minimal kernel module would hold

| Piece | Verdict | Reason |
|---|---|---|
| Doorbell store, GPFIFO ring write, QMD, command words, completion polling | ALREADY OURS | `nvrm.c:720-745`, `m16/`, Omega `src/omega_blackwell_submit.c` |
| Address assignment and CPU/GPU same-address layout (`va_take`, fixed `mmap`) | ALREADY OURS | `nvrm.c:326-415`, :485 |
| NV_ESC_CARD_INFO, GET_ID_INFO_V2, GET_CLASSLIST, GET_GID_INFO, GR_GET_INFO, GET_WORK_SUBMIT_TOKEN | REPLACE (small) | answered by the kernel from tables or arithmetic, no firmware; a minimal module could return fixed answers for one GPU model. Cost is small; benefit is also small |
| NV_ESC_REGISTER_FD, NV_ESC_RM_MAP_MEMORY (usermode page and buffers) | REPLACE | host work: a Linux file handle and page mapping. Needs PCI base address and bus mapping code, which is ours to write |
| UVM (all 11 calls) | REPLACE, candidate | UVM is needed only because we create an "externally owned" address space with page faulting (`nvrm.c:299`); the RM refuses to schedule such a channel until UVM binds it (`kernel_channel_group_api.c:1092-1105`, flag handling `gpu_vaspace.c:511`). INFERRED, UNTESTED: allocating a normal RM-owned address space would remove UVM. Not measured; no measurement was allowed in this task |
| Object creation (device, subdevice, memory, address space, group, sub-context, channel, compute object) | KEEP | each creates state inside the firmware that only the firmware can write (section 2) |
| NV_ESC_RM_MAP_MEMORY_DMA / UNMAP_MEMORY_DMA, SCHEDULE | KEEP | each ends in a firmware message (MAP_MEMORY_DMA 14, GSP_RM_CONTROL 76) that edits GPU page tables or the run list |
| GSP firmware and its signed boot (FSP) | KEEP | hardware boot check, section 2 |

**A minimal module serving only Omega** would have to: bind the PCI device and map its register window; allocate and pin system memory pages for buffers; load the signed `gsp_ga10x.bin` and run the boot-start sequence with the FSP exactly as `kernel_gsp_gh100.c:782-866` does (this is the largest and most risky item: the message format and register sequence are NVIDIA's); speak the GSP message queue protocol (`message_queue_cpu.c` and the `rpc.c` message layouts) well enough to send the message kinds seen in section 1 (GSP_RM_ALLOC 103, GSP_RM_CONTROL 76, free, ALLOC_MEMORY 4, MAP_MEMORY_DMA 14, UNMAP_MEMORY_DMA 15, plus the start-up messages such as GSP_SET_SYSTEM_INFO 72); handle whatever interrupts the firmware needs (UNKNOWN, not traced); and map the doorbell page. It would drop the multi-user object server, the other 90 percent of the classes, the display and power code, and UVM. Size: UNKNOWN as a number. For orientation only: the kernel half under `src/nvidia/src/kernel` is 425,539 lines and `kernel-open/nvidia` is 38,985 lines, and the part Omega touches is a small fraction, but I did not measure how small and will not guess. What it must still hand to the GSP: creation of every object in 1c, page-table edits, and scheduling. Skipped options with a named physical reason: replacing the firmware (signature check in the chip's boot logic, section 2), and moving page-table writes to the host (the firmware holds the only write path, section 2).

## 5. What is not in the path

No libcuda, no CUDA runtime, no cubin or ELF kernels, no PTX. The `nvrm.h` header comment says so (lines 3-9) and Omega's gates prove it at run time: `omega_blackwell_verify_zero_libcuda_linkage` (`omega/src/omega_blackwell_submit.c:588`, runs `ldd` on the running binary and rejects libcuda or libcudart), `omega_blackwell_verify_zero_cuda_symbols` (:611, runs `nm -u`), and `omega_blackwell_verify_zero_libcuda_runtime` (:637, scans `/proc/self/maps`), reported as `OMEGA_BW_VECTOR_ZERO_LIBCUDA_LINK_PASS`, `..._ZERO_CUDA_SYMBOL_PASS` and `..._ZERO_LIBCUDA_RUNTIME_PASS` in `omega/src/omega_blackwell_gates.c:299-307, 380-382`. I read these gates; I did not run them.

## Corrections to the starting finding

- Two kernel modules, not one (UVM is the second).
- 8 ioctls on `nvidia.ko` as expected, plus 11 UVM ioctls, 3 classes beyond the listed set (FERMI_VASPACE_A, KEPLER_CHANNEL_GROUP_A, FERMI_CONTEXT_SHARE_A) plus BLACKWELL_COMPUTE_B, and one extra control (NV0000_CTRL_CMD_GPU_GET_ID_INFO_V2).
- The kernel does not check the firmware signature; the chip's boot logic does.
