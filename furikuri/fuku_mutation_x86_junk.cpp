#include "stdafx.h"
#include "fuku_mutation_x86_junk.h"

uint32_t reg_sizes[] = {
    1,
    2,
    4
};


//transfer reg1,reg2
bool junk_low_pattern_1(fuku_assambler& f_asm, fuku_code_holder& code_holder, linestorage::iterator lines_iter, 
    bool unstable_stack, uint64_t eflags_changes, uint64_t regs_changes) {

    uint32_t reg_size = reg_sizes[FUKU_GET_RAND(0, 2)];

    fuku_register reg1_ = get_random_free_flag_reg(regs_changes, reg_size, true, X86_REGISTER_SP | X86_REGISTER_ESP);

    if (reg1_.get_reg() == FUKU_REG_NONE) {
        return false;
    }
    

    switch (FUKU_GET_RAND(0, 1)) {
        case 0: {
            fuku_register reg2_ = get_random_reg(reg_size, true);

            f_asm.mov(reg1_, reg2_);
            f_asm.get_context().inst->
                set_eflags(eflags_changes)
                .set_custom_flags(regs_changes);


            break;
        }
        case 1: {
            fuku_register reg2_ = get_random_free_flag_reg(regs_changes, reg_size, true, X86_REGISTER_SP | X86_REGISTER_ESP);

            f_asm.xchg(reg1_, reg2_);
            f_asm.get_context().inst->
                set_eflags(eflags_changes)
                .set_custom_flags(regs_changes);


            break;
        }
    }
    return true;
}


//logical reg1,reg2
bool junk_low_pattern_2(fuku_assambler& f_asm, fuku_code_holder& code_holder, linestorage::iterator lines_iter,
    bool unstable_stack, uint64_t eflags_changes, uint64_t regs_changes) {


    if (!has_inst_free_eflags(eflags_changes,
        X86_EFLAGS_MODIFY_OF | X86_EFLAGS_MODIFY_SF | X86_EFLAGS_MODIFY_ZF | X86_EFLAGS_MODIFY_AF | X86_EFLAGS_MODIFY_CF | X86_EFLAGS_MODIFY_PF)) {

        return false;
    }

    uint32_t reg_size = reg_sizes[FUKU_GET_RAND(0, 2)];


    fuku_register reg1_ = get_random_free_flag_reg(regs_changes, reg_size, true, X86_REGISTER_SP | X86_REGISTER_ESP);

    if (reg1_.get_reg() == FUKU_REG_NONE) {
        return false;
    }
    fuku_register reg2_ = get_random_reg(reg_size, true);


    switch (FUKU_GET_RAND(0, 3)) {

    case 0: {
        f_asm.xor_(reg1_, reg2_);
        f_asm.get_context().inst->
            set_eflags(eflags_changes)
            .set_custom_flags(regs_changes);

        break;
    }

    case 1: {
        f_asm.and_(reg1_, reg2_);
        f_asm.get_context().inst->
            set_eflags(eflags_changes)
            .set_custom_flags(regs_changes);

        break;
    }
    case 2: {
        f_asm.or_(reg1_, reg2_);
        f_asm.get_context().inst->
            set_eflags(eflags_changes)
            .set_custom_flags(regs_changes);

        break;
    }
    case 3: {
        f_asm.test(reg1_, reg2_);
        f_asm.get_context().inst->
            set_eflags(eflags_changes)
            .set_custom_flags(regs_changes);

        break;
    }
    }

    return true;
}


//arithmetic reg1,reg2
bool junk_low_pattern_3(fuku_assambler& f_asm, fuku_code_holder& code_holder, linestorage::iterator lines_iter,
    bool unstable_stack, uint64_t eflags_changes, uint64_t regs_changes) {


    if (!has_inst_free_eflags(eflags_changes,
        X86_EFLAGS_MODIFY_OF | X86_EFLAGS_MODIFY_SF | X86_EFLAGS_MODIFY_ZF | X86_EFLAGS_MODIFY_AF | X86_EFLAGS_MODIFY_CF | X86_EFLAGS_MODIFY_PF)) {

        return false;
    }

    uint32_t reg_size = reg_sizes[FUKU_GET_RAND(0, 2)];

    fuku_register reg1_ = get_random_free_flag_reg(regs_changes, reg_size, true, X86_REGISTER_SP | X86_REGISTER_ESP);

    if (reg1_.get_reg() == FUKU_REG_NONE) {
        return false;
    }
    fuku_register reg2_ = get_random_reg(reg_size, true);


    switch (FUKU_GET_RAND(0, 4)) {

    case 0: {
        f_asm.add(reg1_, reg2_);
        f_asm.get_context().inst->
            set_eflags(eflags_changes)
            .set_custom_flags(regs_changes);

        break;
    }

    case 1: {
        f_asm.adc(reg1_, reg2_);
        f_asm.get_context().inst->
            set_eflags(eflags_changes)
            .set_custom_flags(regs_changes);

        break;
    }
    case 2: {
        f_asm.sub(reg1_, reg2_);
        f_asm.get_context().inst->
            set_eflags(eflags_changes)
            .set_custom_flags(regs_changes);

        break;
    }
    case 3: {
        f_asm.sbb(reg1_, reg2_);
        f_asm.get_context().inst->
            set_eflags(eflags_changes)
            .set_custom_flags(regs_changes);

        break;
    }
    case 4: {
        f_asm.cmp(reg1_, reg2_);
        f_asm.get_context().inst->
            set_eflags(eflags_changes)
            .set_custom_flags(regs_changes);

        break;
    }
    }

    return true;
}


//rotate reg1,val
bool junk_low_pattern_4(fuku_assambler& f_asm, fuku_code_holder& code_holder, linestorage::iterator lines_iter,
    bool unstable_stack, uint64_t eflags_changes, uint64_t regs_changes) {


    if (!has_inst_free_eflags(eflags_changes,
        X86_EFLAGS_MODIFY_OF | X86_EFLAGS_MODIFY_CF)) {

        return false;
    }

    uint32_t reg_size = reg_sizes[FUKU_GET_RAND(0, 2)];

    

    fuku_register reg1_ = get_random_free_flag_reg(regs_changes, reg_size, true, X86_REGISTER_SP | X86_REGISTER_ESP);

    if (reg1_.get_reg() == FUKU_REG_NONE) {
        return false;
    }

    fuku_type src = fuku_immediate(FUKU_GET_RAND(1, reg_size*16 - 1));

    if (FUKU_GET_RAND(0, 1)) {
        src = fuku_register(FUKU_REG_CL);
    }

    switch (FUKU_GET_RAND(0, 3)) {

    case 0: {
       f_asm.rol(reg1_, src);
       f_asm.get_context().inst->
            set_eflags(eflags_changes)
            .set_custom_flags(regs_changes);

        break;
    }

    case 1: {
        f_asm.ror(reg1_, src);
        f_asm.get_context().inst->
            set_eflags(eflags_changes)
            .set_custom_flags(regs_changes);

        break;
    }
    case 2: {
        f_asm.rcl(reg1_, src);
        f_asm.get_context().inst->
            set_eflags(eflags_changes)
            .set_custom_flags(regs_changes);

        break;
    }
    case 3: {
        f_asm.rcr(reg1_, src);
        f_asm.get_context().inst->
            set_eflags(eflags_changes)
            .set_custom_flags(regs_changes);

        break;
    }

    }

    return true;
}


//shift reg1,val
bool junk_low_pattern_5(fuku_assambler& f_asm, fuku_code_holder& code_holder, linestorage::iterator lines_iter,
    bool unstable_stack, uint64_t eflags_changes, uint64_t regs_changes) {


    if (!has_inst_free_eflags(eflags_changes,
        X86_EFLAGS_MODIFY_OF | X86_EFLAGS_MODIFY_SF | X86_EFLAGS_MODIFY_ZF | X86_EFLAGS_MODIFY_AF | X86_EFLAGS_MODIFY_PF | X86_EFLAGS_MODIFY_CF)) {

        return false;
    }

    uint32_t reg_size = reg_sizes[FUKU_GET_RAND(0, 2)];



    fuku_register reg1_ = get_random_free_flag_reg(regs_changes, reg_size, true, X86_REGISTER_SP | X86_REGISTER_ESP);

    if (reg1_.get_reg() == FUKU_REG_NONE) {
        return false;
    }

    fuku_type src = fuku_immediate(FUKU_GET_RAND(1, reg_size * 16 - 1));

    if (FUKU_GET_RAND(0, 1)) {
        src = fuku_register(FUKU_REG_CL);
    }

    switch (FUKU_GET_RAND(0, 2)) {

    case 0: {
        f_asm.sar(reg1_, src);
        f_asm.get_context().inst->
            set_eflags(eflags_changes)
            .set_custom_flags(regs_changes);

        break;
    }

    case 1: {
        f_asm.shl(reg1_, src);
        f_asm.get_context().inst->
            set_eflags(eflags_changes)
            .set_custom_flags(regs_changes);

        break;
    }
    case 2: {
        f_asm.shr(reg1_, src);
        f_asm.get_context().inst->
            set_eflags(eflags_changes)
            .set_custom_flags(regs_changes);

        break;
    }

    }

    return true;
}



//bittest reg1,val
bool junk_low_pattern_6(fuku_assambler& f_asm, fuku_code_holder& code_holder, linestorage::iterator lines_iter,
    bool unstable_stack, uint64_t eflags_changes, uint64_t regs_changes) {


    if (!has_inst_free_eflags(eflags_changes,
        X86_EFLAGS_MODIFY_OF | X86_EFLAGS_MODIFY_SF | X86_EFLAGS_MODIFY_AF | X86_EFLAGS_MODIFY_PF | X86_EFLAGS_MODIFY_CF)) {

        return false;
    }

    uint32_t reg_size = reg_sizes[FUKU_GET_RAND(1, 2)];



    fuku_register reg1_ = get_random_free_flag_reg(regs_changes, reg_size, true);

    if (reg1_.get_reg() == FUKU_REG_NONE) {
        return false;
    }

    fuku_type src = fuku_immediate(FUKU_GET_RAND(1, reg_size * 16 - 1));

    if (FUKU_GET_RAND(0, 1)) {
        src = reg_(get_random_reg(reg_size, true));
    }

    switch (FUKU_GET_RAND(0, 3)) {

    case 0: {
        f_asm.bt(reg1_, src);
        f_asm.get_context().inst->
            set_eflags(eflags_changes)
            .set_custom_flags(regs_changes);

        break;
    }

    case 1: {
        f_asm.btc(reg1_, src);
        f_asm.get_context().inst->
            set_eflags(eflags_changes)
            .set_custom_flags(regs_changes);

        break;
    }
    case 2: {
        f_asm.bts(reg1_, src);
        f_asm.get_context().inst->
            set_eflags(eflags_changes)
            .set_custom_flags(regs_changes);

        break;
    }
    case 3: {
        f_asm.btr(reg1_, src);
        f_asm.get_context().inst->
            set_eflags(eflags_changes)
            .set_custom_flags(regs_changes);

        break;
    }
    }

    return true;
}

//inc reg
//neg reg
//inc reg
//neg reg
bool junk_high_pattern_1(fuku_assambler& f_asm, fuku_code_holder& code_holder, linestorage::iterator lines_iter, 
    bool unstable_stack, uint64_t eflags_changes, uint64_t regs_changes) {

    if (!has_inst_free_eflags(eflags_changes,
        X86_EFLAGS_MODIFY_OF | X86_EFLAGS_MODIFY_SF | X86_EFLAGS_MODIFY_ZF | X86_EFLAGS_MODIFY_AF | X86_EFLAGS_MODIFY_CF | X86_EFLAGS_MODIFY_PF)) {

        return false;
    }

    uint32_t reg_size = reg_sizes[FUKU_GET_RAND(0, 2)];
    fuku_register reg1_ = get_random_free_flag_reg(regs_changes, reg_size, true, X86_REGISTER_SP | X86_REGISTER_ESP);

    if (reg1_.get_reg() == FUKU_REG_NONE) {
        return false;
    }

    f_asm.inc(reg1_);
    f_asm.get_context().inst->
        set_eflags(eflags_changes)
        .set_custom_flags(regs_changes);
    f_asm.neg(reg1_);
    f_asm.get_context().inst->
        set_eflags(eflags_changes)
        .set_custom_flags(regs_changes);
    f_asm.inc(reg1_);
    f_asm.get_context().inst->
        set_eflags(eflags_changes)
        .set_custom_flags(regs_changes);
    f_asm.neg(reg1_);
    f_asm.get_context().inst->
        set_eflags(eflags_changes)
        .set_custom_flags(regs_changes);

    return true;
}

//not reg1
//not reg1
bool junk_high_pattern_2(fuku_assambler& f_asm, fuku_code_holder& code_holder, linestorage::iterator lines_iter,
    bool unstable_stack, uint64_t eflags_changes, uint64_t regs_changes) {

    uint32_t reg_size = reg_sizes[FUKU_GET_RAND(0, 2)];
    fuku_register reg1_ = get_random_reg(reg_size, true, X86_REGISTER_SP | X86_REGISTER_ESP);

    uint64_t flag_reg = convert_fuku_reg_to_flag_reg(reg1_.get_reg());


    f_asm.not_(reg1_);
    f_asm.get_context().inst->
        set_eflags(eflags_changes)
        .set_custom_flags(regs_changes);
    f_asm.not_(reg1_);
    f_asm.get_context().inst->
        set_eflags(eflags_changes & ~(flag_reg))
        .set_custom_flags(regs_changes);

    return true;
}



//push reg1
//pop  reg1
bool junk_high_pattern_3(fuku_assambler& f_asm, fuku_code_holder& code_holder, linestorage::iterator lines_iter,
    bool unstable_stack, uint64_t eflags_changes, uint64_t regs_changes) {

    if (unstable_stack) {
        return false;
    }

    uint32_t reg_size = reg_sizes[FUKU_GET_RAND(1, 2)];
    fuku_register reg1_ = get_random_reg(reg_size, true, X86_REGISTER_SP | X86_REGISTER_ESP);

    uint64_t flag_reg = convert_fuku_reg_to_flag_reg(reg1_.get_reg());

    f_asm.push(reg1_);
    f_asm.get_context().inst->
        set_eflags(eflags_changes)
        .set_custom_flags(regs_changes);
    f_asm.pop(reg1_);
    f_asm.get_context().inst->
        set_eflags(eflags_changes)
        .set_custom_flags(regs_changes | flag_reg);

    return true;
}


//jcc next_inst : Todo
bool junk_high_pattern_4(fuku_assambler& f_asm, fuku_code_holder& code_holder, linestorage::iterator lines_iter,
    bool unstable_stack, uint64_t eflags_changes, uint64_t regs_changes) {

    if (lines_iter == code_holder.get_lines().end()) {
        return false;
    }

    uint8_t cond = FUKU_GET_RAND(0, 15);

    f_asm.jcc(fuku_condition(cond), imm(-1));
    f_asm.get_context().inst->
        set_eflags(eflags_changes)
        .set_custom_flags(regs_changes)
        .set_rip_relocation_idx(
            code_holder.create_rip_relocation(
                f_asm.get_context().immediate_offset, 
                &(*lines_iter)
            )
        );

    return true;
}


bool fuku_junk_generic_low(fuku_assambler& f_asm, fuku_code_holder& code_holder, linestorage::iterator lines_iter,
    bool unstable_stack, uint64_t eflags_changes, uint64_t regs_changes) {

    switch (FUKU_GET_RAND(0, 5)) {
    case 0: {
        return junk_low_pattern_1(f_asm, code_holder, lines_iter,
            unstable_stack, eflags_changes, regs_changes);
    }
    case 1: {
        return junk_low_pattern_2(f_asm, code_holder, lines_iter,
            unstable_stack, eflags_changes, regs_changes);
    }
    case 2: {
        return junk_low_pattern_3(f_asm, code_holder, lines_iter,
            unstable_stack, eflags_changes, regs_changes);
    }
    case 3: {
        return junk_low_pattern_4(f_asm, code_holder, lines_iter,
            unstable_stack, eflags_changes, regs_changes);
    }
    case 4: {
        return junk_low_pattern_5(f_asm, code_holder, lines_iter,
            unstable_stack, eflags_changes, regs_changes);
    }
    case 5: {
        return junk_low_pattern_6(f_asm, code_holder, lines_iter,
            unstable_stack, eflags_changes, regs_changes);
    }
    }

    return false;
}


bool fuku_junk_generic_high(fuku_assambler& f_asm, fuku_code_holder& code_holder, linestorage::iterator lines_iter,
    bool unstable_stack, uint64_t eflags_changes, uint64_t regs_changes) {

    switch (FUKU_GET_RAND(0, 3)) {
    case 0: {
        return junk_high_pattern_1(f_asm, code_holder, lines_iter,
            unstable_stack, eflags_changes, regs_changes);
    }
    case 1: {
        return junk_high_pattern_2(f_asm, code_holder, lines_iter,
            unstable_stack, eflags_changes, regs_changes);
    }
    case 2: {
        return junk_high_pattern_3(f_asm, code_holder, lines_iter,
            unstable_stack, eflags_changes, regs_changes);
    }
    case 3: {
        //return junk_high_pattern_4(f_asm, code_holder, lines_iter,
        //    unstable_stack, eflags_changes, regs_changes);
    }
    }

    return false;
}

bool fuku_junk_generic(fuku_assambler& f_asm, fuku_code_holder& code_holder, linestorage::iterator lines_iter,
    bool unstable_stack, uint64_t eflags_changes, uint64_t regs_changes) {

    switch (FUKU_GET_RAND(0, 3)) {
    case 0:
    case 1: {
        return fuku_junk_generic_low(f_asm, code_holder, lines_iter,
            unstable_stack, eflags_changes, regs_changes);
    }
    case 2:
    case 3: {
        return fuku_junk_generic_high(f_asm, code_holder, lines_iter,
            unstable_stack, eflags_changes, regs_changes);
    }
    }

    return false;
}
