// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <asmjit/asmjit.h>
#include <atomic>
#include <mutex>

class JIT_Runtime : public asmjit::HostRuntime {
public:
   JIT_Runtime(size_t size);

   ~JIT_Runtime();

   void* allocate(size_t size, size_t alignment = 4) noexcept;

   ASMJIT_API virtual asmjit::Error add(void** dst, asmjit::Assembler* assembler) noexcept;

   ASMJIT_API virtual asmjit::Error release(void* p) noexcept {
      // We do not release memory
      return asmjit::kErrorOk;
   }

   asmjit::Ptr getBaseAddress() const {
      return base_address;
   }

private:
   std::mutex mMutex;
   asmjit::Ptr base_address;
   size_t mIncreaseSize;
   std::atomic<asmjit::Ptr> current_address;
   std::atomic<size_t> mCommittedSize;
};
