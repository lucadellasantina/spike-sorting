// Compute the autocorrelation function for a set of spikes, represented
// by a vector of their arrival times
#define char16_t UINT16_T
#include "mex.h"
#include <vector>

using namespace std;
void AutoCorrEnum(double *t,long n,double tmax,vector<double> &tac,vector<long> *tacindx);
vector<long> AutoCorrBin(double *t,long n,double tmax,int nbins);

void AutoCorrEnum(double *t,long n,double tmax,vector<double> &tac,vector<long> *tacindx)
{
	//tac.erase(tac.begin(),tac.end());
	//tacindx.erase(tacindx.begin(),tacindx.end());
	long i,j;
	i = 0;
	double dt;
	while (i < n-1) {
		j = 1;
		while (i+j < n && (dt = t[i+j]-t[i]) < tmax) {
			tac.push_back(dt);
			tacindx[0].push_back(i+1);
			tacindx[1].push_back(i+j+1);
			j += 1;
		}
		i += 1;
	}
}

vector<long> AutoCorrBin(double *t,long n,double tmax,int nbins)
{
	double binnumfac = (double) nbins/tmax;
	vector<long> nInBins(nbins+1,0);		// One extra for dt = tmax
	
	long i,j;
	i = 0;
	double dt;
	while (i < n-1) {
		j = 1;
		while (i+j < n && (dt = t[i+j]-t[i]) < tmax) {
			nInBins[binnumfac*dt]++;
			j += 1;
		}
		i += 1;
	}
	nInBins.erase(--nInBins.end());		// Get rid of the extra one
	return nInBins;
}

// The gateway routine
void mexFunction(int nlhs, mxArray *plhs[],
				 int nrhs, const mxArray *prhs[])
{
	//mexPrintf("Version 2\n");
	// Argument parsing
	if (nrhs < 2 || nrhs > 3)
		mexErrMsgTxt("AutoCorr requires 2 or 3 inputs");
	int bad1 =  (!mxIsNumeric(prhs[0]) || !mxIsDouble(prhs[0]) || mxIsComplex(prhs[0])
			|| (mxGetN(prhs[0]) != 1 && mxGetM(prhs[0]) != 1));
	//if (bad1)
	//	mexWarnMsgTxt("bad1 = 1");
	if (bad1 && !mxIsEmpty(prhs[0]))
	//if (bad1)
		mexErrMsgTxt("The first input to AutoCorr must be a real double vector");
	double *t = mxGetPr(prhs[0]);
	long n = mxGetN(prhs[0])*mxGetM(prhs[0]);
	if (!mxIsNumeric(prhs[1]) || !mxIsDouble(prhs[1]) || mxIsComplex(prhs[1])
			|| mxGetN(prhs[1]) * mxGetM(prhs[1]) != 1)
		mexErrMsgTxt("The second input to AutoCorr must be a real double scalar");
	double tmax = mxGetScalar(prhs[1]);
	int binning = 0;
	int nbins;
	if (nrhs == 3) {
		if (!mxIsNumeric(prhs[2]) || !mxIsDouble(prhs[2]) || mxIsComplex(prhs[2])
				|| mxGetN(prhs[2]) * mxGetM(prhs[2]) != 1)
			mexErrMsgTxt("The third input (if present) to AutoCorr must be a real double scalar");
		else {
			binning = 1;
			nbins = mxGetScalar(prhs[2]);
		}
	}
	if (binning && nlhs != 1)
		mexErrMsgTxt("When binning, one output is required");
	else if(nlhs < 1 || nlhs > 2)
		mexErrMsgTxt("When not binning, one or two outputs are required");
	if (binning) {
		vector<long> nInBin = AutoCorrBin(t,n,tmax,nbins);
		plhs[0] = mxCreateDoubleMatrix(1,nbins,mxREAL);
		double *outp = mxGetPr(plhs[0]);
		for (long i = 0; i < nbins; i++)
			outp[i] = nInBin[i];
		return;
	}
	else {
		vector<double> tac;
		vector<long> tacindx[2];
		AutoCorrEnum(t,n,tmax,tac,tacindx);
		plhs[0] = mxCreateDoubleMatrix(1,tac.size(),mxREAL);
		double *outp = mxGetPr(plhs[0]);
		for (long i = 0; i < tac.size(); i++)
			outp[i] = tac[i];
		if (nlhs == 2) {
			plhs[1] = mxCreateDoubleMatrix(2,tac.size(),mxREAL);
			outp = mxGetPr(plhs[1]);
			for (long i = 0; i < tac.size(); i++) {
				outp[2*i] = tacindx[0][i];
				outp[2*i+1] = tacindx[1][i];
			}	
		}
	}
}
