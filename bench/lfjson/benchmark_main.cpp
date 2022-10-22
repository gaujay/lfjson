/**
 * Copyright 2022 Guillaume AUJAY. All rights reserved.
 *
 */

#define _CRT_SECURE_NO_WARNINGS     // Windows fopen warning
#define LFJ_STRINGPOOL_INSTRUMENTED // LFJSON instrumented string pool

#include "bench_utils.h"
#include "bench_memory.h"
#include "bench_deserialize.h"
#include "bench_serialize.h"

#include <string>
#include <vector>


int main()
{
  const bool benchMemory      = true;
  const bool benchDeserialize = false;
  const bool benchSerialize   = false;
  
  // Input files to parse
  const std::string folderPath = BENCH_EXAMPLES_DIR;
  
  const std::vector<std::string> filePaths = {
    folderPath + "twitter.json",
    folderPath + "numbers.json",
    folderPath + "bool_array.json",
    folderPath + "github_events.json"
  };
  
  // Run
  if (benchMemory)
    bench_memory_lfjson(filePaths);
  
  if (benchDeserialize)
    bench_deserialize(filePaths);
  
  if (benchSerialize)
    bench_serialize(filePaths);
  
  return 0;
}
