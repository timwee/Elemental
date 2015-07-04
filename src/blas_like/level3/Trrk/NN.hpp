/*
   Copyright (c) 2009-2015, Jack Poulson
   All rights reserved.

   This file is part of Elemental and is under the BSD 2-Clause License, 
   which can be found in the LICENSE file in the root directory, or at 
   http://opensource.org/licenses/BSD-2-Clause
*/
#ifndef EL_TRRK_NN_HPP
#define EL_TRRK_NN_HPP

namespace El {
namespace trrk {

// Distributed C := alpha A B + C
template<typename Ring>
void TrrkNN
( UpperOrLower uplo,
  Ring alpha, const AbstractDistMatrix<Ring>& APre,
              const AbstractDistMatrix<Ring>& BPre,
                    AbstractDistMatrix<Ring>& CPre )
{
    DEBUG_ONLY(
      CSE cse("trrk::TrrkNN");
      if( CPre.Height() != CPre.Width() || APre.Height() != CPre.Height() || 
          BPre.Width() != CPre.Width() || APre.Width() != BPre.Height() )
          LogicError("Nonconformal TrrkNN");
    )
    const Int r = APre.Width();
    const Int bsize = Blocksize();
    const Grid& g = CPre.Grid();

    auto APtr = ReadProxy<Ring,MC,MR>( &APre );      auto& A = *APtr;
    auto BPtr = ReadProxy<Ring,MC,MR>( &BPre );      auto& B = *BPtr;
    auto CPtr = ReadWriteProxy<Ring,MC,MR>( &CPre ); auto& C = *CPtr;

    DistMatrix<Ring,MC,STAR> A1_MC_STAR(g);
    DistMatrix<Ring,MR,STAR> B1Trans_MR_STAR(g);

    A1_MC_STAR.AlignWith( C );
    B1Trans_MR_STAR.AlignWith( C );

    for( Int k=0; k<r; k+=bsize )
    {
        const Int nb = Min(bsize,r-k);

        const Range<Int> ind1( k, k+nb );

        auto A1 = A( ALL,  ind1 );
        auto B1 = B( ind1, ALL  );

        A1_MC_STAR = A1;
        Transpose( B1, B1Trans_MR_STAR );
        LocalTrrk
        ( uplo, TRANSPOSE, alpha, A1_MC_STAR, B1Trans_MR_STAR, Ring(1), C );
    }
}

} // namespace trrk
} // namespace El

#endif // ifndef EL_TRRK_NN_HPP
