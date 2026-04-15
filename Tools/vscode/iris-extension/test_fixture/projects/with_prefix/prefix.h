typedef struct MY_Complex
{
    float real;
    float imaginary;
} MY_Complex;

MY_Complex MY_add(MY_Complex lhs, MY_Complex rhs);

typedef struct Other
{
    float real;
    float imaginary;
} Other;

Other add_other(Other lhs, Other rhs);
