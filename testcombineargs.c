#include "combineargs.h"

#include <stdio.h>

void main(void) {
    printf("Test zero: %s\n",combineargs(23,"one","twooo","three",NULL));
    printf("Test one: %s\n",combineargs(23,"one","twooo","three",NULL));
    printf("Test two: %s\n",combineargs(23,"one","twooo","three",NULL));
}
