#include <stdio.h>
#include <string.h>
#include "gbv.h"

int main () {
    Library lib;
    char *biblioteca = "arquivoteste.gbv";

    if (gbv_open(&lib, biblioteca) != 0) {
        printf("Erro ao abrir biblioteca 1 %s\n", biblioteca);
        return 1;
    }
    if (gbv_open(&lib, biblioteca) != 0) {
        printf("Erro ao abrir biblioteca 2 %s\n", biblioteca);
        return 1;
    }

    gbv_add(&lib, biblioteca, "documents/jukebox_mercenaria.pdf");


    return 0;
}
