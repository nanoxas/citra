// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

// a simple lockless thread-safe,
// single reader, single writer queue

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <functional>
#include <future>
#include <mutex>
#include <type_traits>
#include "common/common_types.h"
#include "common/logging/log.h"

namespace Common {
template <typename T, bool NeedSize = true>
class SPSCQueue {
public:
    SPSCQueue() : size(0) {
        write_ptr = read_ptr = new ElementPtr();
    }
    ~SPSCQueue() {
        // this will empty out the whole queue
        delete read_ptr;
    }

    u32 Size() const {
        static_assert(NeedSize, "using Size() on FifoQueue without NeedSize");
        return size.load();
    }

    bool Empty() const {
        return !read_ptr->next.load();
    }
    T& Front() const {
        return read_ptr->current;
    }
    template <typename Arg>
    void Push(Arg&& t) {
        // create the element, add it to the queue
        write_ptr->current = std::move(std::forward<Arg>(t));
        // set the next pointer to a new element ptr
        // then advance the write pointer
        ElementPtr* new_ptr = new ElementPtr();
        write_ptr->next.store(new_ptr, std::memory_order_release);
        write_ptr = new_ptr;
        if (NeedSize)
            size++;
    }

    void Pop() {
        if (NeedSize)
            size--;
        ElementPtr* tmpptr = read_ptr;
        // advance the read pointer
        read_ptr = tmpptr->next.load();
        // set the next element to nullptr to stop the recursive deletion
        tmpptr->next.store(nullptr);
        delete tmpptr; // this also deletes the element
    }

    bool Pop(T& t) {
        if (Empty())
            return false;

        if (NeedSize)
            size--;

        ElementPtr* tmpptr = read_ptr;
        read_ptr = tmpptr->next.load(std::memory_order_acquire);
        t = std::move(tmpptr->current);
        tmpptr->next.store(nullptr);
        delete tmpptr;
        return true;
    }

    // not thread-safe
    void Clear() {
        size.store(0);
        delete read_ptr;
        write_ptr = read_ptr = new ElementPtr();
    }

private:
    // stores a pointer to element
    // and a pointer to the next ElementPtr
    class ElementPtr {
    public:
        ElementPtr() : next(nullptr) {}
        ~ElementPtr() {
            ElementPtr* next_ptr = next.load();

            if (next_ptr)
                delete next_ptr;
        }

        T current;
        std::atomic<ElementPtr*> next;
    };

    ElementPtr* write_ptr;
    ElementPtr* read_ptr;
    std::atomic<u32> size;
};

// a simple thread-safe,
// single reader, multiple writer queue

template <typename T, bool NeedSize = true>
class MPSCQueue : public SPSCQueue<T, NeedSize> {
public:
    template <typename Arg>
    void Push(Arg&& t) {
        std::lock_guard<std::mutex> lock(write_lock);
        SPSCQueue<T, NeedSize>::Push(t);
    }

private:
    std::mutex write_lock;
};

template <typename F, typename... Args>
class TaskQueue {
    using ResultType = typename std::result_of<F(Args...)>::type;

public:
    void Run() {
        LOG_CRITICAL(Common, "Starting listening for tasks");
        std::packaged_task<ResultType(void)> task;
        while (true) {
            std::unique_lock<std::mutex> lock(has_work);
            has_work_cv.wait(lock, [&] { return finished || task_list.Pop(task); });
            if (finished) {
                break;
            }
            LOG_CRITICAL(Common, "Executing task");
            task();
        }
    }

    virtual auto Accept(F task, Args... args) -> std::future<ResultType> {
        LOG_CRITICAL(Common, "Accepting task");
        std::lock_guard<std::mutex> lock(has_work);
        std::packaged_task<ResultType(void)> p([&] { return task(std::forward<Args>(args)...); });
        auto future = p.get_future();
        task_list.Push(std::move(p));
        has_work_cv.notify_one();
        return future;
    }

    void Done() {
        finished = true;
    }

private:
    std::atomic_bool finished{false};
    std::mutex has_work{};
    std::condition_variable has_work_cv{};
    MPSCQueue<std::packaged_task<ResultType(void)>> task_list{};
};

} // namespace Common
