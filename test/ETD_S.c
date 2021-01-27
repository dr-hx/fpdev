/*	******************************************************************
	** V_etd4_s.c													** 
	** This is four-order etd serial program for Vacancy model.		**
	** run ./V_etd4_s N	TIME									    **
	** N is the maximum size of cluster.							**
	** TIME is the totol time for evolving.							**
	******************************************************************
*/

//#include <iostream>
//#include "iomanip"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <time.h>
#include <real/EAST.h>

#define KB 8.625e-5
#define N 500
#define T 823
#define PI 3.141592
#define TIME 1000

int main(int argc, char *argv[])
{
	// int N = atoi(argv[1]);	//从运行命令中给参数N的值
	// double TIME = atoi(argv[2]);
	double start, end;
	double totaltime;
	start = clock();
	int i;
	double sumalpha = 0.0, sumalpha_a = 0.0, sumalpha_b = 0.0, sumalpha_d = 0.0;
	double sumbeta = 0.0, sumbeta_a = 0.0, sumbeta_b = 0.0, sumbeta_d = 0.0;
	double dt = 1.0e-1;
	double t = 0.0, k = 0.0;

	double Vat = 1.205e-29; //m^3。1.205e-29m^3=1.205e-23cm^3
	//cout<<"Vat="<<Vat<<"\n"<<endl;
	double EFvac = 1.77;						   //空位的形成能eV
	double EMvac = 1.1;							   //空位的迁移能eV
	double Dvac;
	Dvac = 1.0e-6 * exp(-EMvac / (KB * T)); //m^2/s。1.0e-6*exp(-EMvac/(KB*T))m^2/s 空位的扩散系数,m^2/s=1.0e+4nm^2/s
	
	//cout<<"Dvac="<<Dvac<<endl;
	double Gama = 6.25e18; //eV/m^2。6.25e+18eV/m^2,eV/m^2=1.0e-4eV/nm^2
	//cout<<"Gama="<<Gama<<"\n"<<endl;
	double Cinit = 1.0e-7; //atom-1 初始浓度

	double *C = (double *)malloc(N * sizeof(double));
	double *a = (double *)malloc(N * sizeof(double));
	double *b = (double *)malloc(N * sizeof(double));
	double *d = (double *)malloc(N * sizeof(double));
	double *Fa = (double *)malloc(N * sizeof(double));
	double *Fb = (double *)malloc(N * sizeof(double));
	double *Fc = (double *)malloc(N * sizeof(double));
	double *Fd = (double *)malloc(N * sizeof(double));
	double *r = (double *)malloc(N * sizeof(double));
	double *EBvac = (double *)malloc(N * sizeof(double));
	double *Alpha = (double *)malloc(N * sizeof(double));
	double *alpha = (double *)malloc(N * sizeof(double));
	double *alpha_a = (double *)malloc(N * sizeof(double));
	double *alpha_b = (double *)malloc(N * sizeof(double));
	double *alpha_d = (double *)malloc(N * sizeof(double));
	double *Beta = (double *)malloc(N * sizeof(double));
	double *beta = (double *)malloc(N * sizeof(double));
	double *beta_a = (double *)malloc(N * sizeof(double));
	double *beta_b = (double *)malloc(N * sizeof(double));
	double *beta_d = (double *)malloc(N * sizeof(double));
	//va = (double *)malloc(N * sizeof(double));
	//vb = (double *)malloc(N * sizeof(double));
	double *L3 = (double *)malloc(N * sizeof(double));
	double *phi1 = (double *)malloc(N * sizeof(double));
	double *phi2 = (double *)malloc(N * sizeof(double));
	double *phi3 = (double *)malloc(N * sizeof(double));
	double *poly1 = (double *)malloc(N * sizeof(double));
	double *poly2 = (double *)malloc(N * sizeof(double));
	double *poly3 = (double *)malloc(N * sizeof(double));

	//double acons = 6.02e23;
	//double at2cb = 8.385e22;
	//double cb2at = 1./at2cb;
	for(i=0;i<N;i++){
		C[i]=0.0;
		EAST_ANALYZE_ERROR(C[i]);
		Fa[i]=Fb[i]=Fc[i]=Fd[i]=0.0;

	}
	C[0] = Cinit;

	//C[0] = Cinit*at2cb;                 //atom-1转换成cm

	double Alpha0, Beta0;
	Beta0 = Alpha0 = pow((48.0 * PI * PI / Vat / Vat), 1.0 / 3.0) * Dvac;
	for (i = 0; i < N; i++)
	{
		r[i] = pow((3.0 * (i + 1.0) * Vat / (4 * PI)), 1.0 / 3.0) * 1.0; //nm
		EBvac[i] = EFvac - 2.0* Gama * Vat / r[i];						 //eV空位缺陷结合能
		Alpha[i] = Alpha0 * pow( (i+1.0),  1.0 / 3.0) * exp(-EBvac[i] / (KB * T));
		Beta[i] = Beta0 * pow( (i+1.0),  1.0 / 3.0); //公式2
		if (i == 0)
		{
			L3[0] = -pow( (1.0 + k),  -3);
			phi1[0] = exp(-dt * (1.0 + k) / 2);
			phi2[0] = (1.0 - exp(-dt * (1.0 + k) / 2)) / (1.0 + k);
			phi3[0] = exp(-dt * (1.0 + k));
			poly1[0] =-4.0+ dt * (1.0 + k) + exp(-dt * (1.0 + k)) * (4 + 3.0 * dt * (1.0 + k) + dt * dt * (1.0 + k) * (1.0 + k));
			poly2[0] =4.0- 2.0* dt * (1.0 + k) + exp(-dt * (1.0 + k)) * (-4 - 2.0* dt * (1.0 + k));
			poly3[0] =-4.0+ 3.0 * dt * (1.0 + k) - dt * dt * (1.0 + k) * (1.0 + k) + exp(-dt * (1.0 + k)) * (4 + dt * (1.0 + k));
		}
		else
		{
			L3[i] = -pow( (Alpha[i] + k),  -3);
			phi1[i] = exp(-dt * (Alpha[i] + k) / 2);
			phi2[i] = (1.0 - exp(-dt * (Alpha[i] + k) / 2)) / (Alpha[i] + k);
			phi3[i] = exp(-dt * (Alpha[i] + k));
			poly1[i] =-4.0+ dt * (Alpha[i] + k) + exp(-dt * (Alpha[i] + k)) * (4 + 3.0 * dt * (Alpha[i] + k) + dt * dt * (Alpha[i] + k) * (Alpha[i] + k));
			poly2[i] =4.0- 2.0* dt * (Alpha[i] + k) + exp(-dt * (Alpha[i] + k)) * (-4 - 2.0* dt * (Alpha[i] + k));
			poly3[i] =-4.0+ 3.0 * dt * (Alpha[i] + k) - dt * dt * (Alpha[i] + k) * (Alpha[i] + k) + exp(-dt * (Alpha[i] + k)) * (4 + dt * (Alpha[i] + k));
		}
	}
	for (i = 0; i < N; i++)
	{
		//printf("phi3[%d]=%e\n",i,phi3[i]);
		//printf("C[%d]=%e\n",i,C[i]);
		//printf("L3[%d]=%e\n",i,L3[i]);
		//printf("poly1[%d]=%e\n",i,poly1[i]);
		//printf("poly2[%d]=%e\n",i,poly2[i]);
		//printf("poly3[%d]=%e\n",i,poly3[i]);
	}

	while (t < TIME)
	{
		//printf("t=%f******sumalpha=%e\tsumbeta=%e\n",t,sumalpha,sumbeta);
		for (i = 0; i < N; i++)
		{
			if (i == 0)
			{
				alpha[i] = 0.0;
				beta[i] = 0.0;
			}
			else
			{
				alpha[i] = Alpha[i] * C[i];
				beta[i] = Beta[i] * C[i] * C[0];
			}
			sumbeta += beta[i];
			sumalpha += alpha[i];
		}
		//printf("t=%f******sumalpha=%e\tsumbeta=%e\n",t,sumalpha,sumbeta);

		Fc[0] = -2 * Beta[0] * C[0] * C[0] - sumbeta + sumalpha + Alpha[1] * C[1] + (1.0 + k) * C[0];
		for (i = 1; i <= N - 2; i++)
		{
			Fc[i] = Beta[i - 1] * C[i - 1] * C[0] - Beta[i] * C[i] * C[0] + Alpha[i + 1] * C[i + 1] + k * C[i];
		}
		Fc[N - 1] =0.0;
		Fc[N - 1] = Beta[N - 2] * C[N - 2] * C[0] - Beta[N - 1] * C[N - 1] * C[0] + k * C[N - 1];
		/*
		for(i=0;i<N;i++){
			printf("Fc[%d]=%e\n",i,Fc[i]);
		}*/

		for (i = 0; i < N; i++)
		{
			a[i] =0.0;
			a[i]= phi1[i] * C[i] + phi2[i] * Fc[i];
			if (i == 0)
			{
				alpha_a[i] = 0.0;
				beta_a[i] = 0.0;
			}
			else
			{
				alpha_a[i] = Alpha[i] * a[i];
				beta_a[i] = Beta[i] * a[i] * a[0];
			}
			sumalpha_a += alpha_a[i];
			sumbeta_a += beta_a[i];
		}
		Fa[0] = -2 * Beta[0] * a[0] * a[0] - sumbeta_a + sumalpha_a + Alpha[1] * a[1] + (1.0 + k) * a[0];
		for (i = 1; i <= N - 2; i++)
		{
			Fa[i] = Beta[i - 1] * a[i - 1] * a[0] - Beta[i] * a[i] * a[0] + Alpha[i + 1] * a[i + 1] + k * a[i];
		}
		Fa[N - 1] = Beta[N - 2] * a[N - 2] * a[0] - Beta[N - 1] * a[N - 1] * a[0] + k * a[N - 1];
		/*for(i=0;i<N;i++){
			printf("Fa[%d]=%e\n",i,Fa[i]);
		}*/

		for (i = 0; i < N; i++)
		{
			b[i] = phi1[i] * C[i] + phi2[i] * Fa[i];
			if (i == 0)
			{
				alpha_b[i] = 0.0;
				beta_b[i] = 0.0;
			}
			else
			{
				alpha_b[i] = Alpha[i] * b[i];
				beta_b[i] = Beta[i] * b[i] * b[0];
			}
			sumalpha_b += alpha_b[i];
			sumbeta_b += beta_b[i];
		}
		Fb[0] = -2 * Beta[0] * b[0] * b[0] - sumbeta_b + sumalpha_b + Alpha[1] * b[1] + (1.0 + k) * b[0];
		for (i = 1; i <= N - 2; i++)
		{
			Fb[i] = Beta[i - 1] * b[i - 1] * b[0] - Beta[i] * b[i] * b[0] + Alpha[i + 1] * b[i + 1] + k * b[i];
		}
		Fb[N - 1] = Beta[N - 2] * b[N - 2] * b[0] - Beta[N - 1] * b[N - 1] * b[0] + k * b[N - 1];
		/*for(i=0;i<N;i++){
			printf("Fb[%d]=%e\n",i,Fb[i]);
		}*/

		for (i = 0; i < N; i++)
		{
			d[i] = phi1[i] * a[i] + phi2[i] * (2 * Fb[i] - Fc[i]);
			if (i == 0)
			{
				alpha_d[i] = 0.0;
				beta_d[i] = 0.0;
			}
			else
			{
				alpha_d[i] = Alpha[i] * d[i];
				beta_d[i] = Beta[i] * d[i] * d[0];
			}
			sumalpha_d += alpha_d[i];
			sumbeta_d += beta_d[i];
		}
		Fd[0] = -2 * Beta[0] * d[0] * d[0] - sumbeta_d + sumalpha_d + Alpha[1] * d[1] + (1.0 + k) * d[0];
		for (i = 1; i <= N - 2; i++)
		{
			Fd[i] = Beta[i - 1] * d[i - 1] * d[0] - Beta[i] * d[i] * d[0] + Alpha[i + 1] * d[i + 1] + k * d[i];
		}
		Fd[N - 1] = Beta[N - 2] * d[N - 2] * d[0] - Beta[N - 1] * d[N - 1] * d[0] + k * d[N - 1];
		/*for(i=0;i<N;i++){
			printf("Fd[%d]=%e\n",i,Fd[i]);
		}*/

		EAST_TRACKING_ON();
		for (i = 0; i < N; i++)
		{
			C[i] = phi3[i] * C[i] + pow( dt,  -2) * L3[i] * (poly1[i] * Fc[i] + poly2[i] * (Fa[i] + Fb[i]) + poly3[i] * Fd[i]);
			EAST_ANALYZE_ERROR(C[i]);
			// if(isnan(C[i]))
			// {
			// 	EAST_ANALYZE(std::cout, C[i]);
			// }
		}
		EAST_TRACKING_OFF();
		
		t = t + dt;
		sumalpha = sumalpha_a = sumalpha_b = sumalpha_d = 0.0;
		sumbeta = sumbeta_a = sumbeta_b = sumbeta_d = 0.0;
	}

	end = clock();
	for (i = 0; i < N; i++)
	{
		printf("C[%d]=%e\n", i, C[i]);
		EAST_DUMP_ERROR(std::cout, C[i]);
	}
	EAST_DUMP_ERROR(std::cout, C[0]);
	EAST_DRAW_ERROR("C0", C[0], "/Volumes/Macintosh HD Data/Haskel IDE/fpdev/output.dot");
	totaltime = (end - start) / CLOCKS_PER_SEC;
	//for(i=0;i<N;i++)
	//printf("C[%d] = %e\n",i,C[i]);

	free(poly3);
	poly3 = NULL;
	free(poly2);
	poly2 = NULL;
	free(poly1);
	poly1 = NULL;
	free(phi3);
	phi3 = NULL;
	free(phi2);
	phi2 = NULL;
	free(phi1);
	phi1 = NULL;
	free(L3);
	L3 = NULL;
	free(beta_d);
	beta_d = NULL;
	free(beta_b);
	beta_b = NULL;
	free(beta_a);
	beta_a = NULL;
	free(beta);
	beta = NULL;
	free(Beta);
	Beta = NULL;
	free(alpha_d);
	alpha_d = NULL;
	free(alpha_b);
	alpha_b = NULL;
	free(alpha_a);
	alpha_a = NULL;
	free(alpha);
	alpha = NULL;
	free(Alpha);
	Alpha = NULL;
	free(EBvac);
	EBvac = NULL;
	free(r);
	r = NULL;
	free(Fd);
	Fd = NULL;
	free(Fc);
	Fc = NULL;
	free(Fb);
	Fb = NULL;
	free(Fa);
	Fa = NULL;
	free(d);
	d = NULL;
	free(b);
	b = NULL;
	free(a);
	a = NULL;
	free(C);
	C = NULL;

	//system("pause");
	printf("totaltime=%fs\n", totaltime);
	return 0;
}
