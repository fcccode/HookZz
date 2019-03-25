#include "globals.h"

#include "InstructionRelocation/arm64/ARM64InstructionRelocation.h"

#include "core/arch/arm64/registers-arm64.h"
#include "core/modules/assembler/assembler-arm64.h"
#include "core/modules/codegen/codegen-arm64.h"

using namespace zz::arm64;

// Compare and branch.
enum CompareBranchOp {
  CompareBranchFixed     = 0x34000000,
  CompareBranchFixedMask = 0x7E000000,
  CompareBranchMask      = 0xFF000000,
};

// Conditional branch.
enum ConditionalBranchOp {
  ConditionalBranchFixed     = 0x54000000,
  ConditionalBranchFixedMask = 0xFE000000,
  ConditionalBranchMask      = 0xFF000010,
};

typedef struct _PseudoLabelData {
  PseudoLabel label;
  uint64_t address;

public:
  _PseudoLabelData(uint64_t address) {
    address = address;
  }
} PseudoLabelData;

AssemblyCode *GenRelocateCode(void *buffer, int *relocate_size, addr_t from_pc, addr_t to_pc) {

  uint64_t cur_addr    = (uint64_t)buffer;
  uint64_t cur_src_pc  = from_pc;
  uint64_t cur_dest_pc = to_pc;
  uint32_t inst        = *(uint32_t *)cur_addr;

  // std::vector<PseudoLabelData> labels;
  LiteMutableArray *labels = new LiteMutableArray;

  TurboAssembler turbo_assembler_(0);
#define _ turbo_assembler_.
  while (cur_addr < ((uint64_t)buffer + *relocate_size)) {
    int off = turbo_assembler_.GetCodeBuffer()->getSize();

    if ((inst & LoadRegLiteralFixedMask) == LoadRegLiteralFixed) {
      int rt                  = bits(inst, 0, 4);
      int32_t imm19           = bits(inst, 5, 23);
      uint64_t target_address = (imm19 << 2) + cur_src_pc;

      _ AdrpAddMov(X(rt), cur_dest_pc, target_address);
      _ ldr(X(rt), 0);
    } else if ((inst & CompareBranchFixedMask) == CompareBranchFixed) {
      int32_t rt;
      int32_t imm19;
      imm19               = bits(inst, 5, 24);
      
      int offset = (imm19 << 2) + (cur_dest_pc - cur_src_pc);
      imm19 = offset >> 2;
      int32_t compare_branch_instr = (inst & 0xff00001f) | LFT(imm19, 19, 5);
      
      _ Emit(compare_branch_instr);
    } else if ((inst & UnconditionalBranchFixedMask) == UnconditionalBranchFixed) {
      int32_t imm26;
      imm26          = bits(inst, 0, 25);
      
      int32_t offset = (imm26 << 2) + (cur_dest_pc - cur_src_pc);
      imm26 = offset >> 2;
      int32_t unconditional_branch_instr = (inst & 0xfc000000) | LFT(imm26, 26, 0);
      
      _ Emit(unconditional_branch_instr);
    } else if ((inst & ConditionalBranchFixedMask) == ConditionalBranchFixed) {
      int32_t imm19;
      imm19          = bits(inst, 5, 23);
      
      int offset = (imm19 << 2) + (cur_dest_pc - cur_src_pc);
      imm19 = offset >> 2;
      int32_t b_cond_instr = (inst & 0xff00001f) | LFT(imm19, 19, 5);
      
      _ Emit(b_cond_instr);
      
      
    } else {
      // origin write the instruction bytes
      _ Emit(inst);
    }

    // Move to next instruction
    cur_dest_pc += turbo_assembler_.GetCodeBuffer()->getSize() - off;
    cur_src_pc += 4;
    cur_addr += 4;
    inst = *(arm64_inst_t *)cur_addr;
  }

  // Branch to the rest of instructions
  CodeGen codegen(&turbo_assembler_);
  codegen.LiteralLdrBranch(cur_src_pc);
  _ AdrpAddMov(x17, cur_dest_pc, cur_src_pc);
  _ br(x17);

  // Generate executable code
  AssemblyCode *code = AssemblyCode::FinalizeFromTurboAssember(&turbo_assembler_);
  return code;
}