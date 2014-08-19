/*
   Copyright (c) 2009-2014, Jack Poulson
   All rights reserved.

   This file is part of Elemental and is under the BSD 2-Clause License, 
   which can be found in the LICENSE file in the root directory, or at 
   http://opensource.org/licenses/BSD-2-Clause
*/
#pragma once
#ifndef EL_LDL_SOLVEAFTER_HPP
#define EL_LDL_SOLVEAFTER_HPP

namespace El {
namespace ldl {

template<typename F> 
void SolveAfter( const Matrix<F>& A, Matrix<F>& B, bool conjugated )
{
    DEBUG_ONLY(
        CallStackEntry cse("ldl::SolveAfter");
        if( A.Height() != A.Width() )
            LogicError("A must be square");
        if( A.Height() != B.Height() )
            LogicError("A and B must be the same height");
    )
    const Orientation orientation = ( conjugated ? ADJOINT : TRANSPOSE );
    const bool checkIfSingular = false;
    const auto d = A.GetDiagonal();
    Trsm( LEFT, LOWER, NORMAL, UNIT, F(1), A, B );
    DiagonalSolve( LEFT, NORMAL, d, B, checkIfSingular );
    Trsm( LEFT, LOWER, orientation, UNIT, F(1), A, B );
}

template<typename F> 
void SolveAfter
( const AbstractDistMatrix<F>& APre, AbstractDistMatrix<F>& B, bool conjugated )
{
    DEBUG_ONLY(
        CallStackEntry cse("ldl::SolveAfter");
        AssertSameGrids( APre, B );
        if( APre.Height() != APre.Width() )
            LogicError("A must be square");
        if( APre.Height() != B.Height() )
            LogicError("A and B must be the same height");
    )
    const Orientation orientation = ( conjugated ? ADJOINT : TRANSPOSE );
    const bool checkIfSingular = false;

    DistMatrix<F> A(APre.Grid());
    Copy( APre, A, READ_PROXY );

    const auto d = A.GetDiagonal();

    Trsm( LEFT, LOWER, NORMAL, UNIT, F(1), A, B );
    DiagonalSolve( LEFT, NORMAL, d, B, checkIfSingular );
    Trsm( LEFT, LOWER, orientation, UNIT, F(1), A, B );
}

template<typename F> 
void SolveAfter
( const Matrix<F>& A, const Matrix<F>& dSub, 
  const Matrix<Int>& p, Matrix<F>& B, bool conjugated )
{
    DEBUG_ONLY(
        CallStackEntry cse("ldl::SolveAfter");
        if( A.Height() != A.Width() )
            LogicError("A must be square");
        if( A.Height() != B.Height() )
            LogicError("A and B must be the same height");
        if( p.Height() != A.Height() )
            LogicError("A and p must be the same height");
        // TODO: Check for dSub
    )
    const Orientation orientation = ( conjugated ? ADJOINT : TRANSPOSE );
    const auto d = A.GetDiagonal();

    Matrix<Int> pInv;
    InvertPermutation( p, pInv );

    PermuteRows( B, p, pInv );
    Trsm( LEFT, LOWER, NORMAL, UNIT, F(1), A, B );
    QuasiDiagonalSolve( LEFT, LOWER, d, dSub, B, conjugated );
    Trsm( LEFT, LOWER, orientation, UNIT, F(1), A, B );
    PermuteRows( B, pInv, p );
}

template<typename F> 
void SolveAfter
( const AbstractDistMatrix<F>& APre, const AbstractDistMatrix<F>& dSub, 
  const AbstractDistMatrix<Int>& p, AbstractDistMatrix<F>& BPre, 
  bool conjugated )
{
    DEBUG_ONLY(
        CallStackEntry cse("ldl::SolveAfter");
        AssertSameGrids( APre, BPre, p );
        if( APre.Height() != APre.Width() )
            LogicError("A must be square");
        if( APre.Height() != BPre.Height() )
            LogicError("A and B must be the same height");
        if( APre.Height() != p.Height() )
            LogicError("A and p must be the same height");
        // TODO: Check for dSub
    )
    const Orientation orientation = ( conjugated ? ADJOINT : TRANSPOSE );

    const Grid& g = APre.Grid();
    DistMatrix<F> A(g), B(g);
    Copy( APre, A, READ_PROXY );
    Copy( BPre, B, READ_WRITE_PROXY );

    const auto d = A.GetDiagonal();

    DistMatrix<Int,VC,STAR> pInv(p.Grid());
    InvertPermutation( p, pInv );

    PermuteRows( B, p, pInv );
    Trsm( LEFT, LOWER, NORMAL, UNIT, F(1), A, B );
    QuasiDiagonalSolve( LEFT, LOWER, d, dSub, B, conjugated );
    Trsm( LEFT, LOWER, orientation, UNIT, F(1), A, B );
    PermuteRows( B, pInv, p );

    Copy( B, BPre, RESTORE_READ_WRITE_PROXY );
}

} // namespace ldl
} // namespace El

#endif // ifndef EL_LDL_SOLVEAFTER_HPP
