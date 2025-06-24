// ocpch.h — Overcast Language Precompiled Header
#pragma once
// some std headers
#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>
#include <algorithm>

// LLVM core includes
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/Verifier.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Bitcode/BitcodeWriter.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/TargetParser/Triple.h"                   
#include "llvm/IR/LLVMContext.h"               
#include "llvm/IR/Module.h"                   
#include "llvm/Support/TargetSelect.h"                      
#include "llvm/MC/TargetRegistry.h"        
#include "llvm/Target/TargetMachine.h"           
#include "llvm/Target/TargetOptions.h"            
#include "llvm/Support/raw_ostream.h"            
#include "llvm/Passes/PassBuilder.h"            
#include "llvm/IR/LegacyPassManager.h"          
#include "llvm/Support/FileSystem.h"  
#include "llvm/TargetParser/Host.h"
#include "llvm/Support/Error.h"       

#include "ProjectSystem/toml.hpp"