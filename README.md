# Surf Path Tracer

The surf path tracer contains a CPU path tracer implementation with simple diffuse, specular, and dielectric materials.
It also contains a GPU WaveFront path tracer with the same capabilities, implemented according to the implementation detailed in
Megakernels Considered Harmful (https://research.nvidia.com/sites/default/files/pubs/2013-07_Megakernels-Considered-Harmful/laine2013hpg_paper.pdf).

SPT contains only a single test scene, it is an indoor scene with a single dielectric, and multiple diffuse and specular materials.

## Implementation details

SPT is implemented using Vulkan compute, bounce count in path tracing is not limited. Next Event Estimation, cosine weighting in the random walk, and Russian Roulette are implemented for diffuse materials.

## Requirements

SPT has the following system requirements:
- A graphics device with at least Vulkan 1.3 support and at least 1GB of VRAM

## Contributors

- Tijmen Verhoef [Portfolio](https://www.tverhoef.com)
