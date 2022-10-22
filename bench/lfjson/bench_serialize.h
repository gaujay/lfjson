/**
 * Copyright 2022 Guillaume AUJAY. All rights reserved.
 *
 */

// 3rd-parties
#include "rapidjson/document.h"
#include "rapidjson/filereadstream.h"
#include "rapidjson/stringbuffer.h"

// Src
#include "lfjson/lfjson.h"
using namespace  lfjson;

// Utils
#include "bench_utils.h"

// Std
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <cassert>
#include <string>
#include <vector>
#include <chrono>
#include <iostream>
#include <fstream>
#include <algorithm>

#define SERIALIZE_MAIN_LOOPS    5
static_assert(SERIALIZE_MAIN_LOOPS  > 0, "SERIALIZE_MAIN_LOOPS <= 0");
#define SERIALIZE_INNER_LOOPS   1000  // ensure min time Vs clock resolution
static_assert(SERIALIZE_INNER_LOOPS > 0, "SERIALIZE_INNER_LOOPS <= 0");


void bench_serialize(const std::vector<std::string>& filePaths)
{
  // Use pretty writer
  const bool prettyOutput = false;
  
  for (const auto& filePath : filePaths)
  {
    std::cout << "\n------------------------------\n" << std::endl;
    std::cout << "FilePath: " << filePath << "\n" << std::endl;
    
    // Read file to memory
    std::ifstream ifs(filePath, std::ifstream::in);
    assert(ifs.good());
    std::string json(std::istreambuf_iterator<char>{ifs}, {});
    
    // RapidJSON
    std::vector<double> rapidTimes;
    rapidTimes.reserve(SERIALIZE_MAIN_LOOPS);
    
    {
      rapidjson::Document doc;
      doc.Parse(json.c_str());
      
      for (int i = 0; i < SERIALIZE_MAIN_LOOPS; ++i)
      {
        auto start = std::chrono::high_resolution_clock::now();
        
        for (int j = 0; j < SERIALIZE_INNER_LOOPS; ++j)
        {
          rapidjson::StringBuffer buffer;
          if (prettyOutput)
          {
            rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
            writer.SetIndent(' ', 2);
            doc.Accept(writer);
          }
          else
          {
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            doc.Accept(writer);
          }
          
          if (buffer.GetSize() == 0u)
            exit(1);
        }
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> diff = end - start;
        rapidTimes.push_back(diff.count() * 1000.);
      }
    }
    
    // LFJSON
    std::vector<double> lfTimes;
    lfTimes.reserve(SERIALIZE_MAIN_LOOPS);
    
    {
      DynamicDocument doc;
      auto handler = doc.makeHandler();
      RapidHandler<> rapidHandler(handler);
      
      rapidjson::Reader reader;
      rapidjson::StringStream ss(json.c_str());
      
      reader.Parse(ss, rapidHandler);
      handler.finalize();
      
      for (int i = 0; i < SERIALIZE_MAIN_LOOPS; ++i)
      {
        auto start = std::chrono::high_resolution_clock::now();
        
        for (int j = 0; j < SERIALIZE_INNER_LOOPS; ++j)
        {
          rapidjson::StringBuffer buffer;
          if (prettyOutput)
          {
            RapidWriter::prettyWrite(buffer, doc.croot(), ' ', 2);
          }
          else
          {
            RapidWriter::write(buffer, doc.croot());
          }
          
          if (buffer.GetSize() == 0u)
            exit(1);
        }
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> diff = end - start;
        lfTimes.push_back(diff.count() * 1000.);
      }
    }
    
    // Results
    std::sort(rapidTimes.begin(), rapidTimes.end());
    std::sort(lfTimes.begin(),    lfTimes.end());
    
    double rapidMedian = rapidTimes[(rapidTimes.size() - 1) / 2];
    double lfMedian    = lfTimes[(lfTimes.size() - 1) / 2];
    
    double fastRatio   = 100. - (lfTimes[0] * 100. / (double)rapidTimes[0]);
    double medianRatio = 100. - (lfMedian   * 100. / (double)rapidMedian);
    
    std::cout << "Serialize" << std::endl;
    std::cout << "-> RapidJSON fastest: " << rapidTimes[0] << " ms" << std::endl;
    std::cout << "-> LFJSON fastest:    " << lfTimes[0]    << " ms" << std::endl;
    std::cout << "-> Fastest diff:      " << fastRatio     << " %"  << std::endl;
    std::cout << "-> RapidJSON median: " << rapidMedian << " ms" << std::endl;
    std::cout << "-> LFJSON median:    " << lfMedian    << " ms" << std::endl;
    std::cout << "-> Median diff:      " << medianRatio << " %"  << std::endl;
  }
}
