/*
   Copyright (c) 2009-2015, Jack Poulson
   All rights reserved.

   This file is part of Elemental and is under the BSD 2-Clause License, 
   which can be found in the LICENSE file in the root directory, or at 
   http://opensource.org/licenses/BSD-2-Clause
*/
#include "El.hpp"

#include "./Syrk/LN.hpp"
#include "./Syrk/LT.hpp"
#include "./Syrk/UN.hpp"
#include "./Syrk/UT.hpp"

namespace El {

template<typename Ring>
void Syrk
( UpperOrLower uplo, Orientation orientation,
  Ring alpha, const Matrix<Ring>& A, 
  Ring beta,        Matrix<Ring>& C, bool conjugate )
{
    DEBUG_ONLY(
      CSE cse("Syrk");
      if( orientation == NORMAL )
      {
          if( A.Height() != C.Height() || A.Height() != C.Width() )
              LogicError("Nonconformal Syrk");
      }
      else
      {
          if( A.Width() != C.Height() || A.Width() != C.Width() )
              LogicError("Nonconformal Syrk");
      }
    )
    const char uploChar = UpperOrLowerToChar( uplo );
    const char transChar = OrientationToChar( orientation );
    const Int k = ( orientation == NORMAL ? A.Width() : A.Height() );
    if( conjugate )
    {
        blas::Herk
        ( uploChar, transChar, C.Height(), k,
          RealPart(alpha), A.LockedBuffer(), A.LDim(),
          RealPart(beta),  C.Buffer(),       C.LDim() );
    }
    else
    {
        blas::Syrk
        ( uploChar, transChar, C.Height(), k,
          alpha, A.LockedBuffer(), A.LDim(),
          beta,  C.Buffer(),       C.LDim() );
    }
}

template<typename Ring>
void Syrk
( UpperOrLower uplo, Orientation orientation,
  Ring alpha, const Matrix<Ring>& A, Matrix<Ring>& C, bool conjugate )
{
    DEBUG_ONLY(CSE cse("Syrk"))
    const Int n = ( orientation==NORMAL ? A.Height() : A.Width() );
    Zeros( C, n, n );
    Syrk( uplo, orientation, alpha, A, Ring(0), C, conjugate );
}

template<typename Ring>
void Syrk
( UpperOrLower uplo, Orientation orientation,
  Ring alpha, const AbstractDistMatrix<Ring>& A, 
  Ring beta,        AbstractDistMatrix<Ring>& C, bool conjugate )
{
    DEBUG_ONLY(CSE cse("Syrk"))
    ScaleTrapezoid( beta, uplo, C );
    if( uplo == LOWER && orientation == NORMAL )
        syrk::LN( alpha, A, C, conjugate );
    else if( uplo == LOWER )
        syrk::LT( alpha, A, C, conjugate );
    else if( orientation == NORMAL )
        syrk::UN( alpha, A, C, conjugate );
    else
        syrk::UT( alpha, A, C, conjugate );
}

template<typename Ring>
void Syrk
( UpperOrLower uplo, Orientation orientation,
  Ring alpha, const AbstractDistMatrix<Ring>& A, 
                    AbstractDistMatrix<Ring>& C, bool conjugate )
{
    DEBUG_ONLY(CSE cse("Syrk"))
    const Int n = ( orientation==NORMAL ? A.Height() : A.Width() );
    Zeros( C, n, n );
    Syrk( uplo, orientation, alpha, A, Ring(0), C, conjugate );
}

template<typename Ring>
void Syrk
( UpperOrLower uplo, Orientation orientation,
  Ring alpha, const SparseMatrix<Ring>& A, 
  Ring beta,        SparseMatrix<Ring>& C, bool conjugate )
{
    DEBUG_ONLY(CSE cse("Syrk"))

    if( orientation == NORMAL )
    {
        SparseMatrix<Ring> B;
        Transpose( A, B, conjugate );
        const Orientation newOrient = ( conjugate ? ADJOINT : TRANSPOSE );
        Syrk( uplo, newOrient, alpha, B, beta, C, conjugate );
        return;
    }

    const Int m = A.Height();
    const Int n = A.Width();
    if( C.Height() != n || C.Width() != n )
        LogicError("C was of the incorrect size");

    ScaleTrapezoid( beta, uplo, C );

    // Compute an upper bound on the required capacity
    // ===============================================
    Int newCapacity = C.NumEntries();
    for( Int k=0; k<m; ++k )
    {
        const Int numConn = A.NumConnections(k);
        newCapacity += numConn*numConn;
    }
    C.Reserve( newCapacity );

    // Queue the updates
    // =================
    for( Int k=0; k<m; ++k )
    {
        const Int offset = A.EntryOffset(k);
        const Int numConn = A.NumConnections(k);
        for( Int iConn=0; iConn<numConn; ++iConn )
        {
            const Int i = A.Col(offset+iConn);
            const Ring A_ki = A.Value(offset+iConn);
            for( Int jConn=0; jConn<numConn; ++jConn )
            {
                const Int j = A.Col(offset+jConn);
                if( (uplo == LOWER && i >= j) || (uplo == UPPER && i <= j) )
                {
                    const Ring A_kj = A.Value(offset+jConn);
                    if( conjugate )
                        C.QueueUpdate( i, j, Ring(alpha)*Conj(A_ki)*A_kj ); 
                    else
                        C.QueueUpdate( i, j, Ring(alpha)*A_ki*A_kj );
                }
            }
        }
    }
    C.ProcessQueues();
}

template<typename Ring>
void Syrk
( UpperOrLower uplo, Orientation orientation,
  Ring alpha, const SparseMatrix<Ring>& A, 
                    SparseMatrix<Ring>& C, bool conjugate )
{
    DEBUG_ONLY(CSE cse("Syrk"))
    const Int m = A.Height();
    const Int n = A.Width();
    if( orientation == NORMAL )
        Zeros( C, m, m );
    else
        Zeros( C, n, n );
    Syrk( uplo, orientation, alpha, A, Ring(0), C, conjugate );
}

template<typename Ring>
void Syrk
( UpperOrLower uplo, Orientation orientation,
  Ring alpha, const DistSparseMatrix<Ring>& A, 
  Ring beta,        DistSparseMatrix<Ring>& C, bool conjugate )
{
    DEBUG_ONLY(CSE cse("Syrk"))

    if( orientation == NORMAL )
    {
        DistSparseMatrix<Ring> B(A.Comm());
        Transpose( A, B, conjugate );
        const Orientation newOrient = ( conjugate ? ADJOINT : TRANSPOSE );
        Syrk( uplo, newOrient, alpha, B, beta, C, conjugate );
        return;
    }

    const Int n = A.Width();
    mpi::Comm comm = A.Comm();

    if( C.Height() != n || C.Width() != n )
        LogicError("C was of the incorrect size");
    if( C.Comm() != comm )
        LogicError("Communicators of A and C must match");

    ScaleTrapezoid( beta, uplo, C );

    // Reserve space for the updates
    // =============================
    Int numUpdates = 0;
    const Int localHeightA = A.LocalHeight();
    for( Int kLoc=0; kLoc<localHeightA; ++kLoc )
    {
        const Int numConn = A.NumConnections(kLoc);
        numUpdates += numConn*numConn;
    }
    C.Reserve( C.NumLocalEntries()+numUpdates, numUpdates );

    // Queue the updates
    // ================= 
    for( Int kLoc=0; kLoc<localHeightA; ++kLoc )
    {
        const Int offset = A.EntryOffset(kLoc);
        const Int numConn = A.NumConnections(kLoc);
        for( Int iConn=0; iConn<numConn; ++iConn ) 
        {
            const Int i = A.Col(offset+iConn);
            const Ring A_ki = A.Value(offset+iConn);
            for( Int jConn=0; jConn<numConn; ++jConn )
            {
                const Int j = A.Col(offset+jConn);
                if( (uplo==LOWER && i>=j) || (uplo==UPPER && i<=j) )
                {
                    const Ring A_kj = A.Value(offset+jConn);
                    if( conjugate )
                        C.QueueUpdate( i, j, Ring(alpha)*Conj(A_ki)*A_kj );
                    else
                        C.QueueUpdate( i, j, Ring(alpha)*A_ki*A_kj );
                }
            }
        }
    }
    C.ProcessQueues();
}

template<typename Ring>
void Syrk
( UpperOrLower uplo, Orientation orientation,
  Ring alpha, const DistSparseMatrix<Ring>& A, 
                    DistSparseMatrix<Ring>& C, bool conjugate )
{
    DEBUG_ONLY(CSE cse("Syrk"))
    const Int m = A.Height();
    const Int n = A.Width();
    if( orientation == NORMAL )
        Zeros( C, m, m );
    else
        Zeros( C, n, n );
    Syrk( uplo, orientation, alpha, A, Ring(0), C, conjugate );
}

#define PROTO(Ring) \
  template void Syrk \
  ( UpperOrLower uplo, Orientation orientation, \
    Ring alpha, const Matrix<Ring>& A, \
    Ring beta,        Matrix<Ring>& C, bool conjugate ); \
  template void Syrk \
  ( UpperOrLower uplo, Orientation orientation, \
    Ring alpha, const Matrix<Ring>& A, Matrix<Ring>& C, bool conjugate ); \
  template void Syrk \
  ( UpperOrLower uplo, Orientation orientation, \
    Ring alpha, const AbstractDistMatrix<Ring>& A, \
    Ring beta, AbstractDistMatrix<Ring>& C, bool conjugate ); \
  template void Syrk \
  ( UpperOrLower uplo, Orientation orientation, \
    Ring alpha, const AbstractDistMatrix<Ring>& A, \
                      AbstractDistMatrix<Ring>& C, bool conjugate ); \
  template void Syrk \
  ( UpperOrLower uplo, Orientation orientation, \
    Ring alpha, const SparseMatrix<Ring>& A, \
    Ring beta,        SparseMatrix<Ring>& C, bool conjugate ); \
  template void Syrk \
  ( UpperOrLower uplo, Orientation orientation, \
    Ring alpha, const SparseMatrix<Ring>& A, \
                      SparseMatrix<Ring>& C, bool conjugate ); \
  template void Syrk \
  ( UpperOrLower uplo, Orientation orientation, \
    Ring alpha, const DistSparseMatrix<Ring>& A, \
    Ring beta,        DistSparseMatrix<Ring>& C, bool conjugate ); \
  template void Syrk \
  ( UpperOrLower uplo, Orientation orientation, \
    Ring alpha, const DistSparseMatrix<Ring>& A, \
                      DistSparseMatrix<Ring>& C, bool conjugate );

// blas::Syrk is not yet supported for Int
#define EL_NO_INT_PROTO
#include "El/macros/Instantiate.h"

} // namespace El
