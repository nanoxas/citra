// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include "common/logging/log.h"
#include "common/param_package.h"

namespace Common {
/// An abstract class template for a factory that can create devices.
template <typename Type>
class Factory {
public:
    virtual ~Factory() = default;
    virtual std::unique_ptr<Type> Create(const Common::ParamPackage&) = 0;
};

namespace Impl {

template <typename Type>
using FactoryListType = std::unordered_map<std::string, std::shared_ptr<Factory<Type>>>;

template <typename Type>
struct FactoryList {
    static FactoryListType<Type> list;
};

template <typename Type>
FactoryListType<Type> FactoryList<Type>::list;

} // namespace Impl

/**
 * Registers a device factory.
 * @tparam Type the type of devices the factory can create
 * @param name the name of the factory. Will be used to match the "engine" parameter when creating
 *     a device
 * @param factory the factory object to register
 */
template <typename Type>
void RegisterFactory(const std::string& name, std::shared_ptr<Factory<Type>> factory) {
    auto pair = std::make_pair(name, std::move(factory));
    if (!Impl::FactoryList<Type>::list.insert(std::move(pair)).second) {
        LOG_ERROR(Frontend, "Factory %s already registered", name.c_str());
    }
}

/**
 * Unregisters a device factory.
 * @tparam Type the type of input devices the factory can create
 * @param name the name of the factory to unregister
 */
template <typename Type>
void UnregisterFactory(const std::string& name) {
    if (Impl::FactoryList<Type>::list.erase(name) == 0) {
        LOG_ERROR(Frontend, "Factory %s not registered", name.c_str());
    }
}

/**
 * Create a device from given paramters.
 * @tparam Type the type of devices to create
 * @param params a serialized ParamPackage string contains all parameters for creating the device
 */
template <typename Type>
std::unique_ptr<Type> CreateDevice(const std::string& params) {
    const Common::ParamPackage package(params);
    const std::string engine = package.Get("engine", "null");
    const auto& factory_list = Impl::FactoryList<Type>::list;
    const auto pair = factory_list.find(engine);
    if (pair == factory_list.end()) {
        if (engine != "null") {
            LOG_ERROR(Frontend, "Unknown engine name: %s", engine.c_str());
        }
        return std::make_unique<Type>();
    }
    return pair->second->Create(package);
}

} // namespace Common
