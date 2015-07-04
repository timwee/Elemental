/*
   Copyright (c) 2009-2015, Jack Poulson
   All rights reserved.

   Copyright (c) 2013, The University of Texas at Austin
   All rights reserved.

   This file is part of Elemental and is under the BSD 2-Clause License, 
   which can be found in the LICENSE file in the root directory, or at 
   http://opensource.org/licenses/BSD-2-Clause
*/
#pragma once
#ifndef EL_TRMM_LUT_HPP
#define EL_TRMM_LUT_HPP

namespace El {
namespace trmm {

template<typename Ring>
inline void
LocalAccumulateLUT
( Orientation orientation, UnitOrNonUnit diag, Ring alpha,
  const DistMatrix<Ring>& U,
  const DistMatrix<Ring,MC,STAR>& X,
        DistMatrix<Ring,MR,STAR>& Z )
{
    DEBUG_ONLY(
      CSE cse("trmm::LocalAccumulateLUT");
      AssertSameGrids( U, X, Z );
      if( U.Height() != U.Width() ||
          U.Height() != X.Height() ||
          U.Height() != Z.Height() )
          LogicError
          ("Nonconformal:\n",DimsString(U,"U"),"\n",DimsString(X,"X"),"\n",
           DimsString(Z,"Z"));
      if( X.ColAlign() != U.ColAlign() || Z.ColAlign() != U.RowAlign() )
          LogicError("Partial matrix distributions are misaligned");
    )
    const Int m = Z.Height();
    const Int bsize = Blocksize();
    const Grid& g = U.Grid();

    DistMatrix<Ring> D11(g);

    const Int ratio = Max( g.Height(), g.Width() );
    for( Int k=0; k<m; k+=ratio*bsize )
    {
        const Int nb = Min(ratio*bsize,m-k);

        auto U01 = U( IR(0,k),    IR(k,k+nb) );
        auto U11 = U( IR(k,k+nb), IR(k,k+nb) );

        auto X0 = X( IR(0,k),    ALL );
        auto X1 = X( IR(k,k+nb), ALL );
 
        auto Z1 = Z( IR(k,k+nb), ALL );

        D11.AlignWith( U11 );
        D11 = U11;
        MakeTrapezoidal( UPPER, D11 );
        if( diag == UNIT )
            FillDiagonal( D11, Ring(1) );
        LocalGemm( alpha, D11.Orient(orientation), X1.N(), Ring(1), Z1 );
        LocalGemm( alpha, U01.Orient(orientation), X0.N(), Ring(1), Z1 );
    }
}

template<typename Ring>
inline void
LUTA
( Orientation orientation, UnitOrNonUnit diag,
  const AbstractDistMatrix<Ring>& UPre, AbstractDistMatrix<Ring>& XPre )
{
    DEBUG_ONLY(
      CSE cse("trmm::LUTA");
      AssertSameGrids( UPre, XPre );
      if( orientation == NORMAL )
          LogicError("Expected (Conjugate)Transpose option");
      if( UPre.Height() != UPre.Width() || UPre.Height() != XPre.Height() )
          LogicError
          ("Nonconformal: \n",DimsString(UPre,"U"),"\n",DimsString(XPre,"X"))
    )
    const Int m = XPre.Height();
    const Int n = XPre.Width();
    const Int bsize = Blocksize();
    const Grid& g = UPre.Grid();

    auto UPtr = ReadProxy<Ring,MC,MR>( &UPre );      auto& U = *UPtr;
    auto XPtr = ReadWriteProxy<Ring,MC,MR>( &XPre ); auto& X = *XPtr;

    DistMatrix<Ring,MC,STAR> X1_MC_STAR(g);
    DistMatrix<Ring,MR,STAR> Z1_MR_STAR(g);
    DistMatrix<Ring,MR,MC  > Z1_MR_MC(g);

    X1_MC_STAR.AlignWith( U );
    Z1_MR_STAR.AlignWith( U );

    for( Int k=0; k<n; k+=bsize )
    {
        const Int nb = Min(bsize,n-k);

        auto X1 = X( ALL, IR(k,k+nb) );

        X1_MC_STAR = X1;
        Zeros( Z1_MR_STAR, m, nb );
        LocalAccumulateLUT
        ( orientation, diag, Ring(1), U, X1_MC_STAR, Z1_MR_STAR );

        Contract( Z1_MR_STAR, Z1_MR_MC );
        X1 = Z1_MR_MC;
    }
}

template<typename Ring>
inline void
LUTCOld
( Orientation orientation, UnitOrNonUnit diag,
  const AbstractDistMatrix<Ring>& UPre, AbstractDistMatrix<Ring>& XPre )
{
    DEBUG_ONLY(
        CSE cse("trmm::LUTCOld");
        AssertSameGrids( UPre, XPre );
        if( orientation == NORMAL )
            LogicError("Expected (Conjugate)Transpose option");
        if( UPre.Height() != UPre.Width() || UPre.Height() != XPre.Height() )
            LogicError
            ("Nonconformal: \n",DimsString(UPre,"U"),"\n",DimsString(XPre,"X"))
    )
    const Int m = XPre.Height();
    const Int n = XPre.Width();
    const Int bsize = Blocksize();
    const Grid& g = UPre.Grid();
    const bool conjugate = ( orientation == ADJOINT );

    auto UPtr = ReadProxy<Ring,MC,MR>( &UPre );      auto& U = *UPtr;
    auto XPtr = ReadWriteProxy<Ring,MC,MR>( &XPre ); auto& X = *XPtr;

    DistMatrix<Ring,MC,  STAR> U01_MC_STAR(g);
    DistMatrix<Ring,STAR,STAR> U11_STAR_STAR(g); 
    DistMatrix<Ring,STAR,VR  > X1_STAR_VR(g);
    DistMatrix<Ring,MR,  STAR> D1Trans_MR_STAR(g);
    DistMatrix<Ring,MR,  MC  > D1Trans_MR_MC(g);
    DistMatrix<Ring,MC,  MR  > D1(g);

    const Int kLast = LastOffset( m, bsize );
    for( Int k=kLast; k>=0; k-=bsize )
    {
        const Int nb = Min(bsize,m-k);

        auto U01 = U( IR(0,k),    IR(k,k+nb) );
        auto U11 = U( IR(k,k+nb), IR(k,k+nb) );

        auto X0 = X( IR(0,k),    ALL );
        auto X1 = X( IR(k,k+nb), ALL );

        X1_STAR_VR = X1;
        U11_STAR_STAR = U11;
        LocalTrmm
        ( LEFT, UPPER, orientation, diag, Ring(1), U11_STAR_STAR, X1_STAR_VR );
        X1 = X1_STAR_VR;
        
        U01_MC_STAR.AlignWith( X0 );
        U01_MC_STAR = U01;
        D1Trans_MR_STAR.AlignWith( X1 );
        LocalGemm
        ( Ring(1), X0.Orient(orientation), U01_MC_STAR.N(), D1Trans_MR_STAR );
        D1Trans_MR_MC.AlignWith( X1 );
        Contract( D1Trans_MR_STAR, D1Trans_MR_MC );
        D1.AlignWith( X1 );
        Zeros( D1, nb, n );
        Transpose( D1Trans_MR_MC.Matrix(), D1.Matrix(), conjugate );
        X1 += D1;
    }
}

template<typename Ring>
inline void
LUTC
( Orientation orientation, UnitOrNonUnit diag,
  const AbstractDistMatrix<Ring>& UPre, AbstractDistMatrix<Ring>& XPre )
{
    DEBUG_ONLY(
      CSE cse("trmm::LUTC");
      AssertSameGrids( UPre, XPre );
      if( orientation == NORMAL )
          LogicError("Expected (Conjugate)Transpose option");
      if( UPre.Height() != UPre.Width() || UPre.Height() != XPre.Height() )
          LogicError
          ("Nonconformal: \n",DimsString(UPre,"U"),"\n",DimsString(XPre,"X"))
    )
    const Int m = XPre.Height();
    const Int bsize = Blocksize();
    const Grid& g = UPre.Grid();

    auto UPtr = ReadProxy<Ring,MC,MR>( &UPre );      auto& U = *UPtr;
    auto XPtr = ReadWriteProxy<Ring,MC,MR>( &XPre ); auto& X = *XPtr;

    DistMatrix<Ring,STAR,MC  > U12_STAR_MC(g);
    DistMatrix<Ring,STAR,STAR> U11_STAR_STAR(g);
    DistMatrix<Ring,STAR,VR  > X1_STAR_VR(g);
    DistMatrix<Ring,MR,  STAR> X1Trans_MR_STAR(g);

    const Int kLast = LastOffset( m, bsize );
    for( Int k=kLast; k>=0; k-=bsize )
    {
        const Int nb = Min(bsize,m-k);

        auto U11 = U( IR(k,k+nb), IR(k,k+nb) );
        auto U12 = U( IR(k,k+nb), IR(k+nb,m) );

        auto X1 = X( IR(k,k+nb), ALL );
        auto X2 = X( IR(k+nb,m), ALL );

        U12_STAR_MC.AlignWith( X2 );
        U12_STAR_MC = U12;
        X1Trans_MR_STAR.AlignWith( X2 ); 
        Transpose( X1, X1Trans_MR_STAR );
        LocalGemm
        ( Ring(1), U12_STAR_MC.Orient(orientation), X1Trans_MR_STAR.T(), 
          Ring(1), X2 );

        U11_STAR_STAR = U11;
        X1_STAR_VR.AlignWith( X1 );
        Transpose( X1Trans_MR_STAR, X1_STAR_VR );
        LocalTrmm
        ( LEFT, UPPER, orientation, diag, Ring(1), U11_STAR_STAR, X1_STAR_VR );
        X1 = X1_STAR_VR;
    }
}

// Left Upper (Conjugate)Transpose (Non)Unit Trmm
//   X := triu(U)^T  X, 
//   X := triu(U)^H  X,
//   X := triuu(U)^T X, or
//   X := triuu(U)^H X
template<typename Ring>
inline void
LUT
( Orientation orientation, UnitOrNonUnit diag,
  const AbstractDistMatrix<Ring>& U, AbstractDistMatrix<Ring>& X )
{
    DEBUG_ONLY(CSE cse("trmm::LUT"))
    // TODO: Come up with a better routing mechanism
    if( U.Height() > 5*X.Width() )
        LUTA( orientation, diag, U, X );
    else
        LUTC( orientation, diag, U, X );
}

} // namespace trmm
} // namespace El

#endif // ifndef EL_TRMM_LUT_HPP
