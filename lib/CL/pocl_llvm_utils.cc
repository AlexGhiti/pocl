/* pocl_llvm_utils.cc: various helpers for pocl LLVM API.

   Copyright (c) 2013 Kalle Raiskila
                 2013-2017 Pekka Jääskeläinen

   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software and associated documentation files (the "Software"), to deal
   in the Software without restriction, including without limitation the rights
   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
   copies of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
   THE SOFTWARE.
*/

#include "config.h"
#include "pocl_runtime_config.h"
#include "pocl_llvm_api.h"
#include "pocl_debug.h"

#include <llvm/ADT/StringRef.h>
#include <llvm/ADT/StringMap.h>

#include <llvm/Support/MutexGuard.h>
#include <llvm/Support/Host.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/Signals.h>

#include <llvm/IR/Module.h>
#include <llvm/IR/LLVMContext.h>

#include <llvm/Target/TargetMachine.h>

#include <llvm/IRReader/IRReader.h>

#ifdef LLVM_OLDER_THAN_4_0
#include <llvm/Bitcode/ReaderWriter.h>
#else
#include <llvm/Bitcode/BitcodeReader.h>
#include <llvm/Bitcode/BitcodeWriter.h>
#endif

#include <llvm/Support/raw_os_ostream.h>
#include <llvm/PassRegistry.h>

#ifdef LLVM_OLDER_THAN_3_7
#include <llvm/PassManager.h>
#else
#include <llvm/IR/LegacyPassManager.h>
#define PassManager legacy::PassManager
#endif




using namespace llvm;

#include <string>
#include <map>

llvm::Module *parseModuleIR(const char *path) {
  SMDiagnostic Err;
  return parseIRFile(path, Err, GlobalContext()).release();
}


void writeModuleIR(const Module *mod, std::string &str) {
  llvm::raw_string_ostream sos(str);
  WriteBitcodeToFile(mod, sos);
  sos.str(); // flush
}

llvm::Module *parseModuleIRMem(const char *input_stream, size_t size) {
  StringRef input_stream_ref(input_stream, size);
  std::unique_ptr<MemoryBuffer> buffer =
      MemoryBuffer::getMemBufferCopy(input_stream_ref);

#ifdef LLVM_OLDER_THAN_3_8
  llvm::ErrorOr<std::unique_ptr<llvm::Module>> parsed_module =
      parseBitcodeFile(buffer->getMemBufferRef(), GlobalContext());
  if (std::error_code ec = parsed_module.getError())
    return nullptr;
#else
  auto parsed_module =
      parseBitcodeFile(buffer->getMemBufferRef(), GlobalContext());
  if (!parsed_module)
    return nullptr;
#endif
  return parsed_module.get().release();
}

int getModuleTriple(const char *input_stream, size_t size,
                    std::string &triple) {
  StringRef input_stream_ref(input_stream, size);
  std::unique_ptr<MemoryBuffer> buffer =
      MemoryBuffer::getMemBufferCopy(input_stream_ref);

#ifdef LLVM_OLDER_THAN_4_0
  triple = getBitcodeTargetTriple(buffer->getMemBufferRef(), GlobalContext());
#else
  auto triple_e = getBitcodeTargetTriple(buffer->getMemBufferRef());
  if (!triple_e)
    return -1;
  triple = triple_e.get();
#endif
  return 0;
}

char *get_cpu_name() {
#ifdef __mips__
  // TODO test: this may be handled by the if-"generic"-then-"HOST_CPU" below.
  //
  // The MIPS backend isn't able to automatically detect the host yet and the
  // value returned by llvm::sys::getHostCPUName() isn't usable in the
  // -target-cpu option so we must use the CPU detected by CMake.
  StringRef r = OCL_KERNEL_TARGET_CPU;
#else
  StringRef r = llvm::sys::getHostCPUName();
#endif

#ifdef LLVM_3_8
  // https://github.com/pocl/pocl/issues/413
  if (r.str() == "skylake") {
    r = llvm::StringRef("haswell");
  }
#endif

  if (r.str() == "generic") {
    POCL_MSG_WARN("LLVM does not recognize your cpu, trying to use "
                   HOST_CPU " for -target-cpu\n");
    r = llvm::StringRef(HOST_CPU);
  }

  assert(r.size() > 0);
  char *cpu_name = (char *)malloc(r.size() + 1);
  strncpy(cpu_name, r.data(), r.size());
  cpu_name[r.size()] = 0;
  return cpu_name;
}



// TODO this should be fixed to not require LLVM eventually,
// so that LLVM-less builds also report FMA correctly.
#ifdef OCS_AVAILABLE
int cpu_has_fma() {
  StringMap<bool> features;
  bool res = llvm::sys::getHostCPUFeatures(features);
  assert(res);
  return ((features["fma"] || features["fma4"]) ? 1 : 0);
}
#endif

int pocl_llvm_remove_file_on_signal(const char *file) {
  return llvm::sys::RemoveFileOnSignal(
            StringRef(file)) ? 0 : -1;
}

/*
 * Use one global LLVMContext across all LLVM bitcodes. This is because
 * we want to cache the bitcode IR libraries and reuse them when linking
 * new kernels. The CloneModule etc. seem to assume we are linking
 * bitcodes with a same LLVMContext. Unfortunately, this requires serializing
 * all calls to the LLVM APIs with mutex.
 * Freeing/deleting the context crashes LLVM 3.2 (at program exit), as a
 * work-around, allocate this from heap.
 */
static LLVMContext *globalContext = NULL;
static bool LLVMInitialized = false;

llvm::LLVMContext &GlobalContext() {
  if (globalContext == NULL)
    globalContext = new LLVMContext();
  return *globalContext;
}

/* The LLVM API interface functions are not at the moment not thread safe,
 * Pocl needs to ensure only one thread is using this layer at the time.
 */
static pocl_lock_t kernelCompilerLock = POCL_LOCK_INITIALIZER;

PoclCompilerMutexGuard::PoclCompilerMutexGuard(void *unused) {
  POCL_LOCK(kernelCompilerLock);
}

PoclCompilerMutexGuard::~PoclCompilerMutexGuard() {
  POCL_UNLOCK(kernelCompilerLock);
}

std::string currentWgMethod;

/* must be called with kernelCompilerLock locked */
void InitializeLLVM() {

  if (LLVMInitialized)
    return;
  // We have not initialized any pass managers for any device yet.
  // Run the global LLVM pass initialization functions.
  InitializeAllTargets();
  InitializeAllTargetMCs();
  InitializeAllAsmPrinters();
  InitializeAllAsmParsers();

  PassRegistry &Registry = *PassRegistry::getPassRegistry();

  initializeCore(Registry);
  initializeScalarOpts(Registry);
  initializeVectorization(Registry);
  initializeIPO(Registry);
  initializeAnalysis(Registry);
#ifdef LLVM_OLDER_THAN_3_8
  initializeIPA(Registry);
#endif
  initializeTransformUtils(Registry);
  initializeInstCombine(Registry);
  initializeInstrumentation(Registry);
  initializeTarget(Registry);

// Set the options only once. TODO: fix it so that each
// device can reset their own options. Now one cannot compile
// with different options to different devices at one run.

#ifdef LLVM_OLDER_THAN_3_7
  StringMap<llvm::cl::Option *> opts;
  llvm::cl::getRegisteredOptions(opts);
#else
  StringMap<llvm::cl::Option *> &opts = llvm::cl::getRegisteredOptions();
#endif

  llvm::cl::Option *O = nullptr;

  currentWgMethod = pocl_get_string_option("POCL_WORK_GROUP_METHOD", "loopvec");

  if (currentWgMethod == "loopvec") {

    O = opts["scalarize-load-store"];
    assert(O && "could not find LLVM option 'scalarize-load-store'");
    O->addOccurrence(1, StringRef("scalarize-load-store"), StringRef("1"),
                     false);

    // LLVM inner loop vectorizer does not check whether the loop inside
    // another loop, in which case even a small trip count loops might be
    // worthwhile to vectorize.
    O = opts["vectorizer-min-trip-count"];
    assert(O && "could not find LLVM option 'vectorizer-min-trip-count'");
    O->addOccurrence(1, StringRef("vectorizer-min-trip-count"), StringRef("2"),
                     false);

    if (pocl_get_bool_option("POCL_VECTORIZER_REMARKS", 0) == 1) {
      // Enable diagnostics from the loop vectorizer.
      O = opts["pass-remarks-missed"];
      assert(O && "could not find LLVM option 'pass-remarks-missed'");
      O->addOccurrence(1, StringRef("pass-remarks-missed"),
                       StringRef("loop-vectorize"), false);

      O = opts["pass-remarks-analysis"];
      assert(O && "could not find LLVM option 'pass-remarks-analysis'");
      O->addOccurrence(1, StringRef("pass-remarks-analysis"),
                       StringRef("loop-vectorize"), false);

      O = opts["pass-remarks"];
      assert(O && "could not find LLVM option 'pass-remarks'");
      O->addOccurrence(1, StringRef("pass-remarks"),
                       StringRef("loop-vectorize"), false);
    }
  }
  if (pocl_get_bool_option("POCL_DEBUG_LLVM_PASSES", 0) == 1) {
    O = opts["debug"];
    assert(O && "could not find LLVM option 'debug'");
    O->addOccurrence(1, StringRef("debug"), StringRef("true"), false);
  }

  O = opts["unroll-threshold"];
  assert(O && "could not find LLVM option 'unroll-threshold'");
  O->addOccurrence(1, StringRef("unroll-threshold"), StringRef("1"), false);

  LLVMInitialized = true;
}


// TODO FIXME currently pocl_llvm_release() only works when
// there are zero programs with IRs, because
// programs hold references to LLVM IRs
long numberOfIRs = 0;

void pocl_llvm_release() {

  PoclCompilerMutexGuard lockHolder(NULL);

  assert(numberOfIRs >= 0);

  if (numberOfIRs > 0) {
    POCL_MSG_PRINT_LLVM("still have references to IRs - not releasing LLVM\n");
    return;
  } else {
    POCL_MSG_PRINT_LLVM("releasing LLVM\n");
  }

  clearKernelPasses();
  clearTargetMachines();
  cleanKernelLibrary();

  delete globalContext;
  globalContext = nullptr;
  LLVMInitialized = false;
}
