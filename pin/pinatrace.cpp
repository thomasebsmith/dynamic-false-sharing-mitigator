/*
 * Copyright (C) 2004-2021 Intel Corporation.
 * SPDX-License-Identifier: MIT
 */

/*! @file
 *  This file contains an ISA-portable PIN tool for tracing memory accesses.
 */

// #include "mutex.PH"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <pin.H>
#include <sstream>
using std::cerr;
using std::dec;
using std::endl;
using std::hex;
using std::ios;
using std::setw;
using std::string;

class mutex {
  PIN_MUTEX _mu;

public:
  mutex() { PIN_MutexInit(&_mu); }
  ~mutex() { PIN_MutexFini(&_mu); }
  void lock() { PIN_MutexLock(&_mu); }
  void unlock() { PIN_MutexUnlock(&_mu); }
};

class lock_guard {
  mutex &_mu;

public:
  lock_guard(mutex &mu) : _mu(mu) { _mu.lock(); }
  ~lock_guard() { _mu.unlock(); }
};

/* ===================================================================== */
/* Global Variables */
/* ===================================================================== */

mutex tf_mu;
std::ofstream TraceFile;

/* ===================================================================== */
/* Commandline Switches */
/* ===================================================================== */

KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool", "o",
                            "pinatrace.out", "specify trace file name");
KNOB<BOOL> KnobValues(KNOB_MODE_WRITEONCE, "pintool", "values", "1",
                      "Output memory values reads and written");

/* ===================================================================== */
/* Print Help Message                                                    */
/* ===================================================================== */

static INT32 Usage() {
  cerr << "This tool produces a memory address trace.\n"
          "For each (dynamic) instruction reading or writing to memory the the "
          "ip and ea are recorded\n"
          "\n";

  cerr << KNOB_BASE::StringKnobSummary();

  cerr << endl;

  return -1;
}

static VOID EmitMem(std::ostream &os, VOID *ea, INT32 size) {
  if (!KnobValues)
    return;

  switch (size) {
  case 0:
    os << setw(1);
    break;

  case 1:
    os << static_cast<UINT32>(*static_cast<UINT8 *>(ea));
    break;

  case 2:
    os << *static_cast<UINT16 *>(ea);
    break;

  case 4:
    os << *static_cast<UINT32 *>(ea);
    break;

  case 8:
    os << *static_cast<UINT64 *>(ea);
    break;

  default:
    os.unsetf(ios::showbase);
    os << setw(1) << "0x";
    for (INT32 i = 0; i < size; i++) {
      os << static_cast<UINT32>(static_cast<UINT8 *>(ea)[i]);
    }
    os.setf(ios::showbase);
    break;
  }
}

static VOID RecordMem(VOID *ip, CHAR r, VOID *addr, INT32 size, THREADID id,
                      BOOL isPrefetch) {
  lock_guard lock(tf_mu);
  TraceFile << ip << ": " << r << " " << setw(2 + 2 * sizeof(ADDRINT)) << addr
            << " " << dec << setw(2) << size << " " << id << " " << hex
            << setw(2 + 2 * sizeof(ADDRINT));
  if (!isPrefetch)
    EmitMem(TraceFile, addr, size);
  TraceFile << endl;
  //   if (TraceString.str().length() > 68)
  //     cerr << TraceString.str().length() << " " << TraceString.str() << endl;
}

static VOID *WriteAddr;
static INT32 WriteSize;

static VOID RecordWriteAddrSize(VOID *addr, INT32 size) {
  WriteAddr = addr;
  WriteSize = size;
}

static THREADID ThreadId;
static VOID RecordThreadID(THREADID id) { ThreadId = id; }

static VOID RecordMemWrite(VOID *ip) {
  RecordMem(ip, 'W', WriteAddr, WriteSize, ThreadId, false);
}

VOID Instruction(INS ins, VOID *v) {
  // instruments loads using a predicated call, i.e.
  // the call happens iff the load will be actually executed

  if (INS_IsMemoryRead(ins) && INS_IsStandardMemop(ins)) {
    INS_InsertPredicatedCall(
        ins, IPOINT_BEFORE, (AFUNPTR)RecordMem, IARG_INST_PTR, IARG_UINT32, 'R',
        IARG_MEMORYREAD_EA, IARG_MEMORYREAD_SIZE, IARG_THREAD_ID, IARG_BOOL,
        INS_IsPrefetch(ins), IARG_END);
  }

  if (INS_HasMemoryRead2(ins) && INS_IsStandardMemop(ins)) {
    INS_InsertPredicatedCall(
        ins, IPOINT_BEFORE, (AFUNPTR)RecordMem, IARG_INST_PTR, IARG_UINT32, 'R',
        IARG_MEMORYREAD2_EA, IARG_MEMORYREAD_SIZE, IARG_THREAD_ID, IARG_BOOL,
        INS_IsPrefetch(ins), IARG_END);
  }

  // instruments stores using a predicated call, i.e.
  // the call happens iff the store will be actually executed
  if (INS_IsMemoryWrite(ins) && INS_IsStandardMemop(ins)) {
    INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)RecordWriteAddrSize,
                             IARG_MEMORYWRITE_EA, IARG_MEMORYWRITE_SIZE,
                             IARG_END);
    INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)RecordThreadID,
                             IARG_THREAD_ID, IARG_END);

    if (INS_IsValidForIpointAfter(ins)) {
      INS_InsertCall(ins, IPOINT_AFTER, (AFUNPTR)RecordMemWrite, IARG_INST_PTR,
                     IARG_END);
    }
    if (INS_IsValidForIpointTakenBranch(ins)) {
      INS_InsertCall(ins, IPOINT_TAKEN_BRANCH, (AFUNPTR)RecordMemWrite,
                     IARG_INST_PTR, IARG_END);
    }
  }
}

/* ===================================================================== */

VOID Fini(INT32 code, VOID *v) {
  lock_guard lock(tf_mu);
  TraceFile << "#eof" << endl;

  TraceFile.close();
}

/* ===================================================================== */
/* Main                                                                  */
/* ===================================================================== */

int main(int argc, char *argv[]) {
  string trace_header = string("#\n"
                               "# Memory Access Trace Generated By Pin\n"
                               "#\n");

  if (PIN_Init(argc, argv)) {
    return Usage();
  }

  {
    lock_guard lock(tf_mu);
    TraceFile.open(KnobOutputFile.Value().c_str());
    TraceFile.write(trace_header.c_str(), trace_header.size());
    TraceFile.setf(ios::showbase);
  }

  INS_AddInstrumentFunction(Instruction, 0);
  PIN_AddFiniFunction(Fini, 0);

  // Never returns

  PIN_StartProgram();

  RecordMemWrite(0);
  RecordWriteAddrSize(0, 0);
  RecordThreadID(0);

  return 0;
}

/* ===================================================================== */
/* eof */
/* ===================================================================== */
