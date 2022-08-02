# Beam search
Repository contains tools for performing beam search algorithms.

## Installation

Repository uses cmake as its assembly and build system.

## Data structures

`CircularArrayCTCBeamSearchTree` is a beam search entries manager with its own allocator that is based upon circular array of a fixed size, its main properties are
* Container separates shared prefix part (first several nodes that has only one child) and active part (the rest)
* Active part has limited capacity which is defined in container initialization, detached part is unlimited
* For the active part there is the only allocation performed at initialization, detached part allocation is std:vector based
* Garbage collection is based upon reference counting and has amortized linear complexity in terms of number of queries to the structure, no actual deallocation is performed during the search but some number of unused entries can still be presented in the tree due to algorithm limits
