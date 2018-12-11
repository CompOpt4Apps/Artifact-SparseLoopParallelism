# Unroll-and-jam nonrectangular loop nest using example of triangular
# matrix multiply. Compare with paper "Register Tiling in
# Nonrectangular Iteration Spaces" by Jimenez et al, TOPLAS 2002.

from chill import *

source('trmm.c')
procedure('trmm')
loop(0)

original()
unroll(0,2,2)
unroll(0,3,2)
print_code()

