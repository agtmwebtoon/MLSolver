# MLSolver - Finite Element Analysis Program

## Overview

This project implements a finite element analysis (FEA) solver for simulating compressible hyperelastic materials using deal.II. The solver supports 2D and 3D simulations with various material properties and boundary conditions. It also includes support for multithreaded execution using Intel TBB.

---

## Features

- Supports 2D and 3D geometries.
- Implements compressible Neo-Hookean materials with three-field formulation.
- Includes static condensation and nonlinear solver functionality.
- Handles user-defined meshes via UCD format.
- Generates VTU output for visualization in Paraview or similar tools.

---

## Requirements

### Software Dependencies

- **deal.II Library** (v9.6.0 or higher)
- **Intel TBB**
- **CMake** (v3.13.4 or higher)
- **Clang/LLVM** (or compatible C++ compiler)

### System Requirements

- Multicore CPU for multithreaded execution (optional)
- Paraview for visualizing simulation results (optional)

---

## Building the Program

1. Clone the repository:
    ```bash
    git clone https://github.com/agtmwebtoon/MLSolver.git
    cd MLSolver
    ```

2. Create a build directory:
    ```bash
    mkdir build
    cd build
    ```

3. Configure the build system with CMake:
    ```bash
    cmake -DDEAL_II_WITH_TBB=ON -DCMAKE_BUILD_TYPE=Release \
          -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_C_COMPILER=clang \
          -GXcode ..
    ```

4. Build the program:
    ```bash
    make
    ```

---

## Usage

### Running the Solver

1. Prepare a `parameters.prm` file in the root directory. Example parameters can be found in `examples/parameters.prm`.

2. Run the program:
    ```bash
    ./mlsolver
    ```

### Input Format

- For custom meshes, place a UCD file (e.g., `output_sample.ucd`) in the root directory.

---

## Output

- Simulation results are written as `.vtu` files (e.g., `solution-3d-0.vtu`).
- These files can be visualized using Paraview.

---

## Examples

### Example Parameter File

```text
end_time = 10.0
delta_t = 0.1
scale = 1.0
global_refinement = 2
poly_degree = 2
quad_order = 3
mu = 1.0
nu = 0.3
