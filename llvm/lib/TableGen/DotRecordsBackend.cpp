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
#include "llvm/Support/GraphWriter.h"
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

static cl::opt<std::string>
FilterDef("filter-def", cl::desc("Def to generate dot graph for"), cl::value_desc("DefName"), cl::init(""));

using NodeMap = std::unordered_map<Record*, int>;
// Any helper data structures can be defined here. Some backends use
// structs to collect information from the records.

class DotRecordsEmitter {
private:
  RecordKeeper &Records;
  NodeMap Nodes;

  void printGraphEdges(raw_ostream &OS, Record& RootNode);
  void printNodeLabel(Record* Node, raw_ostream &OS);
  void printNodes(raw_ostream &OS);
  void indexGraphNodes(Record &RootNode);
  void generateDotGraph(raw_ostream &OS, Record &RootNode);

public:
  DotRecordsEmitter(RecordKeeper &RK) : Records(RK) {}
  void run(raw_ostream &OS);
}; // emitter class

} // anonymous namespace

static void printEdge(Record& ChildNode, Record& ParentNode, NodeMap& Nodes, raw_ostream &OS) {
    std::string ChildLabel = ChildNode.getNameInitAsString();
    std::string ParentLabel = ParentNode.getNameInitAsString();
    int ChildIndex = Nodes[&ChildNode];
    int ParentIndex = Nodes[&ParentNode];
    assert(Nodes.find(&ChildNode) != Nodes.end() && "Child node not indexed");
    assert(Nodes.find(&ParentNode) != Nodes.end() && "Parent node not indexed");
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

void DotRecordsEmitter::printNodeLabel(Record* Node, raw_ostream &OS){
  std::string NodeName = Node->getNameInitAsString();
  int NodeIndex = Nodes[Node];
  if (!Node->isClass()){
    OS << std::to_string(NodeIndex) +  "[shape=ellipse label=\"" + NodeName + "\" ];\n";
    return;
  }

  // TODO: Can use formatv for better string formatting
  OS << std::to_string(NodeIndex) +  "[shape=record label=\"" + NodeName + " | {Template args | ";
  ArrayRef<Init *> TemplateArgs = Node->getTemplateArgs();
  for (const Init *TA : TemplateArgs) {
    const RecordVal *Value = Node->getValue(TA);
    // FIXME: Tablegen string escape issue in generated string hence can't use OS << *Value;
    OS << DOT::EscapeString(Value->getPrintType()) + " " + Value->getNameInitAsString() + "\\n";
  }
  OS << "}\" ];\n";
}

void DotRecordsEmitter::printNodes(raw_ostream &OS) {
  for(auto NodeInfo: Nodes){
    Record* Node = NodeInfo.first;
    printNodeLabel(Node, OS);
    OS << NL;
  }
}

void DotRecordsEmitter::indexGraphNodes(Record &RootNode) {
  int index = 0;
  std::queue<Record*>Queue;
  // SmallVectorImpl<Record *> Classes;
  Queue.push(&RootNode);
  while(Queue.size()){
    Record* Current = Queue.front();
    std::string ClassName = Current->getNameInitAsString();
    Queue.pop();
    
    // Skip visited nodes
    if(Nodes.find(Current) != Nodes.end())
      continue;

    Nodes[Current] = index++;
    
    // Push parent nodes into queue
    // FIXME: Use getDirectSuperClasses
    auto Superclasses = Current->getSuperClasses();
    for (const auto &SuperClassPair : Superclasses)
      Queue.push(SuperClassPair.first);
    
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
  if(FilterClass.empty() && FilterDef.empty()){
    OS << "Specify either -filter-class or -filter-def" << NL;
    return;
  }
  if(!FilterClass.empty() && !FilterDef.empty()){
    OS << "Specify only one out of -filter-class and -filter-def" << NL;
    return;
  }
  Record* RootNode;
  // FIXME: Add asserts for invalid Class or Def name
  if(!FilterClass.empty())
    RootNode = Records.getClass(FilterClass);
  else
    RootNode = Records.getDef(FilterDef);
  OS << "Root Node is " << RootNode->getNameInitAsString() << NL;

  // FIXME: No need to print file header in .dot graph, will cause parsing error
  // emitSourceFileHeader("Skeleton data structures", OS);

  indexGraphNodes(*RootNode);
  generateDotGraph(OS, *RootNode);

  (void)Records; // To suppress unused variable warning; remove on use.
}

namespace llvm {

// This function is called by TableGen after parsing the files.

void EmitDotRecords(RecordKeeper &RK, raw_ostream &OS) {
  // Instantiate the emitter class and invoke run().
  DotRecordsEmitter(RK).run(OS);
}

} // namespace llvm