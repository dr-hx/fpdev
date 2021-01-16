// #include <stdlib.h>
// #include <stdio.h>

#define HELLO d + x

#pragma K
int main(int argc, char const *argv[])
{
    if (double d = 0; double x = 5)
        HELLO;

    int *p;

    *p = 5;


    for (int i = 0, j = 0;; i++, --j)
    {
    }

    
    double t;
    double &q = t;
    for (double u = (0.0, t = 1);;)
        ;
    for (t = 0.0, *p = 1;;)
        ;
    
    switch (int c = 1)
    {
    case 1:
        c = 2;
        break;
    default:
        break;
    }

    while(int i=0)
        i++;

    double x, y = 2, z = 4;

    return 0;
}
typedef double FP;

class Test
{
public:
    double test(double f) { return *this + 0; }
    double operator+(double x)
    {
        return test(x);
        ;
    }
};

FP f(FP x, int y)
{
    if (f(f(x + 2, y), y + 1) > 0)
    {
        Test tt;
        return tt + x + 2;
    }
    x++;
    return x + 1;
}

void testPassOne() {
    for(double x=0;;);
    for(double x=0;;x++);
    for(double x=0;x++<6;);
    for(double x=0;x++<6;x++);

    double x;
    for(x=0;x++<6;x++);

    while(x=5<7) x=x=x++;
}

double testPassTwo(int x)
{
    double a = x;
    double &b = a;
    testPassTwo(x);
    testPassTwo(x)+1;
    x = testPassTwo(x);
    x = true ? testPassTwo(1) + 1 : testPassTwo(2);
    double y = testPassTwo(x);
    double z = testPassTwo(x) + testPassTwo(x+1);
    if (testPassTwo(x) > 0)
        return testPassTwo(2); /*F*/
    return testPassTwo(x)+1;
}
