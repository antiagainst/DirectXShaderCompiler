//===--- tools/hlsl2spirv/Hlsl2Spirv.cpp - HLSL to SPIR-V =----------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "clang/Basic/TargetInfo.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Frontend/TextDiagnosticBuffer.h"
#include "clang/Frontend/TextDiagnosticPrinter.h"
#include "llvm/Support/CommandLine.h"

llvm::cl::opt<std::string> inputFileName(llvm::cl::Positional,
                                         llvm::cl::desc("<input file>"),
                                         llvm::cl::init("-"));

int main(int argc, const char **argv) {
  // llvm::sys::PrintStackTraceOnErrorSignal();
  llvm::cl::ParseCommandLineOptions(argc, argv);

  clang::CompilerInstance compiler;

  std::string log;
  llvm::raw_string_ostream diagStream(log);
  compiler.createDiagnostics(new clang::TextDiagnosticPrinter(
                                 diagStream, &compiler.getDiagnosticOpts()),
                             true);

  llvm::Triple triple("dxil-ms-dx");
  compiler.getTargetOpts().Triple = triple.str();
  compiler.setTarget(clang::TargetInfo::CreateTargetInfo(
      compiler.getDiagnostics(),
      std::make_shared<clang::TargetOptions>(compiler.getTargetOpts())));

  clang::FrontendInputFile file(inputFileName.c_str(), clang::IK_HLSL);
  compiler.getFrontendOpts().Inputs.push_back(file);
  compiler.setOutStream(&llvm::outs());

  compiler.createFileManager();
  compiler.createSourceManager(compiler.getFileManager());

  compiler.getLangOpts().HLSL2015 = true;

  compiler.getFrontendOpts().ASTDumpDecls = true;
  clang::ASTDumpAction dumpAction;
  if (!dumpAction.BeginSourceFile(compiler, file)) {
    return -1;
  }
  dumpAction.Execute();
  dumpAction.EndSourceFile();

  clang::DiagnosticConsumer *consumer = compiler.getDiagnostics().getClient();
  consumer->finish();

  if (consumer->getNumErrors() > 0) {
    llvm::errs() << log;
    return -1;
  }

  return 0;
}
