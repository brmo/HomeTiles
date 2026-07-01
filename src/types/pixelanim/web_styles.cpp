#include "src/types/pixelanim/web_styles.h"

void append_pixelanim_styles(String& html) {
  // The WebUI tile preview for an animation just shows the (black) card with an
  // optional title; no dedicated preview styling is required.
  html += R"html(
  <style>
    .tile.animation { background:#000; }
    .tile.animation .tile-title { text-align:left; align-self:start; width:100%; }
  </style>
)html";
}
