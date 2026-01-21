#include "src/types/key/web_styles.h"

void append_key_styles(String& html) {
  html += R"html(
  <style>
    .tile.key { display:flex; flex-direction:column; align-items:center; justify-content:center; }
    .tile.key .tile-title { text-align:center; align-self:auto; width:100%; margin-top:4px; }
    .tile.key .tile-icon { margin-bottom:4px; }
  </style>
)html";
}
