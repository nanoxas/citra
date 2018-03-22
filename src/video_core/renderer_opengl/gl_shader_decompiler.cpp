// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <map>
#include <set>
#include <string>
#include <nihstro/shader_bytecode.h>
#include <queue>
#include "common/assert.h"
#include "common/common_types.h"
#include "video_core/renderer_opengl/gl_shader_decompiler.h"

namespace Pica {
namespace Shader {
namespace Decompiler {

using nihstro::Instruction;
using nihstro::OpCode;
using nihstro::RegisterType;
using nihstro::SourceRegister;
using nihstro::SwizzlePattern;

template <SwizzlePattern::Selector (SwizzlePattern::*getter)(int) const>
std::string GetSelectorSrc(const SwizzlePattern& pattern) {
    std::string out;
    for (std::size_t i = 0; i < 4; ++i) {
        switch ((pattern.*getter)(i)) {
        case SwizzlePattern::Selector::x:
            out += "x";
            break;
        case SwizzlePattern::Selector::y:
            out += "y";
            break;
        case SwizzlePattern::Selector::z:
            out += "z";
            break;
        case SwizzlePattern::Selector::w:
            out += "w";
            break;
        default:
            UNREACHABLE();
            return "";
        }
    }
    return out;
}

constexpr auto GetSelectorSrc1 = GetSelectorSrc<&SwizzlePattern::GetSelectorSrc1>;
constexpr auto GetSelectorSrc2 = GetSelectorSrc<&SwizzlePattern::GetSelectorSrc2>;
constexpr auto GetSelectorSrc3 = GetSelectorSrc<&SwizzlePattern::GetSelectorSrc3>;

std::string GetCommonDeclarations() {
    return R"(
struct pica_uniforms {
    bool b[16];
    uvec4 i[4];
    vec4 f[96];
};

bool exec_shader();

)";
}

constexpr u32 PROGRAM_END = MAX_PROGRAM_CODE_LENGTH;
constexpr bool PRINT_DEBUG = true;

class ShaderWriter {
public:
    void AddLine(const std::string& text) {
        ASSERT(scope >= 0);
        if (PRINT_DEBUG && !text.empty()) {
            shader_source += std::string(static_cast<size_t>(scope) * 4, ' ');
        }
        shader_source += text + '\n';
    }

    std::string GetResult() const {
        return shader_source;
    }

    int scope = 0;

private:
    std::string shader_source;
};

class Impl {
private:
    enum class ExitMethod {
        Undetermined,
        AlwaysReturn,
        Conditional,
        AlwaysEnd,
    };

    static ExitMethod ParallelExit(ExitMethod a, ExitMethod b) {
        if (a == ExitMethod::Undetermined) {
            ASSERT_MSG(b != ExitMethod::Undetermined, "Undetermined parellel exit");
            return b;
        }
        if (b == ExitMethod::Undetermined) {
            return a;
        }
        if (a == b) {
            return a;
        }
        return ExitMethod::Conditional;
    }

    static ExitMethod SeriesExit(ExitMethod a, ExitMethod b) {
        // This should be handled before evaluating b
        ASSERT(a != ExitMethod::AlwaysEnd);

        if (a == ExitMethod::Undetermined || a == ExitMethod::AlwaysReturn) {
            return b;
        }
        return ExitMethod::Conditional;
    }

    std::map<std::pair<u32, u32>, ExitMethod> exit_method_map;
    ExitMethod ScanExitMethod(u32 begin, u32 end) {
        auto [iter, inserted] =
            exit_method_map.emplace(std::make_pair(begin, end), ExitMethod::Undetermined);
        ExitMethod& exit_method = iter->second;
        if (!inserted)
            return exit_method;

        u32 offset = begin;
        for (u32 offset = begin; offset < (begin > end ? PROGRAM_END : end); ++offset) {
            const Instruction instr = {program_code[offset]};
            switch (instr.opcode.Value()) {
            case OpCode::Id::END: {
                return exit_method = ExitMethod::AlwaysEnd;
            }
            case OpCode::Id::JMPC:
            case OpCode::Id::JMPU: {
                ExitMethod no_jmp = ScanExitMethod(offset + 1, end);
                ExitMethod jmp = ScanExitMethod(instr.flow_control.dest_offset, end);
                return exit_method = ParallelExit(no_jmp, jmp);
            }
            case OpCode::Id::CALL: {
                ExitMethod call = ScanExitMethod(instr.flow_control.dest_offset,
                                                 instr.flow_control.dest_offset +
                                                     instr.flow_control.num_instructions);
                ASSERT_MSG(call != ExitMethod::Undetermined, "Recursive function call detected");
                if (call == ExitMethod::AlwaysEnd)
                    return exit_method = ExitMethod::AlwaysEnd;
                ExitMethod after_call = ScanExitMethod(offset + 1, end);
                return exit_method = SeriesExit(call, after_call);
            }
            case OpCode::Id::LOOP: {
                ExitMethod loop = ScanExitMethod(offset + 1, instr.flow_control.dest_offset + 1);
                if (loop == ExitMethod::AlwaysEnd)
                    return exit_method = ExitMethod::AlwaysEnd;
                ExitMethod after_loop = ScanExitMethod(instr.flow_control.dest_offset + 1, end);
                return exit_method = SeriesExit(loop, after_loop);
            }
            case OpCode::Id::CALLC:
            case OpCode::Id::CALLU: {
                ExitMethod call = ScanExitMethod(instr.flow_control.dest_offset,
                                                 instr.flow_control.dest_offset +
                                                     instr.flow_control.num_instructions);
                ASSERT_MSG(call != ExitMethod::Undetermined, "Recursive function call detected");
                ExitMethod after_call = ScanExitMethod(offset + 1, end);
                return exit_method =
                           SeriesExit(ParallelExit(call, ExitMethod::AlwaysReturn), after_call);
            }
            case OpCode::Id::IFU:
            case OpCode::Id::IFC: {
                ExitMethod exit_if = ScanExitMethod(offset + 1, instr.flow_control.dest_offset);
                ExitMethod exit_else = ScanExitMethod(instr.flow_control.dest_offset,
                                                      instr.flow_control.dest_offset +
                                                          instr.flow_control.num_instructions);
                ExitMethod both = ParallelExit(exit_if, exit_else);
                if (both == ExitMethod::AlwaysEnd)
                    return exit_method = ExitMethod::AlwaysEnd;
                ExitMethod after_call = ScanExitMethod(
                    instr.flow_control.dest_offset + instr.flow_control.num_instructions, end);
                return exit_method = SeriesExit(both, after_call);
            }
            }
        }
        return exit_method = ExitMethod::AlwaysReturn;
    }

    struct Subroutine {
        Subroutine(u32 begin_, u32 end_) : begin(begin_), end(end_) {}

        std::string GetName() const {
            return "sub_" + std::to_string(begin) + "_" + std::to_string(end);
        }

        u32 begin;
        u32 end;

        std::set<std::pair<u32, u32>> discovered;
        ExitMethod exit_method;

        std::set<u32> labels;
    };

    std::map<std::pair<u32, u32>, Subroutine> subroutines;

    /**
     * Gets the Subroutine object corresponding to the specified address. If it is not in the
     * subroutine list yet, adds it to the list
     */
    Subroutine& GetOrAddRoutine(u32 begin, u32 end) {
        auto [iter, inserted] =
            subroutines.emplace(std::make_pair(std::make_pair(begin, end), Subroutine{begin, end}));
        auto& sub = iter->second;
        if (inserted) {
            sub.exit_method = exit_method_map.at(std::make_pair(begin, end));
            ASSERT_MSG(sub.exit_method != ExitMethod::Undetermined,
                       "Undetermined exit in subroutine");
        }
        return sub;
    }

    /**
     * Gets the Subroutine object corresponding to the specified address.
     */
    const Subroutine& GetRoutine(u32 begin, u32 end) const {
        return subroutines.at(std::make_pair(begin, end));
    }

    Subroutine& AnalyzeControlFlow() {
        ASSERT(ScanExitMethod(main_offset, PROGRAM_END) == ExitMethod::AlwaysEnd);
        auto& program_main = GetOrAddRoutine(main_offset, PROGRAM_END);

        std::queue<std::tuple<u32, u32, Subroutine*>> discover_queue;
        discover_queue.emplace(main_offset, PROGRAM_END, &program_main);

        while (!discover_queue.empty()) {
            u32 begin;
            u32 end;
            Subroutine* routine;
            std::tie(begin, end, routine) = discover_queue.front();
            discover_queue.pop();

            if (!routine->discovered.emplace(begin, end).second) {
                continue;
            }

            for (u32 offset = begin; offset < (begin > end ? PROGRAM_END : end); ++offset) {
                const Instruction instr = {program_code[offset]};
                switch (instr.opcode.Value()) {
                case OpCode::Id::END: {
                    // Breaks the outer for loop if the program ends
                    offset = PROGRAM_END;
                    break;
                }

                case OpCode::Id::JMPC:
                case OpCode::Id::JMPU: {
                    routine->labels.insert(instr.flow_control.dest_offset);
                    discover_queue.emplace(instr.flow_control.dest_offset, routine->end, routine);
                    break;
                }

                case OpCode::Id::CALL:
                case OpCode::Id::CALLU:
                case OpCode::Id::CALLC: {
                    std::pair<u32, u32> sub_range{instr.flow_control.dest_offset,
                                                  instr.flow_control.dest_offset +
                                                      instr.flow_control.num_instructions};

                    auto& sub = GetOrAddRoutine(sub_range.first, sub_range.second);

                    discover_queue.emplace(sub_range.first, sub_range.second, &sub);

                    if (instr.opcode.Value() == OpCode::Id::CALL &&
                        sub.exit_method == ExitMethod::AlwaysEnd) {
                        // Breaks the outer for loop if the unconditial subroutine ends the program
                        offset = PROGRAM_END;
                    }
                    break;
                }

                case OpCode::Id::IFU:
                case OpCode::Id::IFC: {
                    const u32 if_offset = offset + 1;
                    const u32 else_offset = instr.flow_control.dest_offset;
                    const u32 endif_offset =
                        instr.flow_control.dest_offset + instr.flow_control.num_instructions;
                    ASSERT(else_offset > if_offset);

                    // Both branches are treated as subroutine, so skips the if-else block in this
                    // routine
                    offset = endif_offset - 1;

                    auto& sub_if = GetOrAddRoutine(if_offset, else_offset);

                    discover_queue.emplace(if_offset, else_offset, &sub_if);

                    if (instr.flow_control.num_instructions != 0) {
                        auto& sub_else = GetOrAddRoutine(else_offset, endif_offset);

                        discover_queue.emplace(else_offset, endif_offset, &sub_else);

                        if (sub_if.exit_method == ExitMethod::AlwaysEnd &&
                            sub_else.exit_method == ExitMethod::AlwaysEnd) {
                            // Breaks the outer for loop if both branches end the program
                            offset = PROGRAM_END;
                        }
                    }
                    break;
                }

                case OpCode::Id::LOOP: {
                    std::pair<u32, u32> sub_range{offset + 1, instr.flow_control.dest_offset + 1};
                    ASSERT(sub_range.second > sub_range.first);

                    auto& sub = GetOrAddRoutine(sub_range.first, sub_range.second);

                    discover_queue.emplace(sub_range.first, sub_range.second, &sub);

                    // The lopp block is treated as subroutine, so skips the if-else block in this
                    // routine.
                    // Note: the first instruction after the loop block is at dest_offset + 1 (due
                    // to PICA design), and the next instruction to scan is at offset + 1 (due to
                    // for loop increment). The two offset-by-one cancel each other here.
                    offset = instr.flow_control.dest_offset;

                    if (sub.exit_method == ExitMethod::AlwaysEnd) {
                        // Breaks the outer for loop if the loop block ends the program
                        offset = PROGRAM_END;
                    }
                    break;
                }
                }
            }
        }
        return program_main;
    }

    /// Generates condition evaluation code for the flow control instruction.
    static std::string EvaluateCondition(Instruction::FlowControlType flow_control) {
        using Op = Instruction::FlowControlType::Op;

        std::string result_x =
            flow_control.refx.Value() ? "conditional_code.x" : "!conditional_code.x";
        std::string result_y =
            flow_control.refy.Value() ? "conditional_code.y" : "!conditional_code.y";

        switch (flow_control.op) {
        case Op::JustX:
            return result_x;
        case Op::JustY:
            return result_y;
        case Op::Or:
        case Op::And: {
            std::string and_or = flow_control.op == Op::Or ? "any" : "all";
            std::string bvec;
            if (flow_control.refx.Value() && flow_control.refy.Value()) {
                bvec = "conditional_code";
            } else if (!flow_control.refx.Value() && !flow_control.refy.Value()) {
                bvec = "not(conditional_code)";
            } else {
                bvec = "bvec2(" + result_x + ", " + result_y + ")";
            }
            return and_or + "(" + bvec + ")";
        }
        default:
            UNREACHABLE();
            return "";
        }
    };

    /// Generates code representing the source register.
    std::string GetSourceRegister(const SourceRegister& source_reg,
                                  u32 address_register_index) const {
        u32 index = static_cast<u32>(source_reg.GetIndex());
        std::string index_str = std::to_string(index);

        switch (source_reg.GetRegisterType()) {
        case RegisterType::Input:
            return inputreg_getter(index);
        case RegisterType::Temporary:
            return "reg_tmp" + index_str;
        case RegisterType::FloatUniform:
            if (address_register_index != 0) {
                index_str +=
                    std::string(" + address_registers.") + "xyz"[address_register_index - 1];
            }
            return "uniforms.f[" + index_str + "]";
        default:
            UNREACHABLE();
            return "";
        }
    };

    std::string GetUniformBool(u32 index) const {
        if (is_gs && index == 15) {
            // The uniform b15 is set to true after every geometry shader invocation.
            return "((gl_PrimitiveIDIn == 0) || uniforms.b[15])";
        }
        return "uniforms.b[" + std::to_string(index) + "]";
    };

    /**
     * Adds code that calls a subroutine.
     * @param shader a ShaderWriter object to write GLSL code.
     * @param subroutine the subroutine to call.
     */
    static void CallSubroutine(ShaderWriter& shader, const Subroutine& subroutine) {
        if (subroutine.exit_method == ExitMethod::AlwaysEnd) {
            shader.AddLine(subroutine.GetName() + "();");
            shader.AddLine("return true;");
        } else if (subroutine.exit_method == ExitMethod::Conditional) {
            shader.AddLine("if (" + subroutine.GetName() + "()) { return true; }");
        } else {
            shader.AddLine(subroutine.GetName() + "();");
        }
    };

    /**
     * Compiles a single instruction from PICA to GLSL.
     * @param shader a ShaderWriter object to write GLSL code.
     * @param offset the offset of the PICA shader instruction.
     * @return the offset of the next instruction to execute. Usually it is the current offset + 1.
     * If the current instruction is IF or LOOP, the next instruction is after the IF or LOOP block.
     * If the current instruction always terminates the program, returns PROGRAM_END.
     */
    u32 CompileInstr(ShaderWriter& shader, u32 offset) const {
        const Instruction instr = {program_code[offset]};

        size_t swizzle_offset = instr.opcode.Value().GetInfo().type == OpCode::Type::MultiplyAdd
                                    ? instr.mad.operand_desc_id
                                    : instr.common.operand_desc_id;
        const SwizzlePattern swizzle = {swizzle_data[swizzle_offset]};

        if (PRINT_DEBUG) {
            shader.AddLine("// " + std::to_string(offset) + ": " +
                           instr.opcode.Value().GetInfo().name);
        }

        auto set_dest = [&shader, &swizzle](const std::string& reg, const std::string& value,
                                            u32 dest_num_components, u32 value_num_components) {
            u32 dest_mask_num_components = 0;
            std::string dest_mask_swizzle = ".";

            for (u32 i = 0; i < dest_num_components; ++i) {
                if (swizzle.DestComponentEnabled(static_cast<int>(i))) {
                    dest_mask_swizzle += "xyzw"[i];
                    ++dest_mask_num_components;
                }
            }

            if (reg.empty() || dest_mask_num_components == 0) {
                return;
            }
            ASSERT(value_num_components >= dest_num_components || value_num_components == 1);

            std::string dest = reg + (dest_num_components != 1 ? dest_mask_swizzle : "");

            std::string src = value;
            if (value_num_components == 1) {
                if (dest_mask_num_components != 1) {
                    src = "vec" + std::to_string(dest_mask_num_components) + "(" + value + ")";
                }
            } else if (value_num_components != dest_mask_num_components) {
                src = "(" + value + ")" + dest_mask_swizzle;
            }

            shader.AddLine(dest + " = " + src + ";");
        };

        switch (instr.opcode.Value().GetInfo().type) {
        case OpCode::Type::Arithmetic: {
            const bool is_inverted =
                (0 != (instr.opcode.Value().GetInfo().subtype & OpCode::Info::SrcInversed));

            std::string src1 = swizzle.negate_src1 ? "-" : "";
            src1 += GetSourceRegister(instr.common.GetSrc1(is_inverted),
                                      !is_inverted * instr.common.address_register_index);
            src1 += "." + GetSelectorSrc1(swizzle);

            std::string src2 = swizzle.negate_src2 ? "-" : "";
            src2 += GetSourceRegister(instr.common.GetSrc2(is_inverted),
                                      is_inverted * instr.common.address_register_index);
            src2 += "." + GetSelectorSrc2(swizzle);

            std::string dest_reg =
                (instr.common.dest.Value() < 0x10)
                    ? outputreg_getter(static_cast<u32>(instr.common.dest.Value().GetIndex()))
                    : (instr.common.dest.Value() < 0x20)
                          ? "reg_tmp" + std::to_string(instr.common.dest.Value().GetIndex())
                          : "";

            switch (instr.opcode.Value().EffectiveOpCode()) {
            case OpCode::Id::ADD: {
                set_dest(dest_reg, src1 + " + " + src2, 4, 4);
                break;
            }

            case OpCode::Id::MUL: {
                if (sanitize_mul) {
                    set_dest(dest_reg, "sanitize_mul(" + src1 + ", " + src2 + ")", 4, 4);
                } else {
                    set_dest(dest_reg, src1 + " * " + src2, 4, 4);
                }
                break;
            }

            case OpCode::Id::FLR: {
                set_dest(dest_reg, "floor(" + src1 + ")", 4, 4);
                break;
            }

            case OpCode::Id::MAX: {
                set_dest(dest_reg, "max(" + src1 + ", " + src2 + ")", 4, 4);
                break;
            }

            case OpCode::Id::MIN: {
                set_dest(dest_reg, "min(" + src1 + ", " + src2 + ")", 4, 4);
                break;
            }

            case OpCode::Id::DP3:
            case OpCode::Id::DP4:
            case OpCode::Id::DPH:
            case OpCode::Id::DPHI: {
                OpCode::Id opcode = instr.opcode.Value().EffectiveOpCode();
                std::string dot;
                if (opcode == OpCode::Id::DP3) {
                    if (sanitize_mul) {
                        dot = "dot(vec3(sanitize_mul(" + src1 + ", " + src2 + ")), vec3(1.0))";
                    } else {
                        dot = "dot(vec3(" + src1 + "), vec3(" + src2 + "))";
                    }
                } else {
                    std::string src1_ = (opcode == OpCode::Id::DPH || opcode == OpCode::Id::DPHI)
                                            ? "vec4(" + src1 + ".xyz, 1.0)"
                                            : src1;
                    if (sanitize_mul) {
                        dot = "dot(sanitize_mul(" + src1_ + ", " + src2 + "), vec4(1.0))";
                    } else {
                        dot = "dot(" + src1 + ", " + src2 + ")";
                    }
                }

                set_dest(dest_reg, dot, 4, 1);
                break;
            }

            case OpCode::Id::RCP: {
                set_dest(dest_reg, "(1.0 / " + src1 + ".x)", 4, 1);
                break;
            }

            case OpCode::Id::RSQ: {
                set_dest(dest_reg, "inversesqrt(" + src1 + ".x)", 4, 1);
                break;
            }

            case OpCode::Id::MOVA: {
                set_dest("address_registers", "ivec2(" + src1 + ")", 2, 2);
                break;
            }

            case OpCode::Id::MOV: {
                set_dest(dest_reg, src1, 4, 4);
                break;
            }

            case OpCode::Id::SGE:
            case OpCode::Id::SGEI: {
                set_dest(dest_reg, "vec4(greaterThanEqual(" + src1 + "," + src2 + "))", 4, 4);
                break;
            }

            case OpCode::Id::SLT:
            case OpCode::Id::SLTI: {
                set_dest(dest_reg, "vec4(lessThan(" + src1 + "," + src2 + "))", 4, 4);
                break;
            }

            case OpCode::Id::CMP: {
                using CompareOp = Instruction::Common::CompareOpType::Op;
                const std::map<CompareOp, std::pair<std::string, std::string>> cmp_ops{
                    {CompareOp::Equal, {"==", "equal"}},
                    {CompareOp::NotEqual, {"!=", "notEqual"}},
                    {CompareOp::LessThan, {"<", "lessThan"}},
                    {CompareOp::LessEqual, {"<=", "lessThanEqual"}},
                    {CompareOp::GreaterThan, {">", "greaterThan"}},
                    {CompareOp::GreaterEqual, {">=", "greaterThanEqual"}}};

                const CompareOp op_x = instr.common.compare_op.x.Value();
                const CompareOp op_y = instr.common.compare_op.y.Value();

                if (cmp_ops.find(op_x) == cmp_ops.end()) {
                    LOG_ERROR(HW_GPU, "Unknown compare mode %x", static_cast<int>(op_x));
                } else if (cmp_ops.find(op_y) == cmp_ops.end()) {
                    LOG_ERROR(HW_GPU, "Unknown compare mode %x", static_cast<int>(op_y));
                } else if (op_x != op_y) {
                    shader.AddLine("conditional_code.x = " + src1 + ".x " +
                                   cmp_ops.find(op_x)->second.first + " " + src2 + ".x;");
                    shader.AddLine("conditional_code.y = " + src1 + ".y " +
                                   cmp_ops.find(op_y)->second.first + " " + src2 + ".y;");
                } else {
                    shader.AddLine("conditional_code = " + cmp_ops.find(op_x)->second.second +
                                   "(vec2(" + src1 + "), vec2(" + src2 + "));");
                }
                break;
            }

            case OpCode::Id::EX2: {
                set_dest(dest_reg, "exp2(" + src1 + ".x)", 4, 1);
                break;
            }

            case OpCode::Id::LG2: {
                set_dest(dest_reg, "log2(" + src1 + ".x)", 4, 1);
                break;
            }

            default: {
                LOG_ERROR(HW_GPU, "Unhandled arithmetic instruction: 0x%02x (%s): 0x%08x",
                          (int)instr.opcode.Value().EffectiveOpCode(),
                          instr.opcode.Value().GetInfo().name, instr.hex);
                DEBUG_ASSERT(false);
                break;
            }
            }

            break;
        }

        case OpCode::Type::MultiplyAdd: {
            if ((instr.opcode.Value().EffectiveOpCode() == OpCode::Id::MAD) ||
                (instr.opcode.Value().EffectiveOpCode() == OpCode::Id::MADI)) {
                bool is_inverted = (instr.opcode.Value().EffectiveOpCode() == OpCode::Id::MADI);

                std::string src1 = swizzle.negate_src1 ? "-" : "";
                src1 += GetSourceRegister(instr.mad.GetSrc1(is_inverted), 0);
                src1 += "." + GetSelectorSrc1(swizzle);

                std::string src2 = swizzle.negate_src2 ? "-" : "";
                src2 += GetSourceRegister(instr.mad.GetSrc2(is_inverted),
                                          !is_inverted * instr.mad.address_register_index);
                src2 += "." + GetSelectorSrc2(swizzle);

                std::string src3 = swizzle.negate_src3 ? "-" : "";
                src3 += GetSourceRegister(instr.mad.GetSrc3(is_inverted),
                                          is_inverted * instr.mad.address_register_index);
                src3 += "." + GetSelectorSrc3(swizzle);

                std::string dest_reg =
                    (instr.mad.dest.Value() < 0x10)
                        ? outputreg_getter(static_cast<u32>(instr.mad.dest.Value().GetIndex()))
                        : (instr.mad.dest.Value() < 0x20)
                              ? "reg_tmp" + std::to_string(instr.mad.dest.Value().GetIndex())
                              : "";

                if (sanitize_mul) {
                    set_dest(dest_reg, "sanitize_mul(" + src1 + ", " + src2 + ") + " + src3, 4, 4);
                } else {
                    set_dest(dest_reg, src1 + " * " + src2 + " + " + src3, 4, 4);
                }
            } else {
                LOG_ERROR(HW_GPU, "Unhandled multiply-add instruction: 0x%02x (%s): 0x%08x",
                          (int)instr.opcode.Value().EffectiveOpCode(),
                          instr.opcode.Value().GetInfo().name, instr.hex);
            }
            break;
        }

        default: {
            switch (instr.opcode.Value()) {
            case OpCode::Id::END: {
                shader.AddLine("return true;");
                offset = PROGRAM_END - 1;
                break;
            }

            case OpCode::Id::JMPC:
            case OpCode::Id::JMPU: {
                std::string condition;
                if (instr.opcode.Value() == OpCode::Id::JMPC) {
                    condition = EvaluateCondition(instr.flow_control);
                } else {
                    bool invert_test = instr.flow_control.num_instructions & 1;
                    condition = (invert_test ? "!" : "") +
                                GetUniformBool(instr.flow_control.bool_uniform_id);
                }

                shader.AddLine("if (" + condition + ") {");
                ++shader.scope;
                shader.AddLine("{ jmp_to = " + std::to_string(instr.flow_control.dest_offset) +
                               "u; break; }");

                --shader.scope;
                shader.AddLine("}");
                break;
            }

            case OpCode::Id::CALL:
            case OpCode::Id::CALLC:
            case OpCode::Id::CALLU: {
                std::string condition;
                if (instr.opcode.Value() == OpCode::Id::CALLC) {
                    condition = EvaluateCondition(instr.flow_control);
                } else if (instr.opcode.Value() == OpCode::Id::CALLU) {
                    condition = GetUniformBool(instr.flow_control.bool_uniform_id);
                }

                shader.AddLine(condition.empty() ? "{" : "if (" + condition + ") {");
                ++shader.scope;

                auto& call_sub = GetRoutine(instr.flow_control.dest_offset,
                                            instr.flow_control.dest_offset +
                                                instr.flow_control.num_instructions);

                CallSubroutine(shader, call_sub);
                if (instr.opcode.Value() == OpCode::Id::CALL &&
                    call_sub.exit_method == ExitMethod::AlwaysEnd) {
                    offset = PROGRAM_END - 1;
                }

                --shader.scope;
                shader.AddLine("}");
                break;
            }

            case OpCode::Id::NOP: {
                break;
            }

            case OpCode::Id::IFC:
            case OpCode::Id::IFU: {
                std::string condition;
                if (instr.opcode.Value() == OpCode::Id::IFC) {
                    condition = EvaluateCondition(instr.flow_control);
                } else {
                    condition = GetUniformBool(instr.flow_control.bool_uniform_id);
                }

                const u32 if_offset = offset + 1;
                const u32 else_offset = instr.flow_control.dest_offset;
                const u32 endif_offset =
                    instr.flow_control.dest_offset + instr.flow_control.num_instructions;

                shader.AddLine("if (" + condition + ") {");
                ++shader.scope;

                auto& if_sub = GetRoutine(if_offset, else_offset);
                CallSubroutine(shader, if_sub);
                offset = else_offset - 1;

                if (instr.flow_control.num_instructions != 0) {
                    --shader.scope;
                    shader.AddLine("} else {");
                    ++shader.scope;

                    auto& else_sub = GetRoutine(else_offset, endif_offset);
                    CallSubroutine(shader, else_sub);
                    offset = endif_offset - 1;

                    if (if_sub.exit_method == ExitMethod::AlwaysEnd &&
                        else_sub.exit_method == ExitMethod::AlwaysEnd) {
                        offset = PROGRAM_END - 1;
                    }
                }

                --shader.scope;
                shader.AddLine("}");
                break;
            }

            case OpCode::Id::LOOP: {
                std::string int_uniform =
                    "uniforms.i[" + std::to_string(instr.flow_control.int_uniform_id) + "]";

                shader.AddLine("address_registers.z = int(" + int_uniform + ".y);");

                std::string loop_var = "loop" + std::to_string(offset);
                shader.AddLine("for (uint " + loop_var + " = 0u; " + loop_var +
                               " <= " + int_uniform + ".x; address_registers.z += int(" +
                               int_uniform + ".z), ++" + loop_var + ") {");
                ++shader.scope;

                auto& loop_sub = GetRoutine(offset + 1, instr.flow_control.dest_offset + 1);
                CallSubroutine(shader, loop_sub);
                offset = instr.flow_control.dest_offset;

                --shader.scope;
                shader.AddLine("}");

                if (loop_sub.exit_method == ExitMethod::AlwaysEnd) {
                    offset = PROGRAM_END - 1;
                }

                break;
            }

            case OpCode::Id::EMIT: {
                if (is_gs) {
                    shader.AddLine("emit();");
                }
                break;
            }

            case OpCode::Id::SETEMIT: {
                if (is_gs) {
                    ASSERT(instr.setemit.vertex_id < 3);
                    shader.AddLine("setemit(" + std::to_string(instr.setemit.vertex_id) + "u, " +
                                   ((instr.setemit.prim_emit != 0) ? "true" : "false") + ", " +
                                   ((instr.setemit.winding != 0) ? "true" : "false") + ");");
                }
                break;
            }

            default: {
                LOG_ERROR(HW_GPU, "Unhandled instruction: 0x%02x (%s): 0x%08x",
                          (int)instr.opcode.Value().EffectiveOpCode(),
                          instr.opcode.Value().GetInfo().name, instr.hex);
                break;
            }
            }

            break;
        }
        }
        return offset + 1;
    };

    /**
     * Compiles a range of instructions from PICA to GLSL.
     * @param shader a ShaderWriter object to write GLSL code.
     * @param begin the offset of the starting instruction.
     * @param end the offset where the compilation should stop (exclusive).
     * @return the offset of the next instruction to compile. PROGRAM_END if the program terminates.
     */
    u32 CompileRange(ShaderWriter& shader, u32 begin, u32 end) const {
        u32 program_counter;
        for (program_counter = begin; program_counter < (begin > end ? PROGRAM_END : end);) {
            program_counter = CompileInstr(shader, program_counter);
        }
        return program_counter;
    };

public:
    Impl(const std::array<u32, MAX_PROGRAM_CODE_LENGTH>& program_code,
         const std::array<u32, MAX_SWIZZLE_DATA_LENGTH>& swizzle_data, u32 main_offset,
         const std::function<std::string(u32)>& inputreg_getter,
         const std::function<std::string(u32)>& outputreg_getter, bool sanitize_mul, bool is_gs)
        : program_code(program_code), swizzle_data(swizzle_data), main_offset(main_offset),
          inputreg_getter(inputreg_getter), outputreg_getter(outputreg_getter),
          sanitize_mul(sanitize_mul), is_gs(is_gs) {}

    std::string Decompile() {
        auto& program_main = AnalyzeControlFlow();

        ShaderWriter shader;

        if (sanitize_mul) {
            shader.AddLine("vec4 sanitize_mul(vec4 lhs, vec4 rhs) {");
            ++shader.scope;
            shader.AddLine("vec4 product = lhs * rhs;");
            shader.AddLine("return mix(product, mix(mix(vec4(0.0), product, isnan(rhs)), product, "
                           "isnan(lhs)), isnan(product));");
            --shader.scope;
            shader.AddLine("}\n");
        }

        shader.AddLine("bvec2 conditional_code = bvec2(false);");
        shader.AddLine("ivec3 address_registers = ivec3(0);");
        for (int i = 0; i < 16; ++i) {
            shader.AddLine("vec4 reg_tmp" + std::to_string(i) + " = vec4(0.0, 0.0, 0.0, 1.0);");
        }
        shader.AddLine("");

        for (auto& pair : subroutines) {
            auto& subroutine = pair.second;
            shader.AddLine("bool " + subroutine.GetName() + "();");
        }
        shader.AddLine("");

        shader.AddLine("bool exec_shader() {");
        ++shader.scope;
        CallSubroutine(shader, program_main);
        --shader.scope;
        shader.AddLine("}\n");

        for (auto& pair : subroutines) {
            auto& subroutine = pair.second;

            std::set<u32> labels = subroutine.labels;

            shader.AddLine("bool " + subroutine.GetName() + "() {");
            ++shader.scope;

            if (labels.empty()) {
                if (CompileRange(shader, subroutine.begin, subroutine.end) != PROGRAM_END) {
                    shader.AddLine("return false;");
                }
            } else {
                labels.insert(subroutine.begin);
                shader.AddLine("uint jmp_to = " + std::to_string(subroutine.begin) + "u;");
                shader.AddLine("while (true) {");
                ++shader.scope;

                shader.AddLine("switch (jmp_to) {");

                for (auto label : labels) {
                    shader.AddLine("case " + std::to_string(label) + "u: {");
                    ++shader.scope;

                    auto next_it = labels.lower_bound(label + 1);
                    u32 next_label = next_it == labels.end() ? subroutine.end : *next_it;

                    u32 compile_end = CompileRange(shader, label, next_label);
                    if (compile_end > next_label && compile_end != PROGRAM_END) {
                        // This happens only when there is a label inside a IF/LOOP block
                        shader.AddLine("{ jmp_to = " + std::to_string(compile_end) + "u; break; }");
                        labels.emplace(compile_end);
                    }

                    --shader.scope;
                    shader.AddLine("}");
                }

                shader.AddLine("default: return false;");
                shader.AddLine("}");

                --shader.scope;
                shader.AddLine("}");

                shader.AddLine("return false;");
            }

            --shader.scope;
            shader.AddLine("}\n");

            ASSERT(shader.scope == 0);
        }

        return shader.GetResult();
    }

private:
    const std::array<u32, MAX_PROGRAM_CODE_LENGTH>& program_code;
    const std::array<u32, MAX_SWIZZLE_DATA_LENGTH>& swizzle_data;
    u32 main_offset;
    const std::function<std::string(u32)>& inputreg_getter;
    const std::function<std::string(u32)>& outputreg_getter;
    bool sanitize_mul;
    bool is_gs;
};

std::string DecompileProgram(const std::array<u32, MAX_PROGRAM_CODE_LENGTH>& program_code,
                             const std::array<u32, MAX_SWIZZLE_DATA_LENGTH>& swizzle_data,
                             u32 main_offset,
                             const std::function<std::string(u32)>& inputreg_getter,
                             const std::function<std::string(u32)>& outputreg_getter,
                             bool sanitize_mul, bool is_gs) {
    Impl impl(program_code, swizzle_data, main_offset, inputreg_getter, outputreg_getter,
              sanitize_mul, is_gs);
    return impl.Decompile();
}

} // namespace Decompiler
} // namespace Shader
} // namespace Pica
