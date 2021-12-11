add_llvm_library( LLVMGLOBALS MODULE
    globals.cpp
  
    PLUGIN_TOOL
    opt
    )