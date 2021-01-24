// #define __APPLE__ 1
// #include <dispatch/dispatch.h>
#include <stdio.h>
// #include <stdio.h>
#include <EAST.h>

double fabs(double x)
{
    if(x>0) return x;
    else return 0 - x;
}

#define LB -30
#define UB -20
#define INC 0.1

struct TestS
{
  TestS() : x(0) {}
  double x;
  double fp();
};

struct TestT : public TestS
{
  TestT() : TestS() {}
};

double TestS::fp()
{
  return x+1;
}

void testPF0()
{
  for (double x = LB; x < UB; x = x + INC)
  {
    double sum = 0.0;
    double term = 1.0;
    for (int i = 1; sum != sum + term; i++)
    {
      sum = sum + term;
      term = term * x / i;
    }
    // double ae = exp(x);
    std::cout << exp(x) <<"\n";
    EAST_DUMP(std::cout, sum);
    EAST_ANALYZE(std::cout, sum);
  }
}

int main(int argc, char const *argv[])
{

    testPF0();
    // double a = 2.0;
    // double expect_error = 0.000001;
    // double x;
    // double actual_error;
    // unsigned iteration_count = 0;

    // do
    // {
    //     if (a == 0.0)
    //         a = 0.1; /* 避免0做分母 */
    //     x = a / 2 + 1 / a;
    //     actual_error = fabs(2 - x * x);
    //     a = x;

    //     ++iteration_count;
    //     printf("%d\t%.9f\t%.9f\n", iteration_count, a, actual_error);
    //     EAST_DUMP(std::cout, a);
    //     EAST_ANALYZE(std::cout, a);
    // } while (actual_error >= expect_error);

    return 0;
}

// template<int Size>
// double test(double x, double y)
// {
//     int s = 5;
//     double a[]={1,2,3,4,5}; // constant
//     double b[5]; // constant
//     double c[s+1]; //variable type
//     double d[Size]; // dependent type
//     int x = sizeof(c);
//     return test<Size>(x+1, y+1) + test<Size>(x-1, 0);
// }

// #pragma K
// int main(int argc, char const *argv[])
// {
//     if (double d = 0; double x = 5)
//         HELLO;

//     int *p;

//     *p = 5;


//     for (int i = 0, j = 0;; i++, --j)
//     {
//     }

    
//     double t;
//     double &q = t;
//     q=0;
//     double *e = &q;
//     *e = 0;
//     for (double u = (0.0, t = 1);;)
//         ;
//     for (t = 0.0, *p = 1;;)
//         ;
    
//     switch (int c = 1)
//     {
//     case 1:
//         c = 2;
//         break;
//     default:
//         break;
//     }

//     while(int i=0)
//         i++;

//     double x, y = 2, z = 4;

//     return 0;
// }
// typedef double FP;

// class Test
// {
// public:
//     double test(double f) { return *this + 0; }
//     double operator+(double x)
//     {
//         return test(x);
//     }
// };

// FP f(FP x, int y)
// {
//     if (f(f(x + 2, y), y + 1) > 0)
//     {
//         Test tt;
//         return tt + x + 2;
//     }
//     x++;
//     return x + 1;
// }

// void testPassOne() {
//     for(double x=0;;);
//     for(double x=0;;x++);
//     for(double x=0;x++<6;);
//     for(double x=0;x++<6;x++);

//     double x;
//     for(x=0;x++<6;x++);

//     while(x=5<7) x=x=x++;
// }

// double testPassTwo(int x)
// {
//     double a = x;
//     double &b = a;
//     double *d = &a;
//     testPassTwo(*d);
//     testPassTwo(x)+1;
//     x = testPassTwo(x);
//     x = true ? testPassTwo(1) + 1 : testPassTwo(2);
//     double y = testPassTwo(x);
//     double z = testPassTwo(x) + testPassTwo(x+1);
//     if (testPassTwo(x) > 0)
//         return testPassTwo(2); /*F*/
//     return testPassTwo(x)+1;
// }
