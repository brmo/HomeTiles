#include "src/types/navigate/web_styles.h"

void append_navigate_styles(String& html) {
  html += R"html(
  <style>
    .tile.navigate { display:flex; flex-direction:column; align-items:center; justify-content:center; }
    .tile.navigate .tile-title {
      text-align:center;
      align-self:auto;
      width:100%;
      margin-top:4px;
      min-height:calc(1.3em + 2px);
      padding:0 1px 2px;
      line-height:1.3;
      overflow:visible;
      text-overflow:clip;
    }
    .tile.navigate .tile-icon { margin-bottom:4px; }
  </style>
)html";
}
