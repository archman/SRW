/************************************************************************//**
 * File: srmagfld.cpp
 * Description: Magnetic elements
 * Project: Synchrotron Radiation Workshop
 * First release: 2000
 *
 * Copyright (C) European Synchrotron Radiation Facility, Grenoble, France
 * Portions Copyright (C) European XFEL, Hamburg, Germany
 * All Rights Reserved
 *
 * @author O.Chubar, P.Elleaume
 * @version 0.06
 ***************************************************************************/

#include "srmagfld.h"
#include "srtrjdat.h"
#include "srptrjdt.h"
#include "srtrjdat3d.h"
//#include "srmamet.h"
#include "gminterp.h"
#include "gmfunc.h"
#include "gmfft.h"
#include "srpersto.h"
#include "srthckbm.h"
#include "srmagcnt.h"
#include "srwlib.h"
#include <algorithm>

//*************************************************************************

extern srTIntVect gVectWarnNos;

//*************************************************************************

int srTMagElemSummary::SetupMagElement(srTStringVect* pMagElemInfo, CHMagFld& MagElemHndl)
{
	char* ElemID = (*pMagElemInfo)[0];

	if((!strcmp(ElemID, "Quadrupole")) || (!strcmp(ElemID, "quadrupole")) || (!strcmp(ElemID, "QUADRUPOLE")))
	{
		//MagElemHndl = CHMagFld(new srTMagQuad(pMagElemInfo));
		MagElemHndl = CHMagFld(new srTMagMult(pMagElemInfo));
	}
	else if((!strcmp(ElemID, "Chicane")) || (!strcmp(ElemID, "chicane")) || (!strcmp(ElemID, "CHICANE")))
	{
		MagElemHndl = CHMagFld(new srTMagChicane(pMagElemInfo));
	}
	// Continue for new Optical Elements

	else return UNKNOWN_MAGNET_ELEMENT;

	if(MagElemHndl.rep->ErrorCode != 0) return MagElemHndl.rep->ErrorCode;

	return 0;
}

//*************************************************************************

int srTMagElem::FindMagElemWithSmallestLongPos(CObjCont<CGenObject>& AuxCont)
{
	if(AuxCont.size() <= 0) return 0;
	int ElemInd = 0;
	double MinLongPos = 1.E+23;
	for(CMHGenObj::const_iterator iter = AuxCont.data.begin(); iter != AuxCont.data.end(); ++iter)
	{
        srTMagElem* pMag = dynamic_cast<srTMagElem*>(((*iter).second).rep);
		if(pMag == 0) continue;

		if(MinLongPos > pMag->gsStart) 
		{
			MinLongPos = pMag->gsStart;
			ElemInd = (*iter).first;
		}
	}
	return ElemInd;
}

//*************************************************************************

srTMagFieldPeriodic::srTMagFieldPeriodic(double Per, double L, double In_sCen, srTMagHarm* HarmArr, int nHarm, char Type, double SpecPar)
{
	if(Per <= 0) throw INCORRECT_MAG_FIELD_PER;
	if(L <= 0) throw INCORRECT_PER_MAG_FIELD_LENGTH;
	//if((HarmArr == 0) || (nHarm == 0)) throw INCORRECT_NUMBER_OF_HARMONICS;

	PerLength = Per;
	TotLength = L;
	AmOfHarm = nHarm; 

	//double dHalfPer = L/(0.5*PerLength);
	//int iHalfPer = (int)dHalfPer;
	//double dif = dHalfPer - (double)iHalfPer;
	//if(dif >= 0.5) iHalfPer++;
	//FieldLongSym = (((iHalfPer >> 1) << 1) == iHalfPer)? -1 : 1;

	sCen = In_sCen;	//to implement in relevant methods!

	if(Type == 'i') TypeOfUnd = 0;
	else if(Type == 't') { TypeOfUnd = 2; TaperPar_TU = SpecPar;}
	else if(Type == 'k') { TypeOfUnd = 3; PhaseSh_OK = SpecPar;}
	else TypeOfUnd = 1;
	//TypeOfUnd; // 0- infinite, 1- normal, 2- tapered, 3- optical klystron

	if((HarmArr != 0) && (nHarm > 0))
	{
		if(!HarmVect.empty()) HarmVect.erase(HarmVect.begin(), HarmVect.end());
		for(int k=0; k<nHarm; k++)
		{//fill-in srTMagHarmVect HarmVect
			HarmVect.push_back(HarmArr[k]);
		}
	}
	gsStart = In_sCen - 0.5*L;
    gsEnd = In_sCen + 0.5*L;
}

//*************************************************************************

srTMagFieldPeriodic::srTMagFieldPeriodic(const SRWLMagFldU& inUnd, const TVector3d& inCenP) : srTMagElem(inCenP) //SRWLIB
{//for SRWLIB
	PerLength = inUnd.per;
	if(PerLength <= 0) throw INCORRECT_MAG_FIELD_PER;
	if(inUnd.nPer <= 0) throw INCORRECT_PER_MAG_FIELD_LENGTH;

	//number of periods is approximated by demi-integer
	//double dHalfPer = inUnd.nPer/0.5;
	//int iHalfPer = (int)dHalfPer;
	//double dif = dHalfPer - (double)iHalfPer;
	//if(dif >= 0.5) iHalfPer++;

	int iHalfPer = 2*inUnd.nPer;

	//FieldLongSym = (((iHalfPer >> 1) << 1) == iHalfPer)? -1 : 1;

	TotLength = iHalfPer*inUnd.per*0.5;
	if(TotLength <= 0) throw INCORRECT_PER_MAG_FIELD_LENGTH;

	AmOfHarm = inUnd.nHarm; 
	sCen = inCenP.z; //needed?
	TypeOfUnd = 1; //only "regular" undulator is supported from this entry

	double halfTotLenWithTerm = 0.5*TotLength + 4*PerLength;
	gsStart = inCenP.z - halfTotLenWithTerm; //OC01302011
	gsEnd = inCenP.z + halfTotLenWithTerm;

	if((inUnd.arHarm != 0) && (AmOfHarm > 0))
	{
		if(!HarmVect.empty()) HarmVect.erase(HarmVect.begin(), HarmVect.end());

		SRWLMagFldH *t_FldH = inUnd.arHarm;
		for(int k=0; k<AmOfHarm; k++)
		{//fill-in srTMagHarmVect HarmVect
			char cHorV = t_FldH->h_or_v;
			if((cHorV == 'h') || (cHorV == 'H')) cHorV = 'x';
			else if((cHorV == 'v') || (cHorV == 'V')) cHorV = 'z';

			double curK = 93.37290417576577*PerLength*t_FldH->B; //lengths are in [m]

			srTMagHarm curHarm(t_FldH->n, cHorV, curK, t_FldH->ph, t_FldH->s, t_FldH->a);

			HarmVect.push_back(curHarm);
			t_FldH++;
		}
	}
}

//*************************************************************************

void srTMagFieldPeriodic::SetupWigSASE(srTWigComSASE& WigCom) //sets up SASE wiggler for Genesis
{
	//double aw0, delaw, xlamd, fbess, wcoefz[3], awd, awdr, quadf, quadd, fbess0, qfdx, qfdy, fl, dl, drl, f1st, xkx, xky;
	//long iseed, iwityp, iertyp, iseed0;
	//long simcom_nwig;

	if(AmOfHarm <= 0) return;

	char IsPlanar = (AmOfHarm == 1);
	//char FocusingIsNatural = (NatFocTypeSASE == 0);

	WigCom.aw0 = IsPlanar? HarmVect[0].K/sqrt(2.) : HarmVect[0].K; 
	//for helical und: Kz = Kx = HarmVect[0].K, K = sqrt(Kz^2 + Kx^2), aw0 = K/sqrt(2.), i.e. same normalization as for planar

	WigCom.awd = WigCom.aw0;
	WigCom.iwityp = IsPlanar? 0 : 1;
	WigCom.xlamd = PerLength;
	WigCom.simcom_nwig = int(TotLength/PerLength);

	//WigCom.xkx = FocusingIsNatural? (IsPlanar? 0 : 0.5) : NatFocNxSASE;
	//WigCom.xky = 1 - WigCom.xkx;
	WigCom.xkx = NatFocNxSASE;
	WigCom.xky = NatFocNySASE;

	WigCom.fbess0 = 0; // automatic calculation by Genesis

	WigCom.delaw = (FldErrTypeSASE > 0)? FldErrRMS : 0;
	if(FldErrTypeSASE == 0) WigCom.iertyp = 0;
	else if(FldErrTypeSASE == 1) WigCom.iertyp = 1;
	else if(FldErrTypeSASE == 2) WigCom.iertyp = -1;
	else if(FldErrTypeSASE == 3) WigCom.iertyp = 2;
	else if(FldErrTypeSASE == 4) WigCom.iertyp = -2;
	// 0- No errors; 1- Uniform uncorrelated; 2- Uniform correlated; 3- Gaussian uncorrelated; 4- Gaussian correlated

	WigCom.wcoefz[0] = TaperStartSASE; // re-consider if 0 is at the end of undulator
	WigCom.wcoefz[1] = TaperRelFldChgSASE; 
	WigCom.wcoefz[2] = TaperTypeSASE; // 0- No taper; 1- Linear; 2- Quadratic

	WigCom.UndulatorJustPassed = 1;
}

//*************************************************************************

srTGenTrjDat* srTMagFieldPeriodic::CreateAndSetupNewTrjDat(srTEbmDat* pEbmDat)
{
	srTPerTrjDat* pOut = new srTPerTrjDat();
	pOut->MagPer = *this;
	if(pEbmDat != 0) pOut->EbmDat = *pEbmDat;
	pOut->CheckIfHorOrVertFieldIsZero();
	return pOut;
}

//*************************************************************************

void srTMagFieldPeriodic::ComputeSR_Stokes(srTEbmDat* pElecBeam, srTWfrSmp* pWfrSmp, void* pPrcPar, srTStokesStructAccessData* pStokes)
{
    srTRadIntPeriodic::ComputeStokes(pElecBeam, this, pWfrSmp, pPrcPar, pStokes);
}

//*************************************************************************

srTMagFldTrUnif* srTMagFieldPeriodic::CreateAndSetupMagFldTrUnif()
{
	//to implement

	return 0;
}

//*************************************************************************

//void srTMagQuad::SetupWigSASE(srTWigComSASE& WigCom) //sets up SASE wiggler for Genesis
void srTMagMult::SetupWigSASE(srTWigComSASE& WigCom) //sets up SASE wiggler for Genesis
{
	//double aw0, delaw, xlamd, fbess, wcoefz[3], awd, awdr, quadf, quadd, fbess0, qfdx, qfdy, fl, dl, drl, f1st, xkx, xky;
	//long iseed, iwityp, iertyp, iseed0;
	//long simcom_nwig;

	char IsFocusing = (Strength > 0);
	char WigPeriodIsKnown = (WigCom.xlamd > 0);
	//continue here: define lengthes in units of und. period

	if(IsFocusing)
	{
		WigCom.quadf = Strength;
		WigCom.fl = Length;
		if(WigPeriodIsKnown) WigCom.fl /= WigCom.xlamd; // check necessity of integer number of steps !
	}
	else
	{
		WigCom.quadd = -Strength;
		WigCom.dl = Length;
		if(WigPeriodIsKnown) WigCom.dl /= WigCom.xlamd;
	}

	m = 2; //to ensure that this is quad

	WigCom.qfdx = mCenP.x; //TransvCenPoint.x;
	WigCom.qfdy = mCenP.y; //TransvCenPoint.y;
}

//*************************************************************************

void srTMagGroup::SetupWigSASE(srTWigComSASE& WigCom)
{
	//double aw0, delaw, xlamd, fbess, wcoefz[3], awd, awdr, quadf, quadd, fbess0, qfdx, qfdy, fl, dl, drl, f1st, xkx, xky;
	//long iseed, iwityp, iertyp, iseed0;
	//long simcom_nwig;

	int AmOfElem = (int)PosAndElemVect.size();
	if(AmOfElem <= 0) return;

	const double DefaultNonSetVal = -1.E+23;

	double sStartFirstFocQuad = DefaultNonSetVal, sStartFirstDeFocQuad = DefaultNonSetVal;
	double sStartFirstUndSect, sStartSecondUndSect;
	bool FirstUndSectAlreadyPassed = false, SecondUndSectAlreadyPassed = false;
	bool FODO_AlreadyStarted = false;
	int AmOfUndSections = 0;
	for(int i=0; i<AmOfElem; i++)
	{
		srTMagPosAndElem PosAndElem = PosAndElemVect[i];
		srTMagElem *pMagElem = PosAndElem.MagHndl.rep;
		if(pMagElem == 0) continue;

		bool flWasZero = (WigCom.fl == 0);
		bool dlWasZero = (WigCom.dl == 0);

		double sStart = PosAndElem.s;

		WigCom.UndulatorJustPassed = 0;
		pMagElem->SetupWigSASE(WigCom);

		if(WigCom.UndulatorJustPassed) 
		{
			if(FirstUndSectAlreadyPassed) 
			{
				if(!SecondUndSectAlreadyPassed)
				{
					sStartSecondUndSect = sStart;
					SecondUndSectAlreadyPassed = true;
				}
			}
			else
			{
				sStartFirstUndSect = sStart;
			}

			FirstUndSectAlreadyPassed = true;
			AmOfUndSections++;
		}

		bool flIsZero = (WigCom.fl == 0);
		bool dlIsZero = (WigCom.dl == 0);

		bool FocQuadWasJustSet = (flWasZero && (!flIsZero));
		bool DeFocQuadWasJustSet = (dlWasZero && (!dlIsZero));

		if(FocQuadWasJustSet) sStartFirstFocQuad = sStart; 
		if(DeFocQuadWasJustSet) sStartFirstDeFocQuad = sStart;

		if(FocQuadWasJustSet || DeFocQuadWasJustSet) FODO_AlreadyStarted = true;
	}

	bool WigPeriodIsKnown = (WigCom.xlamd > 0);

	if((sStartFirstFocQuad != DefaultNonSetVal) && (sStartFirstDeFocQuad != DefaultNonSetVal))
	{
		if(sStartFirstFocQuad < sStartFirstDeFocQuad)
		{
			if(WigPeriodIsKnown) WigCom.drl = sStartFirstDeFocQuad - (sStartFirstFocQuad + WigCom.fl*WigCom.xlamd);
		}
		else
		{
			if(WigPeriodIsKnown) WigCom.drl = sStartFirstFocQuad - (sStartFirstDeFocQuad + WigCom.dl*WigCom.xlamd);
		}
	}

	if(WigPeriodIsKnown) WigCom.drl /= WigCom.xlamd;

	if(FirstUndSectAlreadyPassed && SecondUndSectAlreadyPassed && WigPeriodIsKnown)
	{
		double GapLen = sStartSecondUndSect - sStartFirstUndSect - WigCom.xlamd*WigCom.simcom_nwig;
		
		if((WigCom.fl == 0) && (WigCom.dl == 0) && (GapLen != 0)) 
		{//Since GENESIS aligns each next wiggler section with FODO lattice, 
		 //ensuring longitudinal gap between neighbouring wiggler sections 
		 //is only possible if there are no any quadrupoles.

			////int iDRL = GapLen/WigCom.xlamd;
			//int iDRL = (int)(0.5*GapLen/WigCom.xlamd);
			////if(fabs(GapLen - iDRL*WigCom.xlamd) > 0.5) iDRL++;
			//WigCom.drl = iDRL;

			WigCom.drl = 0.5*(GapLen*0.9999 + WigCom.xlamd*WigCom.simcom_nwig)/WigCom.xlamd;
			//to ensure correct gap between sections
		}
		//WigCom.f1st = 0.5*((WigCom.fl + WigCom.dl + WigCom.drl)*WigCom.xlamd - GapLen)/WigCom.xlamd;
		WigCom.f1st = 0.; //0.5*((WigCom.fl + WigCom.dl + WigCom.drl)*WigCom.xlamd - GapLen)/WigCom.xlamd;
		//to make available from interface

		WigCom.GapLen = GapLen;
	}
	if(FirstUndSectAlreadyPassed && FODO_AlreadyStarted && WigPeriodIsKnown)
	{
		WigCom.f1st = (sStartFirstUndSect - sStartFirstFocQuad)/WigCom.xlamd;
	}

	WigCom.simcom_nsec = AmOfUndSections;
}

//*************************************************************************

void srTMagFldTrUnif::SetupTrjDat(srTTrjDat* pTrjDat)
{
	if(pTrjDat == 0) return;
	if(Np <= 0) return;
	if((BxArr == 0) && (BzArr == 0)) return;

	pTrjDat->LenFieldData = Np;
	pTrjDat->sStep = sStep;
	pTrjDat->sStart = sStart;

	srTFunDer*& BxInData = pTrjDat->BxInData;
	srTFunDer*& BzInData = pTrjDat->BzInData;
	double& FieldZeroTolerance = pTrjDat->FieldZeroTolerance;
	bool BxIsZero = true, BzIsZero = true;

	if(BxArr != 0)
	{
		if(BxInData != 0) delete[] BxInData;
		BxInData = new srTFunDer[Np];

		srTFunDer* tBxInData = BxInData;
		double *tBx = BxArr, BxVal;
		for(int k=0; k<Np; k++)
		{
			BxVal = *(tBx++);

			if(::fabs(BxVal) > FieldZeroTolerance) BxIsZero = false;
			else BxVal = 0.;
			(tBxInData++)->f = BxVal;
		}
	}
	if(BzArr != 0)
	{
		if(BzInData != 0) delete[] BzInData;
		BzInData = new srTFunDer[Np];

		srTFunDer* tBzInData = BzInData;
		double *tBz = BzArr, BzVal;
		for(int k=0; k<Np; k++)
		{
			BzVal = *(tBz++);

			if(::fabs(BzVal) > FieldZeroTolerance) BzIsZero = false;
			else BzVal = 0.;
			(tBzInData++)->f = BzVal;
		}
	}

	pTrjDat->HorFieldIsNotZero = !BxIsZero;
	pTrjDat->VerFieldIsNotZero = !BzIsZero;
}

//*************************************************************************

srTGenTrjDat* srTMagFldTrUnif::CreateAndSetupNewTrjDat(srTEbmDat* pEbmDat) 
{ 
	srTTrjDat* pOut = new srTTrjDat();
	if(pEbmDat != 0) pOut->EbmDat = *pEbmDat;
	SetupTrjDat(pOut);
	int res = 0;
	if(res = pOut->ComputeInterpolatingStructure()) throw res;

	return pOut;
}

//*************************************************************************

void srTMagFldTrUnif::ComputeSR_Stokes(srTEbmDat* pElecBeam, srTWfrSmp* pWfrSmp, void* pPrcPar, srTStokesStructAccessData* pStokes)
{
	pStokes->UpdateLongitudinalGridParams(pWfrSmp->yStart, ((pWfrSmp->ny > 1)? (pWfrSmp->yEnd - pWfrSmp->yStart)/(pWfrSmp->ny - 1) : 0), pWfrSmp->ny); // to avoid passing pWfrSmp to ComputeStokes

	srTParPrecStokesArb *pPrc = (srTParPrecStokesArb*)pPrcPar;
	if(pPrc->IntOrFlux == 'f')
	{
        double dx=0, dz=0;
		pWfrSmp->OutRangesForFluxComp(dx, dz);
		pStokes->UpdateSlitDimensions(dx, dz);
	}

	srTRadIntThickBeam::ComputeStokes(pElecBeam, this, 0, pPrc, pStokes);
}

//*************************************************************************

srTMagFieldPeriodic* srTMagFldTrUnif::CreateAndSetupMagFieldPeriodic(double RelPrec, int MaxHarm, double MaxPerLen_m)
{
	bool HorFieldIsDefined = ((BxArr != 0) && (Np > 0));
	bool VertFieldIsDefined = ((BzArr != 0) && (Np > 0));
	if((!HorFieldIsDefined) && (!VertFieldIsDefined)) return 0;

	const double AbsFldTol = 1.E-06; //[T]
	const double RelFldTolForPerSearch = 2.E-01; //1.E-02; //OC190112

	double Per_HorFld = 0, Per_VertFld = 0, L_HorFld = 0, L_VertFld = 0, sCen_HorFld = 0, sCen_VertFld = 0;
	double sStartPer_HorFld = 0, sStartPer_VertFld = 0;
	double MaxAbsBx = 0., MaxAbsBz = 0.;

	if(HorFieldIsDefined)
	{
		MaxAbsBx = FindMaxAbsVal(BxArr, Np);
		if(MaxAbsBx > AbsFldTol)
		{
            FindBasicFieldPeriodicParam(BxArr, Np, sStart, sStep, RelFldTolForPerSearch*MaxAbsBx, Per_HorFld, L_HorFld, sCen_HorFld, sStartPer_HorFld);
		}
		else HorFieldIsDefined = false;
	}
	if(VertFieldIsDefined)
	{
		MaxAbsBz = FindMaxAbsVal(BzArr, Np);
		if(MaxAbsBz > AbsFldTol)
		{
            FindBasicFieldPeriodicParam(BzArr, Np, sStart, sStep, RelFldTolForPerSearch*MaxAbsBz, Per_VertFld, L_VertFld, sCen_VertFld, sStartPer_VertFld);
		}
		else VertFieldIsDefined = false;
	}
	double Per = MaxPerLen_m, L = 0, sCen = 0, sStartPer = 0;
	ChooseDominantBasicFieldPeriodicParam(Per_HorFld, L_HorFld, sCen_HorFld, sStartPer_HorFld, MaxAbsBx, Per_VertFld, L_VertFld, sCen_VertFld, sStartPer_VertFld, MaxAbsBz, Per, L, sCen, sStartPer);

    srTMagHarm *MagHarmArr_HorFld = 0, *MagHarmArr_VertFld = 0;
	int NumHarm_HorFld = 0, NumHarm_VertFld = 0;

	if(HorFieldIsDefined)
	{
		NumHarm_HorFld = MaxHarm;
		FindFieldHarmonics(BxArr, Np, sStart, sStep, Per, sStartPer, RelPrec, 'x', NumHarm_HorFld, MagHarmArr_HorFld);
	}
	if(VertFieldIsDefined)
	{
		NumHarm_VertFld = MaxHarm;
		FindFieldHarmonics(BzArr, Np, sStart, sStep, Per, sStartPer, RelPrec, 'z', NumHarm_VertFld, MagHarmArr_VertFld);
	}

	srTMagHarm *TotHarmArr = 0;
	int TotAmOfHarm = 0;
	SumUpFieldHarmonics(MagHarmArr_HorFld, NumHarm_HorFld, MagHarmArr_VertFld, NumHarm_VertFld, TotHarmArr, TotAmOfHarm);

	srTMagFieldPeriodic* pMagFieldPeriodic = new srTMagFieldPeriodic(Per, L, sCen, TotHarmArr, TotAmOfHarm, 0, 0);

	if(MagHarmArr_HorFld != 0) delete[] MagHarmArr_HorFld;
	if(MagHarmArr_VertFld != 0) delete[] MagHarmArr_VertFld;
	if(TotHarmArr != 0) delete[] TotHarmArr;

	return pMagFieldPeriodic;
}

//*************************************************************************

void srTMagFldTrUnif::FindBasicFieldPeriodicParam(double* pB, int nB, double sInit, double sDelta, double absTolB, double& Per, double& L, double& sCen, double& sStartOnePer)
{// Improve the procedure of finding proper period !!!
	Per = 0;
	if((pB == 0) || (nB <= 0)) return;

	const int MaxAmOfZeros = 50000;
	double ArgFldZerosIncr[MaxAmOfZeros], ArgFldZerosDecr[MaxAmOfZeros];
	int AmOfZeros = MaxAmOfZeros;
	FindFieldZeros(pB, nB, sInit, sDelta, absTolB, ArgFldZerosIncr, ArgFldZerosDecr, AmOfZeros);
	if(AmOfZeros <= 1) return;

	FindOnePeriod(ArgFldZerosIncr, AmOfZeros, sStartOnePer, Per);
	if(Per <= 0.) return;

	L = ArgFldZerosIncr[AmOfZeros - 1] - *ArgFldZerosIncr; // to improve?
	sCen = *ArgFldZerosIncr + 0.5*L;
}

//*************************************************************************

void srTMagFldTrUnif::FindFieldHarmonics(double* pB, int nB, double sInit, double sDelta, double Per, double sStartOnePer, double RelPrec, char XorZ, int& NumHarm, srTMagHarm*& MarHarmArr)
{
	if((pB == 0) || (nB <= 0)) return;

	const int AmOfInterpolPoints = 128; //100;
	double OnePerB[AmOfInterpolPoints];
	InterpolateOnePeriodData(pB, nB, sInit, sDelta, sStartOnePer, Per, OnePerB, AmOfInterpolPoints);
	RotateOnePeriodData(OnePerB, AmOfInterpolPoints);
	AnalyzeForHarmonics(OnePerB, AmOfInterpolPoints, Per, RelPrec, XorZ, NumHarm, MarHarmArr);
}

//*************************************************************************

void srTMagFldTrUnif::FindFieldZeros(double* pB, int nB, double sStart, double sStep, double absTolB, double* ArgFldZerosIncr, double* ArgFldZerosDecr, int& AmOfZeros)
{
	if((pB == 0) || (nB <= 0)) return;

	double s = sStart;
	double *tB = pB, *tZerosIncr = ArgFldZerosIncr, *tZerosDecr = ArgFldZerosDecr;
	double PrevB, PrevS;
	int ZerosIncrCount = 0, ZerosDecrCount = 0;
	bool FldWasPositive = false, ZerosIncrFilled = false, ZerosDecrFilled = false, FldWasZero = false;

	for(int i=0; i<nB; i++)
	{
		//bool FldIsPositive = (*tB > 0.);
		bool FldIsZero = (*tB == 0.);
		bool FldIsPositive = (*tB > absTolB); 
		//to make it insensitive to noise of mag. measurements
		//check possible negative consequences!
		//bool FldIsZero = (::fabs(*tB) <= absTolB);

		if(i > 0)
		{
			if(FldIsPositive)
			{
				if((!FldWasPositive) && (!FldWasZero))
				{
					if(ZerosIncrCount < AmOfZeros) 
					{
						*(tZerosIncr++) = IntersectLineWithZero(PrevS, s, PrevB, *tB);
						ZerosIncrCount++;
					}
					else
					{
						if(ZerosDecrFilled) break;
						ZerosIncrFilled = true;
					}
				}
			}
			else
			{
				if(FldWasPositive && (!FldIsZero))
				{
                    if(ZerosDecrCount < AmOfZeros) 
					{
						*(tZerosDecr++) = IntersectLineWithZero(PrevS, s, PrevB, *tB);
						ZerosDecrCount++;
					}
					else
					{
						if(ZerosIncrFilled) break;
						ZerosDecrFilled = true;
					}
				}
			}
		}
		PrevB = *(tB++); 
		PrevS = s;
		FldWasPositive = FldIsPositive;
		FldWasZero = FldIsZero;
		s += sStep;
	}
	AmOfZeros = 0;
	if((ZerosIncrCount > 0) && (ZerosDecrCount > 0))
	{
        AmOfZeros = ZerosIncrCount;
        if(AmOfZeros > ZerosDecrCount) AmOfZeros = ZerosDecrCount;
	}
}

//*************************************************************************

void srTMagFldTrUnif::FindOnePeriod(double* ArgFldZeros, int AmOfZeros, double& sStartOnePer, double& Per)
{
	sStartOnePer = Per = 0.;
	if((ArgFldZeros == 0) || (AmOfZeros <= 1)) return;
	if(AmOfZeros == 2)
	{
		sStartOnePer = *ArgFldZeros;
		Per = *(ArgFldZeros + 1) - *ArgFldZeros;
		return;
	}

	int iMinDif = -1;
	double MinDifVal = ::fabs(*(ArgFldZeros + (AmOfZeros - 1)) - *ArgFldZeros);

	for(int i=2; i<AmOfZeros; i++)
	{
		double* pZeros = (ArgFldZeros + (i - 2));
		double xm2 = *(pZeros++);
		double xm1 = *(pZeros++);
		double x0 = *pZeros;

		double CurDif = ::fabs(x0 - 2*xm1 + xm2);
		if(MinDifVal > CurDif)
		{
            MinDifVal = CurDif;
            iMinDif = i;
		}
	}
	if(iMinDif < 0) return;
	sStartOnePer = ArgFldZeros[iMinDif - 1];
    Per = ArgFldZeros[iMinDif] - sStartOnePer;
}

//*************************************************************************

void srTMagFldTrUnif::InterpolateOnePeriodData(double* pB, int nB, double sInit, double sDelta, double sStartOnePer, double Per, double* InterpB, int AmOfInterpPts)
{
	if((pB == 0) || (nB <= 1) || (sDelta == 0.) || (Per <= 0.) || (InterpB == 0) || (AmOfInterpPts <= 0)) return;

	int IndOrigStart = int((sStartOnePer - sInit)/sDelta) - 3;
	if(IndOrigStart >= (nB - 1)) IndOrigStart = nB - 2;
	if(IndOrigStart < 0) IndOrigStart = 0;

	int IndOrigEnd = int(((sStartOnePer + Per) - sInit)/sDelta) + 3;
	if(IndOrigEnd >= nB) IndOrigEnd = nB - 1;
	if(IndOrigEnd < 1) IndOrigEnd = 1;

	//int ActNp = IndOrigEnd - IndOrigStart;
	int ActNp = IndOrigEnd - IndOrigStart + 1; //OC210403 ????;

	if(ActNp <= 0) ActNp = 1;

	double Act_sStart = sInit + IndOrigStart*sDelta;
	double* pB_Start = pB + IndOrigStart;

	double sStepInterp = Per/double(AmOfInterpPts);

	//srTMathInterpol1D* pMathInterpol1D = new srTMathInterpol1D(pB_Start, ActNp, Act_sStart, sDelta);
	CGenMathInterp* pMathInterpol1D = new CGenMathInterp(pB_Start, ActNp, Act_sStart, sDelta);
	pMathInterpol1D->Interpolate(sStartOnePer, sStepInterp, AmOfInterpPts, InterpB);

	delete pMathInterpol1D;
}

//*************************************************************************

void srTMagFldTrUnif::RotateOnePeriodData(double* InterpB, int AmOfInterpPts)
{//Ensures Cos-like data layout
	if((InterpB == 0) || (AmOfInterpPts <= 0)) return;

	int AmOfPtsQuarter = AmOfInterpPts >> 2;
	int iStartMove = AmOfInterpPts - AmOfPtsQuarter;

	double* AuxArr = new double[AmOfInterpPts];

	double *tAuxArr = AuxArr, *tOrigArr = InterpB + iStartMove;
	for(int i=0; i<AmOfPtsQuarter; i++) *(tAuxArr++) = *(tOrigArr++);

	tOrigArr = InterpB;
	for(int j=0; j<3*AmOfPtsQuarter; j++) *(tAuxArr++) = *(tOrigArr++);

	tAuxArr = AuxArr;
	tOrigArr = InterpB;
	for(int k=0; k<AmOfInterpPts; k++) *(tOrigArr++) = *(tAuxArr++);

	delete[] AuxArr;
}

//*************************************************************************

void srTMagFldTrUnif::AnalyzeForHarmonics(double* pB, int AmOfPts, double Per, double RelPrec, char XorZ, int& AmOfHarm, srTMagHarm*& MagHarmArr)
{
	if((pB == 0) || (AmOfPts <= 0) || (Per <= 0) || (RelPrec <= 0)) return;

	float *AuxDataContIn=0, *AuxDataContOut=0;
	double *CkArr=0, *PhikArr=0;
	int *HarmNoArr=0;

	AuxDataContIn = new float[AmOfPts << 1];
	if(AuxDataContIn == 0) throw MEMORY_ALLOCATION_FAILURE;

	AuxDataContOut = new float[AmOfPts << 1];
	if(AuxDataContOut == 0) throw MEMORY_ALLOCATION_FAILURE;

	double *tB = pB;
	float *tIn = AuxDataContIn;
	double MaxAbsB = 0;
	for(int i=0; i<AmOfPts; i++) 
	{
		double CurB = *(tB++);
		*(tIn++) = (float)CurB; *(tIn++) = 0.;
		
		double CurAbsB = ::fabs(CurB);
		if(MaxAbsB < CurAbsB) MaxAbsB = CurAbsB;
	}
	if(MaxAbsB <= 0)
	{
	    AnalyzeForHarmonics_DeleteAuxArrays(AuxDataContIn, AuxDataContOut, CkArr, PhikArr, HarmNoArr);
		return;
	}

	double Step = Per/double(AmOfPts);
	double Start = -0.5*Per;

	CGenMathFFT1DInfo FFT1DInfo;
	FFT1DInfo.pInData = AuxDataContIn;
	FFT1DInfo.pOutData = AuxDataContOut;
	FFT1DInfo.Dir = -1;
	FFT1DInfo.xStep = Step;
	FFT1DInfo.xStart = Start;
	FFT1DInfo.Nx = AmOfPts;
	FFT1DInfo.HowMany = 1;
	FFT1DInfo.UseGivenStartTrValue = 0;

	CGenMathFFT1D FFT1D;
	FFT1D.Make1DFFT(FFT1DInfo);

	int HalfAmOfPts = AmOfPts >> 1;
	int MaxAmOfHarm = AmOfHarm;
	if(MaxAmOfHarm >= HalfAmOfPts) MaxAmOfHarm = HalfAmOfPts - 1;

	CkArr = new double[MaxAmOfHarm];
	if(CkArr == 0) throw MEMORY_ALLOCATION_FAILURE;

	PhikArr = new double[MaxAmOfHarm];
	if(PhikArr == 0) throw MEMORY_ALLOCATION_FAILURE;

	HarmNoArr = new int[MaxAmOfHarm];
	if(HarmNoArr == 0) throw MEMORY_ALLOCATION_FAILURE;

	double CoefMult = 2./Per;
	double AbsThreshold = RelPrec*MaxAbsB/CoefMult;

	float *tOutFFT = AuxDataContOut + AmOfPts + 2;
	double *tCkArr = CkArr, *tPhikArr = PhikArr;
	int *tHarmNoArr = HarmNoArr;
	int HarmCount = 0;
	for(int j=0; j<MaxAmOfHarm; j++)
	{
		double Ak = *(tOutFFT++);
		double Bk = *(tOutFFT++);
		if((::fabs(Ak) < AbsThreshold) && (::fabs(Bk) < AbsThreshold)) continue;

		double Ck = sqrt(Ak*Ak + Bk*Bk);
		if(Ck < AbsThreshold) continue;

		*(tCkArr++) = CoefMult*Ck;
		//*(tPhikArr++) = srTMathFunctions::Argument(Ak, -Bk);
		*(tPhikArr++) = CGenMathFunc::Argument(Ak, -Bk);
		*(tHarmNoArr++) = (j + 1);
		HarmCount++;
	}
	if(HarmCount <= 0) 
	{
		AnalyzeForHarmonics_DeleteAuxArrays(AuxDataContIn, AuxDataContOut, CkArr, PhikArr, HarmNoArr);
		return;
	}

	const double Pi = 3.1415926535897932;
	const double eEl = 1.602176462E-19; //[C]
    const double me = 9.10938188E-31; //[kg]
	const double c = 2.99792458E+08; //[m/c]
	double MultB2K = eEl*Per/(2.*Pi*me*c);

	MagHarmArr = new srTMagHarm[HarmCount];
	srTMagHarm *tMagHarmArr = MagHarmArr;
	tCkArr = CkArr;
	tPhikArr = PhikArr;
    tHarmNoArr = HarmNoArr;
	for(int k=0; k<HarmCount; k++)
	{
        tMagHarmArr->HarmNo = *(tHarmNoArr++);
		tMagHarmArr->XorZ = XorZ;
		tMagHarmArr->K = MultB2K*(*(tCkArr++))/(tMagHarmArr->HarmNo);
		tMagHarmArr->Phase = *(tPhikArr++);
		tMagHarmArr++;
	}
	AmOfHarm = HarmCount;
    AnalyzeForHarmonics_DeleteAuxArrays(AuxDataContIn, AuxDataContOut, CkArr, PhikArr, HarmNoArr);
}

//*************************************************************************

void srTMagFldTrUnif::AnalyzeForHarmonics_DeleteAuxArrays(float*& AuxDataContIn, float*& AuxDataContOut, double*& CkArr, double*& PhikArr, int*& HarmNoArr)
{
	if(AuxDataContIn != 0) { delete[] AuxDataContIn; AuxDataContIn = 0;}
	if(AuxDataContOut != 0) { delete[] AuxDataContOut; AuxDataContOut = 0;}
	if(CkArr != 0) { delete[] CkArr; CkArr = 0;}
	if(PhikArr != 0) { delete[] PhikArr; PhikArr = 0;}
	if(HarmNoArr != 0) { delete[] HarmNoArr; HarmNoArr = 0;}
}

//*************************************************************************

void srTMagFldTrUnif::ChooseDominantBasicFieldPeriodicParam(
	double Per_HorFld, double L_HorFld, double sCen_HorFld, double sStartPer_HorFld, double MaxAbsHorFld,
	double Per_VertFld, double L_VertFld, double sCen_VertFld, double sStartPer_VertFld, double MaxAbsVertFld,
	double& Per, double& L, double& sCen, double& sStartPer)
{
	Per = 0.; L = 0.; sCen = 0.; sStartPer = 0.;

	if((Per_HorFld <= 0) && (Per_VertFld <= 0)) 
	{
		CErrWarn::AddWarningMessage(&gVectWarnNos, NO_MAGNETIC_FIELD_HARMONICS_FOUND);
		return;
	}

	//if(Per > 0)
	//{
	//	if(Per_HorFld > Per) Per_HorFld = Per;
	//	if(Per_VertFld > Per) Per_VertFld = Per;
	//}

	bool SetFromVertFld = true;

	if((Per_HorFld > 0) && (Per_VertFld <= 0)) SetFromVertFld = false;
	else if((Per_VertFld > 0) && (Per_HorFld <= 0)) SetFromVertFld = true;
	else if((Per_HorFld > 0) && (Per_VertFld > 0))
	{
		if(MaxAbsVertFld > 2.*MaxAbsHorFld) SetFromVertFld = true;
		else if(MaxAbsHorFld > 2.*MaxAbsVertFld) SetFromVertFld = false;
		else if(Per_HorFld > 0.8*Per_VertFld) SetFromVertFld = false;
		else SetFromVertFld = true;
	}

	if(SetFromVertFld)
	{
        Per = Per_VertFld; L = L_VertFld; sCen = sCen_VertFld; sStartPer = sStartPer_VertFld;
	}
	else
	{
        Per = Per_HorFld; L = L_HorFld; sCen = sCen_HorFld; sStartPer = sStartPer_HorFld;
	}

	if(Per <= 0)
	{
		CErrWarn::AddWarningMessage(&gVectWarnNos, NO_MAGNETIC_FIELD_HARMONICS_FOUND);
	}
}

//*************************************************************************

double srTMagFldTrUnif::FindMaxAbsVal(double* Arr, int np)
{
	if((Arr == 0) || (np == 0)) return 0.;

	double MaxAbsX = 0.;
	double AuxMax, AuxMin;
	int IndAuxMax, IndAuxMin;
	//srTMathFunctions::FindMinMax(Arr, np, AuxMin, IndAuxMin, AuxMax, IndAuxMax);
	CGenMathFunc::FindMinMax(Arr, np, AuxMin, IndAuxMin, AuxMax, IndAuxMax);
	MaxAbsX = ::fabs(AuxMin);
    AuxMax = ::fabs(AuxMax);
	if(MaxAbsX < AuxMax) MaxAbsX = AuxMax;
	return MaxAbsX;
}

//*************************************************************************

void srTMagFldTrUnif::SumUpFieldHarmonics(srTMagHarm*& MagHarmArr_HorFld, int NumHarm_HorFld, srTMagHarm*& MagHarmArr_VertFld, int NumHarm_VertFld, srTMagHarm*& TotHarmArr, int& TotAmOfHarm)
{
	TotHarmArr = 0; TotAmOfHarm = 0;
	bool ThereAreHorFldHarm = ((MagHarmArr_HorFld != 0) && (NumHarm_HorFld > 0));
	bool ThereAreVertFldHarm = ((MagHarmArr_VertFld != 0) && (NumHarm_VertFld > 0));

	if((!ThereAreHorFldHarm) && (!ThereAreVertFldHarm)) 
	{
        CErrWarn::AddWarningMessage(&gVectWarnNos, NO_MAGNETIC_FIELD_HARMONICS_FOUND);
		return;
	}
	else if(!ThereAreHorFldHarm)
	{
		TotHarmArr = MagHarmArr_VertFld;
		TotAmOfHarm = NumHarm_VertFld;
		MagHarmArr_VertFld = 0;
	}
	else if(!ThereAreVertFldHarm)
	{
		TotHarmArr = MagHarmArr_HorFld;
		TotAmOfHarm = NumHarm_HorFld;
		MagHarmArr_HorFld = 0;
	}
	else
	{
		TotAmOfHarm = NumHarm_HorFld + NumHarm_VertFld;
		TotHarmArr = new srTMagHarm[TotAmOfHarm];

        srTMagHarm *tHarm = TotHarmArr;
		for(int i=0; i<NumHarm_HorFld; i++) *(tHarm++) = MagHarmArr_HorFld[i];
		for(int j=0; j<NumHarm_VertFld; j++) *(tHarm++) = MagHarmArr_VertFld[j];
	}
}

//*************************************************************************

srTMagFldTrUnif* srTMagFldTrUnif::SumUpSeveralFldTrUnif(srTMagFldCont* pMagTrUnifCont, srTMagFldCont* pMagOptCont)
{
	if(pMagTrUnifCont == 0) return 0;
	if(pMagTrUnifCont->AmountOfMembers() <= 0) return 0;

	double sStartMax = 0, sEndMin = 0;
	bool MinLimitsShouldBeRespected = false;
	if(pMagOptCont != 0)
	{
		sStartMax = pMagOptCont->gsStart;
        sEndMin = pMagOptCont->gsEnd;
		MinLimitsShouldBeRespected = true;
	}


	//to implement
	//sum up different transversely uniform fields 
	//put maximum limits, sStartMax, sEndMin; pad zeros if necessary

	return 0;
}

//*************************************************************************

srTGenTrjDat* srTMagFld3d::CreateAndSetupNewTrjDat(srTEbmDat* pEbmDat)
{
	srTTrjDat3d *pOut = new srTTrjDat3d(*this);
	if(pEbmDat != 0) pOut->EbmDat = *pEbmDat;

	//SetupTrjDat(pOut);
	//int res = 0;
	//if(res = pOut->ComputeInterpolatingStructure()) throw res;
	return pOut;
}

//*************************************************************************

//void srTMagFld3d::SetupTrjDat(srTTrjDat3d* pTrjDat3d)
//{
//	if(pTrjDat3d == 0) return;
//
//	long Np = nx*nz*ns;
//	if(Np <= 0) return;
//	if((BxArr == 0) && (BzArr == 0) && (BsArr == 0)) return;
//
//	//pTrjDat3d->LenFieldData = ns;
//	//pTrjDat3d->sStep = sStep;
//	//pTrjDat3d->sStart = sStart;
//}

//*************************************************************************

void srTMagFldCont::ComputeSR_Stokes(srTEbmDat* pElecBeam, srTWfrSmp* pWfrSmp, void* pPrcPar, srTStokesStructAccessData* pStokes)
{
	if((pElecBeam == 0) || (pWfrSmp == 0) || (pPrcPar == 0) || (pStokes == 0)) throw INCORRECT_PARAMS_SR_COMP;
	if(gMagElems.size() <= 0) throw NO_MAG_FIELD_DEFINED;

	pStokes->UpdateLongitudinalGridParams(pWfrSmp->yStart, ((pWfrSmp->ny > 1)? (pWfrSmp->yEnd - pWfrSmp->yStart)/(pWfrSmp->ny - 1) : 0), pWfrSmp->ny); // to avoid passing pWfrSmp to ComputeStokes
	srTParPrecStokesArb *pPrc = (srTParPrecStokesArb*)pPrcPar;

	srTMagFldCont MagTrUnifCont;
	srTMagFldCont MagContOther; // with drift spaces
	FilterOutTrUnifMagFld(MagTrUnifCont, MagContOther);

	srTMagFldCont* pMagContOpt = 0;
	if(MagContOther.AmountOfMembers() > 0) 
	{
		MagContOther.PrepareContForParticlePropag();
		pMagContOpt = &MagContOther;
	}

	srTMagFldTrUnif* pMagFldTrUnif = srTMagFldTrUnif::SumUpSeveralFldTrUnif(&MagTrUnifCont, pMagContOpt);
    
	srTRadIntThickBeam::ComputeStokes(pElecBeam, pMagFldTrUnif, pMagContOpt, pPrc, pStokes);
	if(pMagFldTrUnif != 0) delete pMagFldTrUnif;
}

//*************************************************************************

void srTMagFldCont::FilterOutTrUnifMagFld(srTMagFldCont& MagTrUnifCont, srTMagFldCont& MagOptCont)
{
	if(gMagElems.size() <= 0) return;
	for(CMHGenObj::const_iterator iter = gMagElems.data.begin(); iter != gMagElems.data.end(); ++iter)
	{
        srTMagElem* pMagElem = (srTMagElem*)(((*iter).second).rep);

		srTMagFldCont* pCont = dynamic_cast<srTMagFldCont*>(pMagElem);
		if(pCont != NULL) { pCont->FilterOutTrUnifMagFld(MagTrUnifCont, MagOptCont); continue;}

		srTMagFldTrUnif* pTrUnif = dynamic_cast<srTMagFldTrUnif*>(pMagElem);
		if(pTrUnif != NULL) { MagTrUnifCont.AddElement((*iter).second); continue;}

		srTMagFieldPeriodic* pPeriodic = dynamic_cast<srTMagFieldPeriodic*>(pMagElem);
		if(pPeriodic != NULL) 
		{
			srTMagFldTrUnif* pTrUnif = pPeriodic->CreateAndSetupMagFldTrUnif();
			if(pTrUnif != NULL)
			{
                CHGenObj hObj(pTrUnif);
                MagTrUnifCont.AddElement(hObj); continue;
			}
		}

		MagOptCont.AddElement((*iter).second);
	}
}

//*************************************************************************

void srTMagFldCont::PrepareContForParticlePropag()
{
	const double AbsLenTol = 0.001; //[m]
	if(AmountOfMembers() <= 0) return;

	SortContVsStartPos();

	CObjCont<CGenObject> LocMagElems;

	double PrevStart, PrevEnd;
	bool OnePassIsDone = false;

	for(CMHGenObj::const_iterator iter = gMagElems.data.begin(); iter != gMagElems.data.end(); ++iter)
    {
		CHGenObj hCurElem = (*iter).second;
		srTMagElem* pMagElem = dynamic_cast<srTMagElem*>(hCurElem.rep);
		if(pMagElem == 0) continue;
		srTMagFldTrUnif* pMagFldTrUnif = dynamic_cast<srTMagFldTrUnif*>(pMagElem);
		if(pMagFldTrUnif != 0) continue; // normally this is already filtered out

		double CurStart = pMagElem->gsStart;
		double CurEnd = pMagElem->gsEnd;

		if(OnePassIsDone)
		{
			if(CurStart < PrevEnd - 100*AbsLenTol) throw OVERLAPPING_MAG_ELEMS;
			if(CurStart > PrevEnd + AbsLenTol) 
			{
				LocMagElems.insert(new srTMagDrift(CurStart - PrevEnd, PrevEnd));
			}
		}
        LocMagElems.insert(hCurElem);

        PrevStart = CurStart;
        PrevEnd = CurEnd;
        OnePassIsDone = true;
	}

	if(LocMagElems.size() > 0)
	{
        gMagElems.erase();
        gMagElems.copy(LocMagElems);
	}
}

//*************************************************************************

void srTMagFldCont::SortContVsStartPos()
{
	int AmOfMem = AmountOfMembers();
	if(AmOfMem <= 0) return;

	DetermineLongStartAndEndPos();

	CObjCont<CGenObject> LocMagElems(gMagElems);
	gMagElems.erase();

	for(int i=0; i<AmOfMem; i++)
	{
        int IndCurElem = FindMagElemWithSmallestLongPos(LocMagElems);
		if(IndCurElem < 0) break;

		CHGenObj hCurElem = LocMagElems.get(IndCurElem);
		gMagElems.insert(hCurElem);
		LocMagElems.erase(IndCurElem);
	}
}

//*************************************************************************

void srTMagFldCont::DetermineLongStartAndEndPos()
{
	if(AmountOfMembers() <= 0) return;

	double sStartMin=1E+23, sEndMax=-1E+23;
	for(CMHGenObj::const_iterator iter = gMagElems.data.begin(); iter != gMagElems.data.end(); ++iter)
    {
        srTMagElem* pMagElem = dynamic_cast<srTMagElem*>(((*iter).second).rep);
		if(pMagElem == 0) continue;

		srTMagFldCont* pCont = dynamic_cast<srTMagFldCont*>(pMagElem);
		if(pCont != 0) pCont->DetermineLongStartAndEndPos();

		if(sStartMin > pMagElem->gsStart) sStartMin = pMagElem->gsStart;
		if(sEndMax < pMagElem->gsEnd) sEndMax = pMagElem->gsEnd;
	}
    gsStart = sStartMin;
    gsEnd = sEndMax;
}

//*************************************************************************

