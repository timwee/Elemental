/*
   Copyright (c) 2009-2015, Jack Poulson
   All rights reserved.

   This file is part of Elemental and is under the BSD 2-Clause License, 
   which can be found in the LICENSE file in the root directory, or at 
   http://opensource.org/licenses/BSD-2-Clause
*/

namespace El {
namespace syrk {

template<typename Ring>
inline void
LT
( Ring alpha, const AbstractDistMatrix<Ring>& APre, 
                    AbstractDistMatrix<Ring>& CPre, bool conjugate=false )
{
    DEBUG_ONLY(
      CSE cse("syrk::LT");
      AssertSameGrids( APre, CPre );
      if( APre.Width() != CPre.Height() || APre.Width() != CPre.Width() )
          LogicError
          ("Nonconformal:\n",DimsString(APre,"A"),"\n",DimsString(CPre,"C"))
    )
    const Int r = APre.Height();
    const Int bsize = Blocksize();
    const Grid& g = APre.Grid();
    const Orientation orientation = ( conjugate ? ADJOINT : TRANSPOSE );

    auto APtr = ReadProxy<Ring,MC,MR>( &APre );      auto& A = *APtr;
    auto CPtr = ReadWriteProxy<Ring,MC,MR>( &CPre ); auto& C = *CPtr;

    // Temporary distributions
    DistMatrix<Ring,MR,  STAR> A1Trans_MR_STAR(g);
    DistMatrix<Ring,STAR,VR  > A1_STAR_VR(g);
    DistMatrix<Ring,STAR,MC  > A1_STAR_MC(g);

    A1Trans_MR_STAR.AlignWith( C );
    A1_STAR_MC.AlignWith( C );

    for( Int k=0; k<r; k+=bsize )
    {
        const Int nb = Min(bsize,r-k);
        auto A1 = A( IR(k,k+nb), ALL );

        Transpose( A1, A1Trans_MR_STAR );
        Transpose( A1Trans_MR_STAR, A1_STAR_VR );
        A1_STAR_MC = A1_STAR_VR;

        LocalTrrk
        ( LOWER, orientation, TRANSPOSE, 
          alpha, A1_STAR_MC, A1Trans_MR_STAR, Ring(1), C );
    }
}

} // namespace syrk
} // namespace El
