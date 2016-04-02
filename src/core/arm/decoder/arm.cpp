// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cstddef>
#include <cstring>

#include <boost/optional.hpp>

#include "common/assert.h"
#include "common/common_types.h"
#include "common/make_unique.h"

#include "core/arm/decoder/decoder.h"

namespace ArmDecoder {

namespace Impl {
    // std::integer_sequence and std::make_integer_sequence are only available in C++14

    /// This type represents a sequence of integers
    template<size_t ...>
    struct integer_sequence {};

    /// This metafunction generates a sequence of integers from 0..N
    template<size_t N, size_t ...seq>
    struct make_integer_sequence : make_integer_sequence<N - 1, N - 1, seq...> {};

    // Internal implementation for make_integer_sequence
    template<size_t ...seq>
    struct make_integer_sequence<0, seq...> {
        typedef integer_sequence<seq...> type;
    };

    /**
     * This function takes a member function of Visitor and calls it with the parameters specified in list.
     * @tparam NumArgs Number of arguments that the member function fn has.
     * @param v The Visitor.
     * @param fn Member function to call on v.
     * @param list List of arguments that will be splatted.
     */
    template<size_t NumArgs, typename Function, typename Container>
    void call(Visitor* v, Function fn, const Container& list) {
        call_impl(typename make_integer_sequence<NumArgs>::type(), v, fn, list);
    }

    // Internal implementation for call
    template<size_t ...seq, typename Function, typename Container>
    void call_impl(integer_sequence<seq...>, Visitor* v, Function fn, const Container& list) {
        (v->*fn)(std::get<seq>(list)...);
    }

    /// Function has NumArgs arguments
    template<size_t NumArgs, typename Function>
    struct MatcherImpl : Matcher {
        std::array<u32, NumArgs> masks = {};
        std::array<size_t, NumArgs> shifts = {};
        Function fn = nullptr;
        virtual void visit(Visitor *v, u32 inst) override {
            std::array<u32, NumArgs> values;
            for (size_t i = 0; i < NumArgs; i++) {
                values[i] = (inst & masks[i]) >> shifts[i];
            }
            call<NumArgs>(v, fn, values);
        }
    };
}

template<size_t NumArgs, typename Function>
static std::unique_ptr<Matcher> MakeMatcher(const char format[32], Function fn) {
    auto ret = Common::make_unique<Impl::MatcherImpl<NumArgs, Function>>();
    ret->fn = fn;
    ret->masks.fill(0);
    ret->shifts.fill(0);

    char ch = 0;
    int arg = -1;

    for (int i = 0; i < 32; i++) {
        const u32 bit = 1 << (31 - i);

        if (format[i] == '0') {
            ret->bit_mask |= bit;
            ch = 0;
            continue;
        } else if (format[i] == '1') {
            ret->bit_mask |= bit;
            ret->expected |= bit;
            ch = 0;
            continue;
        } else if (format[i] == '-') {
            ch = 0;
            continue;
        }

        // Ban some characters
        ASSERT(format[i] != 'I');
        ASSERT(format[i] != 'l');
        ASSERT(format[i] != 'O');

        if (format[i] != ch){
            arg++;
            ASSERT(arg < NumArgs);
            ch = format[i];
        }

        ret->masks[arg] |= bit;
        ret->shifts[arg] = 31 - i;
    }

    ASSERT(arg == NumArgs - 1);

    return std::unique_ptr<Matcher>(std::move(ret));
}

static const std::array<Instruction, 220> arm_instruction_table = {{
    // Branch instructions
    { "BLX (immediate)",     MakeMatcher<2>("1111101hvvvvvvvvvvvvvvvvvvvvvvvv", &Visitor::BLX_imm)   }, // ARMv5
    { "BLX (register)",      MakeMatcher<2>("cccc000100101111111111110011mmmm", &Visitor::BLX_reg)   }, // ARMv5
    { "B",                   MakeMatcher<2>("cccc1010vvvvvvvvvvvvvvvvvvvvvvvv", &Visitor::B)         }, // all
    { "BL",                  MakeMatcher<2>("cccc1011vvvvvvvvvvvvvvvvvvvvvvvv", &Visitor::BL)        }, // all
    { "BX",                  MakeMatcher<2>("cccc000100101111111111110001mmmm", &Visitor::BX)        }, // ARMv4T
    { "BXJ",                 MakeMatcher<2>("cccc000100101111111111110010mmmm", &Visitor::BXJ)       }, // ARMv5J

    // Coprocessor instructions
    { "CDP2",                MakeMatcher<0>("11111110-------------------1----", &Visitor::CDP)       }, // ARMv5 (Generic Coprocessor)
    { "CDP",                 MakeMatcher<0>("----1110-------------------0----", &Visitor::CDP)       }, // ARMv2 (Generic Coprocessor)
    { "LDC2",                MakeMatcher<0>("1111110----1--------------------", &Visitor::LDC)       }, // ARMv5 (Generic Coprocessor)
    { "LDC",                 MakeMatcher<0>("----110----1--------------------", &Visitor::LDC)       }, // ARMv2 (Generic Coprocessor)
    { "MCR2",                MakeMatcher<0>("----1110---0---------------1----", &Visitor::MCR)       }, // ARMv5 (Generic Coprocessor)
    { "MCR",                 MakeMatcher<0>("----1110---0---------------1----", &Visitor::MCR)       }, // ARMv2 (Generic Coprocessor)
    { "MCRR2",               MakeMatcher<0>("111111000100--------------------", &Visitor::MCRR)      }, // ARMv6 (Generic Coprocessor)
    { "MCRR",                MakeMatcher<0>("----11000100--------------------", &Visitor::MCRR)      }, // ARMv5E (Generic Coprocessor)
    { "MRC2",                MakeMatcher<0>("11111110---1---------------1----", &Visitor::MRC)       }, // ARMv5 (Generic Coprocessor)
    { "MRC",                 MakeMatcher<0>("----1110---1---------------1----", &Visitor::MRC)       }, // ARMv2 (Generic Coprocessor)
    { "MRRC2",               MakeMatcher<0>("111111000101--------------------", &Visitor::MRRC)      }, // ARMv6 (Generic Coprocessor)
    { "MRRC",                MakeMatcher<0>("----11000101--------------------", &Visitor::MRRC)      }, // ARMv5E (Generic Coprocessor)
    { "STC2",                MakeMatcher<0>("1111110----0--------------------", &Visitor::STC)       }, // ARMv5 (Generic Coprocessor)
    { "STC",                 MakeMatcher<0>("----110----0--------------------", &Visitor::STC)       }, // ARMv2 (Generic Coprocessor)

    // Data Processing instructions
    { "ADC (imm)",           MakeMatcher<6>("cccc0010101Snnnnddddrrrrvvvvvvvv", &Visitor::ADC_imm)   }, // all
    { "ADC (reg)",           MakeMatcher<7>("cccc0000101Snnnnddddvvvvvrr0mmmm", &Visitor::ADC_reg)   }, // all
    { "ADC (rsr)",           MakeMatcher<7>("cccc0000101Snnnnddddssss0rr1mmmm", &Visitor::ADC_rsr)   }, // all
    { "ADD (imm)",           MakeMatcher<6>("cccc0010100Snnnnddddrrrrvvvvvvvv", &Visitor::ADD_imm)   }, // all
    { "ADD (reg)",           MakeMatcher<7>("cccc0000100Snnnnddddvvvvvrr0mmmm", &Visitor::ADD_reg)   }, // all
    { "ADD (rsr)",           MakeMatcher<7>("cccc0000100Snnnnddddssss0rr1mmmm", &Visitor::ADD_rsr)   }, // all
    { "AND (imm)",           MakeMatcher<6>("cccc0010000Snnnnddddrrrrvvvvvvvv", &Visitor::AND_imm)   }, // all
    { "AND (reg)",           MakeMatcher<7>("cccc0000000Snnnnddddvvvvvrr0mmmm", &Visitor::AND_reg)   }, // all
    { "AND (rsr)",           MakeMatcher<7>("cccc0000000Snnnnddddssss0rr1mmmm", &Visitor::AND_rsr)   }, // all
    { "BIC (imm)",           MakeMatcher<6>("cccc0011110Snnnnddddrrrrvvvvvvvv", &Visitor::BIC_imm)   }, // all
    { "BIC (reg)",           MakeMatcher<7>("cccc0001110Snnnnddddvvvvvrr0mmmm", &Visitor::BIC_reg)   }, // all
    { "BIC (rsr)",           MakeMatcher<7>("cccc0001110Snnnnddddssss0rr1mmmm", &Visitor::BIC_rsr)   }, // all
    { "CMN (imm)",           MakeMatcher<4>("cccc00110111nnnn0000rrrrvvvvvvvv", &Visitor::CMN_imm)   }, // all
    { "CMN (reg)",           MakeMatcher<5>("cccc00010111nnnn0000vvvvvrr0mmmm", &Visitor::CMN_reg)   }, // all
    { "CMN (rsr)",           MakeMatcher<5>("cccc00010111nnnn0000ssss0rr1mmmm", &Visitor::CMN_rsr)   }, // all
    { "CMP (imm)",           MakeMatcher<4>("cccc00110101nnnn0000rrrrvvvvvvvv", &Visitor::CMP_imm)   }, // all
    { "CMP (reg)",           MakeMatcher<5>("cccc00010101nnnn0000vvvvvrr0mmmm", &Visitor::CMP_reg)   }, // all
    { "CMP (rsr)",           MakeMatcher<5>("cccc00010101nnnn0000ssss0rr1mmmm", &Visitor::CMP_rsr)   }, // all
    { "EOR (imm)",           MakeMatcher<6>("cccc0010001Snnnnddddrrrrvvvvvvvv", &Visitor::EOR_imm)   }, // all
    { "EOR (reg)",           MakeMatcher<7>("cccc0000001Snnnnddddvvvvvrr0mmmm", &Visitor::EOR_reg)   }, // all
    { "EOR (rsr)",           MakeMatcher<7>("cccc0000001Snnnnddddssss0rr1mmmm", &Visitor::EOR_rsr)   }, // all
    { "MOV (imm)",           MakeMatcher<5>("cccc0011101S0000ddddrrrrvvvvvvvv", &Visitor::MOV_imm)   }, // all
    { "MOV (reg)",           MakeMatcher<6>("cccc0001101S0000ddddvvvvvrr0mmmm", &Visitor::MOV_reg)   }, // all
    { "MOV (rsr)",           MakeMatcher<6>("cccc0001101S0000ddddssss0rr1mmmm", &Visitor::MOV_rsr)   }, // all
    { "MVN (imm)",           MakeMatcher<5>("cccc0011111S0000ddddrrrrvvvvvvvv", &Visitor::MVN_imm)   }, // all
    { "MVN (reg)",           MakeMatcher<6>("cccc0001111S0000ddddvvvvvrr0mmmm", &Visitor::MVN_reg)   }, // all
    { "MVN (rsr)",           MakeMatcher<6>("cccc0001111S0000ddddssss0rr1mmmm", &Visitor::MVN_rsr)   }, // all
    { "ORR (imm)",           MakeMatcher<6>("cccc0011100Snnnnddddrrrrvvvvvvvv", &Visitor::ORR_imm)   }, // all
    { "ORR (reg)",           MakeMatcher<7>("cccc0001100Snnnnddddvvvvvrr0mmmm", &Visitor::ORR_reg)   }, // all
    { "ORR (rsr)",           MakeMatcher<7>("cccc0001100Snnnnddddssss0rr1mmmm", &Visitor::ORR_rsr)   }, // all
    { "RSB (imm)",           MakeMatcher<6>("cccc0010011Snnnnddddrrrrvvvvvvvv", &Visitor::RSB_imm)   }, // all
    { "RSB (reg)",           MakeMatcher<7>("cccc0000011Snnnnddddvvvvvrr0mmmm", &Visitor::RSB_reg)   }, // all
    { "RSB (rsr)",           MakeMatcher<7>("cccc0000011Snnnnddddssss0rr1mmmm", &Visitor::RSB_rsr)   }, // all
    { "RSC (imm)",           MakeMatcher<6>("cccc0010111Snnnnddddrrrrvvvvvvvv", &Visitor::RSC_imm)   }, // all
    { "RSC (reg)",           MakeMatcher<7>("cccc0000111Snnnnddddvvvvvrr0mmmm", &Visitor::RSC_reg)   }, // all
    { "RSC (rsr)",           MakeMatcher<7>("cccc0000111Snnnnddddssss0rr1mmmm", &Visitor::RSC_rsr)   }, // all
    { "SBC (imm)",           MakeMatcher<6>("cccc0010110Snnnnddddrrrrvvvvvvvv", &Visitor::SBC_imm)   }, // all
    { "SBC (reg)",           MakeMatcher<7>("cccc0000110Snnnnddddvvvvvrr0mmmm", &Visitor::SBC_reg)   }, // all
    { "SBC (rsr)",           MakeMatcher<7>("cccc0000110Snnnnddddssss0rr1mmmm", &Visitor::SBC_rsr)   }, // all
    { "SUB (imm)",           MakeMatcher<6>("cccc0010010Snnnnddddrrrrvvvvvvvv", &Visitor::SUB_imm)   }, // all
    { "SUB (reg)",           MakeMatcher<7>("cccc0000010Snnnnddddvvvvvrr0mmmm", &Visitor::SUB_reg)   }, // all
    { "SUB (rsr)",           MakeMatcher<7>("cccc0000010Snnnnddddssss0rr1mmmm", &Visitor::SUB_rsr)   }, // all
    { "TEQ (imm)",           MakeMatcher<4>("cccc00110011nnnn0000rrrrvvvvvvvv", &Visitor::TEQ_imm)   }, // all
    { "TEQ (reg)",           MakeMatcher<5>("cccc00010011nnnn0000vvvvvrr0mmmm", &Visitor::TEQ_reg)   }, // all
    { "TEQ (rsr)",           MakeMatcher<5>("cccc00010011nnnn0000ssss0rr1mmmm", &Visitor::TEQ_rsr)   }, // all
    { "TST (imm)",           MakeMatcher<4>("cccc00110001nnnn0000rrrrvvvvvvvv", &Visitor::TST_imm)   }, // all
    { "TST (reg)",           MakeMatcher<5>("cccc00010001nnnn0000vvvvvrr0mmmm", &Visitor::TST_reg)   }, // all
    { "TST (rsr)",           MakeMatcher<5>("cccc00010001nnnn0000ssss0rr1mmmm", &Visitor::TST_rsr)   }, // all

    // Exception Generating instructions
    { "BKPT",                MakeMatcher<0>("----00010010------------0111----", &Visitor::BKPT)      }, // ARMv5
    { "SVC",                 MakeMatcher<0>("----1111------------------------", &Visitor::SVC)       }, // all
    { "UDF",                 MakeMatcher<0>("111001111111------------1111----", &Visitor::UDF)       }, // all

    // Extension instructions
    { "SXTB",                MakeMatcher<0>("----011010101111------000111----", &Visitor::SXTB)      }, // ARMv6
    { "SXTB16",              MakeMatcher<0>("----011010001111------000111----", &Visitor::SXTB16)    }, // ARMv6
    { "SXTH",                MakeMatcher<0>("----011010111111------000111----", &Visitor::SXTH)      }, // ARMv6
    { "SXTAB",               MakeMatcher<0>("----01101010----------000111----", &Visitor::SXTAB)     }, // ARMv6
    { "SXTAB16",             MakeMatcher<0>("----01101000----------000111----", &Visitor::SXTAB16)   }, // ARMv6
    { "SXTAH",               MakeMatcher<0>("----01101011----------000111----", &Visitor::SXTAH)     }, // ARMv6
    { "UXTB",                MakeMatcher<0>("----011011101111------000111----", &Visitor::UXTB)      }, // ARMv6
    { "UXTB16",              MakeMatcher<0>("----011011001111------000111----", &Visitor::UXTB16)    }, // ARMv6
    { "UXTH",                MakeMatcher<0>("----011011111111------000111----", &Visitor::UXTH)      }, // ARMv6
    { "UXTAB",               MakeMatcher<0>("----01101110----------000111----", &Visitor::UXTAB)     }, // ARMv6
    { "UXTAB16",             MakeMatcher<0>("----01101100----------000111----", &Visitor::UXTAB16)   }, // ARMv6
    { "UXTAH",               MakeMatcher<0>("----01101111----------000111----", &Visitor::UXTAH)     }, // ARMv6

    // Hint instructions
    { "PLD",                 MakeMatcher<0>("111101---101----1111------------", &Visitor::PLD)       }, // ARMv5E
    { "SEV",                 MakeMatcher<0>("----0011001000001111000000000100", &Visitor::SEV)       }, // ARMv6K
    { "WFE",                 MakeMatcher<0>("----0011001000001111000000000010", &Visitor::WFE)       }, // ARMv6K
    { "WFI",                 MakeMatcher<0>("----0011001000001111000000000011", &Visitor::WFI)       }, // ARMv6K
    { "YIELD",               MakeMatcher<0>("----0011001000001111000000000001", &Visitor::YIELD)     }, // ARMv6K

    // Synchronization Primitive instructions
    { "CLREX",               MakeMatcher<0>("11110101011111111111000000011111", &Visitor::CLREX)     }, // ARMv6K
    { "LDREX",               MakeMatcher<0>("----00011001--------111110011111", &Visitor::LDREX)     }, // ARMv6
    { "LDREXB",              MakeMatcher<0>("----00011101--------111110011111", &Visitor::LDREXB)    }, // ARMv6K
    { "LDREXD",              MakeMatcher<0>("----00011011--------111110011111", &Visitor::LDREXD)    }, // ARMv6K
    { "LDREXH",              MakeMatcher<0>("----00011111--------111110011111", &Visitor::LDREXH)    }, // ARMv6K
    { "STREX",               MakeMatcher<0>("----00011000--------11111001----", &Visitor::STREX)     }, // ARMv6
    { "STREXB",              MakeMatcher<0>("----00011100--------11111001----", &Visitor::STREXB)    }, // ARMv6K
    { "STREXD",              MakeMatcher<0>("----00011010--------11111001----", &Visitor::STREXD)    }, // ARMv6K
    { "STREXH",              MakeMatcher<0>("----00011110--------11111001----", &Visitor::STREXH)    }, // ARMv6K
    { "SWP",                 MakeMatcher<0>("----00010-00--------00001001----", &Visitor::SWP)       }, // ARMv2S

    // Load/Store instructions
    { "LDR (imm)",           MakeMatcher<7>("cccc010pu0w1nnnnddddvvvvvvvvvvvv", &Visitor::LDR_imm)   },
    { "LDR (reg)",           MakeMatcher<9>("cccc011pu0w1nnnnddddvvvvvrr0mmmm", &Visitor::LDR_reg)   },
    { "LDRB (imm)",          MakeMatcher<7>("cccc010pu1w1nnnnddddvvvvvvvvvvvv", &Visitor::LDRB_imm)  },
    { "LDRB (reg)",          MakeMatcher<9>("cccc011pu1w1nnnnddddvvvvvrr0mmmm", &Visitor::LDRB_reg)  },
    { "LDRBT (A1)",          MakeMatcher<0>("----0100-111--------------------", &Visitor::LDRBT)     },
    { "LDRBT (A2)",          MakeMatcher<0>("----0110-111---------------0----", &Visitor::LDRBT)     },
    { "LDRD (imm)",          MakeMatcher<8>("cccc000pu1w0nnnnddddvvvv1101vvvv", &Visitor::LDRD_imm)  }, // ARMv5E
    { "LDRD (reg)",          MakeMatcher<7>("cccc000pu0w0nnnndddd00001101mmmm", &Visitor::LDRD_reg)  }, // ARMv5E
    { "LDRH (imm)",          MakeMatcher<8>("cccc000pu1w1nnnnddddvvvv1011vvvv", &Visitor::LDRH_imm)  },
    { "LDRH (reg)",          MakeMatcher<7>("cccc000pu0w1nnnndddd00001011mmmm", &Visitor::LDRH_reg)  },
    { "LDRHT (A1)",          MakeMatcher<0>("----0000-111------------1011----", &Visitor::LDRHT)     },
    { "LDRHT (A2)",          MakeMatcher<0>("----0000-011--------00001011----", &Visitor::LDRHT)     },
    { "LDRSB (imm)",         MakeMatcher<8>("cccc000pu1w1nnnnddddvvvv1101vvvv", &Visitor::LDRSB_imm) },
    { "LDRSB (reg)",         MakeMatcher<7>("cccc000pu0w1nnnndddd00001101mmmm", &Visitor::LDRSB_reg) },
    { "LDRSBT (A1)",         MakeMatcher<0>("----0000-111------------1101----", &Visitor::LDRSBT)    },
    { "LDRSBT (A2)",         MakeMatcher<0>("----0000-011--------00001101----", &Visitor::LDRSBT)    },
    { "LDRSH (imm)",         MakeMatcher<8>("cccc000pu1w1nnnnddddvvvv1111vvvv", &Visitor::LDRSH_imm) },
    { "LDRSH (reg)",         MakeMatcher<7>("cccc000pu0w1nnnndddd00001111mmmm", &Visitor::LDRSH_reg) },
    { "LDRSHT (A1)",         MakeMatcher<0>("----0000-111------------1111----", &Visitor::LDRSHT)    },
    { "LDRSHT (A2)",         MakeMatcher<0>("----0000-011--------00001111----", &Visitor::LDRSHT)    },
    { "LDRT (A1)",           MakeMatcher<0>("----0100-011--------------------", &Visitor::LDRT)      },
    { "LDRT (A2)",           MakeMatcher<0>("----0110-011---------------0----", &Visitor::LDRT)      },
    { "STR (imm)",           MakeMatcher<7>("cccc010pu0w0nnnnddddvvvvvvvvvvvv", &Visitor::STR_imm)   },
    { "STR (reg)",           MakeMatcher<9>("cccc011pu0w0nnnnddddvvvvvrr0mmmm", &Visitor::STR_reg)   },
    { "STRB (imm)",          MakeMatcher<7>("cccc010pu1w0nnnnddddvvvvvvvvvvvv", &Visitor::STRB_imm)  },
    { "STRB (reg)",          MakeMatcher<9>("cccc011pu1w0nnnnddddvvvvvrr0mmmm", &Visitor::STRB_reg)  },
    { "STRBT (A1)",          MakeMatcher<0>("----0100-110--------------------", &Visitor::STRBT)     },
    { "STRBT (A2)",          MakeMatcher<0>("----0110-110---------------0----", &Visitor::STRBT)     },
    { "STRD (imm)",          MakeMatcher<8>("cccc000pu1w0nnnnddddvvvv1111vvvv", &Visitor::STRD_imm)  }, // ARMv5E
    { "STRD (reg)",          MakeMatcher<7>("cccc000pu0w0nnnndddd00001111mmmm", &Visitor::STRD_reg)  }, // ARMv5E
    { "STRH (imm)",          MakeMatcher<8>("cccc000pu1w0nnnnddddvvvv1011vvvv", &Visitor::STRH_imm)  },
    { "STRH (reg)",          MakeMatcher<7>("cccc000pu0w0nnnndddd00001011mmmm", &Visitor::STRH_reg)  },
    { "STRHT (A1)",          MakeMatcher<0>("----0000-110------------1011----", &Visitor::STRHT)     },
    { "STRHT (A2)",          MakeMatcher<0>("----0000-010--------00001011----", &Visitor::STRHT)     },
    { "STRT (A1)",           MakeMatcher<0>("----0100-010--------------------", &Visitor::STRT)      },
    { "STRT (A2)",           MakeMatcher<0>("----0110-010---------------0----", &Visitor::STRT)      },

    // Load/Store Multiple instructions
    { "LDM",                 MakeMatcher<6>("cccc100pu0w1nnnnxxxxxxxxxxxxxxxx", &Visitor::LDM)       }, // all
    { "LDM (usr reg)",       MakeMatcher<0>("----100--101--------------------", &Visitor::LDM_usr)   }, // all
    { "LDM (exce ret)",      MakeMatcher<0>("----100--1-1----1---------------", &Visitor::LDM_eret)  }, // all
    { "STM",                 MakeMatcher<6>("cccc100pu0w0nnnnxxxxxxxxxxxxxxxx", &Visitor::STM)       }, // all
    { "STM (usr reg)",       MakeMatcher<0>("----100--100--------------------", &Visitor::STM_usr)   }, // all

    // Miscellaneous instructions
    { "CLZ",                 MakeMatcher<0>("----000101101111----11110001----", &Visitor::CLZ)       }, // ARMv5
    { "NOP",                 MakeMatcher<0>("----001100100000111100000000----", &Visitor::NOP)       }, // ARMv6K
    { "SEL",                 MakeMatcher<0>("----01101000--------11111011----", &Visitor::SEL)       }, // ARMv6

    // Unsigned Sum of Absolute Differences instructions
    { "USAD8",               MakeMatcher<0>("----01111000----1111----0001----", &Visitor::USAD8)     }, // ARMv6
    { "USADA8",              MakeMatcher<0>("----01111000------------0001----", &Visitor::USADA8)    }, // ARMv6

    // Packing instructions
    { "PKHBT",               MakeMatcher<5>("cccc01101000nnnnddddvvvvv001mmmm", &Visitor::PKHBT)     }, // ARMv6K
    { "PKHTB",               MakeMatcher<5>("cccc01101000nnnnddddvvvvv101mmmm", &Visitor::PKHTB)     }, // ARMv6K

    // Reversal instructions
    { "REV",                 MakeMatcher<0>("----011010111111----11110011----", &Visitor::REV)       }, // ARMv6
    { "REV16",               MakeMatcher<0>("----011010111111----11111011----", &Visitor::REV16)     }, // ARMv6
    { "REVSH",               MakeMatcher<0>("----011011111111----11111011----", &Visitor::REVSH)     }, // ARMv6

    // Saturation instructions
    { "SSAT",                MakeMatcher<0>("----0110101---------------01----", &Visitor::SSAT)      }, // ARMv6
    { "SSAT16",              MakeMatcher<0>("----01101010--------11110011----", &Visitor::SSAT16)    }, // ARMv6
    { "USAT",                MakeMatcher<0>("----0110111---------------01----", &Visitor::USAT)      }, // ARMv6
    { "USAT16",              MakeMatcher<0>("----01101110--------11110011----", &Visitor::USAT16)    }, // ARMv6

    // Multiply (Normal) instructions
    { "MLA",                 MakeMatcher<0>("----0000001-------------1001----", &Visitor::MLA)       }, // ARMv2
    { "MUL",                 MakeMatcher<0>("----0000000-----0000----1001----", &Visitor::MUL)       }, // ARMv2

    // Multiply (Long) instructions
    { "SMLAL",               MakeMatcher<0>("----0000111-------------1001----", &Visitor::SMLAL)     }, // ARMv3M
    { "SMULL",               MakeMatcher<0>("----0000110-------------1001----", &Visitor::SMULL)     }, // ARMv3M
    { "UMAAL",               MakeMatcher<0>("----00000100------------1001----", &Visitor::UMAAL)     }, // ARMv6
    { "UMLAL",               MakeMatcher<0>("----0000101-------------1001----", &Visitor::UMLAL)     }, // ARMv3M
    { "UMULL",               MakeMatcher<0>("----0000100-------------1001----", &Visitor::UMULL)     }, // ARMv3M

    // Multiply (Halfword) instructions
    { "SMLALXY",             MakeMatcher<0>("----00010100------------1--0----", &Visitor::SMLALxy)   }, // ARMv5xP
    { "SMLAXY",              MakeMatcher<0>("----00010000------------1--0----", &Visitor::SMLAxy)    }, // ARMv5xP
    { "SMULXY",              MakeMatcher<0>("----00010110----0000----1--0----", &Visitor::SMULxy)    }, // ARMv5xP

    // Multiply (Word by Halfword) instructions
    { "SMLAWY",              MakeMatcher<0>("----00010010------------1-00----", &Visitor::SMLAWy)    }, // ARMv5xP
    { "SMULWY",              MakeMatcher<0>("----00010010----0000----1-10----", &Visitor::SMULWy)    }, // ARMv5xP

    // Multiply (Most Significant Word) instructions
    { "SMMUL",               MakeMatcher<0>("----01110101----1111----00-1----", &Visitor::SMMUL)     }, // ARMv6
    { "SMMLA",               MakeMatcher<0>("----01110101------------00-1----", &Visitor::SMMLA)     }, // ARMv6
    { "SMMLS",               MakeMatcher<0>("----01110101------------11-1----", &Visitor::SMMLS)     }, // ARMv6

    // Multiply (Dual) instructions
    { "SMLAD",               MakeMatcher<0>("----01110000------------00-1----", &Visitor::SMLAD)     }, // ARMv6
    { "SMLALD",              MakeMatcher<0>("----01110100------------00-1----", &Visitor::SMLALD)    }, // ARMv6
    { "SMLSD",               MakeMatcher<0>("----01110000------------01-1----", &Visitor::SMLSD)     }, // ARMv6
    { "SMLSLD",              MakeMatcher<0>("----01110100------------01-1----", &Visitor::SMLSLD)    }, // ARMv6
    { "SMUAD",               MakeMatcher<0>("----01110000----1111----00-1----", &Visitor::SMUAD)     }, // ARMv6
    { "SMUSD",               MakeMatcher<0>("----01110000----1111----01-1----", &Visitor::SMUSD)     }, // ARMv6

    // Parallel Add/Subtract (Modulo) instructions
    { "SADD8",               MakeMatcher<0>("----01100001--------11111001----", &Visitor::SADD8)     }, // ARMv6
    { "SADD16",              MakeMatcher<0>("----01100001--------11110001----", &Visitor::SADD16)    }, // ARMv6
    { "SASX",                MakeMatcher<0>("----01100001--------11110011----", &Visitor::SASX)      }, // ARMv6
    { "SSAX",                MakeMatcher<0>("----01100001--------11110101----", &Visitor::SSAX)      }, // ARMv6
    { "SSUB8",               MakeMatcher<0>("----01100001--------11111111----", &Visitor::SSUB8)     }, // ARMv6
    { "SSUB16",              MakeMatcher<0>("----01100001--------11110111----", &Visitor::SSUB16)    }, // ARMv6
    { "UADD8",               MakeMatcher<0>("----01100101--------11111001----", &Visitor::UADD8)     }, // ARMv6
    { "UADD16",              MakeMatcher<0>("----01100101--------11110001----", &Visitor::UADD16)    }, // ARMv6
    { "UASX",                MakeMatcher<0>("----01100101--------11110011----", &Visitor::UASX)      }, // ARMv6
    { "USAX",                MakeMatcher<0>("----01100101--------11110101----", &Visitor::USAX)      }, // ARMv6
    { "USUB8",               MakeMatcher<0>("----01100101--------11111111----", &Visitor::USUB8)     }, // ARMv6
    { "USUB16",              MakeMatcher<0>("----01100101--------11110111----", &Visitor::USUB16)    }, // ARMv6

    // Parallel Add/Subtract (Saturating) instructions
    { "QADD8",               MakeMatcher<0>("----01100010--------11111001----", &Visitor::QADD8)     }, // ARMv6
    { "QADD16",              MakeMatcher<0>("----01100010--------11110001----", &Visitor::QADD16)    }, // ARMv6
    { "QASX",                MakeMatcher<0>("----01100010--------11110011----", &Visitor::QASX)      }, // ARMv6
    { "QSAX",                MakeMatcher<0>("----01100010--------11110101----", &Visitor::QSAX)      }, // ARMv6
    { "QSUB8",               MakeMatcher<0>("----01100010--------11111111----", &Visitor::QSUB8)     }, // ARMv6
    { "QSUB16",              MakeMatcher<0>("----01100010--------11110111----", &Visitor::QSUB16)    }, // ARMv6
    { "UQADD8",              MakeMatcher<0>("----01100110--------11111001----", &Visitor::UQADD8)    }, // ARMv6
    { "UQADD16",             MakeMatcher<0>("----01100110--------11110001----", &Visitor::UQADD16)   }, // ARMv6
    { "UQASX",               MakeMatcher<0>("----01100110--------11110011----", &Visitor::UQASX)     }, // ARMv6
    { "UQSAX",               MakeMatcher<0>("----01100110--------11110101----", &Visitor::UQSAX)     }, // ARMv6
    { "UQSUB8",              MakeMatcher<0>("----01100110--------11111111----", &Visitor::UQSUB8)    }, // ARMv6
    { "UQSUB16",             MakeMatcher<0>("----01100110--------11110111----", &Visitor::UQSUB16)   }, // ARMv6

    // Parallel Add/Subtract (Halving) instructions
    { "SHADD8",              MakeMatcher<0>("----01100011--------11111001----", &Visitor::SHADD8)    }, // ARMv6
    { "SHADD16",             MakeMatcher<0>("----01100011--------11110001----", &Visitor::SHADD16)   }, // ARMv6
    { "SHASX",               MakeMatcher<0>("----01100011--------11110011----", &Visitor::SHASX)     }, // ARMv6
    { "SHSAX",               MakeMatcher<0>("----01100011--------11110101----", &Visitor::SHSAX)     }, // ARMv6
    { "SHSUB8",              MakeMatcher<0>("----01100011--------11111111----", &Visitor::SHSUB8)    }, // ARMv6
    { "SHSUB16",             MakeMatcher<0>("----01100011--------11110111----", &Visitor::SHSUB16)   }, // ARMv6
    { "UHADD8",              MakeMatcher<0>("----01100111--------11111001----", &Visitor::UHADD8)    }, // ARMv6
    { "UHADD16",             MakeMatcher<0>("----01100111--------11110001----", &Visitor::UHADD16)   }, // ARMv6
    { "UHASX",               MakeMatcher<0>("----01100111--------11110011----", &Visitor::UHASX)     }, // ARMv6
    { "UHSAX",               MakeMatcher<0>("----01100111--------11110101----", &Visitor::UHSAX)     }, // ARMv6
    { "UHSUB8",              MakeMatcher<0>("----01100111--------11111111----", &Visitor::UHSUB8)    }, // ARMv6
    { "UHSUB16",             MakeMatcher<0>("----01100111--------11110111----", &Visitor::UHSUB16)   }, // ARMv6

    // Saturated Add/Subtract instructions
    { "QADD",                MakeMatcher<0>("----00010000--------00000101----", &Visitor::QADD)      }, // ARMv5xP
    { "QSUB",                MakeMatcher<0>("----00010010--------00000101----", &Visitor::QSUB)      }, // ARMv5xP
    { "QDADD",               MakeMatcher<0>("----00010100--------00000101----", &Visitor::QDADD)     }, // ARMv5xP
    { "QDSUB",               MakeMatcher<0>("----00010110--------00000101----", &Visitor::QDSUB)     }, // ARMv5xP

    // Status Register Access instructions
    { "CPS",                 MakeMatcher<0>("111100010000---00000000---0-----", &Visitor::CPS)       }, // ARMv6
    { "SETEND",              MakeMatcher<1>("1111000100000001000000e000000000", &Visitor::SETEND)    }, // ARMv6
    { "MRS",                 MakeMatcher<0>("----00010-00--------00--00000000", &Visitor::MRS)       }, // ARMv3
    { "MSR",                 MakeMatcher<0>("----00-10-10----1111------------", &Visitor::MSR)       }, // ARMv3
    { "RFE",                 MakeMatcher<0>("----0001101-0000---------110----", &Visitor::RFE)       }, // ARMv6
    { "SRS",                 MakeMatcher<0>("0000011--0-00000000000000001----", &Visitor::SRS)       }, // ARMv6
}};

boost::optional<const Instruction&> DecodeArm(u32 i) {
    auto iterator = std::find_if(arm_instruction_table.cbegin(), arm_instruction_table.cend(), [i](const Instruction& instruction) {
        return instruction.Match(i);
    });

    return iterator != arm_instruction_table.cend() ? boost::make_optional<const Instruction&>(*iterator) : boost::none;
}

};
