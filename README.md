# Mini Rubik's Cube Solver Using RISC-V
A 2×2×2 Rubik's Cube solver implemented in C and RISC-V Assembly, focusing on search algorithms, memory efficiency, instruction-level performance, and visualization using the Ripes simulator.

## 1. Overview
This project implements a 2×2×2 Rubik's Cube solver in both C and RISC-V Assembly.

The objective is to find the shortest sequence of moves that solves a given cube state while satisfying memory and computational constraints.

The project includes two implementations:

### 1-1. C Solver
The C implementation uses an **iterative deepening search algorithm** to find the shortest solution.

The search begins with a depth limit of zero and gradually increases the limit until a solution is found.

This approach combines the advantages of Breadth-First Search (BFS) and Depth-First Search (DFS):

- BFS: Guarantees the shortest solution but requires significant memory.
- DFS: Requires less memory but does not guarantee the shortest solution.
- Iterative Deepening: Finds the shortest solution while maintaining relatively low memory usage.

The solver searches up to a maximum depth of `11` moves, which is sufficient to solve all reachable states in the supported cube representation.

### 1.2 RISC-V Assembly Solver
The RISC-V implementation translates the cube-solving algorithm into Assembly instructions for execution in the Ripes simulator.

The implementation focuses on:
- Iterative deepening search.
- Instruction-level optimization.
- Instruction count measurement using Ripes.
- LED Matrix visualization.
- Efficient register and memory usage.
- Cube state representation and manipulation.

Two configurations are provided:

| Version | Description |
| --- | --- |
| LED Version `solver_led_optimized` | Executes the solver and visualizes cube states using the Ripes LED Matrix. |
| Measurement Version `solver_bench_optimized`| Executes the solver without LED rendering to measure retired instructions. |

The measurement version allows us to evaluate computational performance without the additional instruction overhead of visualization.


## 2. Repository Structure
The repository contains the C implementation, RISC-V Assembly implementation, validation tools, and test data.
```
minirubik
│
├── README.md
├── Makefile
│
├── solver_basic.c
├── verify_host.c
│
├── solver_led_optimized.s
├── solver_bench_optimized.s
```

File Descriptions:
|File|Description|
|---|---|
|`README.MD`|Project overview, setup instructions, and test results.|
|`Makefile`|Automates compilation and testing.|
|`solver_basic.c`|C implementation of the Rubik's Cube solver.|
|`verify_host.c`|Validates the solver and checks algorithmic properties.|
|`solver_led_optimized.s`|RISC-V Assembly solver with LED Matrix visualization.|
|`solver_bench_optimized.s`|RISC-V Assembly solver for instruction measurement.|


## 3. C Solver-Build and Usage
### 3-1. Bulid Instruction

Clone or download the repository.

Navigate to the project directory and compile the C solver using:

```text
make
```

Alternatively, specify GCC explicitly:

```text
make CC=gcc
```

After successful compilation, the executable can be used to solve cube states.


### 3-2. Input Format
Each cube state is represented by a 14-digit string.

The string consists of two parts:

|Position|Description|
|Digits 1~7|`permutation`|
|Digits 8~14|`orientation`|
Example:

21345671111111

The first seven digits describe the positions of the seven movable corner pieces.

The remaining seven digits describe their orientations.

The input uses:

Corner permutation values: 1–7
Corner orientation values: 1–3

The permutation must contain each value exactly once, and the complete state must satisfy the cube's validity constraints.

The solved state is represented as:

12345671111111
### 3-3. Execution Example
Windows PowerShell:
```
.\solver.exe 21345671111111
```

Expected output:
```text
Input: 21345671111111
Searching...
Solution: B R D2 R B R B R D2 B R
Steps: 11
Verification: PASS
```
The output includes:
- `Input`: The initial cube state.
- `Searching...`: Indicates that the search has started.
- `Solution`: The sequence of moves.
- `Steps`: The number of moves required.
- `Verification`: Confirms whether the solution produces the solved state.

The supported moves are:
```
R, R2, R'
B, B2, B'
D, D2, D'
```
Each move represents a clockwise quarter-tur(R, B, D), a half-turn(R2, B2,D2), or a counterclockwise quarter-turn of the corresponding face(R', B', D').


## 4. RISC-VAssembly Solver-Build and Usage
The Assembly implementation is executed using the Ripes RISC-V simulator.

Ripes provides an environment for observing instruction execution, processor registers, memory operations, and pipeline behavior.

### 4-1. Requirements
- Ripes Simulator
- Compatible RV32 processor configuration
- LED Matrix peripheral (for visualization)

The processor configuration must support the instructions used by the Assembly source. Enable the M extension if the program uses multiplication instructions.

### 4-2. Running the Assembly Solver
To execute the solver in Ripes:

1. Open the Ripes simulator.
2. Select a compatible RISC-V processor.
3. Open the Assembly source file.
4. Assemble the program.
5. Configure the required peripherals, ISA Simulator,.
6. Execute the program.


### 4-3. Instruction Measurement
To evaluate instruction-level performance, the measurement version is executed using Ripes CLI.

Unlike the LED version, this configuration excludes visualization instructions from the measurement.

Example using Windows PowerShell:
```text
& "C:\path\to\Ripes.exe" `
  --mode cli `
  --src "solver_bench_optimized.s" `
  -t asm `
  --proc RV32_ISS `
  --reginit "gpr:2=0x7ffffff0" `
  --iret `
  --runinfo
```
Important parameters:
|Parameter|Description|
|---|---|
|`--mode cli`|Runs Ripes in command-line mode.|
|`--src`|Specifies the Assembly source file.|
|`-t asm`|Identifies the input as Assembly code.|
|`--proc RV32_ISS`|Selects the RV32 ISA Simulator.|
|`--reginit`|Initializes a specified register.|
|`--iret`|Reports retired instructions.|
|`--runinfo`|Displays execution information.|

The retired instruction count is used to evaluate the computational cost of the Assembly implementation.

Measurements can also be performed using a compatible five-stage pipelined processor to compare simulator execution performance.


## 5. Testing Results and Documentation
### 5-1. Correctness Verification
The C solver was tested using multiple cube states, including solved states and states requiring different numbers of moves.
|Test Case|Expected Steps|C Solver|
|---|---|---|
|`12345671111111`|0|PASS|
|`25314672313211`|1|PASS|
|`21345671111111`|11|PASS|

An independent BFS verification was also used to determine the exact distance distribution of the reachable cube states.

BFS Verification Results:
```
Total reachable states: 3,674,160
Maximum solution depth: 11
Number of distance-11 states: 2,644

BFS Verification: PASS
```

### 5-2. Memory and Performance Summary
The implementation is designed to satisfy the memory and computational constraints of the assignment.

| Metric | Requirement / Result |
| --- | --- |
| Maximum solution depth | 11 moves |
| Static memory limit | 128 KiB |
| Total reachable states | 3,674,160 |
| Distance-11 states | 2,644 |
| Actual static memory usage | 108.6 KiB |
| Maximum retired instructions | 28,952,476 |
| H3 verification | PAsS |


The performance analysis focuses on:

- Static memory consumption.
- Retired instruction count.
- RV32 ISA Simulator performance.
- Five-stage pipelined processor performance.
- Search efficiency across distance-11 states.

The H3 verification checks whether all 2,644 distance-11 states can be solved within the required instruction budget of 50,000,000 retired instructions per state, using the same measurement configuration.

### 5-3. Detailed Technical Report
For more detailed explanation of the algorithms, implementation, verification methods, and performance measurements, please refer to the technical report.

Full Technical Report — [HackMD](https://hackmd.io/EIOnNnjDQBO7a03yDvGYLQ?both)





























