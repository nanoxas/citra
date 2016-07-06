// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <asmjit/asmjit.h>

#include "common/alignment.h"
#include "common/memory_util.h"
#include "common/logging/log.h"
#include "core/arm/asmjit/jit_runtime.h"

JIT_Runtime::JIT_Runtime(size_t size) {
    // try to avoid low memory addresses to make the memory contiguous
    base_address = *static_cast<u64*>(AllocateExecutableMemoryWithHint(0x100000000, size));

    if (!base_address) {
        LOG_ERROR(Core_JIT, "Unable to get the memory requested for the JIT. Size: %llu", size);
    }

    current_address = base_address;
    increase_size = size;
}

JIT_Runtime::~JIT_Runtime() {
    FreeMemoryPages(static_cast<u64*>(&base_address), committed_size);
}

void* JIT_Runtime::allocate(size_t size, size_t alignment = 4) {
    // Calculate how much we need to allocate to guarentee we can
    //  align the pointer and still have sufficient room for our data.
    size_t alignedSize = Common::AlignUp(size + (alignment - 1), alignment);

    // Try to grab some space
    asmjit::Ptr baseCurAddress = current_address.fetch_add(alignedSize);
    asmjit::Ptr alignedAddress = Common::AlignUp(baseCurAddress, alignment);
    size_t current_size = committed_size.load();

    // Check that we did not overrun the end of the commited area
    if (alignedAddress + size > current_size) {
        // Lock the runtime while we adjusting committed region stuff
        std::unique_lock<std::mutex> lock(runtime_lock);

        // Loop committing new sections until we have enough commited space to cover
        //  our new allocation.  We don't bother doing commissions for other threads
        //  since they are already being forced to lock the mutex anyways.
        auto committed = committed_size.load();
        while (base_address + committed < alignedAddress + size) {
            if (!AllocateExecutableMemoryWithHint(base_address + committed, increase_size)) {
                // We failed to commit more memory for some reason
                return nullptr;
            }
            committed += increase_size;
        }

        committed_size.store(committed);
    }

    return reinterpret_cast<void*>(alignedAddress);
}

asmjit::Error JIT_Runtime::add(void** dst, asmjit::Assembler* assembler) {
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
