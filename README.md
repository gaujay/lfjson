# Light-foot JSON


A memory-optimized and data-oriented JSON library written in C++.

Includes benchmarks and Google Test support.

### Features

- 16-Bytes JSON values (12 on x86)
- 24-Bytes JSON members (16 on x86)
- Short string optimization up to 14 characters (10 on x86)
- Specialized array types for bool, int64_t and double
- Custom StringPool for string deduplication
  - based on an optimized intrusive hash table
  - shareable for easy reuse on consecutive parsing
  - with zero-copy support for const strings
- Allocator-aware PoolAllocator for memory management
  - designed as a slab allocator with dead-cells recycling
  - composable with Heap and StackAllocator
- Fast and easy-to-use reference-based API

### Notes

This library doesn't include a JSON parser and relies on external 3rd-party for this purpose.
[RapidJSON](https://github.com/Tencent/rapidjson) is integrated as a default backend to demonstrate usage in the benchmark example.

### Benchmark results

Benchmark configuration:
- Platform: Windows 10 64-bits, i7-10875h
- 3rd-parties: xxHash 0.8.1, RapidJSON 1.1.0
- Compiler: MinGW 8.1, -O2 -DNDEBUG

Notes:
- Memory benchmarks were done with default chunk size for both libraries (RapidJSON: 65K, LFJSON: 2*32K)
- Speed benchmarks were done with MinGW-64 on Windows 10. Results may vary depending on platform/compiler.

![RelativeMem_x64](doc/RelativeMem_x64.png)
![RelativeMem_x86](doc/RelativeMem_x86.png)
![RelativeSpeed_Deserialize](doc/RelativeSpeed_Deserialize.png)
![RelativeSpeed_Serialize](doc/RelativeSpeed_Serialize.png)

### Dependencies

This project uses git submodule to include Google Test repository:

    $ git clone https://github.com/gaujay/lfjson.git
    $ cd lfjson
    $ git submodule init && git submodule update

LFJSON also uses [xxHash](https://github.com/Cyan4973/xxHash) as an optional 3rd-party to speed up its StringPool hashing requirements.
To disable this dependency, just set the flag `LFJ_NO_XXHASH`.

### Building

Support GCC/MinGW and MSVC (see 'CMakeLists.txt').

You can open 'CMakeLists.txt' with a compatible IDE or use command line:

    $ mkdir build
    $ cd build
    $ cmake ..
    $ make <target> -j

### License

LFJSON - Apache License 2.0
xxHash - BSD 2-Clause License
RapidJSON - MIT License + 3rd-parties
