#pragma once
#include "imgui.h"
#include "imgui_internal.h"
#include <algorithm>

namespace ImGui {

namespace {
void draw_star(float u, float v, float size, uint32_t col)
{
  float c[] = {
     0,         1,
     0.951057,  0.309017,
     0.587785, -0.809017,
    -0.587785, -0.809017,
    -0.951057,  0.309017};
  float x[20];
#if 0
  for(int i=0;i<5;i++)
  { // outline
    x[4*i+0] = u +     size * c[((2*i  )%10)+0];
    x[4*i+1] = v +     size * c[((2*i  )%10)+1];
    x[4*i+2] = u - 0.5*size * c[((2*i+6)%10)+0];
    x[4*i+3] = v - 0.5*size * c[((2*i+6)%10)+1];
  }
#endif
  for(int i=0;i<5;i++)
  { // classic pentagram
    x[2*i+0] = u + size * c[((4*i  )%10)+0];
    x[2*i+1] = v + size * c[((4*i  )%10)+1];
  }
  // imgui's line aa doesn't really do what you'd expect, so we draw
  // really thick lines to not look ridiculous for these sharp angles:
  ImGui::GetWindowDrawList()->AddPolyline(
      (ImVec2 *)x, 5, col, true, 2*size);//vkdt.state.center_ht/50.0f);
}
}

// this is pretty much ImGui::ImageButton with a few custom hacks:
uint32_t ThumbnailImage(
    ImTextureID user_texture_id,
    const ImVec2& size,
    const ImVec2& uv0,
    const ImVec2& uv1,
    int frame_padding,
    const ImVec4& bg_col,
    const ImVec4& tint_col,
    uint16_t rating,
    uint16_t labels)
{
  ImGuiWindow* window = GetCurrentWindow();
  if (window->SkipItems)
    return 0;

  ImGuiContext& g = *GImGui;
  const ImGuiStyle& style = g.Style;

  // Default to using texture ID as ID. User can still push string/integer prefixes.
  // We could hash the size/uv to create a unique ID but that would prevent the user from animating UV.
  PushID((void*)(intptr_t)user_texture_id);
  const ImGuiID id = window->GetID("#image");
  PopID();

  // make sure we're square:
  float padding = frame_padding >= 0 ? frame_padding : style.FramePadding[0];
  float wd = std::max(size[0], size[1]) + 2*padding;
  // overloaded operators are not exposed in imgui_internal, so we'll do the hard way:
  const ImRect bb(window->DC.CursorPos, ImVec2(window->DC.CursorPos[0] + wd, window->DC.CursorPos[1] + wd));
  ImVec2 off((wd - size[0])*.5f, (wd - size[1])*.5f);
  const ImRect image_bb(
      ImVec2(window->DC.CursorPos[0] + off[0],
        window->DC.CursorPos[1] + off[1]),
      ImVec2(window->DC.CursorPos[0] + off[0] + size[0],
        window->DC.CursorPos[1] + off[1] + size[1]));
  ItemSize(bb);
  if (!ItemAdd(bb, id))
    return 0;

  // TODO: double click? stars clicked?
  bool hovered, held;
  bool pressed = ButtonBehavior(bb, id, &hovered, &held);

  // Render
  const ImU32 col = GetColorU32((held && hovered) ? ImGuiCol_ButtonActive : hovered ? ImGuiCol_ButtonHovered : ImGuiCol_Button);
  RenderNavHighlight(bb, id);
  RenderFrame(bb.Min, bb.Max, col, true, ImClamp(padding, 0.0f, style.FrameRounding));
  if (bg_col.w > 0.0f)
    window->DrawList->AddRectFilled(image_bb.Min, image_bb.Max, GetColorU32(bg_col));
  window->DrawList->AddImage(user_texture_id, image_bb.Min, image_bb.Max, uv0, uv1, GetColorU32(tint_col));

  // render decorations (colour labels/stars/etc?)
  uint32_t starcol = 0xff000000u;
  for(int i=0;i<rating;i++)
    draw_star(bb.Min[0] + 0.1*wd*(i+1), bb.Min[1] + 0.1*wd, 0.03*wd, starcol);

  return pressed ? 1 : 0;
}

} // namespace ImGui
