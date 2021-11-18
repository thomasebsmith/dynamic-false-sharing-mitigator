add_llvm_library( LLVMFALSEPROFILE MODULE
    profile.cpp
  
    PLUGIN_TOOL
    opt
    )