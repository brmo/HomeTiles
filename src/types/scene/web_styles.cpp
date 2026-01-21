#include "src/types/scene/web_styles.h"

void append_scene_styles(String& html) {
  html += R"html(
  <style>
    .tile.scene { display:flex; flex-direction:column; align-items:center; justify-content:center; }
    .tile.scene .tile-title { text-align:center; align-self:auto; width:100%; margin-top:4px; }
    .tile.scene .tile-icon { margin-bottom:4px; }
  </style>
)html";
}
