// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <unordered_map>
#include "common/result.h"
#include "core/frontend/applet/interface.h"

namespace Frontend {

enum class AcceptedInput {
    ANYTHING = 0,      /// All inputs are accepted.
    NOTEMPTY,          /// Empty inputs are not accepted.
    NOTEMPTY_NOTBLANK, /// Empty or blank inputs (consisting solely of whitespace) are not
                       /// accepted.
    NOTBLANK, /// Blank inputs (consisting solely of whitespace) are not accepted, but empty
              /// inputs are.
    FIXEDLEN, /// The input must have a fixed length (specified by maxTextLength in
              /// swkbdInit).
};

enum class ButtonConfig {
    SINGLE_BUTTON = 0, /// Ok button
    DUAL_BUTTON,       /// Cancel | Ok buttons
    TRIPLE_BUTTON,     /// Cancel | I Forgot | Ok buttons
    NO_BUTTON,         /// No button (returned by swkbdInputText in special cases)
};

/// Default English button text mappings. Frontends may need to copy this to internationalize it.
static const char* BUTTON_OKAY = "Ok";
static const char* BUTTON_CANCEL = "Cancel";
static const char* BUTTON_FORGOT = "I Forgot";
static const std::unordered_map<ButtonConfig, std::vector<std::string>> DEFAULT_BUTTON_MAPPING = {
    {SINGLE_BUTTON, {BUTTON_OKAY}},
    {DUAL_BUTTON, {BUTTON_CANCEL, BUTTON_OKAY}},
    {TRIPLE_BUTTON, {BUTTON_CANCEL, BUTTON_FORGOT, BUTTON_OKAY}},
};

/// Configuration thats relevent to frontend implementation of applets. Anything missing that we
/// later learn is needed can be added here and filled in by the backed HLE applet
struct KeyboardConfig {
    AcceptedInput accept_mode;   /// What kinds of input are accepted (blank/empty/fixed width)
    bool multiline_mode;         /// True if the keyboard accepts multiple lines of input
    u16 max_text_length;         /// Maximum number of letters allowed if its a text input
    u16 max_digits;              /// Maximum number of numbers allowed if its a number input
    std::string hint_text;       /// Displayed in the field as a hint before
    bool has_custom_button_text; /// If true, use the button_text instead
    std::vector<std::string> button_text; /// Contains the button text that the caller provides
    struct Filters {
        bool disable_digit;   /// Disallow the use of more than a certain number of digits (TODO how
                              /// many is a certain number)
        bool disable_at;      /// Disallow the use of the @ sign.
        bool disable_percent; /// Disallow the use of the % sign.
        bool disable_backslash; /// Disallow the use of the \ sign.
        bool disable_profanity; /// Disallow profanity using Nintendo's profanity filter.
        bool enable_callback;   /// Use a callback in order to check the input.
    } filters;
};

struct KeyboardData {
    std::string text;
    u8 button;
};

enum class ValidationError {
    NONE,
};

class SoftwareKeyboard : public AppletInterface {
public:
    explict SoftwareKeyboard(KeyboardConfig config) : AppletInterface(), config(config) {}

protected:
    /**
     * Validates if the provided string breaks any of the filter rules. This is meant to be called
     * whenever the user input changes to check to see if the new input is valid. Frontends can
     * decide if they want to check the input continuously or once before submission
     */
    ValidationError ValidateFilters(const std::string& input);

    /**
     *
     */
    ValidationError ValidateInput(const std::string& input);

    /**
     * Verifies that the selected button is valid. This should be used as the last check before
     * closing.
     */
    ValidationError ValidateButton(u8 button);

    /**
     * Runs all validation phases. If successful, stores the data in
     */
    ValidationError Finialize(KeyboardData);

private:
    KeyboardData ReceiveData() override {
        return {};
    }

    KeyboardConfig config;
    KeyboardData data;
};

} // namespace Frontend
