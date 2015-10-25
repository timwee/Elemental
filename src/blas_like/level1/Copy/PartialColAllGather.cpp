/*
   Copyright (c) 2009-2015, Jack Poulson
   All rights reserved.

   This file is part of Elemental and is under the BSD 2-Clause License, 
   which can be found in the LICENSE file in the root directory, or at 
   http://opensource.org/licenses/BSD-2-Clause
*/
#include "El.hpp"
#include "El/blas_like/level1/copy_internal.hpp"

namespace El {
namespace copy {

template<typename T,Dist U,Dist V>
void PartialColAllGather
( const DistMatrix<T,        U,   V>& A, 
        DistMatrix<T,Partial<U>(),V>& B ) 
{
    DEBUG_ONLY(CSE cse("copy::PartialColAllGather"))
    AssertSameGrids( A, B );

    const Int height = A.Height();
    const Int width = A.Width();
#ifdef EL_VECTOR_WARNINGS
    if( width == 1 && A.Grid().Rank() == 0 )
    {
        cerr <<
          "The vector version of PartialColAllGather is not yet written but "
          "would only require modifying the vector version of "
          "PartialRowAllGather" << endl;
    }
#endif
#ifdef EL_CACHE_WARNINGS
    if( width && A.Grid().Rank() == 0 )
    {
        cerr <<
          "PartialColAllGather potentially causes a large amount of cache-"
          "thrashing. If possible, avoid it by performing the redistribution"
          "on the (conjugate-)transpose" << endl;
    }
#endif
    B.AlignColsAndResize
    ( Mod(A.ColAlign(),B.ColStride()), height, width, false, false );
    if( !A.Participating() )
        return;

    DEBUG_ONLY(
      if( A.LocalWidth() != A.Width() )
          LogicError("This routine assumes rows are not distributed");
    )

    const Int colStrideUnion = A.PartialUnionColStride();
    const Int colStridePart = A.PartialColStride();
    const Int colDiff = B.ColAlign() - Mod(A.ColAlign(),colStridePart);

    const Int maxLocalHeight = MaxLength(height,A.ColStride());
    const Int portionSize = mpi::Pad( maxLocalHeight*width );

    if( colDiff == 0 )
    {
        if( A.PartialUnionColStride() == 1 )
        {
            Copy( A.LockedMatrix(), B.Matrix() );
        }
        else
        {
            //vector<T> buffer( (colStrideUnion+1)*portionSize );
            vector<T> buffer;
            buffer.reserve( (colStrideUnion+1)*portionSize );
            T* firstBuf = &buffer[0];
            T* secondBuf = &buffer[portionSize];

            // Pack
            util::InterleaveMatrix
            ( A.LocalHeight(), width,
              A.LockedBuffer(), 1, A.LDim(),
              firstBuf,         1, A.LocalHeight() );

            // Communicate
            mpi::AllGather
            ( firstBuf, portionSize, secondBuf, portionSize,
              A.PartialUnionColComm() );

            // Unpack
            util::PartialColStridedUnpack
            ( height, width,
              A.ColAlign(), A.ColStride(),
              colStrideUnion, colStridePart, A.PartialColRank(),
              B.ColShift(), 
              secondBuf, portionSize,
              B.Buffer(), B.LDim() );
        }
    }
    else
    {
#ifdef EL_UNALIGNED_WARNINGS
        if( A.Grid().Rank() == 0 )
            cerr << "Unaligned PartialColAllGather" << endl;
#endif
        //vector<T> buffer( (colStrideUnion+1)*portionSize );
        vector<T> buffer;
        buffer.reserve( (colStrideUnion+1)*portionSize );
        T* firstBuf = &buffer[0];
        T* secondBuf = &buffer[portionSize];

        // Perform a SendRecv to match the row alignments
        util::InterleaveMatrix
        ( A.LocalHeight(), width,
          A.LockedBuffer(), 1, A.LDim(),
          secondBuf,        1, A.LocalHeight() );
        const Int sendColRank = Mod( A.ColRank()+colDiff, A.ColStride() );
        const Int recvColRank = Mod( A.ColRank()-colDiff, A.ColStride() );
        mpi::SendRecv
        ( secondBuf, portionSize, sendColRank,
          firstBuf,  portionSize, recvColRank, A.ColComm() );

        // Use the SendRecv as an input to the partial union AllGather
        mpi::AllGather
        ( firstBuf,  portionSize,
          secondBuf, portionSize, A.PartialUnionColComm() );

        // Unpack
        util::PartialColStridedUnpack
        ( height, width,
          A.ColAlign()+colDiff, A.ColStride(),
          colStrideUnion, colStridePart, A.PartialColRank(),
          B.ColShift(), 
          secondBuf, portionSize,
          B.Buffer(), B.LDim() );
    }
}

template<typename T,Dist U,Dist V>
void PartialColAllGather
( const DistMatrix<T,        U,   V,BLOCK>& A, 
        DistMatrix<T,Partial<U>(),V,BLOCK>& B ) 
{
    DEBUG_ONLY(CSE cse("copy::PartialColAllGather"))
    AssertSameGrids( A, B );
    // TODO: More efficient implementation
    GeneralPurpose( A, B );
}

#define PROTO_DIST(T,U,V) \
  template void PartialColAllGather \
  ( const DistMatrix<T,        U,   V>& A, \
          DistMatrix<T,Partial<U>(),V>& B ); \
  template void PartialColAllGather \
  ( const DistMatrix<T,        U,   V,BLOCK>& A, \
          DistMatrix<T,Partial<U>(),V,BLOCK>& B );

#define PROTO(T) \
  PROTO_DIST(T,CIRC,CIRC) \
  PROTO_DIST(T,MC,  MR  ) \
  PROTO_DIST(T,MC,  STAR) \
  PROTO_DIST(T,MD,  STAR) \
  PROTO_DIST(T,MR,  MC  ) \
  PROTO_DIST(T,MR,  STAR) \
  PROTO_DIST(T,STAR,MC  ) \
  PROTO_DIST(T,STAR,MD  ) \
  PROTO_DIST(T,STAR,MR  ) \
  PROTO_DIST(T,STAR,STAR) \
  PROTO_DIST(T,STAR,VC  ) \
  PROTO_DIST(T,STAR,VR  ) \
  PROTO_DIST(T,VC,  STAR) \
  PROTO_DIST(T,VR,  STAR) 

#define EL_ENABLE_QUAD
#include "El/macros/Instantiate.h"

} // namespace copy
} // namespace El
