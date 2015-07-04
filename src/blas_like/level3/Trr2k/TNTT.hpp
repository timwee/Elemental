/*
   Copyright (c) 2009-2015, Jack Poulson
   All rights reserved.

   This file is part of Elemental and is under the BSD 2-Clause License, 
   which can be found in the LICENSE file in the root directory, or at 
   http://opensource.org/licenses/BSD-2-Clause
*/
#ifndef EL_TRR2K_TNTT_HPP
#define EL_TRR2K_TNTT_HPP

namespace El {
namespace trr2k {

// E := alpha A' B + beta C' D' + E
template<typename Ring>
void Trr2kTNTT
( UpperOrLower uplo,
  Orientation orientA,
  Orientation orientC, Orientation orientD,
  Ring alpha, const AbstractDistMatrix<Ring>& APre, 
              const AbstractDistMatrix<Ring>& BPre,
  Ring beta,  const AbstractDistMatrix<Ring>& CPre, 
              const AbstractDistMatrix<Ring>& DPre,
                    AbstractDistMatrix<Ring>& EPre )
{
    DEBUG_ONLY(
      CSE cse("trr2k::Trr2kTNTT");
      if( EPre.Height() != EPre.Width()  || APre.Height() != CPre.Height() ||
          APre.Width()  != EPre.Height() || CPre.Width()  != EPre.Height() ||
          BPre.Width()  != EPre.Width()  || DPre.Height() != EPre.Width()  ||
          APre.Height() != BPre.Height() || CPre.Height() != DPre.Width() )
          LogicError("Nonconformal Trr2kNNTT");
    )
    const Int r = APre.Height();
    const Int bsize = Blocksize();
    const Grid& g = EPre.Grid();

    auto APtr = ReadProxy<Ring,MC,MR>( &APre );      auto& A = *APtr;
    auto BPtr = ReadProxy<Ring,MC,MR>( &BPre );      auto& B = *BPtr;
    auto CPtr = ReadProxy<Ring,MC,MR>( &CPre );      auto& C = *CPtr;
    auto DPtr = ReadProxy<Ring,MC,MR>( &DPre );      auto& D = *DPtr;
    auto EPtr = ReadWriteProxy<Ring,MC,MR>( &EPre ); auto& E = *EPtr;

    DistMatrix<Ring,STAR,MC  > A1_STAR_MC(g), C1_STAR_MC(g);
    DistMatrix<Ring,MR,  STAR> B1Trans_MR_STAR(g);
    DistMatrix<Ring,VR,  STAR> D1_VR_STAR(g);
    DistMatrix<Ring,STAR,MR  > D1Trans_STAR_MR(g);

    A1_STAR_MC.AlignWith( E );
    B1Trans_MR_STAR.AlignWith( E );
    C1_STAR_MC.AlignWith( E );
    D1_VR_STAR.AlignWith( E );
    D1Trans_STAR_MR.AlignWith( E );

    for( Int k=0; k<r; k+=bsize )
    {
        const Int nb = Min(bsize,r-k);

        const Range<Int> ind1( k, k+nb );

        auto A1 = A( ind1, ALL  );
        auto B1 = B( ind1, ALL  );
        auto C1 = C( ind1, ALL  );
        auto D1 = D( ALL,  ind1 );

        A1_STAR_MC = A1;
        C1_STAR_MC = C1;
        Transpose( B1, B1Trans_MR_STAR );
        D1_VR_STAR = D1;
        Transpose( D1_VR_STAR, D1Trans_STAR_MR, (orientD==ADJOINT) );
        LocalTrr2k
        ( uplo, orientA, TRANSPOSE, orientC, NORMAL,
          alpha, A1_STAR_MC, B1Trans_MR_STAR, 
          beta,  C1_STAR_MC, D1Trans_STAR_MR, Ring(1), E );
    }
}

} // namespace trr2k
} // namespace El

#endif // ifndef EL_TRR2K_TNTT_HPP
