#pragma once

#include "..\capstone\include\capstone\capstone.h"
#include "..\fukutasm\fukutasm\fukutasm.h"

using namespace fukutasm;

class fuku_code_analyzer;
class fuku_obfuscator;
class fuku_instruction;
class fuku_virtualizer;
class fuku_mutation;


struct fuku_ob_settings {
    unsigned int complexity;        //number of passes for single line
    unsigned int number_of_passes;  //number of passes for full code
    float junk_chance;   //0.f - 100.f chance of adding junk
    float block_chance;  //0.f - 100.f chance of generation new code graph
    float mutate_chance; //0.f - 100.f chance of mutation line

    bool fuku_ob_settings::operator==(const fuku_ob_settings& set);
    bool is_null() const;
};

struct fuku_vm_settings {
    fuku_ob_settings ob_settings;

    shibari_module* vm_holder_module;//vm holder
    uint32_t  vm_entry_rva;          //vm offset

    fuku_virtualizer * virtualizer;
};

struct fuku_protected_region {
    uint32_t region_rva;
    uint32_t region_size;
};

enum fuku_code_type {
    fuku_code_obfuscation,
    fuku_code_virtualization,
};

struct fuku_code_list {
    fuku_code_type type;

    std::vector<fuku_protected_region> functions;
   
    fuku_ob_settings settings;
    shibari_module*  target_module;

    shibari_module*  vm_holder_module;
    uint32_t         vm_entry_rva;
    fuku_virtualizer * virtualizer;

    fuku_code_list& fuku_code_list::operator=(const fuku_code_list& set);
};




#include "fuku_code_analyzer.h"
#include "fuku_obfuscator.h"
#include "fuku_virtualizer_imp.h"
#include "fuku_protect_mgr.h"

class furikuri {
    std::vector<shibari_module*> extended_modules;
    shibari_module* main_module;

    std::vector<fuku_code_list> code_lists;
public:
    furikuri::furikuri();
    furikuri::~furikuri();

    bool furikuri::fuku_protect(std::vector<uint8_t>& out_image);
public:
    bool furikuri::set_main_module(shibari_module* module,std::string module_path = "");
    bool furikuri::add_extended_module(shibari_module* module, std::string module_path = "");

    bool furikuri::add_ob_code_list(fuku_protected_region region, shibari_module* target_module,const fuku_ob_settings& settings);
    bool furikuri::add_vm_code_list(fuku_protected_region region, shibari_module* target_module,const fuku_vm_settings& settings);

    void furikuri::clear_code_lists();
    void furikuri::clear_extended_modules(); //delete only from pointer table without destruction classes
public:
    const std::vector<fuku_code_list> & furikuri::get_code_lists() const;
    std::vector<shibari_module*>& furikuri::get_extended_modules();
    shibari_module* furikuri::get_main_module();
};

