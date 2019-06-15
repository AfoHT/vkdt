#include "module.h"
#include <math.h>
#include <stdlib.h>

void commit_params(dt_graph_t *graph, dt_module_t *module)
{
  float *f = (float *)module->committed_param;
  float *p = (float *)module->param;
  // copy x and y
  for(int k=0;k<8;k++) f[k] = p[k];
  // init tangent
  const int n=4;
  float m[5], d[5];
  for(int i=0;i<n-1;i++)
    d[i] = (p[i+5] - p[i+4])/(p[i+1] - p[i]);
  d[n-1] = d[n-2];
  m[0] = d[0];
  m[n-1] = d[n-1];
  for(int i=1;i<n-1;i++)
    m[i] = (d[i-1] + d[i])*.5f;
  // monotone hermite clamping:
  for(int i=0;i<n;i++)
  {
    if(fabsf(d[i]) <= 1e-8f)
      m[i] = m[i+1] = 0.0f;
    else
    {
      const float alpha = m[i]   / d[i];
      const float beta  = m[i+1] / d[i];
      const float tau   = alpha * alpha + beta * beta;
      if(tau > 9.0f)
      {
        m[i]   = 3.0f * m[i]   / sqrtf(tau);
        m[i+1] = 3.0f * m[i+1] / sqrtf(tau);
      }
    }
  }
  for(int k=0;k<4;k++) f[8+k] = m[k];
}

int init(dt_module_t *mod)
{
  mod->committed_param_size = sizeof(float)*12;
  mod->committed_param = malloc(mod->committed_param_size);
  return 0;
}

void cleanup(dt_module_t *mod)
{
  free(mod->committed_param);
}
