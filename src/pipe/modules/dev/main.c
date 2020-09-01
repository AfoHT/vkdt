#include "modules/api.h"
#include <math.h>
#include <stdlib.h>

void modify_roi_in(
    dt_graph_t *graph,
    dt_module_t *module)
{
  // request the full uncropped thing, we want the borders
  module->connector[0].roi.wd = module->connector[0].roi.full_wd;
  module->connector[0].roi.ht = module->connector[0].roi.full_ht;
  module->connector[0].roi.x = 0.0f;
  module->connector[0].roi.y = 0.0f;
  module->connector[0].roi.scale = 1.0f;
}

void modify_roi_out(
    dt_graph_t *graph,
    dt_module_t *module)
{
  // copy to output
  if(module->connector[0].chan == dt_token("rggb"))
  {
    const uint32_t *b = module->img_param.crop_aabb;
    module->connector[1].roi = module->connector[0].roi;
    // TODO: double check potential rounding issues here (align to mosaic blocks etc):
    module->connector[1].roi.full_wd = b[2] - b[0];
    module->connector[1].roi.full_ht = b[3] - b[1];
  }
  else
  {
    module->connector[1].roi = module->connector[0].roi;
  }
}

void
create_nodes(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  const int block =
    (module->connector[0].chan != dt_token("rggb")) ? 1 :
    (module->img_param.filters == 9u ? 3 : 2);
  dt_roi_t roi_half = module->connector[1].roi;
  roi_half.full_wd /= block;
  roi_half.full_ht /= block;
  roi_half.wd /= block;
  roi_half.ht /= block;
  roi_half.x  /= block;
  roi_half.y  /= block;

  const uint32_t *wbi = (uint32_t *)module->img_param.whitebalance;
  float black[4], white[4];
  uint32_t *blacki = (uint32_t *)black;
  uint32_t *whitei = (uint32_t *)white;
  const uint32_t *crop_aabb = module->img_param.crop_aabb;
  for(int k=0;k<4;k++)
    black[k] = module->img_param.black[k]/65535.0f;
  for(int k=0;k<4;k++)
    white[k] = module->img_param.white[k]/65535.0f;
  const float noise[2] = { module->img_param.noise_a, module->img_param.noise_b };
  uint32_t *noisei = (uint32_t *)noise;

  const int wd = roi_half.full_wd;
  const int ht = roi_half.full_ht;

  // wire 4 scales of downsample + assembly node
  int id_down[4] = {0};

  for(int i=0;i<4;i++)
  {
    assert(graph->num_nodes < graph->max_nodes);
    id_down[i] = graph->num_nodes++;
    graph->node[id_down[i]] = (dt_node_t) {
      .name   = dt_token("dev"),
      .kernel = dt_token("down"),
      .module = module,
      .wd     = wd,
      .ht     = ht,
      .dp     = 1,
      .num_connectors = 2,
      .connector = {{
        .name   = dt_token("input"),
        .type   = dt_token("read"),
        .chan   = dt_token("rgba"),
        .format = dt_token("f16"),
        .roi    = roi_half,
        .connected_mi = -1,
      },{
        .name   = dt_token("output"),
        .type   = dt_token("write"),
        .chan   = dt_token("rgba"),
        .format = dt_token("f16"),
        .roi    = roi_half,
      }},
      .push_constant_size = 5*sizeof(uint32_t),
      .push_constant = { wbi[0], wbi[1], wbi[2], wbi[3], i },
    };
  }
  // wire inputs:
  for(int i=1;i<4;i++)
    CONN(dt_node_connect(graph, id_down[i-1], 1, id_down[i], 0));

  // assemble
  assert(graph->num_nodes < graph->max_nodes);
  const uint32_t id_assemble = graph->num_nodes++;
  graph->node[id_assemble] = (dt_node_t) {
    .name   = dt_token("dev"),
    .kernel = dt_token("assemble"),
    .module = module,
    .wd     = wd,
    .ht     = ht,
    .dp     = 1,
    .num_connectors = 6,
    .connector = {{
      .name   = dt_token("s0"),
      .type   = dt_token("read"),
      .chan   = dt_token("rgba"),
      .format = dt_token("f16"),
      .roi    = roi_half,
      .connected_mi = -1,
    },{
      .name   = dt_token("s1"),
      .type   = dt_token("read"),
      .chan   = dt_token("rgba"),
      .format = dt_token("f16"),
      .roi    = roi_half,
      .connected_mi = -1,
    },{
      .name   = dt_token("s2"),
      .type   = dt_token("read"),
      .chan   = dt_token("rgba"),
      .format = dt_token("f16"),
      .roi    = roi_half,
      .connected_mi = -1,
    },{
      .name   = dt_token("s3"),
      .type   = dt_token("read"),
      .chan   = dt_token("rgba"),
      .format = dt_token("f16"),
      .roi    = roi_half,
      .connected_mi = -1,
    },{
      .name   = dt_token("s4"),
      .type   = dt_token("read"),
      .chan   = dt_token("rgba"),
      .format = dt_token("f16"),
      .roi    = roi_half,
      .connected_mi = -1,
    },{
      .name   = dt_token("output"),
      .type   = dt_token("write"),
      .chan   = dt_token("rgba"),
      .format = dt_token("f16"),
      .roi    = roi_half,
    }},
    .push_constant_size = 6*sizeof(uint32_t),
    .push_constant = { wbi[0], wbi[1], wbi[2], wbi[3], noisei[0], noisei[1] },
  };

  // wire downsampled to assembly stage:
  CONN(dt_node_connect(graph, id_down[0], 1, id_assemble, 1));
  CONN(dt_node_connect(graph, id_down[1], 1, id_assemble, 2));
  CONN(dt_node_connect(graph, id_down[2], 1, id_assemble, 3));
  CONN(dt_node_connect(graph, id_down[3], 1, id_assemble, 4));

  if(module->connector[0].chan == dt_token("rggb"))
  { // raw data. need to wrap into mosaic-aware nodes:
    assert(graph->num_nodes < graph->max_nodes);
    const uint32_t id_half = graph->num_nodes++;
    graph->node[id_half] = (dt_node_t) {
      .name   = dt_token("dev"),
      .kernel = dt_token("half"),
      .module = module,
      .wd     = roi_half.full_wd,
      .ht     = roi_half.full_ht,
      .dp     = 1,
      .num_connectors = 2,
      .connector = {{
        .name   = dt_token("input"),
        .type   = dt_token("read"),
        .chan   = dt_token("rggb"),
        .format = dt_token("ui16"),
        .roi    = module->connector[0].roi, // uncropped hi res
        .connected_mi = -1,
      },{
        .name   = dt_token("output"),
        .type   = dt_token("write"),
        .chan   = dt_token("rgba"),
        .format = dt_token("f16"),
        .roi    = roi_half, // cropped lo res
      }},
      // XXX noise levels, white and black points, crop window!
      .push_constant_size = 17*sizeof(uint32_t),
      .push_constant = {
        wbi[0], wbi[1], wbi[2], wbi[3],
        blacki[0], blacki[1], blacki[2], blacki[3],
        whitei[0], whitei[1], whitei[2], whitei[3],
        crop_aabb[0], crop_aabb[1], crop_aabb[2], crop_aabb[3],
        module->img_param.filters },
    };
    assert(graph->num_nodes < graph->max_nodes);
    const uint32_t id_doub = graph->num_nodes++;
    graph->node[id_doub] = (dt_node_t) {
      .name   = dt_token("dev"),
      .kernel = dt_token("doub"),
      .module = module,
      .wd     = roi_half.full_wd, // cropped lo res
      .ht     = roi_half.full_ht,
      .dp     = 1,
      .num_connectors = 3,
      .connector = {{
        .name   = dt_token("orig"),
        .type   = dt_token("read"),
        .chan   = dt_token("rggb"),
        .format = dt_token("ui16"),
        .roi    = module->connector[0].roi, // original rggb input
        .connected_mi = -1,
      },{
        .name   = dt_token("coarse"),
        .type   = dt_token("read"),
        .chan   = dt_token("rgba"),
        .format = dt_token("f16"),
        .roi    = roi_half, // cropped lo res
        .connected_mi = -1,
      },{
        .name   = dt_token("output"),
        .type   = dt_token("write"),
        .chan   = dt_token("rggb"),
        .format = dt_token("f16"),
        .roi    = module->connector[1].roi, // cropped hi res
      }},
      // TODO: needs crop window too!
      .push_constant_size = 17*sizeof(uint32_t),
      .push_constant = {
        wbi[0], wbi[1], wbi[2], wbi[3],
        blacki[0], blacki[1], blacki[2], blacki[3],
        whitei[0], whitei[1], whitei[2], whitei[3],
        crop_aabb[0], crop_aabb[1], crop_aabb[2], crop_aabb[3],
        module->img_param.filters },
    };
    CONN(dt_node_connect(graph, id_half,     1, id_down[0],  0));
    CONN(dt_node_connect(graph, id_half,     1, id_assemble, 0));
    CONN(dt_node_connect(graph, id_assemble, 5, id_doub,     1));
    dt_connector_copy(graph, module, 0, id_half, 0);
    dt_connector_copy(graph, module, 0, id_doub, 0);
    dt_connector_copy(graph, module, 1, id_doub, 2);
  }
  else
  { // wire module i/o connectors to nodes:
    dt_connector_copy(graph, module, 0, id_down[0], 0);
    dt_connector_copy(graph, module, 0, id_assemble, 0);
    dt_connector_copy(graph, module, 1, id_assemble, 5);
  }
}

