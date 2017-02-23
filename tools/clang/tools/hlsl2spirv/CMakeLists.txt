# set(LLVM_LINK_COMPONENTS
#   HLSL
#   Option
#   Support
#   )
#
add_clang_executable(hlsl2spirv
  Hlsl2Spirv.cpp
  )

target_link_libraries(hlsl2spirv
  clangAST
  clangBasic
  # clangCodeGen
  # clangDriver
  clangFrontend
  # clangFrontendTool
  # clangTooling
  )

install(TARGETS hlsl2spirv
  RUNTIME DESTINATION bin)
