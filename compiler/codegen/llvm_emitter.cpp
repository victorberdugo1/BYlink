#include "codegen/llvm_emitter.h"

#include <sstream>
#include <unordered_map>

namespace bylink {

namespace {

using ir::Instr;
using ir::Opcode;
using ir::Value;

std::string llvmType(Type type) {
  if (type == Type::Void) return "void";
  return "i" + std::to_string(bitWidth(type));
}

std::string constText(const Value& v) {
  if (v.type == Type::Bool) return v.bits != 0 ? "true" : "false";
  return std::to_string(signedValue(v.type, v.bits));
}

std::string llvmString(const std::string& text) {
  static const char* hex = "0123456789ABCDEF";
  std::string out;
  for (unsigned char c : text) {
    if (c < 0x20 || c > 0x7E || c == '"' || c == '\\') {
      out += '\\';
      out += hex[c >> 4];
      out += hex[c & 15];
    } else {
      out += static_cast<char>(c);
    }
  }
  return out;
}

std::string functionName(const ir::Module& module, const std::string& name) {
  if (name == "main") return "@main";
  return "@\"" + module.name + "." + name + "\"";
}

const char* binaryMnemonic(BinOp op, Type type) {
  bool sign = isSigned(type);
  switch (op) {
    case BinOp::Add: return "add";
    case BinOp::Sub: return "sub";
    case BinOp::Mul: return "mul";
    case BinOp::Div: return sign ? "sdiv" : "udiv";
    case BinOp::Rem: return sign ? "srem" : "urem";
    case BinOp::And: return "and";
    case BinOp::Or: return "or";
    case BinOp::Xor: return "xor";
  }
  return "?";
}

std::string predicate(CmpOp op, Type type) {
  bool sign = isSigned(type);
  switch (op) {
    case CmpOp::Eq: return "eq";
    case CmpOp::Ne: return "ne";
    case CmpOp::Lt: return sign ? "slt" : "ult";
    case CmpOp::Le: return sign ? "sle" : "ule";
    case CmpOp::Gt: return sign ? "sgt" : "ugt";
    case CmpOp::Ge: return sign ? "sge" : "uge";
  }
  return "?";
}

class FunctionEmitter {
 public:
  FunctionEmitter(const ir::Module& module, const ir::Function& fn, std::ostringstream& out, bool tuned)
      : module_(module), fn_(fn), out_(out), tuned_(tuned) {}

  bool usesTrap() const { return usesTrap_; }

  void run() {
    bool isMain = fn_.name == "main";
    out_ << "define " << (isMain ? "" : "internal ") << llvmType(fn_.ret) << " " << functionName(module_, fn_.name) << "(";
    for (size_t i = 0; i < fn_.params.size(); ++i) {
      if (i != 0) out_ << ", ";
      out_ << llvmType(fn_.params[i]) << " %r" << i;
    }
    out_ << ")" << (tuned_ ? " #0" : "") << " {\n";
    for (size_t b = 0; b < fn_.blocks.size(); ++b) {
      out_ << "bb" << b << ":\n";
      if (b == 0) {
        for (size_t s = 0; s < fn_.slots.size(); ++s) out_ << "  %s" << s << " = alloca " << llvmType(fn_.slots[s]) << "\n";
      }
      for (const Instr& in : fn_.blocks[b].instrs) emit(in);
    }
    out_ << "}\n\n";
  }

 private:
  const ir::Module& module_;
  const ir::Function& fn_;
  std::ostringstream& out_;
  bool tuned_;
  std::unordered_map<uint32_t, std::string> alias_;
  uint32_t labelCounter_ = 0;
  uint32_t tempCounter_ = 0;
  bool usesTrap_ = false;

  std::string text(const Value& v) const {
    if (v.isConst) return constText(v);
    auto it = alias_.find(v.reg);
    return it != alias_.end() ? it->second : "%r" + std::to_string(v.reg);
  }

  std::string temp() { return "%t" + std::to_string(tempCounter_++); }

  void emit(const Instr& in) {
    switch (in.op) {
      case Opcode::Binary: emitBinary(in); break;
      case Opcode::Compare:
        out_ << "  %r" << *in.dst << " = icmp " << predicate(in.cmp, in.type) << " " << llvmType(in.type) << " "
             << text(in.operands[0]) << ", " << text(in.operands[1]) << "\n";
        break;
      case Opcode::Cast: emitCast(in); break;
      case Opcode::Load:
        out_ << "  %r" << *in.dst << " = load " << llvmType(in.type) << ", ptr %s" << in.slot << "\n";
        break;
      case Opcode::Store:
        out_ << "  store " << llvmType(in.type) << " " << text(in.operands[0]) << ", ptr %s" << in.slot << "\n";
        break;
      case Opcode::Call: emitCall(in); break;
      case Opcode::Br: out_ << "  br label %bb" << in.target0 << "\n"; break;
      case Opcode::CondBr:
        out_ << "  br i1 " << text(in.operands[0]) << ", label %bb" << in.target0 << ", label %bb" << in.target1 << "\n";
        break;
      case Opcode::Ret:
        if (in.operands.empty()) {
          out_ << "  ret void\n";
        } else {
          out_ << "  ret " << llvmType(in.type) << " " << text(in.operands[0]) << "\n";
        }
        break;
    }
  }

  void emitBinary(const Instr& in) {
    std::string type = llvmType(in.type);
    std::string a = text(in.operands[0]);
    std::string b = text(in.operands[1]);
    if (in.bin == BinOp::Div || in.bin == BinOp::Rem) emitDivisionGuard(in, type, a, b);
    out_ << "  %r" << *in.dst << " = " << binaryMnemonic(in.bin, in.type) << " " << type << " " << a << ", " << b << "\n";
  }

  void emitDivisionGuard(const Instr& in, const std::string& type, const std::string& a, const std::string& b) {
    usesTrap_ = true;
    uint32_t id = labelCounter_++;
    std::string bad = temp();
    std::string zero = temp();
    out_ << "  " << zero << " = icmp eq " << type << " " << b << ", 0\n";
    if (isSigned(in.type)) {
      std::string isMin = temp();
      std::string isMinusOne = temp();
      std::string overflow = temp();
      out_ << "  " << isMin << " = icmp eq " << type << " " << a << ", " << minSigned(in.type) << "\n";
      out_ << "  " << isMinusOne << " = icmp eq " << type << " " << b << ", -1\n";
      out_ << "  " << overflow << " = and i1 " << isMin << ", " << isMinusOne << "\n";
      out_ << "  " << bad << " = or i1 " << zero << ", " << overflow << "\n";
    } else {
      out_ << "  " << bad << " = or i1 " << zero << ", false\n";
    }
    out_ << "  br i1 " << bad << ", label %trap" << id << ", label %ok" << id << "\n";
    out_ << "trap" << id << ":\n  call void @llvm.trap()\n  unreachable\n";
    out_ << "ok" << id << ":\n";
  }

  void emitCast(const Instr& in) {
    unsigned from = bitWidth(in.fromType);
    unsigned to = bitWidth(in.type);
    const Value& operand = in.operands[0];
    if (from == to) {
      alias_[*in.dst] = text(operand);
      return;
    }
    const char* op = to < from ? "trunc" : (isSigned(in.fromType) ? "sext" : "zext");
    out_ << "  %r" << *in.dst << " = " << op << " " << llvmType(in.fromType) << " " << text(operand) << " to "
         << llvmType(in.type) << "\n";
  }

  void emitCall(const Instr& in) {
    out_ << "  ";
    if (in.dst) out_ << "%r" << *in.dst << " = ";
    out_ << "call " << llvmType(in.type) << " " << functionName(module_, in.callee) << "(";
    for (size_t i = 0; i < in.operands.size(); ++i) {
      if (i != 0) out_ << ", ";
      out_ << llvmType(in.operands[i].type) << " " << text(in.operands[i]);
    }
    out_ << ")\n";
  }
};

}

std::string emitLLVM(const ir::Module& module, const std::string& sourceName, const TargetConfig& config) {
  bool tuned = !config.cpu.empty() || !config.features.empty();
  std::ostringstream body;
  bool usesTrap = false;
  for (const ir::Function& fn : module.functions) {
    FunctionEmitter emitter(module, fn, body, tuned);
    emitter.run();
    usesTrap = usesTrap || emitter.usesTrap();
  }
  std::ostringstream out;
  out << "source_filename = \"" << llvmString(sourceName) << "\"\n";
  out << "target triple = \"" << config.target.llvmTriple << "\"\n\n";
  out << body.str();
  if (usesTrap) out << "declare void @llvm.trap()\n";
  if (tuned) {
    out << "\nattributes #0 = {";
    if (!config.cpu.empty()) out << " \"target-cpu\"=\"" << config.cpu << "\"";
    if (!config.features.empty()) out << " \"target-features\"=\"" << featureString(config.features) << "\"";
    out << " }\n";
  }
  return out.str();
}

}
