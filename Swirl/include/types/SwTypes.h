#pragma once
#include <map>
#include <vector>

#include <llvm/IR/Type.h>


class IdentInfo;
class  LLVMBackend;
struct Var;

struct Type {
    enum SwTypes {
        FUNCTION,
        STRUCT,

        I8,
        I16,
        I32,
        I64,
        I128,

        U8,
        U16,
        U32,
        U64,
        U128,

        F32,
        F64,
        BOOL,
        REFERENCE,
        POINTER
    };


    virtual SwTypes     getSwType() = 0;

    [[nodiscard]] virtual IdentInfo*  getIdent() const = 0;
    virtual llvm::Type* llvmCodegen(LLVMBackend&) = 0;

    virtual bool     isIntegral() { return false; }
    virtual bool     isFloatingPoint() { return false; }
    virtual bool     isUnsigned() { return false; }
    virtual int      getBitWidth() { return -1; }

    virtual bool operator==(Type* other) { return getSwType() == other->getSwType(); }

    virtual ~Type() = default;
};


struct FunctionType final : Type {
    IdentInfo*         ident;
    Type*              ret_type;
    std::vector<Type*> param_types;

    SwTypes getSwType() override { return FUNCTION; }
    [[nodiscard]] IdentInfo* getIdent() const override { return ident; }

    bool operator==(Type* other) override {
        return other->getSwType() == FUNCTION && (param_types == dynamic_cast<FunctionType*>(other)->param_types);
    }

    llvm::Type* llvmCodegen(LLVMBackend& instance) override;
};


struct StructType final : Type {
    IdentInfo* ident;

    // std::unordered_map<IdentInfo*, std::unique_ptr<Var>> fields;
    std::map<IdentInfo*, std::pair<std::size_t, Type*>> fields;

    SwTypes getSwType() override { return STRUCT; }
    [[nodiscard]] IdentInfo* getIdent() const override { return ident; }

    bool operator==(Type* other) override {
        return other->getSwType() == STRUCT && (fields == dynamic_cast<StructType*>(other)->fields);
    }

    llvm::Type* llvmCodegen(LLVMBackend& instance) override;
};


struct ReferenceType final : Type {
    Type* of_type;

    [[nodiscard]] IdentInfo* getIdent() const override { return nullptr; }
    SwTypes    getSwType() override { return REFERENCE; }

    bool operator==(Type* other) override {
        return other->getSwType() == REFERENCE && (of_type == dynamic_cast<ReferenceType*>(other)->of_type);
    }

    llvm::Type* llvmCodegen(LLVMBackend& instance) override;
};

struct PointerType final : Type {
    uint16_t pointer_level;
    Type*   of_type;

    [[nodiscard]] IdentInfo* getIdent() const override { return nullptr; }
    SwTypes    getSwType() override { return POINTER; }

    llvm::Type* llvmCodegen(LLVMBackend& instance) override;
};

struct TypeI8 : Type {
    [[nodiscard]] IdentInfo* getIdent() const override { return nullptr; }
    SwTypes getSwType() override { return I8; }

    bool isIntegral() override { return true; }
    int  getBitWidth() override { return 8; }

    llvm::Type* llvmCodegen(LLVMBackend& instance) override;
};

struct TypeI16 : Type {
    [[nodiscard]] IdentInfo* getIdent() const override { return nullptr; }
    SwTypes getSwType() override { return I16; }

    bool isIntegral() override { return true; }
    int  getBitWidth() override { return 16; }

    llvm::Type* llvmCodegen(LLVMBackend& instance) override;
};

struct TypeI32 : Type {
    [[nodiscard]] IdentInfo* getIdent() const override { return nullptr; }
    SwTypes getSwType() override { return I32; }

    bool isIntegral() override { return true; }
    int  getBitWidth() override { return 32; }

    llvm::Type* llvmCodegen(LLVMBackend& instance) override;
};


struct TypeI64 : Type {
    [[nodiscard]] IdentInfo* getIdent() const override { return nullptr; }
    SwTypes getSwType() override { return I64; }

    bool isIntegral() override { return true; }
    int  getBitWidth() override { return 64; }

    llvm::Type* llvmCodegen(LLVMBackend& instance) override;
};

struct TypeI128 : Type {
    [[nodiscard]] IdentInfo* getIdent() const override { return nullptr; }
    SwTypes getSwType() override { return I128; }

    bool isIntegral() override { return true; }
    int  getBitWidth() override { return 128; }

    llvm::Type* llvmCodegen(LLVMBackend& instance) override;
};

struct TypeU8 final : TypeI8 {
    SwTypes getSwType() override { return U8; }
    bool isUnsigned() override { return true; }
};

struct TypeU16 final : TypeI16 {
    SwTypes getSwType() override { return U16; }
    bool isUnsigned() override { return true; }
};

struct TypeU32 final : TypeI32 {
    SwTypes getSwType() override { return U32; }
    bool isUnsigned() override { return true; }
};

struct TypeU64 final : TypeI64 {
    SwTypes getSwType() override { return U64; }
    bool isUnsigned() override { return true; }
};

struct TypeU128 final : TypeI128 {
    SwTypes getSwType() override { return U128; }
    bool isUnsigned() override { return true; }
};

struct TypeF32 final : Type {
    [[nodiscard]] IdentInfo* getIdent() const override { return nullptr; }
    SwTypes getSwType() override { return F32; }

    bool isFloatingPoint() override { return true; }
    int  getBitWidth() override { return 32; }

    llvm::Type* llvmCodegen(LLVMBackend& instance) override;
};

struct TypeF64 final : Type {
    [[nodiscard]] IdentInfo* getIdent() const override { return nullptr; }
    SwTypes getSwType() override { return F64; }

    bool isFloatingPoint() override { return true; }
    int  getBitWidth() override { return 64; }

    llvm::Type* llvmCodegen(LLVMBackend& instance) override;
};

struct TypeBool final : Type {
    [[nodiscard]] IdentInfo* getIdent() const override { return nullptr; }
    SwTypes getSwType() override { return BOOL; }

    int  getBitWidth() override { return 1; }

    llvm::Type* llvmCodegen(LLVMBackend& instance) override;
};

