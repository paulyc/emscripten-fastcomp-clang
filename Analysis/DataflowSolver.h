//===--- DataflowSolver.h - Skeleton Dataflow Analysis Code -----*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file was developed by Ted Kremenek and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file defines skeleton code for implementing dataflow analyses.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_ANALYSES_DATAFLOW_SOLVER
#define LLVM_CLANG_ANALYSES_DATAFLOW_SOLVER

#include "clang/AST/CFG.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "functional" // STL

namespace clang {

//===----------------------------------------------------------------------===//
/// DataflowWorkListTy - Data structure representing the worklist used for
///  dataflow algorithms.

class DataflowWorkListTy {
  typedef llvm::SmallPtrSet<const CFGBlock*,20> BlockSet;
  BlockSet wlist;
public:
  /// enqueue - Add a block to the worklist.  Blocks already on the worklist
  ///  are not added a second time.  
  void enqueue(const CFGBlock* B) { wlist.insert(B); }
  
  /// dequeue - Remove a block from the worklist.
  const CFGBlock* dequeue() {
    assert (!wlist.empty());
    const CFGBlock* B = *wlist.begin();
    wlist.erase(B);
    return B;          
  }
  
  /// isEmpty - Return true if the worklist is empty.
  bool isEmpty() const { return wlist.empty(); }
};

//===----------------------------------------------------------------------===//
// BlockItrTraits - Traits classes that allow transparent iteration over
//  successors/predecessors of a block depending on the direction of our
//  dataflow analysis.

namespace dataflow {
template<typename Tag> struct ItrTraits {};

template <> struct ItrTraits<forward_analysis_tag> {
  typedef CFGBlock::const_pred_iterator PrevBItr;
  typedef CFGBlock::const_succ_iterator NextBItr;
  typedef CFGBlock::const_iterator      StmtItr;
  
  static PrevBItr PrevBegin(const CFGBlock* B) { return B->pred_begin(); }
  static PrevBItr PrevEnd(const CFGBlock* B) { return B->pred_end(); }
  
  static NextBItr NextBegin(const CFGBlock* B) { return B->succ_begin(); }    
  static NextBItr NextEnd(const CFGBlock* B) { return B->succ_end(); }
  
  static StmtItr StmtBegin(const CFGBlock* B) { return B->begin(); }
  static StmtItr StmtEnd(const CFGBlock* B) { return B->end(); }
  
  static CFG::Edge PrevEdge(const CFGBlock* B, const CFGBlock* PrevBlk) {
    return CFG::Edge(PrevBlk,B);
  }
  
  static CFG::Edge NextEdge(const CFGBlock* B, const CFGBlock* NextBlk) {
    return CFG::Edge(B,NextBlk);
  }
};

template <> struct ItrTraits<backward_analysis_tag> {
  typedef CFGBlock::const_succ_iterator    PrevBItr;
  typedef CFGBlock::const_pred_iterator    NextBItr;
  typedef CFGBlock::const_reverse_iterator StmtItr;
  
  static PrevBItr PrevBegin(const CFGBlock* B) { return B->succ_begin(); }    
  static PrevBItr PrevEnd(const CFGBlock* B) { return B->succ_end(); }
  
  static NextBItr NextBegin(const CFGBlock* B) { return B->pred_begin(); }    
  static NextBItr NextEnd(const CFGBlock* B) { return B->pred_end(); }
  
  static StmtItr StmtBegin(const CFGBlock* B) { return B->rbegin(); }
  static StmtItr StmtEnd(const CFGBlock* B) { return B->rend(); }    
  
  static CFG::Edge PrevEdge(const CFGBlock* B, const CFGBlock* PrevBlk) {
    return CFG::Edge(B,PrevBlk);
  }
  
  static CFG::Edge NextEdge(const CFGBlock* B, const CFGBlock* NextBlk) {
    return CFG::Edge(NextBlk,B);
  }
};
} // end namespace dataflow

//===----------------------------------------------------------------------===//
/// DataflowSolverTy - Generic dataflow solver.
template <typename _DFValuesTy,      // Usually a subclass of DataflowValues
          typename _TransferFuncsTy,
          typename _MergeOperatorTy,
          typename _Equal = std::equal_to<typename _DFValuesTy::ValTy> >
class DataflowSolver {

  //===--------------------------------------------------------------------===//
  // Type declarations.
  //===--------------------------------------------------------------------===//

public:
  typedef _DFValuesTy                              DFValuesTy;
  typedef _TransferFuncsTy                         TransferFuncsTy;
  typedef _MergeOperatorTy                         MergeOperatorTy;
  
  typedef typename _DFValuesTy::AnalysisDirTag     AnalysisDirTag;
  typedef typename _DFValuesTy::ValTy              ValTy;
  typedef typename _DFValuesTy::EdgeDataMapTy      EdgeDataMapTy;
  typedef typename _DFValuesTy::BlockDataMapTy     BlockDataMapTy;

  typedef dataflow::ItrTraits<AnalysisDirTag>      ItrTraits;
  typedef typename ItrTraits::NextBItr             NextBItr;
  typedef typename ItrTraits::PrevBItr             PrevBItr;
  typedef typename ItrTraits::StmtItr              StmtItr;
  
  //===--------------------------------------------------------------------===//
  // External interface: constructing and running the solver.
  //===--------------------------------------------------------------------===//
  
public:
  DataflowSolver(DFValuesTy& d) : D(d), TF(d.getAnalysisData()) {}
  ~DataflowSolver() {}  
  
  /// runOnCFG - Computes dataflow values for all blocks in a CFG.
  void runOnCFG(const CFG& cfg) {
    // Set initial dataflow values and boundary conditions.
    D.InitializeValues(cfg);     
    // Solve the dataflow equations.  This will populate D.EdgeDataMap
    // with dataflow values.
    SolveDataflowEquations(cfg);    
  }
  
  /// runOnBlock - Computes dataflow values for a given block.
  ///  This should usually be invoked only after previously computing
  ///  dataflow values using runOnCFG, as runOnBlock is intended to
  ///  only be used for querying the dataflow values within a block with
  ///  and Observer object.
  void runOnBlock(const CFGBlock* B) {
    BlockDataMapTy& M = D.getBlockDataMap();
    typename BlockDataMapTy::iterator I = M.find(B);

    if (I != M.end()) {
      TF.getVal().copyValues(I->second);
      ProcessBlock(B);
    }
  }
  
  void runOnBlock(const CFGBlock& B) { runOnBlock(&B); }
  void runOnBlock(CFG::iterator& I) { runOnBlock(*I); }
  void runOnBlock(CFG::const_iterator& I) { runOnBlock(*I); }

  void runOnAllBlocks(const CFG& cfg) {
    for (CFG::const_iterator I=cfg.begin(), E=cfg.end(); I!=E; ++I)
      runOnBlock(I);
  }
  
  //===--------------------------------------------------------------------===//
  // Internal solver logic.
  //===--------------------------------------------------------------------===//
  
private:
 
  /// SolveDataflowEquations - Perform the actual
  ///  worklist algorithm to compute dataflow values.  
  void SolveDataflowEquations(const CFG& cfg) {
    EnqueueFirstBlock(cfg,AnalysisDirTag());
    
    while (!WorkList.isEmpty()) {
      const CFGBlock* B = WorkList.dequeue();
      ProcessMerge(B);
      ProcessBlock(B);
      UpdateEdges(B,TF.getVal());
    }
  }
  
  void EnqueueFirstBlock(const CFG& cfg, dataflow::forward_analysis_tag) {
    WorkList.enqueue(&cfg.getEntry());
  }
  
  void EnqueueFirstBlock(const CFG& cfg, dataflow::backward_analysis_tag) {
    WorkList.enqueue(&cfg.getExit());
  }  

  void ProcessMerge(const CFGBlock* B) {
    // Merge dataflow values from all predecessors of this block.
    ValTy& V = TF.getVal();
    V.resetValues(D.getAnalysisData());
    MergeOperatorTy Merge;
    
    EdgeDataMapTy& M = D.getEdgeDataMap();
    bool firstMerge = true;
    
    for (PrevBItr I=ItrTraits::PrevBegin(B),E=ItrTraits::PrevEnd(B); I!=E; ++I){

      typename EdgeDataMapTy::iterator EI = M.find(ItrTraits::PrevEdge(B,*I));

      if (EI != M.end()) {
        if (firstMerge) {
          firstMerge = false;
          V.copyValues(EI->second);
        }
        else Merge(V,EI->second);
      }
    }
    
    // Set the data for the block.
    D.getBlockDataMap()[B].copyValues(V);
  }
  

  /// ProcessBlock - Process the transfer functions for a given block.
  void ProcessBlock(const CFGBlock* B) {
    for (StmtItr I=ItrTraits::StmtBegin(B), E=ItrTraits::StmtEnd(B); I!=E; ++I)
      TF.BlockStmt_Visit(const_cast<Stmt*>(*I));
  }

  /// UpdateEdges - After processing the transfer
  ///   functions for a block, update the dataflow value associated with the
  ///   block's outgoing/incoming edges (depending on whether we do a 
  //    forward/backward analysis respectively)
  void UpdateEdges(const CFGBlock* B, ValTy& V) {
    for (NextBItr I=ItrTraits::NextBegin(B), E=ItrTraits::NextEnd(B); I!=E; ++I)
      UpdateEdgeValue(ItrTraits::NextEdge(B,*I),V,*I);
  }
    
  /// UpdateEdgeValue - Update the value associated with a given edge.
  void UpdateEdgeValue(CFG::Edge E, ValTy& V, const CFGBlock* TargetBlock) {
  
    EdgeDataMapTy& M = D.getEdgeDataMap();
    typename EdgeDataMapTy::iterator I = M.find(E);
      
    if (I == M.end()) {  // First computed value for this edge?
      M[E].copyValues(V);
      WorkList.enqueue(TargetBlock);
    }
    else if (!_Equal()(V,I->second)) {
      I->second.copyValues(V);
      WorkList.enqueue(TargetBlock);
    }
  }
    
private:
  DFValuesTy& D;
  DataflowWorkListTy WorkList;
  TransferFuncsTy TF;
};  
  

} // end namespace clang
#endif
