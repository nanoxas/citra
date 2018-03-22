// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/frontend/applet/swkbd.h"

namespace Frontend {

Common::Result<bool, InputValidationError> SoftwareKeyboard::ValidateFilters(
    const std::string& input) {
    bool valid = true;
    bool local_filter = true;
    if ((filters & SWKBDFILTER_DIGITS) == SWKBDFILTER_DIGITS) {
        valid &= local_filter =
            std::all_of(input.begin(), input.end(), [](const char c) { return !std::isdigit(c); });
        if (!local_filter) {
            std::cout << "Input must not contain any digits" << std::endl;
        }
    }
    if ((filters & SWKBDFILTER_AT) == SWKBDFILTER_AT) {
        valid &= local_filter = input.find("@") == std::string::npos;
        if (!local_filter) {
            std::cout << "Input must not contain the @ symbol" << std::endl;
        }
    }
    if ((filters & SWKBDFILTER_PERCENT) == SWKBDFILTER_PERCENT) {
        valid &= local_filter = input.find("%") == std::string::npos;
        if (!local_filter) {
            std::cout << "Input must not contain the % symbol" << std::endl;
        }
    }
    if ((filters & SWKBDFILTER_BACKSLASH) == SWKBDFILTER_BACKSLASH) {
        valid &= local_filter = input.find("\\") == std::string::npos;
        if (!local_filter) {
            std::cout << "Input must not contain the \\ symbol" << std::endl;
        }
    }
    if ((filters & SWKBDFILTER_PROFANITY) == SWKBDFILTER_PROFANITY) {
        // TODO: check the profanity filter
        LOG_WARNING(Service_APT, "App requested profanity filter, but its not implemented.");
    }
    if ((filters & SWKBDFILTER_CALLBACK) == SWKBDFILTER_CALLBACK) {
        // TODO: check the callback
        LOG_WARNING(Service_APT, "App requested a callback check, but its not implemented.");
    }
    return valid;
}

bool SoftwareKeyboard::ValidateInput(const SoftwareKeyboardConfig& config,
                                     const std::string input) {
    // TODO(jroweboy): Is max_text_length inclusive or exclusive?
    if (input.size() > config.max_text_length) {
        std::cout << Common::StringFromFormat("Input is longer than the maximum length. Max: %u",
                                              config.max_text_length)
                  << std::endl;
        return false;
    }
    // return early if the text is filtered
    if (config.filter_flags && !ValidateFilters(config.filter_flags, input)) {
        return false;
    }

    bool valid;
    switch (config.valid_input) {
    case SwkbdValidInput::FIXEDLEN:
        valid = input.size() == config.max_text_length;
        if (!valid) {
            std::cout << Common::StringFromFormat("Input must be exactly %u characters.",
                                                  config.max_text_length)
                      << std::endl;
        }
        break;
    case SwkbdValidInput::NOTEMPTY_NOTBLANK:
    case SwkbdValidInput::NOTBLANK:
        valid =
            std::any_of(input.begin(), input.end(), [](const char c) { return !std::isspace(c); });
        if (!valid) {
            std::cout << "Input must not be blank." << std::endl;
        }
        break;
    case SwkbdValidInput::NOTEMPTY:
        valid = input.empty();
        if (!valid) {
            std::cout << "Input must not be empty." << std::endl;
        }
        break;
    case SwkbdValidInput::ANYTHING:
        valid = true;
        break;
    default:
        // TODO(jroweboy): What does hardware do in this case?
        LOG_CRITICAL(Service_APT, "Application requested unknown validation method. Method: %u",
                     static_cast<u32>(config.valid_input));
        UNREACHABLE();
    }

    return valid;
}

bool SoftwareKeyboard::ValidateButton(u32 num_buttons, const std::string& input) {
    // check that the input is a valid number
    bool valid = false;
    try {
        u32 num = std::stoul(input);
        valid = num <= num_buttons;
        if (!valid) {
            std::cout << Common::StringFromFormat("Please choose a number between 0 and %u",
                                                  num_buttons)
                      << std::endl;
        }
    } catch (const std::invalid_argument& e) {
        (void)e;
        std::cout << "Unable to parse input as a number." << std::endl;
    } catch (const std::out_of_range& e) {
        (void)e;
        std::cout << "Input number is not valid." << std::endl;
    }
    return valid;
}

} // namespace Frontend
