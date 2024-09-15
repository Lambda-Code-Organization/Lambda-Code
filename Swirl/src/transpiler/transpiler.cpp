#include <iostream>
#include <string>
#include <ranges>
#include <unordered_map>

#include <parser/parser.h>

#include <llvm/IR/Module.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/GlobalValue.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/FileSystem.h>

std::size_t ScopeDepth = 0;

extern std::string SW_OUTPUT;
extern std::vector<std::unique_ptr<Node>> ParsedModule;

llvm::LLVMContext Context;
llvm::IRBuilder<> Builder(Context);

auto LModule = std::make_unique<llvm::Module>(SW_OUTPUT, Context);

namespace states {
    bool IsLocalScope      = false;
    bool ChildHasReturned  = false;
    llvm::IntegerType* IntegralTypeState = llvm::Type::getInt32Ty(Context);
}


std::unordered_map<std::string, llvm::Type*> type_registry = {
    {"i8", llvm::Type::getInt8Ty(Context)},
    {"i32",   llvm::Type::getInt32Ty(Context)},
    {"i64",   llvm::Type::getInt64Ty(Context)},
    {"i128",  llvm::Type::getInt128Ty(Context)},
    {"f32",   llvm::Type::getFloatTy(Context)},
    {"f64",   llvm::Type::getDoubleTy(Context)},
    {"bool",  llvm::Type::getInt1Ty(Context)},
    {"void",  llvm::Type::getVoidTy(Context)},

    {"i8*", llvm::PointerType::getInt8Ty(Context)},
    {"i32*", llvm::PointerType::getInt32Ty(Context)},
    {"i64*", llvm::PointerType::getInt64Ty(Context)},
    {"i128*", llvm::PointerType::getInt128Ty(Context)},
    {"f32*", llvm::PointerType::getFloatTy(Context)},
    {"f64*", llvm::PointerType::getDoubleTy(Context)},
    {"bool*", llvm::PointerType::getInt1Ty(Context)}
};

struct TableEntry {
    llvm::Value* ptr{};
    llvm::Type* type{};

    bool is_const = false;
    bool is_param = false;
};


static std::vector<std::unordered_map<std::string, TableEntry>> SymbolTable{};
// std::unordered_map<std::string, std::variant<llvm::GlobalVariable*, llvm::Function*>> GlobalValueTable{};


// this struct must be instatiated BEFORE IRBuilder::SetInsertPoint is called
struct NewScope {
    llvm::IRBuilderBase::InsertPoint IP;
    bool prev_ls_cache{};

     NewScope() {
         prev_ls_cache = states::IsLocalScope;
         states::IsLocalScope = true;
         SymbolTable.emplace_back();
         // IP = Builder.saveIP();
     }

    ~NewScope() {
         states::IsLocalScope = prev_ls_cache;
         SymbolTable.pop_back();
         states::ChildHasReturned = false;
         //
         // if (IP.getBlock() != nullptr)
         //    Builder.restoreIP(IP);
     }

    llvm::IRBuilderBase::InsertPoint getInsertPoint() const {
         return IP;
     }
};

void codegenChildrenUntilRet(std::vector<std::unique_ptr<Node>>& children) {
    for (const auto& child : children) {
        if (child->getType() == ND_RET) {
            child->codegen();
            states::ChildHasReturned = true;
            return;
        } child->codegen();
    }
}

llvm::Value* IntLit::codegen() {
    return llvm::ConstantInt::get(states::IntegralTypeState, getValue(), 10);
}

llvm::Value* FloatLit::codegen() {
    return llvm::ConstantFP::get(llvm::Type::getFloatTy(Context), value);
}

llvm::Value* StrLit::codegen() {
    return llvm::ConstantDataArray::getString(Context, value, true);
}

llvm::Value* Ident::codegen() {
    for (auto& entry: SymbolTable | std::views::reverse) {
        if (entry.contains(this->value)) {
            const auto [ptr, type, _, is_param] = entry[this->value];
            if (is_param) return ptr;
            return Builder.CreateLoad(type, ptr);
        }
    } throw std::runtime_error("Invalid ident");
}

llvm::Value* Function::codegen() {
    std::vector<llvm::Type*> param_types;

    param_types.reserve(params.size());
    for (const auto& param : params)
        param_types.push_back(type_registry[param.var_type]);

    llvm::FunctionType* f_type = llvm::FunctionType::get(type_registry[this->ret_type], param_types, false);
    llvm::Function*     func   = llvm::Function::Create(f_type, llvm::GlobalValue::InternalLinkage, ident, LModule.get());

    llvm::BasicBlock*   BB     = llvm::BasicBlock::Create(Context, "", func);
    NewScope _;
    Builder.SetInsertPoint(BB);

    for (int i = 0; i < params.size(); i++) {
        const auto p_name = params[i].var_ident;
        const auto param = func->getArg(i);

        param->setName(p_name);
        SymbolTable.back()[p_name] = {.ptr = param, .type = param_types[i], .is_param = true};
    }

    // for (const auto& child : this->children)
    //     child->codegen();

    codegenChildrenUntilRet(children);
    return func;
}

llvm::Value* ReturnStatement::codegen() {
    if (!value.expr.empty()) {
        const auto cache = states::IntegralTypeState;
        const auto ret = value.codegen();

        if (ret->getType()->isIntegerTy())
            states::IntegralTypeState = llvm::dyn_cast<llvm::IntegerType>(ret->getType());

        Builder.CreateRet(ret);
        states::IntegralTypeState = cache;
        return nullptr;
    }

    Builder.CreateRetVoid();
    return nullptr;
}

llvm::Value* Expression::codegen() {
    std::stack<llvm::Value*> eval{};

    static std::unordered_map<std::string, std::function<llvm::Value*(llvm::Value*, llvm::Value*)>> op_table
    = {
            // Standard Arithmetic Operators
        {"+", [](llvm::Value* a, llvm::Value* b) { return Builder.CreateAdd(a, b); }},
        {"-", [](llvm::Value* a, llvm::Value* b) { return Builder.CreateSub(a, b); }},
        {"*", [](llvm::Value* a, llvm::Value* b) { return Builder.CreateMul(a, b); }},
        {"/",
            [](llvm::Value *a, llvm::Value *b) {
                if (a->getType()->isIntegerTy())
                    a = Builder.CreateSIToFP(a, llvm::Type::getFloatTy(Context));
                if (b->getType()->isIntegerTy()) {
                    b = Builder.CreateSIToFP(b, llvm::Type::getFloatTy(Context));
                }
                return Builder.CreateFDiv(a, b);
            },
        },

        // Boolean operators
        {"==", [](llvm::Value* a, llvm::Value* b) {
            if (a->getType()->isIntegerTy() || b->getType()->isIntegerTy())
                return Builder.CreateICmp(llvm::CmpInst::ICMP_EQ, a, b);

            if (a->getType()->isFloatingPointTy() && b->getType()->isFloatingPointTy())
                return Builder.CreateFCmp(llvm::CmpInst::FCMP_OEQ, a, b);

            throw std::runtime_error("undefined operation '==' on the type");
        }}
    };

    for (const std::unique_ptr<Node>& nd : expr) {
        if (nd->getType() != ND_OP) {
            eval.push(nd->codegen());
        }
        else {
            if (nd->getArity() == 2) {
                if (eval.size() < 2) { throw std::runtime_error("Not enough operands on eval stack"); }
                llvm::Value* op2 = eval.top();
                eval.pop();
                llvm::Value* op1 = eval.top();
                eval.pop();
                eval.push(op_table[nd->getValue()](op1, op2));
            }
        }
    }

    if (eval.empty()) throw std::runtime_error("expr-codegen: eval stack is empty");
    return eval.top();
}

llvm::Value* Assignment::codegen() {
    llvm::Value* ptr;
    if (!std::ranges::any_of(SymbolTable | std::views::reverse, [this, &ptr](auto& entry) {
        if (entry.contains(ident) && !entry[ident].is_const) {
            ptr = entry[ident].ptr;
            return true;
        } return false;
    })) throw std::runtime_error("assignment: variable not defined previously or is const");

    Builder.CreateStore(value.codegen(), ptr);
    return nullptr;
}

llvm::Value* Condition::codegen() {
    const auto parent = Builder.GetInsertBlock()->getParent();
    const auto if_block = llvm::BasicBlock::Create(Context, "if", parent);
    const auto else_block = llvm::BasicBlock::Create(Context, "else", parent);
    const auto merge_block = llvm::BasicBlock::Create(Context, "merge", parent);

    const auto if_cond = this->bool_expr.codegen();
    if (!if_cond)
        throw std::runtime_error("condition is null");

    std::vector false_blocks = {else_block};
    Builder.CreateCondBr(if_cond, if_block, else_block);
    {
        NewScope _;
        Builder.SetInsertPoint(if_block);

        // for (auto& child : if_children)
        //     child->codegen();
        codegenChildrenUntilRet(if_children);

        if (!states::ChildHasReturned)
            Builder.CreateBr(merge_block);
    }

    // elif(s)
    for (std::size_t i = 1; auto& [cond, children] : elif_children) {
        NewScope _;
        if (i == 1) {
            Builder.SetInsertPoint(if_block);
            else_block->setName("elif");
            false_blocks.push_back(llvm::BasicBlock::Create(Context, "elif", parent));
            Builder.CreateCondBr(cond.codegen(), else_block, false_blocks.back());
            Builder.SetInsertPoint(else_block);
        } else {
            Builder.SetInsertPoint(false_blocks.at(false_blocks.size() - 2));
            false_blocks.push_back(llvm::BasicBlock::Create(Context, "elif", parent));
            Builder.CreateCondBr(cond.codegen(), false_blocks.at(false_blocks.size() - 2), false_blocks.back());
            Builder.SetInsertPoint(false_blocks.at(false_blocks.size() - 2));
        }

        // for (auto& child : children)
        //     child->codegen();
        codegenChildrenUntilRet(children);

        if (i == elif_children.size() && !states::ChildHasReturned)
            Builder.CreateBr(merge_block);
        i++;
    }

    // else
    {
        NewScope _;
        Builder.SetInsertPoint(false_blocks.back());
        false_blocks.back()->setName("else");

        // for (const auto& child : else_children)
        //     child->codegen();
        codegenChildrenUntilRet(else_children);

        if (!states::ChildHasReturned)
            Builder.CreateBr(merge_block);
    }

    Builder.SetInsertPoint(merge_block);
    return nullptr;
}

llvm::Value *WhileLoop::codegen() {
    const auto parent = Builder.GetInsertBlock()->getParent();
    const auto last_inst = Builder.GetInsertBlock()->getTerminator();

    const auto cond_block  = llvm::BasicBlock::Create(Context, "while_cond", parent);
    const auto body_block  = llvm::BasicBlock::Create(Context, "while_body", parent);
    const auto merge_block = llvm::BasicBlock::Create(Context, "merge", parent);

    if (last_inst == nullptr)
        Builder.CreateBr(cond_block);

    const auto expr  = condition.codegen();

    Builder.SetInsertPoint(cond_block);
    Builder.CreateCondBr(expr, body_block, merge_block);

    {
        NewScope _;
        Builder.SetInsertPoint(body_block);
        codegenChildrenUntilRet(children);
        Builder.CreateBr(cond_block);
    }

    Builder.SetInsertPoint(merge_block);
    return nullptr;
}

llvm::Value* AddressOf::codegen() {
    for (auto& entry: SymbolTable | std::views::reverse) {
        if (entry.contains(this->ident)) {
            const auto [ptr, type, _, is_param] = entry[this->ident];
            if (is_param) throw std::runtime_error("can't take address of a function parameter");
            return ptr;
        }
    } throw std::runtime_error("addr-of: invalid ident: " + ident);
}

llvm::Value* Dereference::codegen() {
    for (auto& entry: SymbolTable | std::views::reverse) {
        if (entry.contains(this->ident)) {
            const auto [ptr, type, _, is_param] = entry[this->ident];
            if (is_param) return ptr;
            return Builder.CreateLoad(type, ptr);
        }
    } throw std::runtime_error("Invalid ident");
}


llvm::Value* FuncCall::codegen() {
    std::vector<llvm::Type*> paramTypes;

    llvm::Function* func = LModule->getFunction(ident);
    if (!func)
        throw std::runtime_error("No such function defined");

    std::vector<llvm::Value*> arguments{};
    arguments.reserve(args.size());

    for (auto& item : args)
        arguments.push_back(item.codegen());

    if (!func->getReturnType()->isVoidTy())
        return Builder.CreateCall(func, arguments, ident);

    Builder.CreateCall(func, arguments);
    return nullptr;
}


llvm::Value* Var::codegen() {
    auto type_iter = type_registry.find(var_type);
    if (type_iter == type_registry.end()) throw std::runtime_error("undefined type");

    const auto state_cache = states::IntegralTypeState;
    if (type_iter->second->isIntegerTy())
        states::IntegralTypeState = llvm::dyn_cast<llvm::IntegerType>(type_iter->second);

    llvm::Value* ret;
    llvm::Value* init = nullptr;

    if (initialized)
        init = value.codegen();

    if (!states::IsLocalScope) {
        auto* var = new llvm::GlobalVariable(
                *LModule, type_iter->second, is_const, llvm::GlobalVariable::InternalLinkage,
                nullptr, var_ident
                );

        if (init) {
            const auto val = llvm::dyn_cast<llvm::Constant>(init);
            var->setInitializer(val);
        } ret = var;
    } else {
        llvm::AllocaInst* var_alloca = Builder.CreateAlloca(type_iter->second, nullptr, var_ident);
        // * is_volatile is false
        if (init != nullptr) Builder.CreateStore(init, var_alloca);
        SymbolTable.back()[var_ident] = {var_alloca, type_registry[var_type]};
        ret = var_alloca;
    }

    states::IntegralTypeState = state_cache;
    return ret;
}


void Condition::print() {
    std::cout <<
        std::format("Condition: if-children-size: {}, else-children-size: {}", if_children.size(), elif_children.size())
    << std::endl;

    std::cout << "IF-children:" << std::endl;
    for (const auto& a : if_children) {
        std::cout << "\t";
        a->print();
    }

    std::cout << "ELSE-children:" << std::endl;
    // for (const auto& a : elif_children) {
    //     for (const auto& a : if_children) {
    //         std::cout << "\t";
    //         a->print();
    //     }
    // }
}

void Var::print() {
    std::cout << std::format("Var: {}", var_ident) << std::endl;
}

void Function::print() {
    std::cout << std::format("func: {}", ident) << std::endl;
    for (const auto& a : children) {
        std::cout << "\t";
        a->print();
    }
}

void printIR() {
    llvm::verifyModule(*LModule.get(), &llvm::errs());
    const auto ln1 = "--------------------[IR]--------------------\n";
    llvm::outs().write(ln1, strlen(ln1));
    LModule->print(llvm::outs(), nullptr);
    const auto ln2 = "----------------[NATIVE-ASM]----------------\n";
    llvm::outs().write(ln2, strlen(ln2));
    llvm::outs().flush();
}
