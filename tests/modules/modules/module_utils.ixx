#include <iostream>
export module utils;

import math;       

export void print_sum(int a, int b) {
    int result = add(a, b);
    std::cout << "Sum of " << a << " and " << b << " is " << result << std::endl;
}
