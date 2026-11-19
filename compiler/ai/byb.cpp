#include "ai/byb.h"

#include <array>
#include <unordered_map>

#include "common/diagnostic.h"
#include "ir/verify.h"

namespace bylink::ai {

namespace {

constexpr char kMagic[4] = {'B', 'Y', 'I', 'R'};
constexpr size_t kMaxFileBytes = size_t{64} << 20;

constexpr std::array<Type, 10> kWireTypes{Type::I8, Type::I16, Type::I32, Type::I64, Type::U8, Type::U16, Type::U32, Type::U64, Type::Bool, Type::Void};
constexpr std::array<BinOp, 8> kWireBinOps{BinOp::Add, BinOp::Sub, BinOp::Mul, BinOp::Div, BinOp::Rem, BinOp::And, BinOp::Or, BinOp::Xor};
constexpr std::array<CmpOp, 6> kWireCmpOps{CmpOp::Eq, CmpOp::Ne, CmpOp::Lt, CmpOp::Le, CmpOp::Gt, CmpOp::Ge};
constexpr std::array<ir::Opcode, 9> kWireOpcodes{ir::Opcode::Binary, ir::Opcode::Compare, ir::Opcode::Cast, ir::Opcode::Load, ir::Opcode::Store,
                                                 ir::Opcode::Call,   ir::Opcode::Br,      ir::Opcode::CondBr, ir::Opcode::Ret};

template <typename T, size_t N>
uint8_t toWire(const std::array<T, N>& table, T value) {
  for (size_t i = 0; i < N; ++i) {
    if (table[i] == value) return static_cast<uint8_t>(i);
  }
  throw CompileError(Category::Semantic, Loc{}, "internal error: value has no .byb wire code");
}

uint64_t zigzag(int64_t v) { return (static_cast<uint64_t>(v) << 1) ^ static_cast<uint64_t>(v >> 63); }
int64_t unzigzag(uint64_t v) { return static_cast<int64_t>(v >> 1) ^ -static_cast<int64_t>(v & 1); }

class Writer {
 public:
  void byte(uint8_t b) { out_.push_back(static_cast<char>(b)); }

  void varint(uint64_t v) {
    while (v >= 0x80) {
      byte(static_cast<uint8_t>(v | 0x80));
      v >>= 7;
    }
    byte(static_cast<uint8_t>(v));
  }

  void string(const std::string& s) {
    varint(s.size());
    out_ += s;
  }

  void raw(const char* data, size_t n) { out_.append(data, n); }

  void type(Type t) { byte(toWire(kWireTypes, t)); }

  void value(const ir::Value& v) {
    byte(static_cast<uint8_t>((toWire(kWireTypes, v.type) << 1) | (v.isConst ? 1 : 0)));
    if (!v.isConst) {
      varint(v.reg);
    } else if (isSigned(v.type)) {
      varint(zigzag(signedValue(v.type, v.bits)));
    } else {
      varint(v.bits);
    }
  }

  std::string take() { return std::move(out_); }

 private:
  std::string out_;
};

class Reader {
 public:
  explicit Reader(const std::string& data) : data_(data) {}

  [[noreturn]] void fail(const std::string& message) const {
    throw CompileError(Category::Syntax, Loc{}, "invalid .byb at byte " + std::to_string(pos_) + ": " + message);
  }

  size_t remaining() const { return data_.size() - pos_; }
  bool atEnd() const { return pos_ == data_.size(); }

  uint8_t byte() {
    if (pos_ >= data_.size()) fail("unexpected end of data");
    return static_cast<uint8_t>(data_[pos_++]);
  }

  uint64_t varint() {
    uint64_t v = 0;
    for (unsigned shift = 0;; shift += 7) {
      uint8_t b = byte();
      if (shift == 63 && b > 1) fail("varint overflows 64 bits");
      v |= static_cast<uint64_t>(b & 0x7F) << shift;
      if ((b & 0x80) == 0) {
        if (b == 0 && shift != 0) fail("non-canonical varint");
        return v;
      }
      if (shift >= 63) fail("varint is too long");
    }
  }

  uint32_t count() {
    uint64_t n = varint();
    if (n > remaining()) fail("element count exceeds the remaining data");
    return static_cast<uint32_t>(n);
  }

  uint32_t index32() {
    uint64_t n = varint();
    if (n > UINT32_MAX) fail("index does not fit in 32 bits");
    return static_cast<uint32_t>(n);
  }

  std::string string() {
    uint64_t n = varint();
    if (n > remaining()) fail("string length exceeds the remaining data");
    std::string s = data_.substr(pos_, n);
    pos_ += n;
    return s;
  }

  Type type() {
    uint8_t t = byte();
    if (t >= kWireTypes.size()) fail("unknown type code " + std::to_string(t));
    return kWireTypes[t];
  }

  ir::Value value() {
    uint8_t tag = byte();
    if ((tag >> 1) >= kWireTypes.size()) fail("unknown type code in operand");
    Type t = kWireTypes[tag >> 1];
    if ((tag & 1) == 0) return ir::Value::fromReg(t, index32());
    uint64_t raw = varint();
    ir::Value v = ir::Value::constant(t, isSigned(t) ? static_cast<uint64_t>(unzigzag(raw)) : raw);
    uint64_t canonical = isSigned(t) ? zigzag(signedValue(t, v.bits)) : v.bits;
    if (canonical != raw) fail("constant does not fit its type");
    return v;
  }

 private:
  const std::string& data_;
  size_t pos_ = 0;
};

void writeInstr(Writer& w, const ir::Module& module, const ir::Instr& in) {
  w.byte(toWire(kWireOpcodes, in.op));
  switch (in.op) {
    case ir::Opcode::Binary:
      w.type(in.type);
      w.byte(toWire(kWireBinOps, in.bin));
      w.varint(*in.dst);
      w.value(in.operands[0]);
      w.value(in.operands[1]);
      break;
    case ir::Opcode::Compare:
      w.type(in.type);
      w.byte(toWire(kWireCmpOps, in.cmp));
      w.varint(*in.dst);
      w.value(in.operands[0]);
      w.value(in.operands[1]);
      break;
    case ir::Opcode::Cast:
      w.type(in.fromType);
      w.type(in.type);
      w.varint(*in.dst);
      w.value(in.operands[0]);
      break;
    case ir::Opcode::Load:
      w.type(in.type);
      w.varint(*in.dst);
      w.varint(in.slot);
      break;
    case ir::Opcode::Store:
      w.type(in.type);
      w.varint(in.slot);
      w.value(in.operands[0]);
      break;
    case ir::Opcode::Call: {
      size_t index = 0;
      while (module.functions[index].name != in.callee) ++index;
      w.type(in.type);
      w.varint(index);
      if (in.type != Type::Void) w.varint(*in.dst);
      w.varint(in.operands.size());
      for (const ir::Value& v : in.operands) w.value(v);
      break;
    }
    case ir::Opcode::Br: w.varint(in.target0); break;
    case ir::Opcode::CondBr:
      w.value(in.operands[0]);
      w.varint(in.target0);
      w.varint(in.target1);
      break;
    case ir::Opcode::Ret:
      w.type(in.type);
      if (in.type != Type::Void) w.value(in.operands[0]);
      break;
  }
}

struct PendingCall {
  size_t function;
  size_t block;
  size_t instr;
  uint32_t callee;
};

ir::Instr readInstr(Reader& r, size_t fnIndex, size_t blockIndex, size_t instrIndex, std::vector<PendingCall>& calls) {
  ir::Instr in;
  uint8_t op = r.byte();
  if (op >= kWireOpcodes.size()) r.fail("unknown opcode " + std::to_string(op));
  in.op = kWireOpcodes[op];
  switch (in.op) {
    case ir::Opcode::Binary:
    case ir::Opcode::Compare: {
      in.type = r.type();
      uint8_t sub = r.byte();
      if (in.op == ir::Opcode::Binary) {
        if (sub >= kWireBinOps.size()) r.fail("unknown binary operation");
        in.bin = kWireBinOps[sub];
      } else {
        if (sub >= kWireCmpOps.size()) r.fail("unknown comparison");
        in.cmp = kWireCmpOps[sub];
      }
      in.dst = r.index32();
      in.operands = {r.value(), r.value()};
      break;
    }
    case ir::Opcode::Cast:
      in.fromType = r.type();
      in.type = r.type();
      in.dst = r.index32();
      in.operands = {r.value()};
      break;
    case ir::Opcode::Load:
      in.type = r.type();
      in.dst = r.index32();
      in.slot = r.index32();
      break;
    case ir::Opcode::Store:
      in.type = r.type();
      in.slot = r.index32();
      in.operands = {r.value()};
      break;
    case ir::Opcode::Call: {
      in.type = r.type();
      uint32_t callee = r.index32();
      if (in.type != Type::Void) in.dst = r.index32();
      uint32_t n = r.count();
      for (uint32_t i = 0; i < n; ++i) in.operands.push_back(r.value());
      calls.push_back({fnIndex, blockIndex, instrIndex, callee});
      break;
    }
    case ir::Opcode::Br: in.target0 = r.index32(); break;
    case ir::Opcode::CondBr:
      in.operands = {r.value()};
      in.target0 = r.index32();
      in.target1 = r.index32();
      break;
    case ir::Opcode::Ret:
      in.type = r.type();
      if (in.type != Type::Void) in.operands = {r.value()};
      break;
  }
  return in;
}

}

std::string encodeBinary(const ir::Module& module) {
  ir::verify(module);
  Writer w;
  w.raw(kMagic, sizeof(kMagic));
  w.byte(kBybVersion);
  w.string(module.name);
  w.varint(module.functions.size());
  for (const ir::Function& fn : module.functions) {
    w.string(fn.name);
    w.varint(fn.params.size());
    for (Type t : fn.params) w.type(t);
    w.type(fn.ret);
    w.varint(fn.slots.size());
    for (Type t : fn.slots) w.type(t);
    w.varint(fn.regCount);
    w.varint(fn.blocks.size());
    for (const ir::Block& b : fn.blocks) {
      w.varint(b.instrs.size());
      for (const ir::Instr& in : b.instrs) writeInstr(w, module, in);
    }
  }
  return w.take();
}

ir::Module decodeBinary(const std::string& bytes) {
  if (bytes.size() > kMaxFileBytes) throw CompileError(Category::Syntax, Loc{}, "invalid .byb: file is larger than " + std::to_string(kMaxFileBytes) + " bytes");
  Reader r(bytes);
  for (char c : kMagic) {
    if (r.byte() != static_cast<uint8_t>(c)) r.fail("bad magic; not a BYlink binary IR file");
  }
  uint8_t version = r.byte();
  if (version != kBybVersion) r.fail("unsupported version " + std::to_string(version));
  ir::Module module;
  module.name = r.string();
  uint32_t functionCount = r.count();
  std::vector<PendingCall> calls;
  for (uint32_t f = 0; f < functionCount; ++f) {
    ir::Function fn;
    fn.name = r.string();
    uint32_t params = r.count();
    for (uint32_t i = 0; i < params; ++i) fn.params.push_back(r.type());
    fn.ret = r.type();
    uint32_t slots = r.count();
    for (uint32_t i = 0; i < slots; ++i) fn.slots.push_back(r.type());
    fn.regCount = r.index32();
    uint32_t blocks = r.count();
    for (uint32_t b = 0; b < blocks; ++b) {
      ir::Block block;
      uint32_t instrs = r.count();
      for (uint32_t i = 0; i < instrs; ++i) block.instrs.push_back(readInstr(r, f, b, i, calls));
      fn.blocks.push_back(std::move(block));
    }
    module.functions.push_back(std::move(fn));
  }
  if (!r.atEnd()) r.fail("trailing data after the module");
  for (const PendingCall& c : calls) {
    if (c.callee >= module.functions.size()) r.fail("call to a function index that does not exist");
    module.functions[c.function].blocks[c.block].instrs[c.instr].callee = module.functions[c.callee].name;
  }
  ir::verify(module);
  return module;
}

}
