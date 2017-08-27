//===--- SemaSPIRV.cpp - Semantic Analysis for SPIR-V ---------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file implements SPIR-V related semantic analysis.
//
//===----------------------------------------------------------------------===//

#ifdef ENABLE_SPIRV_CODEGEN

#include "TreeTransform.h"

namespace clang {
using namespace sema;

namespace {
/// Returns true if the given type will be translated into a SPIR-V image,
/// sampler or struct containing images or samplers.
bool isOpaqueType(QualType type) {
  if (const auto *recordType = type->getAs<RecordType>()) {
    const auto name = recordType->getDecl()->getName();

    if (name == "Texture1D" || name == "RWTexture1D")
      return true;
    if (name == "Texture2D" || name == "RWTexture2D")
      return true;
    if (name == "Texture2DMS" || name == "RWTexture2DMS")
      return true;
    if (name == "Texture3D" || name == "RWTexture3D")
      return true;
    if (name == "TextureCube" || name == "RWTextureCube")
      return true;

    if (name == "Texture1DArray" || name == "RWTexture1DArray")
      return true;
    if (name == "Texture2DArray" || name == "RWTexture2DArray")
      return true;
    if (name == "Texture2DMSArray" || name == "RWTexture2DMSArray")
      return true;
    if (name == "TextureCubeArray" || name == "RWTextureCubeArray")
      return true;

    if (name == "Buffer" || name == "RWBuffer")
      return true;

    if (name == "SamplerState" || name == "SamplerComparisonState")
      return true;
  }
  return false;
}

bool containsOpaqueType(QualType type) {
  if (isOpaqueType(type))
    return false;

  if (const auto *recordType = type->getAs<RecordType>())
    for (const auto *field : recordType->getDecl()->decls())
      if (const auto *fieldDecl = dyn_cast<FieldDecl>(field))
        if (isOpaqueType(fieldDecl->getType()) ||
            containsOpaqueType(fieldDecl->getType()))
          return true;

  return false;
}

class FlattenedVarMap {
public:
  uint32_t appendVariable(VarDecl *decl) {
    const auto index = static_cast<uint32_t>(variables.size());
    variables.push_back(decl);
    offsets.push_back(index);
    return index;
  }
  uint32_t reserveRange(uint32_t size) {
    const auto prevSize = static_cast<uint32_t>(offsets.size());
    offsets.resize(prevSize + size, 0);
    return prevSize;
  }

  void setOffset(uint32_t position, uint32_t offset) {
    assert(position < offsets.size());
    offsets[position] = offset;
  }

  uint32_t index(uint32_t position) {
    assert(position < offsets.size());
    return offsets[position];
  }

  VarDecl *get(uint32_t offset) {
    assert(offset < variables.size());
    return variables[offset];
  }

  const llvm::SmallVectorImpl<VarDecl *> &getVariables() const {
    return variables;
  }

private:
  llvm::SmallVector<VarDecl *, 16> variables;
  llvm::SmallVector<uint32_t, 16> offsets;
};

class FlattenOpaqueStruct : public TreeTransform<FlattenOpaqueStruct> {
  typedef TreeTransform<FlattenOpaqueStruct> BaseTransform;

  ASTContext &context;
  FunctionDecl *curFnDecl;
  llvm::DenseMap<VarDecl *, FlattenedVarMap> flattenedVarMap;
  llvm::DenseMap<FunctionDecl *, FlattenedVarMap> flattenedFnRetMap;

public:
  FlattenOpaqueStruct(Sema &semaRef)
      : BaseTransform(semaRef), context(semaRef.Context), curFnDecl(nullptr) {}

  uint32_t flattenType(QualType type, FlattenedVarMap *varMap, bool genParamVar,
                       Attr *attr = nullptr) {
    if (type->getAs<BuiltinType>() || hlsl::IsHLSLVecMatType(type) ||
        isOpaqueType(type)) {
      VarDecl *param = nullptr;
      if (genParamVar) {
        param = ParmVarDecl::Create(
            context, context.getTranslationUnitDecl(), SourceLocation(),
            SourceLocation(), /*Id=*/nullptr,
            // Use reference type for out variable
            attr ? context.getLValueReferenceType(type) : type,
            /*TInfo=*/nullptr, SC_None, /*DefArg=*/nullptr);
        // Annotate with HLSL out attribute
        if (attr)
          param->addAttr(attr);
      } else {
        param =
            VarDecl::Create(context, context.getTranslationUnitDecl(),
                            SourceLocation(), SourceLocation(),
                            /*Id=*/nullptr, type, /*TInfo=*/nullptr, SC_Auto);
      }

      return varMap->appendVariable(param);
    }

    if (const auto *recordType = type->getAs<RecordType>()) {
      uint32_t numFields = 0;
      for (const auto *field : recordType->getDecl()->decls())
        if (isa<FieldDecl>(field))
          ++numFields;

      const auto structPos = varMap->reserveRange(numFields);
      auto curPos = structPos;
      for (const auto *field : recordType->getDecl()->decls()) {
        if (const auto *fieldDecl = dyn_cast<FieldDecl>(field))
          varMap->setOffset(curPos++, flattenType(fieldDecl->getType(), varMap,
                                                  genParamVar, attr));
      }
      return structPos;
    }

    if (const auto *typedefType = type->getAs<TypedefType>())
      return flattenType(typedefType->desugar(), varMap, genParamVar, attr);

    if (const auto *refType = type->getAs<ReferenceType>())
      return flattenType(refType->getPointeeType(), varMap, genParamVar, attr);

    assert("unhandled type in flattenType()");
    return 0;
  }

  Decl *TransformFunctionDecl(FunctionDecl *fnDecl) {
    llvm::SmallVector<ParmVarDecl *, 16> newParmVars;
    bool flattened = false;

    for (auto *param : fnDecl->params()) {
      const auto paramType = param->getType();
      if (containsOpaqueType(paramType)) {
        auto &varMap = flattenedVarMap[param];
        // TODO: need to carry over other attributes
        Attr *attr = param->getAttr<HLSLOutAttr>();
        attr = param->getAttr<HLSLInOutAttr>();
        (void)flattenType(paramType, &varMap, true, attr);

        for (auto *var : varMap.getVariables())
          newParmVars.push_back(cast<ParmVarDecl>(var));

        flattened = true;
      } else {
        newParmVars.push_back(param);
      }
    }

    QualType newRetType = fnDecl->getReturnType();
    if (containsOpaqueType(newRetType)) {
      auto &varMap = flattenedFnRetMap[fnDecl];
      (void)flattenType(newRetType, &varMap, true,
                        HLSLOutAttr::CreateImplicit(context));

      for (auto *var : varMap.getVariables())
        newParmVars.push_back(cast<ParmVarDecl>(var));

      newRetType = SemaRef.getASTContext().VoidTy;
      flattened = true;
    }

    if (!flattened)
      return fnDecl;

    llvm::SmallVector<QualType, 16> newParmVarTypes;
    for (const auto *param : newParmVars)
      newParmVarTypes.push_back(param->getType());

    auto *oldFnProtoTy = fnDecl->getType()->getAs<FunctionProtoType>();
    const auto extProtoInfo = oldFnProtoTy->getExtProtoInfo();
    const auto paramMods = oldFnProtoTy->getParamMods();
    auto fnProtoType = SemaRef.Context.getFunctionType(
        newRetType, newParmVarTypes, extProtoInfo, {});

    // Carry over most of the information
    auto *newFn = FunctionDecl::Create(
        context, fnDecl->getDeclContext(), fnDecl->getLocStart(),
        fnDecl->getNameInfo(), fnProtoType, fnDecl->getTypeSourceInfo(),
        fnDecl->getStorageClass(), fnDecl->isInlineSpecified(),
        /*hasWrittenPrototype=*/false, fnDecl->isConstexpr());

    curFnDecl = newFn;

    // Change parameter list
    for (auto *param : newParmVars)
      param->setDeclContext(newFn);
    newFn->setParams(newParmVars);

    // TODO: error handling
    newFn->setBody(TransformStmt(fnDecl->getBody()).get());

    return newFn;
  }

  StmtResult TransformStmt(Stmt *stmt) {
    if (auto *declStmt = dyn_cast<DeclStmt>(stmt)) {
      std::vector<Decl *> newVarDecls;
      bool flattened = false;
      for (auto *decl : declStmt->decls())
        if (auto *varDecl = dyn_cast<VarDecl>(decl)) {
          if (TransformVarDecl(varDecl, &newVarDecls)) {
            flattened = true;
          } else {
            newVarDecls.push_back(varDecl);
          }
        }

      for (auto *decl : newVarDecls)
        decl->setDeclContext(curFnDecl);
      if (flattened)
        return RebuildDeclStmt(newVarDecls, declStmt->getLocStart(),
                               declStmt->getLocEnd());
      return declStmt;
    }

    if (auto *retStmt = dyn_cast<ReturnStmt>(stmt)) {
      if (auto *retVal = retStmt->getRetValue())
        if (containsOpaqueType(retVal->getType()))
          return new (context)
              ReturnStmt(retStmt->getLocStart(), nullptr, nullptr);
      return retStmt;
    }

    return BaseTransform::TransformStmt(stmt);
  }

  ExprResult TransformExpr(Expr *expr) {
    if (auto *memberExpr = dyn_cast<MemberExpr>(expr)) {
      // If this is calling a member method like object.method(), we need
      // no special processing and just fall back to the default behavior.
      if (isa<CXXMethodDecl>(memberExpr->getMemberDecl()))
        return BaseTransform::TransformExpr(expr);

      llvm::SmallVector<uint32_t, 4> indices;
      Expr *base = collectArrayStructIndices(memberExpr, &indices);
      Decl *decl = cast<DeclRefExpr>(base)->getDecl();
      VarDecl *varDecl = cast<VarDecl>(decl);

      auto found = flattenedVarMap.find(varDecl);
      if (found == flattenedVarMap.end())
        return expr;

      auto &varMap = found->second;
      uint32_t offset = 0;
      for (auto index : indices)
        offset = varMap.index(offset + index);
      VarDecl *newVar = varMap.get(offset);

      DeclarationNameInfo declNameInfo;
      return RebuildDeclRefExpr(NestedNameSpecifierLoc(), newVar, declNameInfo,
                                /*TemplateArgs=*/nullptr);
    }

    return BaseTransform::TransformExpr(expr);
  }

  Expr *collectArrayStructIndices(Expr *expr,
                                  llvm::SmallVectorImpl<uint32_t> *indices) {
    if (auto *indexing = dyn_cast<MemberExpr>(expr)) {
      Expr *base = collectArrayStructIndices(
          indexing->getBase()->IgnoreParenNoopCasts(context), indices);

      // Append the index of the current level
      const auto *fieldDecl = cast<FieldDecl>(indexing->getMemberDecl());
      assert(fieldDecl);
      indices->push_back(fieldDecl->getFieldIndex());

      return base;
    }

    if (auto *indexing = dyn_cast<ArraySubscriptExpr>(expr)) {
      // The base of an ArraySubscriptExpr has a wrapping LValueToRValue
      // implicit cast. We need to ingore it to avoid creating OpLoad.
      Expr *thisBase = indexing->getBase()->IgnoreParenLValueCasts();
      Expr *base = collectArrayStructIndices(thisBase, indices);
      uint32_t thisIndex = 0;

      Expr::EvalResult evalResult;
      if (indexing->getIdx()->EvaluateAsRValue(evalResult, context) &&
          !evalResult.HasSideEffects) {
        const auto &intValue = evalResult.Val.getInt();
        assert(intValue.isIntN(32));
        thisIndex = static_cast<uint32_t>(intValue.getZExtValue());
      } else {
        assert(0);
      }
      indices->push_back(thisIndex);
      return base;
    }

    // This the deepest we can go. No more array or struct indexing.
    return expr;
  }

  bool TransformVarDecl(VarDecl *varDecl, std::vector<Decl *> *flattendDecls) {
    if (containsOpaqueType(varDecl->getType())) {
      auto &varMap = flattenedVarMap[varDecl];
      (void)flattenType(varDecl->getType(), &varMap, false);

      for (auto *decl : varMap.getVariables())
        flattendDecls->push_back(decl);

      return true;
    }
    return false;
  }
};

} // anonymous namespace

void Sema::PerformSpirvTransforms() {
  FlattenOpaqueStruct transfomer(*this);

  auto *transUnit = Context.getTranslationUnitDecl();

  llvm::MapVector<Decl *, Decl *> declMap;
  for (auto *decl : transUnit->decls())
    if (auto *fnDecl = dyn_cast<FunctionDecl>(decl)) {
      declMap[decl] = transfomer.TransformFunctionDecl(fnDecl);
    }

  for (auto it : declMap)
    if (it.first != it.second) {
      // transUnit->removeDecl(it.first);
      transUnit->addDecl(it.second);
    }
}

} // namespace clang

#else

namespace {
uint32_t ToSuppressEmptyFileWarning;
}

#endif