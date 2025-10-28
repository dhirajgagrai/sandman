set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_llvm_pass_plugin(
  SandmanPlugin
  
  CfgPass.cpp
  DummyPass.cpp
  SandmanPlugin.cpp
)

configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/resources/libc_functions.txt"
  "${CMAKE_CURRENT_BINARY_DIR}/libc_functions.txt"
  COPYONLY
)
