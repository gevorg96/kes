// Стандартная библиотека игрушечного языка

#include <stdio.h>
#include <stdint.h>

int32_t builtin_input() {
  int32_t value = 0;
  if (scanf("%d", &value) != 1) {
    return 0;
  }

  return value;
}

void builtin_print(int32_t value) { printf("%d\n", value); }
