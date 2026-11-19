#include "ir/ir.h"

#include <sstream>

#include "common/version.h"

namespace bylink::ir {

namespace {

const char* binName(BinOp op) {
  switch (op) {
    case BinOp::Add: return "add";
    case BinOp::Sub: return "sub";
    case BinOp::Mul: return "mul";
    case BinOp::Div: return "div";
    case BinOp::Rem: return "rem";
    case BinOp::And: return "and";
    case BinOp::Or: return "or";
    case BinOp::Xor: return "xor";
  }
  return "?";
}

const char* cmpName(CmpOp op) {
  switch (op) {
    case CmpOp::Eq: return "eq";
    case CmpOp::Ne: return "ne";
    case CmpOp::Lt: return "lt";
    case CmpOp::Le: return "le";
    case CmpOp::Gt: return "gt";
    case CmpOp::Ge: return "ge";
  }
  return "?";
}

std::string valueText(const Value& v) {
  if (!v.isConst) return "%" + std::to_string(v.reg);
  if (v.type == Type::Bool) return v.bits != 0 ? "true" : "false";
  if (isSigned(v.type)) return std::to_string(signedValue(v.type, v.bits));
  return std::to_string(v.bits);
}

std::string operandList(const Instr& in) {
  std::string out;
  for (size_t i = 0; i < in.operands.size(); ++i) {
    if (i != 0) out += ", ";
    out += valueText(in.operands[i]);
  }
  return out;
}

std::string instrText(const Instr& in) {
  std::ostringstream out;
  if (in.dst) out << "%" << *in.dst << " = ";
  switch (in.op) {
    case Opcode::Binary: out << binName(in.bin) << " " << typeName(in.type) << " " << operandList(in); break;
    case Opcode::Compare: out << "cmp " << cmpName(in.cmp) << " " << typeName(in.type) << " " << operandList(in); break;
    case Opcode::Cast: out << "cast " << typeName(in.fromType) << " " << operandList(in) << " to " << typeName(in.type); break;
    case Opcode::Load: out << "load " << typeName(in.type) << " $" << in.slot; break;
    case Opcode::Store: out << "store " << typeName(in.type) << " " << operandList(in) << ", $" << in.slot; break;
    case Opcode::Call: {
      out << "call " << typeName(in.type) << " @" << in.callee << "(";
      for (size_t i = 0; i < in.operands.size(); ++i) {
        if (i != 0) out << ", ";
        out << typeName(in.operands[i].type) << " " << valueText(in.operands[i]);
      }
      out << ")";
      break;
    }
    case Opcode::Br: out << "br bb" << in.target0; break;
    case Opcode::CondBr: out << "condbr " << operandList(in) << ", bb" << in.target0 << ", bb" << in.target1; break;
    case Opcode::Ret:
      out << "ret " << typeName(in.type);
      if (!in.operands.empty()) out << " " << operandList(in);
      break;
  }
  return out.str();
}

}

const Function* Module::find(const std::string& functionName) const {
  for (const Function& f : functions) {
    if (f.name == functionName) return &f;
  }
  return nullptr;
}

bool isTerminator(Opcode op) { return op == Opcode::Br || op == Opcode::CondBr || op == Opcode::Ret; }

std::string print(const Module& module) {
  std::ostringstream out;
  out << "byir " << kIRVersion << "\nmodule " << module.name << "\n";
  for (const Function& f : module.functions) {
    out << "\nfunc @" << f.name << "(";
    for (size_t i = 0; i < f.params.size(); ++i) {
      if (i != 0) out << ", ";
      out << typeName(f.params[i]) << " %" << i;
    }
    out << ") -> " << typeName(f.ret) << " {\n";
    for (size_t i = 0; i < f.slots.size(); ++i) out << "  slot $" << i << ": " << typeName(f.slots[i]) << "\n";
    for (size_t b = 0; b < f.blocks.size(); ++b) {
      out << "bb" << b << ":\n";
      for (const Instr& in : f.blocks[b].instrs) out << "  " << instrText(in) << "\n";
    }
    out << "}\n";
  }
  return out.str();
}

}
