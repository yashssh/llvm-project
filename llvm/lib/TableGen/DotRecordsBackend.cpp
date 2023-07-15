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

#include <unordered_map>
#include "llvm/ADT/DenseMapInfo.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/TableGen/TableGenBackend.h"
#include "llvm/TableGen/Record.h"

#define DEBUG_TYPE "detailed-records-backend"

#define NL "\n"

namespace llvm {
class RecordKeeper;
class raw_ostream;
} // namespace llvm

using namespace llvm;

namespace {

using NodeMap = std::unordered_map<std::string, int>;
// Any helper data structures can be defined here. Some backends use
// structs to collect information from the records.

class DotRecordsEmitter {
private:
  RecordKeeper &Records;
  NodeMap Nodes;
public:
  DotRecordsEmitter(RecordKeeper &RK) : Records(RK) {}

  void printEdges(raw_ostream &OS);
  void printNodes(raw_ostream &OS);
  void indexAllNodes();
  void generateDotGraph(raw_ostream &OS);
  void run(raw_ostream &OS);
  void printHello(raw_ostream &OS);
}; // emitter class

} // anonymous namespace

void DotRecordsEmitter::printEdges(raw_ostream &OS) {
  const auto &ClassList = Records.getClasses();
  for(const auto &ClassPair: ClassList){
    auto *const Class = ClassPair.second.get();
    auto Superclasses = Class->getSuperClasses();
    std::string ChildLabel = Class->getNameInitAsString();
    std::string ParentLabel;
    int ChildIndex = Nodes[ChildLabel];
    int ParentIndex;
    for (const auto &SuperclassPair : Superclasses) {
      auto *SuperClass = SuperclassPair.first;
      ParentLabel = SuperClass->getNameInitAsString();
      assert(Nodes.find(ParentLabel) != Nodes.end() && "Parent node not indexed");
      ParentIndex = Nodes[ParentLabel];
      // "node0" -> "node4";
      OS << "\"" << ChildIndex << "\" -> \"" << ParentIndex << "\";" << NL;
    }     
  }
}

void DotRecordsEmitter::printNodes(raw_ostream &OS) {
  for(auto NodeInfo: Nodes){
    std::string NodeLabel = NodeInfo.first;
    int NodeIndex = NodeInfo.second;
    OS << NodeIndex << "[label = \"" << NodeLabel << "\"];" << NL;
  }
}

void DotRecordsEmitter::indexAllNodes(){
  const auto &ClassList = Records.getClasses();
  int Index = 0;
  for(const auto &ClassPair: ClassList){
    auto *const Class = ClassPair.second.get();
    Nodes[Class->getNameInitAsString()] = Index++;
  }
}

void DotRecordsEmitter::generateDotGraph(raw_ostream &OS) {
  // TODO: Expand the graph to cover nodes outside the current file
  OS << "digraph g {" << NL;
  
  // Node type info
  OS << "node [shape = box];" << NL;

  // Generate Nodes
  printNodes(OS);

  // Generate Edges
  printEdges(OS);

  OS << "}" << NL;
}

void DotRecordsEmitter::run(raw_ostream &OS) {
  // printHello(OS);

  // No need to print file header in .dot graph
  // emitSourceFileHeader("Skeleton data structures", OS);

  indexAllNodes();
  generateDotGraph(OS);

  (void)Records; // To suppress unused variable warning; remove on use.
}


// Print the report heading, including the source file name.
void DotRecordsEmitter::printHello(raw_ostream &OS) {
  OS << "Running dot records backend \n";
  OS << "Backend name is: DotRecordsBackend\n";
  OS << "Debug string is 'detailed-records-backend'\n";
  OS << "Invoke using: llvm-tablegen -print-dot-records input.td \n";
}

namespace llvm {

// This function is called by TableGen after parsing the files.

void EmitDotRecords(RecordKeeper &RK, raw_ostream &OS) {
  // Instantiate the emitter class and invoke run().
  DotRecordsEmitter(RK).run(OS);
}

} // namespace llvm