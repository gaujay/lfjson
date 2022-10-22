/**
 * Copyright 2022 Guillaume AUJAY. All rights reserved.
 *
 */

// 3rd-parties
#include "rapidjson/document.h"
#include "rapidjson/filereadstream.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/stringbuffer.h"

#ifndef LFJ_RAPID_ALLOCATOR_INSTRU
  #pragma message "LFJ_RAPID_ALLOCATOR_INSTRU not defined: RapidJSON needs instrumented code to be benchmarked"
#endif

// Src
#include "lfjson/lfjson.h"
#include "lfjson/HeapAllocator.h"
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
#include <iostream>

#define PRINT_MEM_DETAILS // comment for summary only


void bench_memory_lfjson(const std::vector<std::string>& filePaths)
{
  // Write document to console
  const bool printRapid = false;
  const bool printLFJ = false;
  
  for (const auto& filePath : filePaths)
  {
    std::cout << "\n------------------------------\n" << std::endl;
    std::cout << "FilePath: " << filePath << "\n" << std::endl;
    
    // RapidJSON
  #ifdef LFJ_RAPID_ALLOCATOR_INSTRU
    uint64_t rapidSize;
    uint64_t rapidCapa;
    uint64_t rapidAllocated;
    uint64_t rapidAllocPeak;
    uint64_t rapidAllocCount;
    uint64_t rapidStackValCount;
  #endif
    {
      rapidjson::Document doc;
      {
        FILE* fp = fopen(filePath.c_str(), "rb"); // non-Windows use "r"
        assert(fp != 0);
        
        char readBuffer[65536];
        rapidjson::FileReadStream is(fp, readBuffer, sizeof(readBuffer));
        
        doc.ParseStream(is);
         
        fclose(fp);
      }
      
    #ifdef LFJ_RAPID_ALLOCATOR_INSTRU
      const auto* baseAlloc  = doc.GetAllocator().baseAllocator();
      const auto& stackAlloc = doc.stackAllocator();
      const auto& stack = doc.stack();
      rapidSize = doc.GetAllocator().Size();
      rapidCapa = doc.GetAllocator().Capacity();
      rapidAllocated  = baseAlloc->getAllocated()  + stackAlloc.getAllocated();
      rapidAllocPeak  = baseAlloc->getAllPeak();
      rapidAllocCount = baseAlloc->getAllocCount() + stackAlloc.getAllocCount();
      rapidStackValCount = stack.getValCount();
    #ifdef PRINT_MEM_DETAILS
      std::cout << "RapidJSON" << std::endl;
      std::cout << "-> size: " << rapidSize << std::endl;
      std::cout << "-> capa: " << rapidCapa << std::endl;
      std::cout << "-> stackPeak:  " << stackAlloc.getAllocPeak() << std::endl;
      std::cout << "-> stackCount: " << stackAlloc.getAllocCount() << std::endl;
      std::cout << "-> allocated:  " << rapidAllocated << std::endl;
      std::cout << "-> allPeak:    " << rapidAllocPeak << std::endl;
      std::cout << "-> allocCount: " << rapidAllocCount << std::endl;
      std::cout << "-> valCount:   " << rapidStackValCount << std::endl << std::endl;
    #endif  // PRINT_MEM_DETAILS
    #else
      std::cout << "RapidJSON" << std::endl;
      std::cout << "LFJ_RAPID_ALLOCATOR_INSTRU not defined: RapidJSON needs instrumented code to be benchmarked" << std::endl << std::endl;
    #endif  // LFJ_RAPID_ALLOCATOR_INSTRU
      
      if (printRapid)
      {
        rapidjson::StringBuffer buffer;
        rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
        writer.SetIndent(' ', 2);
        doc.Accept(writer);
        
        std::cout << "RapidJson print:" << std::endl;
        std::cout << buffer.GetString() << std::endl;
      }
    }
    
    // LFJSON
    {
      uint64_t stackCapa = 0u;
      const uint16_t ChunkSize = 32768u;
      Document<ChunkSize, HeapAllocator> doc;
      auto handler = doc.makeHandler();
      {
        FILE* fp = fopen(filePath.c_str(), "rb"); // non-Windows use "r"
        assert(fp != 0);
        
        char readBuffer[65536];
        rapidjson::FileReadStream is(fp, readBuffer, sizeof(readBuffer));
        
        RapidHandler<ChunkSize, HeapAllocator> rapidHandler(handler);
        rapidjson::Reader reader;
        
        reader.Parse(is, rapidHandler);
        
        stackCapa = handler.stackCapacity();
        handler.finalize(); // shrink doc + release stack
        
        fclose(fp);
      }
      
      const auto& opa = doc.objectAllocator();
      const auto& spa = doc.stringPool();
    #ifdef LFJ_HEAPALLOCATOR_INSTRUMENTED
      const auto& alc = doc.objectAllocator().callocator();
      uint64_t allocated    = alc.getAllocated();
      uint64_t allocPeak    = alc.getAllocPeak();
      uint64_t allocCount   = alc.getAllocCount();
    #endif
      uint64_t opaChunks       = opa.chunksCount();
      uint64_t opaFallbacks    = opa.countFallbacks();
      uint64_t opaAvail        = opa.countDirectAvailable();
      
      uint64_t spaItems        = spa->size();
      uint64_t spaStringsLen   = spa->count_strings_length();
      uint64_t spaBuckets      = spa->bucket_count();
      uint64_t spaUsedBuckets  = spa->count_used_buckets();
      uint64_t spaMaxChaining  = spa->count_max_chaining();
      float    spaMeanChaining = spa->count_mean_chaining();
      uint64_t spaChunks       = spa->stringPoolAllocator().chunksCount();
      uint64_t spaFallbacks    = spa->stringPoolAllocator().countFallbacks();
      uint64_t spaAvail        = spa->stringPoolAllocator().countDirectAvailable();
      uint64_t spaDCells       = spa->stringPoolAllocator().countDeadCells();
      uint64_t spaDead         = spa->stringPoolAllocator().totalDead();
    #ifdef LFJ_STRINGPOOL_INSTRUMENTED
      uint64_t dedupSKeys      = spa->dedupShortKeys;
      uint64_t dedupLKeys      = spa->dedupLongKeys;
      uint64_t dedupLVals      = spa->dedupLongVals;
      uint64_t dedupKLen       = spa->dedupKeyLen;
      uint64_t dedupVLen       = spa->dedupValLen;
    #endif
    #ifdef LFJ_HANDLER_DEBUG
      uint64_t valCount        = handler.parsedValuesCount();
    #endif
      
    #ifdef PRINT_MEM_DETAILS
      std::cout << "LFJSON" << std::endl;
    #ifdef LFJ_HEAPALLOCATOR_INSTRUMENTED
      std::cout << "-> allocated:  " << allocated << std::endl;
      std::cout << "-> allocPeak:  " << allocPeak << std::endl;
      std::cout << "-> allocCount: " << allocCount << std::endl;
    #endif
      std::cout << "-> stackCapa:  " << stackCapa << std::endl;
    #ifdef LFJ_HANDLER_DEBUG
      std::cout << "-> valCount:   " << valCount << std::endl << std::endl;
    #endif
      
      std::cout << "-> opaChunks:    " << opaChunks << std::endl;
      std::cout << "-> opaFallbacks: " << opaFallbacks << std::endl;
      std::cout << "-> opaAvail:     " << opaAvail << std::endl << std::endl;
      
      std::cout << "-> spaItems:        " << spaItems << std::endl;
      std::cout << "-> spaStringsLen:   " << spaStringsLen << std::endl;
      std::cout << "-> spaBuckets:      " << spaBuckets << std::endl;
      std::cout << "-> spaUsedBuckets:  " << spaUsedBuckets << std::endl;
      std::cout << "-> spaMaxChaining:  " << spaMaxChaining << std::endl;
      std::cout << "-> spaMeanChaining: " << spaMeanChaining << std::endl;
      std::cout << "-> spaChunks:       " << spaChunks << std::endl;
      std::cout << "-> spaFallbacks:    " << spaFallbacks << std::endl;
      std::cout << "-> spaAvail:        " << spaAvail << std::endl;
      std::cout << "-> spaDCells:       " << spaDCells << std::endl;
      std::cout << "-> spaDead:         " << spaDead << std::endl << std::endl;
      
    #ifdef LFJ_STRINGPOOL_INSTRUMENTED
      std::cout << "-> dedupSKeys:  " << dedupSKeys << std::endl;
      std::cout << "-> dedupLKeys:  " << dedupLKeys << std::endl;
      std::cout << "-> dedupLVals:  " << dedupLVals << std::endl;
      std::cout << "-> dedups:      " << dedupSKeys + dedupLKeys + dedupLVals << std::endl;
      std::cout << "-> dedupKLen:   " << dedupKLen << std::endl;
      std::cout << "-> dedupVLen:   " << dedupVLen << std::endl;
      std::cout << "-> dedupLen:    " << dedupKLen + dedupVLen << std::endl << std::endl;
    #endif
    #endif  // PRINT_MEM_DETAILS
      
    #ifndef PRINT_MEM_DETAILS
      std::cout << "Allocated" << std::endl;
      std::cout << "-> RapidJSON mem:  "  << rapidAllocated / 1000. << " KBytes" << std::endl;
      std::cout << "-> RapidJSON peak: "  << rapidAllocPeak / 1000. << " KBytes" << std::endl;
      std::cout << "-> LFJSON mem:     "  << allocated / 1000. << " KBytes" << std::endl;
      std::cout << "-> LFJSON peak:    "  << allocPeak / 1000. << " KBytes" << std::endl << std::endl;
    #endif
      
    #if defined(LFJ_HEAPALLOCATOR_INSTRUMENTED) && defined(LFJ_RAPID_ALLOCATOR_INSTRU)
      std::cout << "Difference" << std::endl;
      double ratio1 = 100. - (allocated * 100. / (double)rapidAllocated);
      double ratio2 = (rapidAllocated * 100. / (double)allocated) - 100.;
      std::cout << "-> gain:  "  << ratio1 << "% mem" << std::endl;
      std::cout << "-> spare:  " << ratio2 << "% mem" << std::endl;
      std::cout << "-> delta:  " << ((int64_t)rapidAllocated - (int64_t)allocated) / 1000. << " KBytes" << std::endl;
    #endif
      
      // Print
      if (printLFJ)
      {
        std::cout << "LFJson print:" << std::endl;
        RapidWriter::printDoc(doc.croot());
      }
    }
  }
}
