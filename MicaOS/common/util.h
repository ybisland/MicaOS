#ifndef NORA_UTIL_H
#define NORA_UTIL_H

#define UTIL_PRIMITIVE_CAT(a, b) a##b
#define UTIL_CAT(a, b) UTIL_PRIMITIVE_CAT(a, b)

#define NORA_INTERNAL_MAX(a, b) (((a) > (b)) ? (a) : (b))
#define NORA_INTERNAL_MIN(a, b) (((a) < (b)) ? (a) : (b))

#endif /* NORA_UTIL_H */
