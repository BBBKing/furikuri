#include "stdafx.h"
#include "fuku_asm_x64.h"

int reg_high_bit(fuku_reg64 reg) {
    return reg >> 3;
}
int reg_low_bits(fuku_reg64 reg) {
    return reg & 0x7;
}

bool is_byte_register(fuku_reg64 reg) {
    return reg <= 3;
}

class OperandBuilder {
public:
    OperandBuilder(fuku_reg64 base, int32_t disp) {
        if (base == fuku_reg64::r_RSP || base == fuku_reg64::r_R12) {
            // SIB byte is needed to encode (rsp + offset) or (r12 + offset).
            set_sib(operand_scale_1, fuku_reg64::r_RSP, base);
        }

        if (disp == 0 && base != fuku_reg64::r_RBP && base != fuku_reg64::r_R13) {
            set_modrm(0, base);
        }
        else if (!(disp & 0xFFFFFF00)) {
            set_modrm(1, base);
            set_disp8(disp);
        }
        else {
            set_modrm(2, base);
            set_disp32(disp);
        }
    }

    OperandBuilder(fuku_reg64 base, fuku_reg64 index, operand_scale scale,
        int32_t disp) {

        set_sib(scale, index, base);
        if (disp == 0 && base != fuku_reg64::r_RBP && base != fuku_reg64::r_R13) {
            // This call to set_modrm doesn't overwrite the REX.B (or REX.X) bits
            // possibly set by set_sib.
            set_modrm(0, fuku_reg64::r_RSP);
        }
        else if ( !(disp&0xFFFFFF00)) {
            set_modrm(1, fuku_reg64::r_RSP);
            set_disp8(disp);
        }
        else {
            set_modrm(2, fuku_reg64::r_RSP);
            set_disp32(disp);
        }
    }

    OperandBuilder(fuku_reg64 index, operand_scale scale, int32_t disp) {
        set_modrm(0, fuku_reg64::r_RSP);
        set_sib(scale, index, fuku_reg64::r_RBP);
        set_disp32(disp);
    }

    OperandBuilder(fuku_operand64& operand, int32_t offset) {
        addend = 0;

        uint8_t modrm = operand.get_buf()[0];
        bool has_sib = ((modrm & 0x07) == 0x04);
        uint8_t mode = modrm & 0xC0;
        int disp_offset = has_sib ? 2 : 1;
        int base_reg = (has_sib ? operand.get_buf()[1] : modrm) & 0x07;
        bool is_baseless =
            (mode == 0) && (base_reg == 0x05);
        int32_t disp_value = 0;
        if (mode == 0x80 || is_baseless) {
            disp_value = *(uint32_t*)&operand.get_buf()[disp_offset];
        }
        else if (mode == 0x40) {
            disp_value = static_cast<signed char>(operand.get_buf()[disp_offset]);
        }

        disp_value += offset;
        rex = operand.get_rex();
        if ((disp_value & 0xFFFFFF00) || is_baseless) {
            buf[0] = (modrm & 0x3F) | (is_baseless ? 0x00 : 0x80);
            len = disp_offset + 4;
            *(uint32_t*)&buf[disp_offset] = disp_value;
        }
        else if (disp_value != 0 || (base_reg == 0x05)) {
            buf[0] = (modrm & 0x3F) | 0x40;
            len = disp_offset + 1;
            buf[disp_offset] = static_cast<uint8_t>(disp_value);
        }
        else {
            buf[0] = (modrm & 0x3F);
            len = disp_offset;
        }
        if (has_sib) {
            buf[1] = operand.get_buf()[1];
        }
    }

    void set_modrm(int mod, fuku_reg64 rm_reg) {
        buf[0] = mod << 6 | reg_low_bits(rm_reg);
        // Set REX.B to the high bit of rm.code().
        rex |= reg_high_bit(rm_reg);
    }

    void set_sib(operand_scale scale, fuku_reg64 index, fuku_reg64 base) {
        buf[1] = (scale << 6) | (reg_low_bits(index) << 3) | reg_low_bits(base);
        rex |= reg_high_bit(index) << 1 | reg_high_bit(base);
        len = 2;
    }

    void set_disp8(int disp) {
        int8_t* p = reinterpret_cast<int8_t*>(&buf[len]);
        *p = disp;
        len += sizeof(int8_t);
    }

    void set_disp32(int disp) {
        int32_t* p = reinterpret_cast<int32_t*>(&buf[len]);
        *p = disp;
        len += sizeof(int32_t);
    }

    void set_disp64(int64_t disp) {
        int64_t* p = reinterpret_cast<int64_t*>(&buf[len]);
        *p = disp;
        len += sizeof(int64_t);
    }

    void init_operand(fuku_operand64& operand) {
        operand.set_rex(rex);
        operand.set_buf(buf, len);
        operand.set_addend(addend);
    };
private:
    uint8_t rex = 0;
    uint8_t buf[9];
    uint8_t len = 1; 
    int8_t addend; 
};


fuku_operand64::fuku_operand64(fuku_reg64 base, int32_t disp){
    OperandBuilder(base, disp).init_operand(*this);
}

fuku_operand64::fuku_operand64(fuku_reg64 base, fuku_reg64 index, operand_scale scale, int32_t disp){
    OperandBuilder(base, index, scale, disp).init_operand(*this);
}

fuku_operand64::fuku_operand64(fuku_reg64 index, operand_scale scale, int32_t disp){
    OperandBuilder(index, scale, disp).init_operand(*this);
}

fuku_operand64::fuku_operand64(fuku_operand64& operand, int32_t offset){
    OperandBuilder(operand, offset).init_operand(*this);
}

void fuku_operand64::set_rex(uint8_t rex) {
    this->rex = rex;
}
void fuku_operand64::set_buf(uint8_t* buf, uint8_t len) {
    memset(this->buf, 0, sizeof(this->buf));
    memcpy(this->buf, buf, len);
    this->len = len;
}
void fuku_operand64::set_addend(int8_t addend) {
    this->addend = addend;
}

bool fuku_operand64::address_uses_reg(fuku_reg64 reg) const {
    int code = reg;

    int base_code = buf[0] & 0x07;
    if (base_code == fuku_reg64::r_RSP) {

        int index_code = ((buf[1] >> 3) & 0x07) | ((rex & 0x02) << 2);
        if (index_code != fuku_reg64::r_RSP && index_code == code) { return true; }

        base_code = (buf[1] & 0x07) | ((rex & 0x01) << 3);
        if (base_code == fuku_reg64::r_RBP && ((buf[0] & 0xC0) == 0)) { return false; }

        return code == base_code;
    }
    else {
        if (base_code == fuku_reg64::r_RBP && ((buf[0] & 0xC0) == 0)) { return false; }

        base_code |= ((rex & 0x01) << 3);
        return code == base_code;
    }
}
bool fuku_operand64::requires_rex() const { 
    return rex != 0; 
}
int  fuku_operand64::operand_size() const { 
    return len; 
}
int8_t fuku_operand64::get_addend() const {
    return this->addend;
}
const uint8_t* fuku_operand64::get_buf() const {
    return this->buf;
}

uint8_t fuku_operand64::get_length() const {
    return this->len;
}

uint8_t fuku_operand64::get_rex() const {
    return this->rex;
}

fuku_asm_x64::fuku_asm_x64()
{
}


fuku_asm_x64::~fuku_asm_x64()
{
}

void fuku_asm_x64::clear_space() {
    memset(bytecode, 0, sizeof(bytecode));
    this->length = 0;
    this->imm_offset = 0;
}

void fuku_asm_x64::emit_b(uint8_t x) {
    bytecode[length] = x;
    length++;
}

void fuku_asm_x64::emit_w(uint16_t x) {
    *(uint16_t*)&bytecode[length] = x;
    length += sizeof(uint16_t);
}

void fuku_asm_x64::emit_dw(uint32_t x) {
    *(uint32_t*)&bytecode[length] = x;
    length += sizeof(uint32_t);
}

void fuku_asm_x64::emit_qw(uint64_t x) {
    *(uint64_t*)&bytecode[length] = x;
    length += sizeof(uint64_t);
}

void fuku_asm_x64::emit_rex_64() { 
    emit_b(0x48);
}

void fuku_asm_x64::emit_rex_64(fuku_reg64 reg, fuku_reg64 rm_reg) {
    emit_b(0x48 | reg_high_bit(reg) << 2 | reg_high_bit(rm_reg));
}


void fuku_asm_x64::emit_rex_64(fuku_reg64 reg, fuku_operand64& op) {
    emit_b(0x48 | reg_high_bit(reg) << 2 | op.get_rex());
}


void fuku_asm_x64::emit_rex_64(fuku_reg64 rm_reg) {
    emit_b(0x48 | reg_high_bit(rm_reg));
}

void fuku_asm_x64::emit_rex_64(fuku_operand64& op) { 
    emit_b(0x48 | op.get_rex()); 
}

void fuku_asm_x64::emit_rex_32(fuku_reg64 reg, fuku_reg64 rm_reg) {
    emit_b(0x40 | reg_high_bit(reg) << 2 | reg_high_bit(rm_reg));
}

void fuku_asm_x64::emit_rex_32(fuku_reg64 reg, fuku_operand64& op) {
    emit_b(0x40 | reg_high_bit(reg) << 2 | op.get_rex());
}


void fuku_asm_x64::emit_rex_32(fuku_reg64 rm_reg) {
    emit_b(0x40 | reg_high_bit(rm_reg));
}

void fuku_asm_x64::emit_rex_32(fuku_operand64& op) { emit_b(0x40 | op.get_rex()); }

void fuku_asm_x64::emit_optional_rex_32(fuku_reg64 reg, fuku_reg64 rm_reg) {
    uint8_t rex_bits = reg_high_bit(reg) << 2 | reg_high_bit(rm_reg);
    if (rex_bits != 0) { emit_b(0x40 | rex_bits); }
}

void fuku_asm_x64::emit_optional_rex_32(fuku_reg64 reg, fuku_operand64& op) {
    uint8_t rex_bits = reg_high_bit(reg) << 2 | op.get_rex();
    if (rex_bits != 0) { emit_b(0x40 | rex_bits); }
}


void fuku_asm_x64::emit_optional_rex_32(fuku_reg64 rm_reg) {
    if (reg_high_bit(rm_reg)) { emit_b(0x41); }
}

void fuku_asm_x64::emit_optional_rex_32(fuku_operand64& op) {
    if (op.get_rex() != 0) { emit_b(0x40 | op.get_rex()); }
}


void fuku_asm_x64::emit_modrm(fuku_reg64 reg, fuku_reg64 rm_reg) {
    emit_b(0xC0 | reg_low_bits(reg) << 3 | reg_low_bits(rm_reg));
}

void fuku_asm_x64::emit_modrm(int code, fuku_reg64 rm_reg) {
    emit_b(0xC0 | code << 3 | reg_low_bits(rm_reg));
}

void fuku_asm_x64::emit_operand(int code, fuku_operand64& adr) {
    const unsigned _length = adr.get_length();

    bytecode[length] = adr.get_buf()[0] | code << 3;

    for (unsigned i = 1; i < _length; i++) { bytecode[length + i] = adr.get_buf()[i]; }
    length += _length;
}

void fuku_asm_x64::arithmetic_op(uint8_t opcode, fuku_reg64 reg, fuku_operand64& op, fuku_asm64_size size) {
    clear_space();
    emit_rex(reg, op, size);
    emit_b(opcode);
    emit_operand(reg, op);
}


void fuku_asm_x64::arithmetic_op(uint8_t opcode,
    fuku_reg64 reg,
    fuku_reg64 rm_reg,
    fuku_asm64_size size) {

    clear_space();
    if (reg_low_bits(rm_reg) == 4) {  // Forces SIB byte.
                                   // Swap reg and rm_reg and change opcode operand order.
        emit_rex(rm_reg, reg, size);
        emit_b(opcode ^ 0x02);
        emit_modrm(rm_reg, reg);
    }
    else {
        emit_rex(reg, rm_reg, size);
        emit_b(opcode);
        emit_modrm(reg, rm_reg);
    }
}


void fuku_asm_x64::arithmetic_op_16(uint8_t opcode, fuku_reg64 reg, fuku_reg64 rm_reg) {
    clear_space();
    
    if (reg_low_bits(rm_reg) == 4) {  // Forces SIB byte.
                                   // Swap reg and rm_reg and change opcode operand order.
        emit_b(0x66);
        emit_optional_rex_32(rm_reg, reg);
        emit_b(opcode ^ 0x02);
        emit_modrm(rm_reg, reg);
    }
    else {
        emit_b(0x66);
        emit_optional_rex_32(reg, rm_reg);
        emit_b(opcode);
        emit_modrm(reg, rm_reg);
    }
}

void fuku_asm_x64::arithmetic_op_16(uint8_t opcode, fuku_reg64 reg, fuku_operand64& rm_reg) {
    clear_space();
    emit_b(0x66);
    emit_optional_rex_32(reg, rm_reg);
    emit_b(opcode);
    emit_operand(reg, rm_reg);
}

void fuku_asm_x64::arithmetic_op_8(uint8_t opcode, fuku_reg64 reg, fuku_operand64& op) {
    clear_space();
    if (!is_byte_register(reg)) {
        emit_rex_32(reg, op);
    }
    else {
        emit_optional_rex_32(reg, op);
    }
    emit_b(opcode);
    emit_operand(reg, op);
}


void fuku_asm_x64::arithmetic_op_8(uint8_t opcode, fuku_reg64 reg, fuku_reg64 rm_reg) {
    clear_space();
    if (reg_low_bits(rm_reg) == 4) {  // Forces SIB byte.
                                   // Swap reg and rm_reg and change opcode operand order.
        if (!is_byte_register(rm_reg) || !is_byte_register(reg)) {
            // Register is not one of al, bl, cl, dl.  Its encoding needs REX.
            emit_rex_32(rm_reg, reg);
        }
        emit_b(opcode ^ 0x02);
        emit_modrm(rm_reg, reg);
    }
    else {
        if (!is_byte_register(reg) || !is_byte_register(rm_reg)) {
            // Register is not one of al, bl, cl, dl.  Its encoding needs REX.
            emit_rex_32(reg, rm_reg);
        }
        emit_b(opcode);
        emit_modrm(reg, rm_reg);
    }
}


void fuku_asm_x64::immediate_arithmetic_op(uint8_t subcode,
    fuku_reg64 dst,
    fuku_immediate& src,
    fuku_asm64_size size) {

    clear_space();
    emit_rex(dst, size);
    if (src.is_imm_8()) {
        emit_b(0x83);
        emit_modrm(subcode, dst);
        emit_b(src.get_imm()&0xFF);
    }
    else if (dst == fuku_reg64::r_RAX) {
        emit_b(0x05 | (subcode << 3));
        emit_dw(uint32_t(src.get_imm()));
    }
    else {
        emit_b(0x81);
        emit_modrm(subcode, dst);
        emit_dw(uint32_t(src.get_imm()));
    }
}

void fuku_asm_x64::immediate_arithmetic_op(uint8_t subcode, fuku_operand64& dst,
    fuku_immediate& src, fuku_asm64_size size) {

    clear_space();
    emit_rex(dst, size);
    if (src.is_imm_8()) {
        emit_b(0x83);
        emit_operand(subcode, dst);
        emit_b(src.get_imm()&0xFF);
    }
    else {
        emit_b(0x81);
        emit_operand(subcode, dst);
        emit_dw(uint32_t(src.get_imm()));
    }
}


void fuku_asm_x64::immediate_arithmetic_op_16(uint8_t subcode,
    fuku_reg64 dst,
    fuku_immediate& src) {

    clear_space();
    emit_b(0x66);  // Operand size override prefix.
    emit_optional_rex_32(dst);
    if (src.is_imm_8()) {
        emit_b(0x83);
        emit_modrm(subcode, dst);
        emit_b(src.get_imm() & 0xFF);
    }
    else if (dst == fuku_reg64::r_RAX) {
        emit_b(0x05 | (subcode << 3));
        emit_w(src.get_imm() & 0xFFFF);
    }
    else {
        emit_b(0x81);
        emit_modrm(subcode, dst);
        emit_w(src.get_imm() & 0xFFFF);
    }
}

void fuku_asm_x64::immediate_arithmetic_op_16(uint8_t subcode, fuku_operand64& dst,
    fuku_immediate& src) {

    clear_space();
    emit_b(0x66);  // Operand size override prefix.
    emit_optional_rex_32(dst);
    if (src.is_imm_8()) {
        emit_b(0x83);
        emit_operand(subcode, dst);
        emit_b(src.get_imm() & 0xFF);
    }
    else {
        emit_b(0x81);
        emit_operand(subcode, dst);
        emit_w(src.get_imm() & 0xFFFF);
    }
}

void fuku_asm_x64::immediate_arithmetic_op_8(uint8_t subcode, fuku_operand64& dst,
    fuku_immediate& src) {

    clear_space();
    emit_optional_rex_32(dst);

    emit_b(0x80);
    emit_operand(subcode, dst);
    emit_b(src.get_imm() & 0xFF);
}


void fuku_asm_x64::immediate_arithmetic_op_8(uint8_t subcode,
    fuku_reg64 dst,
    fuku_immediate& src) {

    clear_space();
    if (!is_byte_register(dst)) {
        // Register is not one of al, bl, cl, dl.  Its encoding needs REX.
        emit_rex_32(dst);
    }
   
    emit_b(0x80);
    emit_modrm(subcode, dst);
    emit_b(src.get_imm()&0xFF);
}

void fuku_asm_x64::emit_rex(fuku_asm64_size size) {
    if (size == fuku_asm64_size::asm64_size_64) {
        emit_rex_64();
    }
}

void fuku_asm_x64::emit_rex(fuku_operand64& p1, fuku_asm64_size size) {
    if (size == fuku_asm64_size::asm64_size_64) {
        emit_rex_64(p1);
    }
    else {
        emit_optional_rex_32(p1);
    }
}
void fuku_asm_x64::emit_rex(fuku_reg64 p1, fuku_asm64_size size) {
    if (size == fuku_asm64_size::asm64_size_64) {
        emit_rex_64(p1);
    }
    else {
        emit_optional_rex_32(p1);
    }
}

void fuku_asm_x64::emit_rex(fuku_reg64 p1, fuku_reg64 p2, fuku_asm64_size size) {
    if (size == fuku_asm64_size::asm64_size_64) {
        emit_rex_64(p1, p2);
    }
    else {
        emit_optional_rex_32(p1, p2);
    }
}

void fuku_asm_x64::emit_rex(fuku_reg64 p1, fuku_operand64& p2, fuku_asm64_size size) {
    if (size == fuku_asm64_size::asm64_size_64) {
        emit_rex_64(p1, p2);
    }
    else {
        emit_optional_rex_32(p1, p2);
    }
}


fuku_instruction fuku_asm_x64::jmp(fuku_reg64 reg) {
    clear_space();

    emit_optional_rex_32(reg);
    emit_b(0xFF);
    emit_modrm(0x4, reg);

    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_JMP).set_eflags(0);
}

fuku_instruction fuku_asm_x64::jmp(fuku_operand64& adr) {
    clear_space();

    emit_optional_rex_32(adr);
    emit_b(0xFF);
    emit_operand(0x4, adr);

    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_JMP).set_eflags(0);
}

fuku_instruction fuku_asm_x64::jmp(uint32_t offset) {
    clear_space();
    emit_b(0xE9);
    emit_dw(offset);
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_JMP).set_eflags(0);
}

fuku_instruction fuku_asm_x64::jcc(fuku_condition cond, uint32_t offset) {
    clear_space();
    emit_b(0x0F);
    emit_b(0x80 | cond);
    emit_dw(offset);

    uint16_t di_jcc[] = {
        X86_INS_JO , X86_INS_JNO ,
        X86_INS_JB , X86_INS_JAE ,
        X86_INS_JE, X86_INS_JNE,
        X86_INS_JBE , X86_INS_JA ,
        X86_INS_JS , X86_INS_JNS ,
        X86_INS_JP , X86_INS_JNP ,
        X86_INS_JL , X86_INS_JGE ,
        X86_INS_JLE , X86_INS_JG ,
    };

    static uint64_t di_fl_jcc[] = {
        X86_EFLAGS_TEST_OF , X86_EFLAGS_TEST_OF,
        X86_EFLAGS_TEST_CF , X86_EFLAGS_TEST_CF,
        X86_EFLAGS_TEST_ZF , X86_EFLAGS_TEST_ZF,
        X86_EFLAGS_TEST_ZF | X86_EFLAGS_TEST_CF, X86_EFLAGS_TEST_ZF | X86_EFLAGS_TEST_CF,
        X86_EFLAGS_TEST_SF , X86_EFLAGS_TEST_SF,
        X86_EFLAGS_TEST_PF , X86_EFLAGS_TEST_PF,
        X86_EFLAGS_TEST_OF | X86_EFLAGS_TEST_SF, X86_EFLAGS_TEST_OF | X86_EFLAGS_TEST_SF,
        X86_EFLAGS_TEST_OF | X86_EFLAGS_TEST_SF | X86_EFLAGS_TEST_ZF, X86_EFLAGS_TEST_OF | X86_EFLAGS_TEST_SF | X86_EFLAGS_TEST_ZF
    };

    return fuku_instruction().set_op_code(bytecode, length).set_id(di_jcc[cond]).set_eflags(di_fl_jcc[cond]);
}

fuku_instruction fuku_asm_x64::clc() {
    clear_space();
    emit_b(0xF8);
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_CLC).set_eflags(X86_EFLAGS_RESET_CF);
}


fuku_instruction fuku_asm_x64::cld() {
    clear_space();
    emit_b(0xFC);
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_CLD).set_eflags(X86_EFLAGS_RESET_DF);
}

fuku_instruction fuku_asm_x64::cdq() {
    clear_space();
    emit_b(0x99);
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_CDQ).set_eflags(0);
}



fuku_instruction fuku_asm_x64::cmpb_al(fuku_immediate& imm8) {
    clear_space();
    emit_b(0x3C);
    emit_b(imm8.get_imm()&0xFF);
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_CMP).set_eflags(X86_EFLAGS_MODIFY_OF | X86_EFLAGS_MODIFY_SF | X86_EFLAGS_MODIFY_ZF | X86_EFLAGS_MODIFY_AF | X86_EFLAGS_MODIFY_PF | X86_EFLAGS_MODIFY_CF);
}

fuku_instruction fuku_asm_x64::cmpxchgb(fuku_operand64& dst, fuku_reg64 src) {
    clear_space();
    if (!is_byte_register(src)) {
        emit_rex_32(src, dst);
    }
    else {
        emit_optional_rex_32(src, dst);
    }
    emit_b(0x0F);
    emit_b(0xB0);
    emit_operand(src, dst);
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_CMPXCHG8B).set_eflags(X86_EFLAGS_MODIFY_OF | X86_EFLAGS_MODIFY_SF | X86_EFLAGS_MODIFY_ZF | X86_EFLAGS_MODIFY_AF | X86_EFLAGS_MODIFY_PF | X86_EFLAGS_MODIFY_CF);
}

fuku_instruction fuku_asm_x64::cmpxchgw(fuku_operand64& dst, fuku_reg64 src) {
    clear_space();
    emit_b(0x66);
    emit_optional_rex_32(src, dst);
    emit_b(0x0F);
    emit_b(0xB1);
    emit_operand(src, dst);
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_CMPXCHG16B).set_eflags(X86_EFLAGS_MODIFY_OF | X86_EFLAGS_MODIFY_SF | X86_EFLAGS_MODIFY_ZF | X86_EFLAGS_MODIFY_AF | X86_EFLAGS_MODIFY_PF | X86_EFLAGS_MODIFY_CF);
}

fuku_instruction fuku_asm_x64::cmpxchg(fuku_operand64& dst, fuku_reg64 src, fuku_asm64_size size) {
    clear_space();
    emit_rex(src, dst, size);
    emit_b(0x0F);
    emit_b(0xB1);
    emit_operand(src, dst);
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_CMPXCHG).set_eflags(X86_EFLAGS_MODIFY_OF | X86_EFLAGS_MODIFY_SF | X86_EFLAGS_MODIFY_ZF | X86_EFLAGS_MODIFY_AF | X86_EFLAGS_MODIFY_PF | X86_EFLAGS_MODIFY_CF);
}

fuku_instruction fuku_asm_x64::lfence() {
    clear_space();
    emit_b(0x0F);
    emit_b(0xAE);
    emit_b(0xE8);
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_LFENCE).set_eflags(0);
}

fuku_instruction fuku_asm_x64::cpuid() {
    clear_space();
    emit_b(0x0F);
    emit_b(0xA2);
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_CPUID).set_eflags(0);
}


fuku_instruction fuku_asm_x64::cqo() {
    clear_space();
    emit_rex_64();
    emit_b(0x99);
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_CQO).set_eflags(0);
}


fuku_instruction fuku_asm_x64::dec(fuku_reg64 dst, fuku_asm64_size size) {
    clear_space();
    emit_rex(dst, size);
    emit_b(0xFF);
    emit_modrm(0x1, dst);
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_DEC).set_eflags(X86_EFLAGS_MODIFY_OF | X86_EFLAGS_MODIFY_SF | X86_EFLAGS_MODIFY_ZF | X86_EFLAGS_MODIFY_AF | X86_EFLAGS_MODIFY_PF);
}

fuku_instruction fuku_asm_x64::dec(fuku_operand64& dst, fuku_asm64_size size) {
    clear_space();
    emit_rex(dst, size);
    emit_b(0xFF);
    emit_operand(1, dst);
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_DEC).set_eflags(X86_EFLAGS_MODIFY_OF | X86_EFLAGS_MODIFY_SF | X86_EFLAGS_MODIFY_ZF | X86_EFLAGS_MODIFY_AF | X86_EFLAGS_MODIFY_PF);
}


fuku_instruction fuku_asm_x64::decb(fuku_reg64 dst) {
    clear_space();

    if (!is_byte_register(dst)) {
        emit_rex_32(dst);
    }
    emit_b(0xFE);
    emit_modrm(0x1, dst);
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_DEC).set_eflags(X86_EFLAGS_MODIFY_OF | X86_EFLAGS_MODIFY_SF | X86_EFLAGS_MODIFY_ZF | X86_EFLAGS_MODIFY_AF | X86_EFLAGS_MODIFY_PF);
}

fuku_instruction fuku_asm_x64::decb(fuku_operand64& dst) {
    clear_space();
    emit_optional_rex_32(dst);
    emit_b(0xFE);
    emit_operand(1, dst);
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_DEC).set_eflags(X86_EFLAGS_MODIFY_OF | X86_EFLAGS_MODIFY_SF | X86_EFLAGS_MODIFY_ZF | X86_EFLAGS_MODIFY_AF | X86_EFLAGS_MODIFY_PF);
}


fuku_instruction fuku_asm_x64::enter(fuku_immediate& size) {
    clear_space();
    emit_b(0xC8);
    emit_w(size.get_imm()&0xFFFF);
    emit_b(0);
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_ENTER).set_eflags(0);
}


fuku_instruction fuku_asm_x64::hlt() {
    clear_space();
    emit_b(0xF4);
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_HLT).set_eflags(0);
}


fuku_instruction fuku_asm_x64::idiv(fuku_reg64 src, fuku_asm64_size size) {
    clear_space();
    emit_rex(src, size);
    emit_b(0xF7);
    emit_modrm(0x7, src);
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_IDIV).set_eflags(X86_EFLAGS_UNDEFINED_OF | X86_EFLAGS_UNDEFINED_SF | X86_EFLAGS_UNDEFINED_ZF | X86_EFLAGS_UNDEFINED_AF | X86_EFLAGS_UNDEFINED_PF | X86_EFLAGS_UNDEFINED_CF);
}


fuku_instruction fuku_asm_x64::div(fuku_reg64 src, fuku_asm64_size size) {
    clear_space();
    emit_rex(src, size);
    emit_b(0xF7);
    emit_modrm(0x6, src);
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_DIV).set_eflags(X86_EFLAGS_UNDEFINED_OF | X86_EFLAGS_UNDEFINED_SF | X86_EFLAGS_UNDEFINED_ZF | X86_EFLAGS_UNDEFINED_AF | X86_EFLAGS_UNDEFINED_PF | X86_EFLAGS_UNDEFINED_CF);
}


fuku_instruction fuku_asm_x64::imul(fuku_reg64 src, fuku_asm64_size size) {
    clear_space();
    emit_rex(src, size);
    emit_b(0xF7);
    emit_modrm(0x5, src);
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_IMUL).set_eflags(X86_EFLAGS_MODIFY_SF | X86_EFLAGS_MODIFY_CF | X86_EFLAGS_MODIFY_OF | X86_EFLAGS_UNDEFINED_ZF | X86_EFLAGS_UNDEFINED_AF | X86_EFLAGS_UNDEFINED_PF);
}

fuku_instruction fuku_asm_x64::imul(fuku_operand64& src, fuku_asm64_size size) {
    clear_space();
    emit_rex(src, size);
    emit_b(0xF7);
    emit_operand(0x5, src);
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_IMUL).set_eflags(X86_EFLAGS_MODIFY_SF | X86_EFLAGS_MODIFY_CF | X86_EFLAGS_MODIFY_OF | X86_EFLAGS_UNDEFINED_ZF | X86_EFLAGS_UNDEFINED_AF | X86_EFLAGS_UNDEFINED_PF);
}


fuku_instruction fuku_asm_x64::imul(fuku_reg64 dst, fuku_reg64 src, fuku_asm64_size size) {
    clear_space();
    emit_rex(dst, src, size);
    emit_b(0x0F);
    emit_b(0xAF);
    emit_modrm(dst, src);
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_IMUL).set_eflags(X86_EFLAGS_MODIFY_SF | X86_EFLAGS_MODIFY_CF | X86_EFLAGS_MODIFY_OF | X86_EFLAGS_UNDEFINED_ZF | X86_EFLAGS_UNDEFINED_AF | X86_EFLAGS_UNDEFINED_PF);
}

fuku_instruction fuku_asm_x64::imul(fuku_reg64 dst, fuku_operand64& src, fuku_asm64_size size) {
    clear_space();
    emit_rex(dst, src, size);
    emit_b(0x0F);
    emit_b(0xAF);
    emit_operand(dst, src);
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_IMUL).set_eflags(X86_EFLAGS_MODIFY_SF | X86_EFLAGS_MODIFY_CF | X86_EFLAGS_MODIFY_OF | X86_EFLAGS_UNDEFINED_ZF | X86_EFLAGS_UNDEFINED_AF | X86_EFLAGS_UNDEFINED_PF);
}


fuku_instruction fuku_asm_x64::imul(fuku_reg64 dst, fuku_reg64 src, fuku_immediate& imm, fuku_asm64_size size) {
    clear_space();
    emit_rex(dst, src, size);
    if (imm.is_imm_8()) {
        emit_b(0x6B);
        emit_modrm(dst, src);
        emit_b(imm.get_imm()&0xFF);
    }
    else {
        emit_b(0x69);
        emit_modrm(dst, src);
        emit_dw(imm.get_imm()&0xFFFFFFFF);
    }
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_IMUL).set_eflags(X86_EFLAGS_MODIFY_SF | X86_EFLAGS_MODIFY_CF | X86_EFLAGS_MODIFY_OF | X86_EFLAGS_UNDEFINED_ZF | X86_EFLAGS_UNDEFINED_AF | X86_EFLAGS_UNDEFINED_PF);
}

fuku_instruction fuku_asm_x64::imul(fuku_reg64 dst, fuku_operand64& src, fuku_immediate& imm, fuku_asm64_size size) {
    clear_space();
    emit_rex(dst, src, size);
    if (imm.is_imm_8()) {
        emit_b(0x6B);
        emit_operand(dst, src);
        emit_b(imm.get_imm() & 0xFF);
    }
    else {
        emit_b(0x69);
        emit_operand(dst, src);
        emit_dw(imm.get_imm()&0xFFFFFFFF);
    }
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_IMUL).set_eflags(X86_EFLAGS_MODIFY_SF | X86_EFLAGS_MODIFY_CF | X86_EFLAGS_MODIFY_OF | X86_EFLAGS_UNDEFINED_ZF | X86_EFLAGS_UNDEFINED_AF | X86_EFLAGS_UNDEFINED_PF);
}


fuku_instruction fuku_asm_x64::inc(fuku_reg64 dst, fuku_asm64_size size) {
    clear_space();
    emit_rex(dst, size);
    emit_b(0xFF);
    emit_modrm(0x0, dst);
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_INC).set_eflags(X86_EFLAGS_MODIFY_OF | X86_EFLAGS_MODIFY_SF | X86_EFLAGS_MODIFY_ZF | X86_EFLAGS_MODIFY_AF | X86_EFLAGS_MODIFY_PF);
}

fuku_instruction fuku_asm_x64::inc(fuku_operand64& dst, fuku_asm64_size size) {
    clear_space();
    emit_rex(dst, size);
    emit_b(0xFF);
    emit_operand(0, dst);
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_INC).set_eflags(X86_EFLAGS_MODIFY_OF | X86_EFLAGS_MODIFY_SF | X86_EFLAGS_MODIFY_ZF | X86_EFLAGS_MODIFY_AF | X86_EFLAGS_MODIFY_PF);
}


fuku_instruction fuku_asm_x64::int3() {
    clear_space();
    emit_b(0xCC);
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_INT3).set_eflags(0);
}


fuku_instruction fuku_asm_x64::lea(fuku_reg64 dst, fuku_operand64& src, fuku_asm64_size size) {
    clear_space();
    emit_rex(dst, src, size);
    emit_b(0x8D);
    emit_operand(dst, src);
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_LEA).set_eflags(0);
}


fuku_instruction fuku_asm_x64::leave() {
    clear_space();
    emit_b(0xC9);
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_LEAVE).set_eflags(0);
}

fuku_instruction fuku_asm_x64::movb(fuku_reg64 dst, fuku_operand64& src) {
    clear_space();
    if (!is_byte_register(dst)) {
        emit_rex_32(dst, src);
    }
    else {
        emit_optional_rex_32(dst, src);
    }
    emit_b(0x8A);
    emit_operand(dst, src);
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_MOV).set_eflags(0);
}


fuku_instruction fuku_asm_x64::movb(fuku_reg64 dst, fuku_immediate& imm) {
    clear_space();
    if (!is_byte_register(dst)) {
        emit_rex_32(dst);
    }
    emit_b(0xB0 + reg_low_bits(dst));
    emit_b(imm.get_imm() & 0xFF);
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_MOV).set_eflags(0);
}

fuku_instruction fuku_asm_x64::movb(fuku_operand64& dst, fuku_reg64 src) {
    clear_space();
    if (!is_byte_register(src)) {
        emit_rex_32(src, dst);
    }
    else {
        emit_optional_rex_32(src, dst);
    }
    emit_b(0x88);
    emit_operand(src, dst);
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_MOV).set_eflags(0);
}

fuku_instruction fuku_asm_x64::movb(fuku_operand64& dst, fuku_immediate& imm) {
    clear_space();
    emit_optional_rex_32(dst);
    emit_b(0xC6);
    emit_operand(0x0, dst);
    emit_b(imm.get_imm()&0xFF);
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_MOV).set_eflags(0);
}

fuku_instruction fuku_asm_x64::movw(fuku_reg64 dst, fuku_operand64& src) {
    clear_space();
    emit_b(0x66);
    emit_optional_rex_32(dst, src);
    emit_b(0x8B);
    emit_operand(dst, src);
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_MOV).set_eflags(0);
}

fuku_instruction fuku_asm_x64::movw(fuku_operand64& dst, fuku_reg64 src) {
    clear_space();
    emit_b(0x66);
    emit_optional_rex_32(src, dst);
    emit_b(0x89);
    emit_operand(src, dst);
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_MOV).set_eflags(0);
}

fuku_instruction fuku_asm_x64::movw(fuku_operand64& dst, fuku_immediate& imm) {
    clear_space();
    emit_b(0x66);
    emit_optional_rex_32(dst);
    emit_b(0xC7);
    emit_operand(0x0, dst);
    emit_b(imm.get_imm()&0xFF);
    emit_b(uint8_t(imm.get_imm() >> 8));
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_MOV).set_eflags(0);
}

fuku_instruction fuku_asm_x64::mov(fuku_reg64 dst, fuku_operand64& src, fuku_asm64_size size) {
    clear_space();
    emit_rex(dst, src, size);
    emit_b(0x8B);
    emit_operand(dst, src);
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_MOV).set_eflags(0);
}


fuku_instruction fuku_asm_x64::mov(fuku_reg64 dst, fuku_reg64 src, fuku_asm64_size size) {
    clear_space();
    if (reg_low_bits(src) == 4) {
        emit_rex(src, dst, size);
        emit_b(0x89);
        emit_modrm(src, dst);
    }
    else {
        emit_rex(dst, src, size);
        emit_b(0x8B);
        emit_modrm(dst, src);
    }
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_MOV).set_eflags(0);
}

fuku_instruction fuku_asm_x64::mov(fuku_operand64& dst, fuku_reg64 src, fuku_asm64_size size) {
    clear_space();
    emit_rex(src, dst, size);
    emit_b(0x89);
    emit_operand(src, dst);
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_MOV).set_eflags(0);
}


fuku_instruction fuku_asm_x64::mov(fuku_reg64 dst, fuku_immediate& value, fuku_asm64_size size) {
    clear_space();
    emit_rex(dst, size);
    if (size == fuku_asm64_size::asm64_size_64) {
        emit_b(0xC7);
        emit_modrm(0x0, dst);
    }
    else {
        emit_b(0xB8 + reg_low_bits(dst));
    }
    emit_b(value.get_imm()&0xFF);
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_MOV).set_eflags(0);
}

fuku_instruction fuku_asm_x64::mov(fuku_operand64& dst, fuku_immediate& value, fuku_asm64_size size) {
    clear_space();
    emit_rex(dst, size);
    emit_b(0xC7);
    emit_operand(0x0, dst);
    emit_b(value.get_imm() & 0xFF);
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_MOV).set_eflags(0);
}

fuku_instruction fuku_asm_x64::movq(fuku_reg64 dst, int64_t value) {
    clear_space();
    emit_rex_64(dst);
    emit_b(0xB8 | reg_low_bits(dst));

    emit_qw(value);
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_MOV).set_eflags(0);
}

fuku_instruction fuku_asm_x64::movq(fuku_reg64 dst, uint64_t value) {
    return movq(dst, int64_t(value));
}

fuku_instruction fuku_asm_x64::movsxbl(fuku_reg64 dst, fuku_reg64 src) {
    clear_space();

    if (!is_byte_register(src)) {
        emit_rex_32(dst, src);
    }
    else {
        emit_optional_rex_32(dst, src);
    }

    emit_b(0x0F);
    emit_b(0xBE);
    emit_modrm(dst, src);
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_MOVSX).set_eflags(0);
}

fuku_instruction fuku_asm_x64::movsxbl(fuku_reg64 dst, fuku_operand64& src) {
    clear_space();
    emit_optional_rex_32(dst, src);
    emit_b(0x0F);
    emit_b(0xBE);
    emit_operand(dst, src);
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_MOVSX).set_eflags(0);
}

fuku_instruction fuku_asm_x64::movsxbq(fuku_reg64 dst, fuku_operand64& src) {
    clear_space();
    emit_rex_64(dst, src);
    emit_b(0x0F);
    emit_b(0xBE);
    emit_operand(dst, src);
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_MOVSX).set_eflags(0);
}

fuku_instruction fuku_asm_x64::movsxbq(fuku_reg64 dst, fuku_reg64 src) {
    clear_space();
    emit_rex_64(dst, src);
    emit_b(0x0F);
    emit_b(0xBE);
    emit_modrm(dst, src);
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_MOVSX).set_eflags(0);
}

fuku_instruction fuku_asm_x64::movsxwl(fuku_reg64 dst, fuku_reg64 src) {
    clear_space();
    emit_optional_rex_32(dst, src);
    emit_b(0x0F);
    emit_b(0xBF);
    emit_modrm(dst, src);
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_MOVSX).set_eflags(0);
}

fuku_instruction fuku_asm_x64::movsxwl(fuku_reg64 dst, fuku_operand64& src) {
    clear_space();
    emit_optional_rex_32(dst, src);
    emit_b(0x0F);
    emit_b(0xBF);
    emit_operand(dst, src);
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_MOVSX).set_eflags(0);
}

fuku_instruction fuku_asm_x64::movsxwq(fuku_reg64 dst, fuku_operand64& src) {
    clear_space();
    emit_rex_64(dst, src);
    emit_b(0x0F);
    emit_b(0xBF);
    emit_operand(dst, src);
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_MOVSX).set_eflags(0);
}

fuku_instruction fuku_asm_x64::movsxwq(fuku_reg64 dst, fuku_reg64 src) {
    clear_space();
    emit_rex_64(dst, src);
    emit_b(0x0F);
    emit_b(0xBF);
    emit_modrm(dst, src);
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_MOVSX).set_eflags(0);
}

fuku_instruction fuku_asm_x64::movsxlq(fuku_reg64 dst, fuku_reg64 src) {
    clear_space();
    emit_rex_64(dst, src);
    emit_b(0x63);
    emit_modrm(dst, src);
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_MOVSX).set_eflags(0);
}

fuku_instruction fuku_asm_x64::movsxlq(fuku_reg64 dst, fuku_operand64& src) {
    clear_space();
    emit_rex_64(dst, src);
    emit_b(0x63);
    emit_operand(dst, src);
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_MOVSX).set_eflags(0);
}

fuku_instruction fuku_asm_x64::movzxb(fuku_reg64 dst, fuku_operand64& src, fuku_asm64_size size) {
    clear_space();
   
    emit_optional_rex_32(dst, src);
    emit_b(0x0F);
    emit_b(0xB6);
    emit_operand(dst, src);
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_MOVZX).set_eflags(0);
}


fuku_instruction fuku_asm_x64::movzxb(fuku_reg64 dst, fuku_reg64 src, fuku_asm64_size size) {
    clear_space();

    if (!is_byte_register(src)) {
        emit_rex_32(dst, src);
    }
    else {
        emit_optional_rex_32(dst, src);
    }
    emit_b(0x0F);
    emit_b(0xB6);
    emit_modrm(dst, src);
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_MOVZX).set_eflags(0);
}

fuku_instruction fuku_asm_x64::movzxw(fuku_reg64 dst, fuku_operand64& src, fuku_asm64_size size) {
    clear_space();

    emit_optional_rex_32(dst, src);
    emit_b(0x0F);
    emit_b(0xB7);
    emit_operand(dst, src);
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_MOVZX).set_eflags(0);
}


fuku_instruction fuku_asm_x64::movzxw(fuku_reg64 dst, fuku_reg64 src, fuku_asm64_size size) {
    clear_space();

    emit_optional_rex_32(dst, src);
    emit_b(0x0F);
    emit_b(0xB7);
    emit_modrm(dst, src);
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_MOVZX).set_eflags(0);
}


fuku_instruction fuku_asm_x64::repmovsb() {
    clear_space();
    emit_b(0xF3);
    emit_b(0xA4);
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_MOVSB).set_eflags(0);
}


fuku_instruction fuku_asm_x64::repmovsw() {
    clear_space();
    emit_b(0x66); 
    emit_b(0xF3);
    emit_b(0xA4);
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_MOVSW).set_eflags(0);
}


fuku_instruction fuku_asm_x64::repmovs(fuku_asm64_size size) {
    clear_space();
    emit_b(0xF3);
    emit_rex(size);
    emit_b(0xA5);
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_MOVSD).set_eflags(0);
}


fuku_instruction fuku_asm_x64::mull(fuku_reg64 src) {
    clear_space();
    emit_optional_rex_32(src);
    emit_b(0xF7);
    emit_modrm(0x4, src);
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_MUL).set_eflags(X86_EFLAGS_MODIFY_SF | X86_EFLAGS_MODIFY_CF | X86_EFLAGS_MODIFY_OF | X86_EFLAGS_UNDEFINED_ZF | X86_EFLAGS_UNDEFINED_AF | X86_EFLAGS_UNDEFINED_PF);
}

fuku_instruction fuku_asm_x64::mull(fuku_operand64& src) {
    clear_space();
    emit_optional_rex_32(src);
    emit_b(0xF7);
    emit_operand(0x4, src);
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_MUL).set_eflags(X86_EFLAGS_MODIFY_SF | X86_EFLAGS_MODIFY_CF | X86_EFLAGS_MODIFY_OF | X86_EFLAGS_UNDEFINED_ZF | X86_EFLAGS_UNDEFINED_AF | X86_EFLAGS_UNDEFINED_PF);
}


fuku_instruction fuku_asm_x64::mulq(fuku_reg64 src) {
    clear_space();
    emit_rex_64(src);
    emit_b(0xF7);
    emit_modrm(0x4, src);
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_MUL).set_eflags(X86_EFLAGS_MODIFY_SF | X86_EFLAGS_MODIFY_CF | X86_EFLAGS_MODIFY_OF | X86_EFLAGS_UNDEFINED_ZF | X86_EFLAGS_UNDEFINED_AF | X86_EFLAGS_UNDEFINED_PF);
}

fuku_instruction fuku_asm_x64::neg(fuku_reg64 dst, fuku_asm64_size size) {
    clear_space();
    emit_rex(dst, size);
    emit_b(0xF7);
    emit_modrm(0x3, dst);
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_NEG).set_eflags(X86_EFLAGS_MODIFY_OF | X86_EFLAGS_MODIFY_SF | X86_EFLAGS_MODIFY_ZF | X86_EFLAGS_MODIFY_AF | X86_EFLAGS_MODIFY_PF | X86_EFLAGS_MODIFY_CF);
}

fuku_instruction fuku_asm_x64::neg(fuku_operand64& dst, fuku_asm64_size size) {
    clear_space();
    emit_rex_64(dst);
    emit_b(0xF7);
    emit_operand(3, dst);
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_NEG).set_eflags(X86_EFLAGS_MODIFY_OF | X86_EFLAGS_MODIFY_SF | X86_EFLAGS_MODIFY_ZF | X86_EFLAGS_MODIFY_AF | X86_EFLAGS_MODIFY_PF | X86_EFLAGS_MODIFY_CF);
}


fuku_instruction fuku_asm_x64::not(fuku_reg64 dst, fuku_asm64_size size) {
    clear_space();
    emit_rex(dst, size);
    emit_b(0xF7);
    emit_modrm(0x2, dst);
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_NOT).set_eflags(0);
}

fuku_instruction fuku_asm_x64::not(fuku_operand64& dst, fuku_asm64_size size) {
    clear_space();
    emit_rex(dst, size);
    emit_b(0xF7);
    emit_operand(2, dst);
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_NOT).set_eflags(0);
}


fuku_instruction fuku_asm_x64::nop(int n) {

    clear_space();
    while (n > 0) {
        switch (n) {
        case 2:
            emit_b(0x66);
        case 1:
            emit_b(0x90);
            return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_NOP).set_eflags(0);
        case 3:
            emit_b(0x0F);
            emit_b(0x1F);
            emit_b(0x00);
            return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_NOP).set_eflags(0);
        case 4:
            emit_b(0x0F);
            emit_b(0x1F);
            emit_b(0x40);
            emit_b(0x00);
            return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_NOP).set_eflags(0);
        case 6:
            emit_b(0x66);
        case 5:
            emit_b(0x0F);
            emit_b(0x1F);
            emit_b(0x44);
            emit_b(0x00);
            emit_b(0x00);
            return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_NOP).set_eflags(0);
        case 7:
            emit_b(0x0F);
            emit_b(0x1F);
            emit_b(0x80);
            emit_b(0x00);
            emit_b(0x00);
            emit_b(0x00);
            emit_b(0x00);
            return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_NOP).set_eflags(0);
        default:
        case 11:
            emit_b(0x66);
            n--;
        case 10:
            emit_b(0x66);
            n--;
        case 9:
            emit_b(0x66);
            n--;
        case 8:
            emit_b(0x0F);
            emit_b(0x1F);
            emit_b(0x84);
            emit_b(0x00);
            emit_b(0x00);
            emit_b(0x00);
            emit_b(0x00);
            emit_b(0x00);
            n -= 8;
        }
    }
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_NOP).set_eflags(0);
}


fuku_instruction fuku_asm_x64::popq(fuku_reg64 dst) {
    clear_space();
    emit_optional_rex_32(dst);
    emit_b(0x58 | reg_low_bits(dst));
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_POP).set_eflags(0);
}

fuku_instruction fuku_asm_x64::popq(fuku_operand64& dst) {
    clear_space();
    emit_optional_rex_32(dst);
    emit_b(0x8F);
    emit_operand(0, dst);
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_POP).set_eflags(0);
}


fuku_instruction fuku_asm_x64::popfq() {
    clear_space();
    emit_b(0x9D);
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_POPF).set_eflags(X86_EFLAGS_MODIFY_AF | X86_EFLAGS_MODIFY_CF | X86_EFLAGS_MODIFY_SF | X86_EFLAGS_MODIFY_ZF | X86_EFLAGS_MODIFY_PF | X86_EFLAGS_MODIFY_OF | X86_EFLAGS_MODIFY_TF | X86_EFLAGS_MODIFY_IF | X86_EFLAGS_MODIFY_DF | X86_EFLAGS_MODIFY_NT | X86_EFLAGS_MODIFY_RF);
}


fuku_instruction fuku_asm_x64::pushq(fuku_reg64 src) {
    clear_space();
    emit_optional_rex_32(src);
    emit_b(0x50 | reg_low_bits(src));
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_PUSH).set_eflags(0);
}

fuku_instruction fuku_asm_x64::pushq(fuku_operand64& src) {
    clear_space();
    emit_optional_rex_32(src);
    emit_b(0xFF);
    emit_operand(6, src);
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_PUSH).set_eflags(0);
}


fuku_instruction fuku_asm_x64::pushq(fuku_immediate& value) {
    clear_space();
    if (value.is_imm_8()) {
        emit_b(0x6A);
        emit_b(value.get_imm()&0xFF);
    }
    else {
        emit_b(0x68);
        emit_dw(value.get_imm()&0xFFFFFFFF);
    }
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_PUSH).set_eflags(0);
}


fuku_instruction fuku_asm_x64::pushq_imm32(int32_t imm32) {
    clear_space();
    emit_b(0x68);
    emit_dw(imm32);
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_PUSH).set_eflags(0);
}


fuku_instruction fuku_asm_x64::pushfq() {
    clear_space();
    emit_b(0x9C);
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_PUSHF).set_eflags(0);
}


fuku_instruction fuku_asm_x64::ret(int imm16) {
    clear_space();

    if (imm16 == 0) {
        emit_b(0xC3);
    }
    else {
        emit_b(0xC2);
        emit_b(imm16 & 0xFF);
        emit_b((imm16 >> 8) & 0xFF);
    }
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_RET).set_eflags(0);
}


fuku_instruction fuku_asm_x64::ud2() {
    clear_space();
    emit_b(0x0F);
    emit_b(0x0B);
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_UD2).set_eflags(0);
}

fuku_instruction fuku_asm_x64::shld(fuku_reg64 dst, fuku_reg64 src) {
    clear_space();
    emit_rex_64(src, dst);
    emit_b(0x0F);
    emit_b(0xA5);
    emit_modrm(src, dst);
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_SHLD).set_eflags(X86_EFLAGS_MODIFY_CF | X86_EFLAGS_MODIFY_SF | X86_EFLAGS_MODIFY_ZF | X86_EFLAGS_MODIFY_PF | X86_EFLAGS_MODIFY_OF);
}


fuku_instruction fuku_asm_x64::shrd(fuku_reg64 dst, fuku_reg64 src) {
    clear_space();
    emit_rex_64(src, dst);
    emit_b(0x0F);
    emit_b(0xAD);
    emit_modrm(src, dst);
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_SHRD).set_eflags(X86_EFLAGS_UNDEFINED_OF | X86_EFLAGS_MODIFY_SF | X86_EFLAGS_MODIFY_ZF | X86_EFLAGS_UNDEFINED_AF | X86_EFLAGS_MODIFY_PF | X86_EFLAGS_MODIFY_CF);
}

fuku_instruction fuku_asm_x64::xchgb(fuku_reg64 reg, fuku_operand64& op) {
    clear_space();
    if (!is_byte_register(reg)) {
        emit_rex_32(reg, op);
    }
    else {
        emit_optional_rex_32(reg, op);
    }
    emit_b(0x86);
    emit_operand(reg, op);
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_XCHG).set_eflags(0);
}

fuku_instruction fuku_asm_x64::xchgw(fuku_reg64 reg, fuku_operand64& op) {
    clear_space();
    emit_b(0x66);
    emit_optional_rex_32(reg, op);
    emit_b(0x87);
    emit_operand(reg, op);
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_XCHG).set_eflags(0);
}

fuku_instruction fuku_asm_x64::xchg(fuku_reg64 dst, fuku_reg64 src, fuku_asm64_size size) {
    clear_space();
    if (src == fuku_reg64::r_RAX || dst == fuku_reg64::r_RAX) {
        fuku_reg64 other = src == fuku_reg64::r_RAX ? dst : src;
        emit_rex(other, size);
        emit_b(0x90 | reg_low_bits(other));
    }
    else if (reg_low_bits(dst) == 4) {
        emit_rex(dst, src, size);
        emit_b(0x87);
        emit_modrm(dst, src);
    }
    else {
        emit_rex(src, dst, size);
        emit_b(0x87);
        emit_modrm(src, dst);
    }
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_XCHG).set_eflags(0);
}

fuku_instruction fuku_asm_x64::xchg(fuku_reg64 dst, fuku_operand64& src, fuku_asm64_size size) {
    clear_space();
    emit_rex(dst, src, size);
    emit_b(0x87);
    emit_operand(dst, src);
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_XCHG).set_eflags(0);
}


fuku_instruction fuku_asm_x64::testb(fuku_reg64 dst, fuku_reg64 src) {
    return test(dst, src, fuku_asm64_size::asm64_size_8);
}

fuku_instruction fuku_asm_x64::testb(fuku_reg64 reg, fuku_immediate& mask) {
    return test(reg, mask, fuku_asm64_size::asm64_size_8);
}

fuku_instruction fuku_asm_x64::testb(fuku_operand64& op, fuku_immediate& mask) {
    return test(op, mask, fuku_asm64_size::asm64_size_8);
}

fuku_instruction fuku_asm_x64::testb(fuku_operand64& op, fuku_reg64 reg) {
    return test(op, reg, fuku_asm64_size::asm64_size_8);
}

fuku_instruction fuku_asm_x64::testw(fuku_reg64 dst, fuku_reg64 src) {
    return test(dst, src, fuku_asm64_size::asm64_size_16);
}

fuku_instruction fuku_asm_x64::testw(fuku_reg64 reg, fuku_immediate& mask) {
    return test(reg, mask, fuku_asm64_size::asm64_size_16);
}

fuku_instruction fuku_asm_x64::testw(fuku_operand64& op, fuku_immediate& mask) {
    return test(op, mask, fuku_asm64_size::asm64_size_16);
}

fuku_instruction fuku_asm_x64::testw(fuku_operand64& op, fuku_reg64 reg) {
    return test(op, reg, fuku_asm64_size::asm64_size_16);
}

fuku_instruction fuku_asm_x64::test(fuku_reg64 dst, fuku_reg64 src, fuku_asm64_size size) {
    clear_space();
    if (reg_low_bits(src) == 4) std::swap(dst, src);
    if (size == fuku_asm64_size::asm64_size_16) {
        emit_b(0x66);
        size = fuku_asm64_size::asm64_size_32;
    }
    bool byte_operand = size == fuku_asm64_size::asm64_size_8;
    if (byte_operand) {
        size = fuku_asm64_size::asm64_size_32;
        if (!is_byte_register(src) || !is_byte_register(dst)) {
            emit_rex_32(dst, src);
        }
    }
    else {
        emit_rex(dst, src, size);
    }
    emit_b(byte_operand ? 0x84 : 0x85);
    emit_modrm(dst, src);
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_TEST).set_eflags(X86_EFLAGS_RESET_OF | X86_EFLAGS_MODIFY_SF | X86_EFLAGS_MODIFY_ZF | X86_EFLAGS_UNDEFINED_AF | X86_EFLAGS_MODIFY_PF | X86_EFLAGS_RESET_CF);
}


fuku_instruction fuku_asm_x64::test(fuku_reg64 reg, fuku_immediate& mask, fuku_asm64_size size) {
    if (mask.is_imm_8()) {
        size = fuku_asm64_size::asm64_size_8;
    }
    else if (mask.is_imm_16()) {
        size = fuku_asm64_size::asm64_size_16;
    }
    clear_space();
    bool half_word = size == fuku_asm64_size::asm64_size_16;
    if (half_word) {
        emit_b(0x66);
        size = fuku_asm64_size::asm64_size_32;
    }
    bool byte_operand = size == fuku_asm64_size::asm64_size_8;
    if (byte_operand) {
        size = fuku_asm64_size::asm64_size_32;
        if (!is_byte_register(reg)) emit_rex_32(reg);
    }
    else {
        emit_rex(reg, size);
    }
    if (reg == fuku_reg64::r_RAX) {
        emit_b(byte_operand ? 0xA8 : 0xA9);
    }
    else {
        emit_b(byte_operand ? 0xF6 : 0xF7);
        emit_modrm(0x0, reg);
    }
    if (byte_operand) {
        emit_b(mask.get_imm() & 0xFF);
    }
    else if (half_word) {
        emit_w(mask.get_imm() & 0xFFFF);
    }
    else {
        emit_b(mask.get_imm() & 0xFF);
    }
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_TEST).set_eflags(X86_EFLAGS_RESET_OF | X86_EFLAGS_MODIFY_SF | X86_EFLAGS_MODIFY_ZF | X86_EFLAGS_UNDEFINED_AF | X86_EFLAGS_MODIFY_PF | X86_EFLAGS_RESET_CF);
}

fuku_instruction fuku_asm_x64::test(fuku_operand64& op, fuku_immediate& mask, fuku_asm64_size size) {
    if (mask.is_imm_8()) {
        size = fuku_asm64_size::asm64_size_8;
    }
    else if (mask.is_imm_16()) {
        size = fuku_asm64_size::asm64_size_16;
    }
    clear_space();
    bool half_word = size == fuku_asm64_size::asm64_size_16;
    if (half_word) {
        emit_b(0x66);
        size = fuku_asm64_size::asm64_size_32;
    }
    bool byte_operand = size == fuku_asm64_size::asm64_size_8;
    if (byte_operand) {
        size = fuku_asm64_size::asm64_size_32;
    }
    emit_rex(fuku_reg64::r_RAX, op, size);
    emit_b(byte_operand ? 0xF6 : 0xF7);
    emit_operand(fuku_reg64::r_RAX, op); 
    if (byte_operand) {
        emit_b(mask.get_imm() & 0xFF);
    }
    else if (half_word) {
        emit_w(mask.get_imm() & 0xFFFF);
    }
    else {
        emit_b(mask.get_imm() & 0xFF);
    }
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_TEST).set_eflags(X86_EFLAGS_RESET_OF | X86_EFLAGS_MODIFY_SF | X86_EFLAGS_MODIFY_ZF | X86_EFLAGS_UNDEFINED_AF | X86_EFLAGS_MODIFY_PF | X86_EFLAGS_RESET_CF);
}

fuku_instruction fuku_asm_x64::test(fuku_operand64& op, fuku_reg64 reg, fuku_asm64_size size) {
    clear_space();
    if (size == fuku_asm64_size::asm64_size_16) {
        emit_b(0x66);
        size = fuku_asm64_size::asm64_size_32;
    }
    bool byte_operand = size == fuku_asm64_size::asm64_size_8;
    if (byte_operand) {
        size = fuku_asm64_size::asm64_size_32;
        if (!is_byte_register(reg)) {
            emit_rex_32(reg, op);
        }
        else {
            emit_optional_rex_32(reg, op);
        }
    }
    else {
        emit_rex(reg, op, size);
    }
    emit_b(byte_operand ? 0x84 : 0x85);
    emit_operand(reg, op);
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_TEST).set_eflags(X86_EFLAGS_RESET_OF | X86_EFLAGS_MODIFY_SF | X86_EFLAGS_MODIFY_ZF | X86_EFLAGS_UNDEFINED_AF | X86_EFLAGS_MODIFY_PF | X86_EFLAGS_RESET_CF);
}

fuku_instruction fuku_asm_x64::add(fuku_reg64 dst, fuku_reg64 src, fuku_asm64_size size) {
    arithmetic_op(0x03, dst, src, size);
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_ADD).set_eflags(X86_EFLAGS_MODIFY_OF | X86_EFLAGS_MODIFY_SF | X86_EFLAGS_MODIFY_ZF | X86_EFLAGS_MODIFY_AF | X86_EFLAGS_MODIFY_PF | X86_EFLAGS_MODIFY_CF);
}

fuku_instruction fuku_asm_x64::add(fuku_reg64 dst, fuku_immediate& src, fuku_asm64_size size) {
    immediate_arithmetic_op(0x0, dst, src, size);
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_ADD).set_eflags(X86_EFLAGS_MODIFY_OF | X86_EFLAGS_MODIFY_SF | X86_EFLAGS_MODIFY_ZF | X86_EFLAGS_MODIFY_AF | X86_EFLAGS_MODIFY_PF | X86_EFLAGS_MODIFY_CF);
}

fuku_instruction fuku_asm_x64::add(fuku_reg64 dst, fuku_operand64& src, fuku_asm64_size size) {
    arithmetic_op(0x03, dst, src, size);
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_ADD).set_eflags(X86_EFLAGS_MODIFY_OF | X86_EFLAGS_MODIFY_SF | X86_EFLAGS_MODIFY_ZF | X86_EFLAGS_MODIFY_AF | X86_EFLAGS_MODIFY_PF | X86_EFLAGS_MODIFY_CF);
}

fuku_instruction fuku_asm_x64::add(fuku_operand64& dst, fuku_reg64 src, fuku_asm64_size size) {
    arithmetic_op(0x1, src, dst, size);
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_ADD).set_eflags(X86_EFLAGS_MODIFY_OF | X86_EFLAGS_MODIFY_SF | X86_EFLAGS_MODIFY_ZF | X86_EFLAGS_MODIFY_AF | X86_EFLAGS_MODIFY_PF | X86_EFLAGS_MODIFY_CF);
}

fuku_instruction fuku_asm_x64::add(fuku_operand64& dst, fuku_immediate& src, fuku_asm64_size size) {
    immediate_arithmetic_op(0x0, dst, src, size);
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_ADD).set_eflags(X86_EFLAGS_MODIFY_OF | X86_EFLAGS_MODIFY_SF | X86_EFLAGS_MODIFY_ZF | X86_EFLAGS_MODIFY_AF | X86_EFLAGS_MODIFY_PF | X86_EFLAGS_MODIFY_CF);
}

fuku_instruction fuku_asm_x64::and(fuku_reg64 dst, fuku_reg64 src, fuku_asm64_size size) {
    arithmetic_op(0x23, dst, src, size);
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_AND).set_eflags(X86_EFLAGS_RESET_OF | X86_EFLAGS_MODIFY_SF | X86_EFLAGS_MODIFY_ZF | X86_EFLAGS_UNDEFINED_AF | X86_EFLAGS_MODIFY_PF | X86_EFLAGS_RESET_CF);
}

fuku_instruction fuku_asm_x64::and(fuku_reg64 dst, fuku_operand64& src, fuku_asm64_size size) {
    arithmetic_op(0x23, dst, src, size);
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_AND).set_eflags(X86_EFLAGS_RESET_OF | X86_EFLAGS_MODIFY_SF | X86_EFLAGS_MODIFY_ZF | X86_EFLAGS_UNDEFINED_AF | X86_EFLAGS_MODIFY_PF | X86_EFLAGS_RESET_CF);
}

fuku_instruction fuku_asm_x64::and(fuku_operand64& dst, fuku_reg64 src, fuku_asm64_size size) {
    arithmetic_op(0x21, src, dst, size);
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_AND).set_eflags(X86_EFLAGS_RESET_OF | X86_EFLAGS_MODIFY_SF | X86_EFLAGS_MODIFY_ZF | X86_EFLAGS_UNDEFINED_AF | X86_EFLAGS_MODIFY_PF | X86_EFLAGS_RESET_CF);
}

fuku_instruction fuku_asm_x64::and(fuku_reg64 dst, fuku_immediate& src, fuku_asm64_size size) {
    immediate_arithmetic_op(0x4, dst, src, size);
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_AND).set_eflags(X86_EFLAGS_RESET_OF | X86_EFLAGS_MODIFY_SF | X86_EFLAGS_MODIFY_ZF | X86_EFLAGS_UNDEFINED_AF | X86_EFLAGS_MODIFY_PF | X86_EFLAGS_RESET_CF);
}

fuku_instruction fuku_asm_x64::and(fuku_operand64& dst, fuku_immediate& src, fuku_asm64_size size) {
    immediate_arithmetic_op(0x4, dst, src, size);
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_AND).set_eflags(X86_EFLAGS_RESET_OF | X86_EFLAGS_MODIFY_SF | X86_EFLAGS_MODIFY_ZF | X86_EFLAGS_UNDEFINED_AF | X86_EFLAGS_MODIFY_PF | X86_EFLAGS_RESET_CF);
}

fuku_instruction fuku_asm_x64::cmp(fuku_reg64 dst, fuku_reg64 src, fuku_asm64_size size) {
    arithmetic_op(0x3B, dst, src, size);
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_CMP).set_eflags(X86_EFLAGS_MODIFY_OF | X86_EFLAGS_MODIFY_SF | X86_EFLAGS_MODIFY_ZF | X86_EFLAGS_MODIFY_AF | X86_EFLAGS_MODIFY_PF | X86_EFLAGS_MODIFY_CF);
}

fuku_instruction fuku_asm_x64::cmp(fuku_reg64 dst, fuku_operand64& src, fuku_asm64_size size) {
    arithmetic_op(0x3B, dst, src, size);
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_CMP).set_eflags(X86_EFLAGS_MODIFY_OF | X86_EFLAGS_MODIFY_SF | X86_EFLAGS_MODIFY_ZF | X86_EFLAGS_MODIFY_AF | X86_EFLAGS_MODIFY_PF | X86_EFLAGS_MODIFY_CF);
}

fuku_instruction fuku_asm_x64::cmp(fuku_operand64& dst, fuku_reg64 src, fuku_asm64_size size) {
    arithmetic_op(0x39, src, dst, size);
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_CMP).set_eflags(X86_EFLAGS_MODIFY_OF | X86_EFLAGS_MODIFY_SF | X86_EFLAGS_MODIFY_ZF | X86_EFLAGS_MODIFY_AF | X86_EFLAGS_MODIFY_PF | X86_EFLAGS_MODIFY_CF);
}

fuku_instruction fuku_asm_x64::cmp(fuku_reg64 dst, fuku_immediate& src, fuku_asm64_size size) {
    immediate_arithmetic_op(0x7, dst, src, size);
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_CMP).set_eflags(X86_EFLAGS_MODIFY_OF | X86_EFLAGS_MODIFY_SF | X86_EFLAGS_MODIFY_ZF | X86_EFLAGS_MODIFY_AF | X86_EFLAGS_MODIFY_PF | X86_EFLAGS_MODIFY_CF);
}

fuku_instruction fuku_asm_x64::cmp(fuku_operand64& dst, fuku_immediate& src, fuku_asm64_size size) {
    immediate_arithmetic_op(0x7, dst, src, size); 
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_CMP).set_eflags(X86_EFLAGS_MODIFY_OF | X86_EFLAGS_MODIFY_SF | X86_EFLAGS_MODIFY_ZF | X86_EFLAGS_MODIFY_AF | X86_EFLAGS_MODIFY_PF | X86_EFLAGS_MODIFY_CF);
}

fuku_instruction fuku_asm_x64::or(fuku_reg64 dst, fuku_reg64 src, fuku_asm64_size size) {
    arithmetic_op(0x0B, dst, src, size);
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_OR).set_eflags(X86_EFLAGS_RESET_OF | X86_EFLAGS_MODIFY_SF | X86_EFLAGS_MODIFY_ZF | X86_EFLAGS_UNDEFINED_AF | X86_EFLAGS_MODIFY_PF | X86_EFLAGS_RESET_CF);
}

fuku_instruction fuku_asm_x64::or(fuku_reg64 dst, fuku_operand64& src, fuku_asm64_size size) {
    arithmetic_op(0x0B, dst, src, size);
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_OR).set_eflags(X86_EFLAGS_RESET_OF | X86_EFLAGS_MODIFY_SF | X86_EFLAGS_MODIFY_ZF | X86_EFLAGS_UNDEFINED_AF | X86_EFLAGS_MODIFY_PF | X86_EFLAGS_RESET_CF);
}

fuku_instruction fuku_asm_x64::or(fuku_operand64& dst, fuku_reg64 src, fuku_asm64_size size) {
    arithmetic_op(0x9, src, dst, size);
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_OR).set_eflags(X86_EFLAGS_RESET_OF | X86_EFLAGS_MODIFY_SF | X86_EFLAGS_MODIFY_ZF | X86_EFLAGS_UNDEFINED_AF | X86_EFLAGS_MODIFY_PF | X86_EFLAGS_RESET_CF);
}

fuku_instruction fuku_asm_x64::or(fuku_reg64 dst, fuku_immediate& src, fuku_asm64_size size) {
    immediate_arithmetic_op(0x1, dst, src, size);
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_OR).set_eflags(X86_EFLAGS_RESET_OF | X86_EFLAGS_MODIFY_SF | X86_EFLAGS_MODIFY_ZF | X86_EFLAGS_UNDEFINED_AF | X86_EFLAGS_MODIFY_PF | X86_EFLAGS_RESET_CF);
}

fuku_instruction fuku_asm_x64::or(fuku_operand64& dst, fuku_immediate& src, fuku_asm64_size size) {
    immediate_arithmetic_op(0x1, dst, src, size);
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_OR).set_eflags(X86_EFLAGS_RESET_OF | X86_EFLAGS_MODIFY_SF | X86_EFLAGS_MODIFY_ZF | X86_EFLAGS_UNDEFINED_AF | X86_EFLAGS_MODIFY_PF | X86_EFLAGS_RESET_CF);
}

fuku_instruction fuku_asm_x64::sbb(fuku_reg64 dst, fuku_reg64 src, fuku_asm64_size size) {
    arithmetic_op(0x1b, dst, src, size);
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_SBB).set_eflags(X86_EFLAGS_MODIFY_OF | X86_EFLAGS_MODIFY_SF | X86_EFLAGS_MODIFY_ZF | X86_EFLAGS_MODIFY_AF | X86_EFLAGS_MODIFY_PF | X86_EFLAGS_MODIFY_CF);
}

fuku_instruction fuku_asm_x64::sub(fuku_reg64 dst, fuku_reg64 src, fuku_asm64_size size) {
    arithmetic_op(0x2B, dst, src, size);
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_SUB).set_eflags(X86_EFLAGS_MODIFY_OF | X86_EFLAGS_MODIFY_SF | X86_EFLAGS_MODIFY_ZF | X86_EFLAGS_MODIFY_AF | X86_EFLAGS_MODIFY_PF | X86_EFLAGS_MODIFY_CF);
}

fuku_instruction fuku_asm_x64::sub(fuku_reg64 dst, fuku_immediate& src, fuku_asm64_size size) {
    immediate_arithmetic_op(0x5, dst, src, size);
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_SUB).set_eflags(X86_EFLAGS_MODIFY_OF | X86_EFLAGS_MODIFY_SF | X86_EFLAGS_MODIFY_ZF | X86_EFLAGS_MODIFY_AF | X86_EFLAGS_MODIFY_PF | X86_EFLAGS_MODIFY_CF);
}

fuku_instruction fuku_asm_x64::sub(fuku_reg64 dst, fuku_operand64& src, fuku_asm64_size size) {
    arithmetic_op(0x2B, dst, src, size);
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_SUB).set_eflags(X86_EFLAGS_MODIFY_OF | X86_EFLAGS_MODIFY_SF | X86_EFLAGS_MODIFY_ZF | X86_EFLAGS_MODIFY_AF | X86_EFLAGS_MODIFY_PF | X86_EFLAGS_MODIFY_CF);
}

fuku_instruction fuku_asm_x64::sub(fuku_operand64& dst, fuku_reg64 src, fuku_asm64_size size) {
    arithmetic_op(0x29, src, dst, size);
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_SUB).set_eflags(X86_EFLAGS_MODIFY_OF | X86_EFLAGS_MODIFY_SF | X86_EFLAGS_MODIFY_ZF | X86_EFLAGS_MODIFY_AF | X86_EFLAGS_MODIFY_PF | X86_EFLAGS_MODIFY_CF);
}

fuku_instruction fuku_asm_x64::sub(fuku_operand64& dst, fuku_immediate& src, fuku_asm64_size size) {
    immediate_arithmetic_op(0x5, dst, src, size);
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_SUB).set_eflags(X86_EFLAGS_MODIFY_OF | X86_EFLAGS_MODIFY_SF | X86_EFLAGS_MODIFY_ZF | X86_EFLAGS_MODIFY_AF | X86_EFLAGS_MODIFY_PF | X86_EFLAGS_MODIFY_CF);
}

fuku_instruction fuku_asm_x64::xor(fuku_reg64 dst, fuku_reg64 src, fuku_asm64_size size) {
    if (size == fuku_asm64_size::asm64_size_64 && dst == src) {
        arithmetic_op(0x33, dst, src, fuku_asm64_size::asm64_size_32);
    }
    else {
        arithmetic_op(0x33, dst, src, size);
    }
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_XOR).set_eflags(X86_EFLAGS_RESET_OF | X86_EFLAGS_MODIFY_SF | X86_EFLAGS_MODIFY_ZF | X86_EFLAGS_UNDEFINED_AF | X86_EFLAGS_MODIFY_PF | X86_EFLAGS_RESET_CF);
}

fuku_instruction fuku_asm_x64::xor(fuku_reg64 dst, fuku_operand64& src, fuku_asm64_size size) {
    arithmetic_op(0x33, dst, src, size);
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_XOR).set_eflags(X86_EFLAGS_RESET_OF | X86_EFLAGS_MODIFY_SF | X86_EFLAGS_MODIFY_ZF | X86_EFLAGS_UNDEFINED_AF | X86_EFLAGS_MODIFY_PF | X86_EFLAGS_RESET_CF);
}

fuku_instruction fuku_asm_x64::xor(fuku_reg64 dst, fuku_immediate& src, fuku_asm64_size size) {
    immediate_arithmetic_op(0x6, dst, src, size);
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_XOR).set_eflags(X86_EFLAGS_RESET_OF | X86_EFLAGS_MODIFY_SF | X86_EFLAGS_MODIFY_ZF | X86_EFLAGS_UNDEFINED_AF | X86_EFLAGS_MODIFY_PF | X86_EFLAGS_RESET_CF);
}

fuku_instruction fuku_asm_x64::xor(fuku_operand64& dst, fuku_immediate& src, fuku_asm64_size size) {
    immediate_arithmetic_op(0x6, dst, src, size);
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_XOR).set_eflags(X86_EFLAGS_RESET_OF | X86_EFLAGS_MODIFY_SF | X86_EFLAGS_MODIFY_ZF | X86_EFLAGS_UNDEFINED_AF | X86_EFLAGS_MODIFY_PF | X86_EFLAGS_RESET_CF);
}

fuku_instruction fuku_asm_x64::xor(fuku_operand64& dst, fuku_reg64 src, fuku_asm64_size size) {
    arithmetic_op(0x31, src, dst, size);
    return fuku_instruction().set_op_code(bytecode, length).set_id(X86_INS_XOR).set_eflags(X86_EFLAGS_RESET_OF | X86_EFLAGS_MODIFY_SF | X86_EFLAGS_MODIFY_ZF | X86_EFLAGS_UNDEFINED_AF | X86_EFLAGS_MODIFY_PF | X86_EFLAGS_RESET_CF);
}