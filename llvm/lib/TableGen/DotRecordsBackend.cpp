//===- DotRecordsBackend.cpp - Dot Records TableGen backend     -*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This Tablegen backend emits ...
//
//===----------------------------------------------------------------------===//

#include "llvm/ADT/DenseMapInfo.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/TableGen/TableGenBackend.h"

#define DEBUG_TYPE "detailed-records-backend"

namespace llvm {
class RecordKeeper;
class raw_ostream;
} // namespace llvm

using namespace llvm;

namespace {

// Any helper data structures can be defined here. Some backends use
// structs to collect information from the records.

class DotRecordsEmitter {
private:
  RecordKeeper &Records;

public:
  DotRecordsEmitter(RecordKeeper &RK) : Records(RK) {}

  void run(raw_ostream &OS);
  void printHello(raw_ostream &OS);
}; // emitter class

} // anonymous namespace

// Print the report heading, including the source file name.
void DotRecordsEmitter::printHello(raw_ostream &OS) {
  OS << "Running dot records backend \n";
  OS << "Backend name is: DotRecordsBackend\n";
  OS << "Debug string is 'detailed-records-backend'\n";
  OS << "Invoke using: llvm-tablegen -print-dot-records input.td \n";
}

void DotRecordsEmitter::run(raw_ostream &OS) {

  printHello(OS);

  emitSourceFileHeader("Skeleton data structures", OS);

  (void)Records; // To suppress unused variable warning; remove on use.
}


namespace llvm {

// This function is called by TableGen after parsing the files.

void EmitDotRecords(RecordKeeper &RK, raw_ostream &OS) {
  // Instantiate the emitter class and invoke run().
  DotRecordsEmitter(RK).run(OS);
}

} // namespace llvm