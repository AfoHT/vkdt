#include "../../../core/lut.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

int main(int argc, char *argv[])
{
  FILE *f = argc > 1 ? fopen(argv[1], "rb") : 0;
  if(!f)
  {
    fprintf(stderr, "usage: lutinfo <camera model>.lut\n"
                    "print metadata information that may be present in lut file\n");
    exit(1);
  }
  dt_lut_header_t header;
  fread(&header, sizeof(header), 1, f);
  size_t sz = header.datatype == dt_lut_header_f16 ? sizeof(uint16_t) : sizeof(float);
  fprintf(stderr, "lut with %d x %d with %d channels at %zu bytes per channel\n",
      header.wd, header.ht, header.channels, sz);
  sz *= header.wd * header.ht * header.channels;
  fseek(f, sz+sizeof(header), SEEK_SET);
  while(!feof(f))
  {
    char buf[BUFSIZ];
    fscanf(f, "%[^\n]", buf);
    fgetc(f);
    fprintf(stdout, "%s\n", buf);
    buf[0] = 0;
  }
  exit(0);
}
