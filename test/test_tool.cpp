int main(int argc, char const *argv[])
{
    /* code */
    return 0;
}

// SECTION test varusage
double v1;
double *v2;
double v3[5];
void test_varusage(double v4, double v5[6],double v7)
{
    double v8;
    double v9[7];
    double *v10 = v9;
    const double v11 = 0; 
    double *v12 = &v4;
}
// !SECTION