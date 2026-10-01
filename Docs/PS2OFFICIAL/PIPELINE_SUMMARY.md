Technical Development Specification: 3D Pipeline, DMA Packets, VRAM Layout and VU1 Microcode for PlayStation 2

1. Introduction and Geometry Pipeline Architecture

The PlayStation 2 architecture represents a milestone in distributed parallel computing, where graphics performance does not depend on a single central processor, but on the orchestration of multiple autonomous cores. The heart of this system, the Emotion Engine (EE), delegates the geometry workload to its Vector Units (VPU), allowing the EE Core to handle high-level logic while the VPUs process massive mathematical transformations in parallel. This autonomy is fundamental: the ability of the vector unit to generate and send data independently to the rendering engine is what defines the performance ceiling of any 3D engine on this platform.

1.1. VU1 Autonomy and the GIF Interface (PATH1)

The VU1 (the operation unit of VPU1) works as the main, independent geometry engine of the system. It has a direct connection to the Graphics Synthesizer (GS) through the Graphic Interface Unit (GIF) via the dedicated bus known as PATH1. This privileged path allows the VU1 to autonomously process and transfer both GS primitives (points, triangles, lines) and Display Lists (groups of primitives specifying batches of images) directly to the drawing engine, freeing the main bus from the geometry transfer load.

1.2. Architectural Differentiation: VU0 vs. VU1

It is imperative to distinguish the role of the vector units to avoid bus contention and maximize performance:

- VU0 (COP2 Coprocessor): Closely tied to the EE Core (identified as COP2 in the instruction manual). It is used for unstructured calculations, such as physics, inverse kinematics or collision logic. Its data flow travels over PATH3, sharing bandwidth with the central processor.
- VU1 (Geometry Engine): Designed for massive vertex processing. Its independence allows it to run transformation and lighting microcode simultaneously with the game logic.

Evaluation of the Path Separation:

- Wait State Prevention: By separating the drawing flow (PATH1) from the system logic (PATH3), the central processor's wait states are eliminated during intense geometry bursts.
- Bus Contention Reduction: The VU1 can feed the GS even if the main bus is saturated by IPU or IOP transfers, keeping the refresh rate constant.
- True Parallelism: It lets the system work on three simultaneous fronts: Logic (EE Core), Geometry (VU1) and Rasterization (GS).

2. Detailed Structure of DMA Packets (DMA Chain & VIF/GIF Headers)

In a distributed memory architecture, efficiency lies in the use of Transfer Lists and Packets (data handled as logical units). These structures allow the DMA controller (DMAC) to orchestrate the massive movement of data between main memory, the SPR (Scratchpad Memory) and the video subsystems with minimal intervention from the EE Core.

2.1. Anatomy of the DMAtag (128 bits)

The DMAtag is the header that defines the size and attributes of the packet. The ADDR field is directly tied to the Address Translation logic (translation of virtual to physical addresses, vAddr to pAddr), where PSIZE is defined as 32 bits in the EE Core instruction manual.

Field Bits Technical Function
QWC 15:0 Quadword Count: Number of 128-bit units to transfer.
PCE 27:26 Priority Control: Priority management in bus arbitration.
ID 30:28 Chain command identifier (DMAC flow control).
IRQ 31 Interrupt Request: Triggers an exception when the transfer finishes.
ADDR 62:32 Physical address (pAddr) of the next element or of the data.
SPR 63 Indicates whether ADDR points to the Scratchpad Memory (SPR).

Note: Although these fields are standard to the system, the EE Core manual focuses on instruction execution, while the DMAC manages these tags at the hardware level.

2.2. Analysis of Command Identifiers (IDs)

The ID field orchestrates the continuity of the drawing flow:

- cnt: Continues to the next packet sequentially.
- call: Jumps to a transfer sub-chain (DMAC stack management).
- ret: Returns from a sub-chain after finishing the packet.
- end: Marks the absolute end of the Transfer List.

  2.3. Control Hierarchy: VIFcode and GIFtag

1. VIFcode (32 bits): Instructions for the VIF decompressor. The UNPACK command moves data to VU memory. The FLG/MSB bit manages the TOPS register, essential for double buffering (the VU1 processes one block while the VIF loads the next).
2. GIFtag (128 bits): Defines the attributes of the primitives in the GS.

- NLOOP: Element count.
- EOP: End of Packet.
- PRE/PRIM: Primitive type and drawing context.
- REGS/NREG: Destination in the GS registers (XYZF2, RGBA, ST).

3. Video Memory (VRAM) Allocation and Addressing Map

The GS VRAM is a critical 4 MB (32 Mbits) resource that requires addressing in 32-bit words (word) and strict manual alignment to guarantee buffer integrity.

3.1. Addressing and Alignment Constraints

Base pointers must meet specific alignment factors to avoid performance degradation:

- FBP (Frame Buffer Pointer): Base / 2048.
- ZBP (Z-Buffer Pointer): Base / 2048.
- TBP (Texture Buffer Pointer): Base / 64.
- CBP (CLUT Buffer Pointer): Base / 64.

  3.2. Practical Resolution and Buffer Configuration

Consumption is calculated with the formula: Width _ Height _ (BPP / 8).

- NTSC (512x448, 32-bit): 512 \times 448 \times 4 = 917,504 bytes (896 KB).
- PAL (512x512, 32-bit): 512 \times 512 \times 4 = 1,048,576 bytes (1024 KB).

Suggested Memory Layout (PSMCT32 Mode):

Buffer Resolution Math (Bytes) Start Address (Hex)
Frame Buffer 512x448 (NTSC) 917,504 0x000000
Z-Buffer 512x448 917,504 0x08C000
Texture Area Remaining ~2.2 MB 0x118000

On PAL, the 128 KB increase per buffer significantly reduces the texture area, forcing more aggressive cache management.

4. VU1 Microcode Programming and the 3D Transformation Pipeline

The VPUs use the VLIW (Very Long Instruction Word) paradigm. It is vital not to confuse EE Core instructions (MIPS I/II) with VU1 microcode. For example, the EE Core MADD instruction (Source Image 8) is a 64-bit extension for integers, whereas on the VU1, MADD is a 4-component floating-point operation.

4.1. Resources and Architecture

The VU1 operates with 32 VF registers (128 bits). VF00 is a fixed constant (0,0,0,1).

- Upper Instruction: FMAC (Floating-point Multiply-Accumulate) operations.
- Lower Instruction: LSU (Load/Store), integers, jumps and the division unit.

  4.2. Transformation Pipeline Implementation

Vertices are transformed by multiplying the projection matrix (registers VF10-VF13) by the vertex (VF16) using MULA and MADD instructions. This process happens in parallel at the instruction level, allowing the X, Y, Z, W components to be processed simultaneously.

4.3. Optimization: Software Pipelining and XGKICK

The division instruction on the VU1 has a latency of 7 cycles. While the EE Core has its own DIV1 and DIVU1 (Divide Pipeline 1) units for integers, the VU1 requires Software Pipelining to hide its latency: while the reciprocal of W is being calculated for one vertex, the transformation of the next one is started.

The pipeline culminates with the XGKICK command, which transfers the processed data from the VU1's local memory directly to the GIF for rasterization.

5. Key Hardware Optimization Rules and Techniques

5.1. DMA Alignment and the "Slice" Concept

To maximize the 2.4 GB/s bus, it is mandatory to align transfers to 128 bytes. According to the official glossary, the physical unit of DMA transfer is called a "Slice", defined as 8 qwords or fewer. Keeping data in multiples of a "Slice" increases performance by 30% to 40% by allowing uninterrupted bursts.

5.2. Graphics Synthesizer Efficiency

The GS must be operated in vertical strips of 32 pixels. This technique is crucial because the GS internal memory is organized in pages; exceeding this width causes "DRAM page breaks" and "page misses", producing severe latencies in the rasterization engine.

5.3. Process Synchronization

- Polling (DMA.STR): Useful for low-latency tasks where the EE Core actively waits for completion.
- Interrupts: Preferable for high-performance engines, allowing the EE Core to process AI or physics (using MIPS I/II instructions) while the DMA transfers the geometry "Slices".

Compliance with these specifications guarantees stability and the maximum use of the PlayStation 2 hardware.
