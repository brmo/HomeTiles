#include "src/types/image/web_styles.h"

void append_image_styles(String& html) {
  html += R"html(
  <style>
    .tile.image { display:flex; flex-direction:column; align-items:center; justify-content:center; }
    .tile.image .tile-title { text-align:center; align-self:auto; width:100%; margin-top:4px; }
    .tile.image .tile-icon { margin-bottom:4px; }
  </style>
)html";
}
