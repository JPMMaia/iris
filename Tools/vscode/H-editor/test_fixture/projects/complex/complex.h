typedef struct Complex
{
    float real;
    float imaginary;
} Complex;

Complex add(Complex lhs, Complex rhs);

enum Precision
{
    Low,
    Medium,
    High,
};

typedef Precision Precision_t;

typedef union Number
{
    short int16;
    int int32;
    float float32;
} Number;

#define PI 3.14
