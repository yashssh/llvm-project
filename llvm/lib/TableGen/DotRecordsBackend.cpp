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
#include <queue>
#include "llvm/ADT/DenseMapInfo.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/CommandLine.h"
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

// TODO: Can make this parameter hidden
// TODO: Find a way to accomodate records too and not just classes
static cl::opt<std::string>
FilterClass("filter-class", cl::desc("Class to generate dot graph for"), cl::value_desc("ClassName"), cl::init(""));

using NodeMap = std::unordered_map<std::string, int>;
// Any helper data structures can be defined here. Some backends use
// structs to collect information from the records.

class DotRecordsEmitter {
private:
  RecordKeeper &Records;
  NodeMap Nodes;
public:
  DotRecordsEmitter(RecordKeeper &RK) : Records(RK) {}

  void printGraphEdges(raw_ostream &OS, Record& RootNode);
  void printNodes(raw_ostream &OS);
  void indexGraphNodes(Record &RootNode);
  Record& queryRootNode(std::string &RootNodeName);
  void generateDotGraph(raw_ostream &OS, Record &RootNode);
  void run(raw_ostream &OS);
  void printHello(raw_ostream &OS);
}; // emitter class

} // anonymous namespace

static void printEdge(Record& ChildNode, Record& ParentNode, NodeMap& Nodes, raw_ostream &OS) {
    std::string ChildLabel = ChildNode.getNameInitAsString();
    std::string ParentLabel = ParentNode.getNameInitAsString();
    int ChildIndex = Nodes[ChildLabel];
    int ParentIndex = Nodes[ParentLabel];
    assert(Nodes.find(ChildLabel) != Nodes.end() && "Child node not indexed");
    assert(Nodes.find(ParentLabel) != Nodes.end() && "Parent node not indexed");
    OS << "\"" << ChildIndex << "\" -> \"" << ParentIndex << "\";" << NL;
}

void DotRecordsEmitter::printGraphEdges(raw_ostream &OS, Record &RootNode) {
  std::queue<Record*>Queue;
  Queue.push(&RootNode);
  while(Queue.size()){
    Record* Child = Queue.front();
    Queue.pop();

    // Print child to parent edge and push parents to queue
    SmallVector<Record*>ParentClasses;
    Child->getDirectSuperClasses(ParentClasses);
    // OS << "Creating edges from " << Child->getNameInitAsString() << " to " << NL;
    for (Record* Parent : ParentClasses){
      printEdge(*Child, *Parent, Nodes, OS);
      Queue.push(Parent);
      // OS << Parent->getNameInitAsString() << NL;
    }
    // OS << NL;
  }
}

void DotRecordsEmitter::printNodes(raw_ostream &OS) {
  for(auto NodeInfo: Nodes){
    std::string NodeLabel = NodeInfo.first;
    int NodeIndex = NodeInfo.second;
    OS << NodeIndex << "[label = \"" << NodeLabel << "\"];" << NL;
  }
}

void DotRecordsEmitter::indexGraphNodes(Record &RootNode) {
  int index = 0;
  std::queue<Record*>Queue;
  // SmallVectorImpl<Record *> Classes;
  Queue.push(&RootNode);
  while(Queue.size()){
    Record* Class = Queue.front();
    std::string ClassName = Class->getNameInitAsString();
    Queue.pop();
    
    // Skip visited nodes
    if(Nodes.find(ClassName) != Nodes.end())
      continue;

    Nodes[ClassName] = index++;
    
    // Push parent nodes into queue
    // FIXME: Use getDirectSuperClasses
    auto Superclasses = Class->getSuperClasses();
    for (const auto &SuperClassPair : Superclasses)
      Queue.push(SuperClassPair.first);
    
  }
}

Record& DotRecordsEmitter::queryRootNode(std::string& RootNodeName) {
  const auto &ClassList = Records.getClasses();
  for(const auto &ClassPair: ClassList){
    auto *const Class = ClassPair.second.get();
    if(RootNodeName ==  Class->getNameInitAsString())
      return *Class;
  }
}

void DotRecordsEmitter::generateDotGraph(raw_ostream &OS, Record &RootNode) {
  // TODO: Expand the graph to cover nodes outside the current file
  OS << "digraph g {" << NL;
  
  // Node type info
  OS << "node [shape = box];" << NL;

  // Generate Nodes
  printNodes(OS);

  // Generate Edges
  printGraphEdges(OS, RootNode);

  OS << "}" << NL;
}

void DotRecordsEmitter::run(raw_ostream &OS) {
  assert(!FilterClass.empty() && "Provide a class name to generate dot graph for. -filter-class=<Name>");
  Record &RootNode = queryRootNode(FilterClass);
  OS << "Root Node is " << RootNode.getNameInitAsString() << NL;
  // printHello(OS);

  // FIXME: No need to print file header in .dot graph, will cause parsing error
  emitSourceFileHeader("Skeleton data structures", OS);

  indexGraphNodes(RootNode);
  generateDotGraph(OS, RootNode);

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