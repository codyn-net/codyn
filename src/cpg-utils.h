#ifndef __CPG_UTILS_H__
#define __CPG_UTILS_H__

#include <stdlib.h>

#define cpg_new(Type, Num) ((Type *)malloc (sizeof (Type) * Num))
#define cpg_new1(Type) cpg_new(Type, 1)

#define array_resize(Ptr, Type, Num) (Ptr = (Type *)realloc(Ptr, sizeof(Type) * Num))

#endif /* __CPG_UTILS_H__ */
