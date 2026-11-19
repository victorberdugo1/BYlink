#include "optimizer/optimizer.h"

#include <unordered_map>
#include <vector>

namespace bylink::ir {

namespace {

std::optional<Value> foldInstr(const Instr& in) {
  for (const Value& v : in.operands) {
    if (!v.isConst) return std::nullopt;
  }
  switch (in.op) {
    case Opcode::Binary: {
      auto bits = evalBinary(in.bin, in.type, in.operands[0].bits, in.operands[1].bits);
      if (!bits) return std::nullopt;
      return Value::constant(in.type, *bits);
    }
    case Opcode::Compare:
      return Value::constant(Type::Bool, evalCompare(in.cmp, in.type, in.operands[0].bits, in.operands[1].bits) ? 1 : 0);
    case Opcode::Cast:
      return Value::constant(in.type, evalCast(in.fromType, in.type, in.operands[0].bits));
    default:
      return std::nullopt;
  }
}

bool foldConstants(Function& f) {
  std::unordered_map<uint32_t, Value> known;
  bool any = false;
  for (bool folded = true; folded;) {
    folded = false;
    for (Block& block : f.blocks) {
      std::vector<Instr> kept;
      for (Instr& in : block.instrs) {
        for (Value& v : in.operands) {
          auto it = known.find(v.reg);
          if (!v.isConst && it != known.end()) v = it->second;
        }
        auto value = foldInstr(in);
        if (value) {
          known[*in.dst] = *value;
          folded = true;
          any = true;
        } else {
          kept.push_back(std::move(in));
        }
      }
      block.instrs = std::move(kept);
    }
  }
  return any;
}

bool simplifyBranches(Function& f) {
  bool changed = false;
  for (Block& block : f.blocks) {
    Instr& term = block.instrs.back();
    if (term.op != Opcode::CondBr || !term.operands[0].isConst) continue;
    uint32_t target = term.operands[0].bits != 0 ? term.target0 : term.target1;
    term = Instr{};
    term.op = Opcode::Br;
    term.target0 = target;
    changed = true;
  }
  return changed;
}

std::vector<uint32_t> successors(const Instr& term) {
  if (term.op == Opcode::Br) return {term.target0};
  if (term.op == Opcode::CondBr) return {term.target0, term.target1};
  return {};
}

bool removeUnreachable(Function& f) {
  std::vector<bool> reachable(f.blocks.size(), false);
  std::vector<uint32_t> work{0};
  reachable[0] = true;
  while (!work.empty()) {
    uint32_t b = work.back();
    work.pop_back();
    for (uint32_t s : successors(f.blocks[b].instrs.back())) {
      if (!reachable[s]) {
        reachable[s] = true;
        work.push_back(s);
      }
    }
  }
  std::vector<uint32_t> newIndex(f.blocks.size(), 0);
  std::vector<Block> kept;
  for (size_t b = 0; b < f.blocks.size(); ++b) {
    if (!reachable[b]) continue;
    newIndex[b] = static_cast<uint32_t>(kept.size());
    kept.push_back(std::move(f.blocks[b]));
  }
  if (kept.size() == f.blocks.size()) {
    f.blocks = std::move(kept);
    return false;
  }
  for (Block& block : kept) {
    Instr& term = block.instrs.back();
    if (term.op == Opcode::Br || term.op == Opcode::CondBr) term.target0 = newIndex[term.target0];
    if (term.op == Opcode::CondBr) term.target1 = newIndex[term.target1];
  }
  f.blocks = std::move(kept);
  return true;
}

void optimizeFunction(Function& f) {
  bool changed = true;
  while (changed) {
    changed = foldConstants(f);
    changed |= simplifyBranches(f);
    changed |= removeUnreachable(f);
  }
}

}

void optimize(Module& module) {
  for (Function& f : module.functions) optimizeFunction(f);
}

}
