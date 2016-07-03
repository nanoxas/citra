// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <asmjit/asmjit.h>

#include "common/memory_util.h"
#include "core/arm/asmjit/jit_runtime.h"

JIT_Runtime::JIT_Runtime(size_t size) {
    // try to avoid low memory addresses to make the memory contiguous
    base_address = &AllocateExecutableMemory(size, false);

    if (!base_address) {
        throw std::logic_error("Failed to map memory for JIT");
    }

    if (!platform::commitMemory(mRootAddress, initialSize, platform::ProtectFlags::ReadWriteExecute)) {
        throw std::logic_error("Failed to commit memory for JIT");
    }

    _sizeLimit = sizeLimit;
    mCommittedSize = initialSize;
    mIncreaseSize = initialSize;
    current_address = base_address;
}

JIT_Runtime::~JIT_Runtime() {
      platform::freeMemory(mRootAddress, _sizeLimit);
   }


   void * JIT_Runtime::allocate(size_t size, size_t alignment = 4) noexcept
   {
      // Calculate how much we need to allocate to guarentee we can
      //  align the pointer and still have sufficient room for our data.
      size_t alignedSize = align_up(size + (alignment - 1), alignment);

      // Try to grab some space
      asmjit::Ptr baseCurAddress = mCurAddress.fetch_add(alignedSize);
      asmjit::Ptr alignedAddress = align_up(baseCurAddress, alignment);
      size_t curCommited = mCommittedSize.load();

      // Check that we did not overrun the end of the commited area
      if (alignedAddress + size > curCommited) {
         // Lock the runtime while we adjusting committed region stuff
         std::unique_lock<std::mutex> lock(mMutex);

         // Loop committing new sections until we have enough commited space to cover
         //  our new allocation.  We don't bother doing commissions for other threads
         //  since they are already being forced to lock the mutex anyways.
         auto committedSize = mCommittedSize.load();
         while (mRootAddress + committedSize < alignedAddress + size) {
            if (!platform::commitMemory(mRootAddress + committedSize, mIncreaseSize, platform::ProtectFlags::ReadWriteExecute)) {
               // We failed to commit more memory for some reason
               return nullptr;
            }
            committedSize += mIncreaseSize;
         }

         mCommittedSize.store(committedSize);
      }

      return reinterpret_cast<void*>(alignedAddress);
   }

   ASMJIT_API virtual asmjit::Error JIT_Runtime::add(void** dst, asmjit::Assembler* assembler) noexcept
   {
      size_t codeSize = assembler->getCodeSize();
      if (codeSize == 0) {
         *dst = nullptr;
         return asmjit::kErrorNoCodeGenerated;
      }

      // Lets allocate some memory for the JIT block, allocate only
      //  fails if we have run out of memory, so make sure to indicate
      //  when that happens.
      auto allocPtr = allocate(codeSize, 8);
      if (!allocPtr) {
         *dst = nullptr;
         return asmjit::kErrorCodeTooLarge;
      }

      // Lets relocate the code to the memory block
      size_t relocSize = assembler->relocCode(allocPtr);
      if (relocSize == 0) {
         return asmjit::kErrorInvalidState;
      }

      flush(allocPtr, codeSize);
      *dst = allocPtr;

      return asmjit::kErrorOk;
   }
};
