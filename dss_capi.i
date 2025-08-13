%module DssCapi

%include "typemaps.i"

%{
  #include "dss_capi.h"
  #include "dss_capi_ctx.h"
  #include "rmqpush.h"
%}


%include "dss_capi.h"
%include "dss_capi_ctx.h"
%include "rmqpush.h"

%include "stdint.i"

%include "cpointer.i"
%pointer_functions(int32_t, intp);
%pointer_functions(int32_t*, intPp);
%pointer_functions(char, charp);
%pointer_functions(char*, charPp);
%pointer_functions(char**, ansiCharPPp);
%pointer_functions(char***, ansiCharPPPp);

%include "carrays.i"
%array_functions(double, doubleArray);
%array_functions(float, floatArray);
%array_functions(int32_t, intArray);
%array_functions(char*, ansiCharPArray);


%pragma(java) jniclasscode=%{
  static {
    try {
        System.loadLibrary("dss_capi_java");
    } catch (UnsatisfiedLinkError e) {
      System.err.println("Native code library failed to load. \n" + e);
      System.exit(1);
    }
  }
%}
