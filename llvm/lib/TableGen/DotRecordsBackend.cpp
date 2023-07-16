//===- DotRecordsBackend.cpp - Dot Records TableGen backend     -*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This Tablegen backend emits the inheritance hierarchy of a tablegen record
// or a class as a dot graph, where each node represents a class/record with 
// additional details such as template args and member variables.
//
//===----------------------------------------------------------------------===//

#include "llvm/ADT/DenseMapInfo.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/GraphWriter.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/TableGen/Record.h"
#include "llvm/TableGen/TableGenBackend.h"
#include <queue>
#include <unordered_map>

#define DEBUG_TYPE "detailed-records-backend"

#define NL "\n"
namespace llvm {
class RecordKeeper;
class raw_ostream;
} // namespace llvm

using namespace llvm;

namespace {

// TODO: Can make this parameter hidden
// TODO: Use single option instead of 2
// TODO: Support multiclass and defm
static cl::opt<std::string>
    FilterClass("filter-class", cl::desc("Class to generate dot graph for"),
                cl::value_desc("ClassName"), cl::init(""));

static cl::opt<std::string> FilterDef("filter-def",
                                      cl::desc("Def to generate dot graph for"),
                                      cl::value_desc("DefName"), cl::init(""));

using NodeMap = std::unordered_map<Record *, int>;

class DotRecordsEmitter {
private:
  RecordKeeper &Records;
  NodeMap Nodes;

  void printGraphEdges(raw_ostream &OS, Record &RootNode);
  void printNode(Record *Node, raw_ostream &OS);
  void printNodes(raw_ostream &OS);
  void indexGraphNodes(Record &RootNode);
  void generateDotGraph(raw_ostream &OS, Record &RootNode);

public:
  DotRecordsEmitter(RecordKeeper &RK) : Records(RK) {}
  void run(raw_ostream &OS);
}; // emitter class

} // anonymous namespace

static void printEdge(Record &ChildNode, Record &ParentNode, NodeMap &Nodes,
                      raw_ostream &OS) {
  int ChildIndex = Nodes[&ChildNode];
  int ParentIndex = Nodes[&ParentNode];
  assert(Nodes.find(&ChildNode) != Nodes.end() && "Child node not indexed");
  assert(Nodes.find(&ParentNode) != Nodes.end() && "Parent node not indexed");
  OS << "\"" << ChildIndex << "\" -> \"" << ParentIndex << "\";" << NL;
}

void DotRecordsEmitter::printGraphEdges(raw_ostream &OS, Record &RootNode) {
  std::queue<Record *> Queue;
  Queue.push(&RootNode);
  while (Queue.size()) {
    Record *Child = Queue.front();
    Queue.pop();

    // Print child to parent edge and push parents to queue
    SmallVector<Record *> ParentClasses;
    Child->getDirectSuperClasses(ParentClasses);
    for (Record *Parent : ParentClasses) {
      printEdge(*Child, *Parent, Nodes, OS);
      Queue.push(Parent);
    }
  }
  OS << NL;
}

// TODO: Can use formatv for better string formatting
void DotRecordsEmitter::printNode(Record *Node, raw_ostream &OS) {
  std::string NodeName = Node->getNameInitAsString();
  int NodeIndex = Nodes[Node];
  if (!Node->isClass()) {
    OS << std::to_string(NodeIndex) + "[shape=ellipse label=\"" + NodeName +
              "\" ];\n";
    return;
  }

  OS << std::to_string(NodeIndex) + "[shape=record label=\"" + NodeName +
            " | {Template args | ";
  ArrayRef<Init *> TemplateArgs = Node->getTemplateArgs();
  for (const Init *TA : TemplateArgs) {
    const RecordVal *Value = Node->getValue(TA);
    // FIXME: Tablegen string escape issue in generated string hence can't use
    // OS << *Value;
    OS << DOT::EscapeString(Value->getPrintType()) + " " +
              Value->getNameInitAsString() + "\\n";
  }
  OS << "}\" ];\n";
  // Member variables
  ArrayRef<RecordVal>Values = Node->getValues(); 
  for(auto Val: Values){
    OS << "// "; 
    if(Val.isTemplateArg())
      OS << "Template arg: ";
    else if(Val.isNonconcreteOK())
      OS << "Non Concrete arg: ";
    else 
      OS << "Normal arg: ";
    OS << Val;
  }
  OS << NL;
}

void DotRecordsEmitter::printNodes(raw_ostream &OS) {
  for (auto NodeInfo : Nodes) {
    Record *Node = NodeInfo.first;
    printNode(Node, OS);
  }
}

void DotRecordsEmitter::indexGraphNodes(Record &RootNode) {
  int index = 0;
  std::queue<Record *> Queue;
  Queue.push(&RootNode);
  while (Queue.size()) {
    Record *Current = Queue.front();
    Queue.pop();

    // Skip visited nodes
    if (Nodes.find(Current) != Nodes.end())
      continue;

    Nodes[Current] = index++;

    // Push parent nodes into queue
    SmallVector<Record *> ParentClasses;
    Current->getDirectSuperClasses(ParentClasses);
    for (const auto ParentClass : ParentClasses)
      Queue.push(ParentClass);
  }
}

void DotRecordsEmitter::generateDotGraph(raw_ostream &OS, Record &RootNode) {
  OS << "// Root Node is " << RootNode.getNameInitAsString() << NL;
  OS << "digraph RecordDependencyGraph {" << NL;
  // Generate Nodes
  printNodes(OS);
  // Generate Edges
  printGraphEdges(OS, RootNode);
  OS << "}" << NL;
}

void DotRecordsEmitter::run(raw_ostream &OS) {
  if (FilterClass.empty() && FilterDef.empty()) {
    OS << "Specify either -filter-class or -filter-def" << NL;
    return;
  }
  if (!FilterClass.empty() && !FilterDef.empty()) {
    OS << "Specify only one out of -filter-class and -filter-def" << NL;
    return;
  }
  Record *RootNode;
  // TODO: Add asserts for invalid Class or Def name
  if (!FilterClass.empty())
    RootNode = Records.getClass(FilterClass);
  else
    RootNode = Records.getDef(FilterDef);

  indexGraphNodes(*RootNode);
  generateDotGraph(OS, *RootNode);
}

namespace llvm {

// This function is called by TableGen after parsing the files.

void EmitDotRecords(RecordKeeper &RK, raw_ostream &OS) {
  // Instantiate the emitter class and invoke run().
  DotRecordsEmitter(RK).run(OS);
}

} // namespace llvm