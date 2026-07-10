#include "src/types/text/web_styles.h"

void append_text_styles(String& html) {
  html += R"html(
  <style>
    .tile.text { display:grid; grid-template-rows:auto 1fr; grid-template-columns:1fr; }
    .tile.text .tile-title { text-align:left; align-self:start; width:100%; }
    .tile.text .tile-text {
      color:#fff;
      font-size:var(--fs28, 14px);
      text-align:center;
      white-space:pre-wrap;
      line-height:1.2;
      align-self:center;
      justify-self:center;
      padding:0 8px;
      overflow:hidden;
    }
    .tile.text .tile-text.sensor-value-size-default { font-size:var(--fs28, 14px); }
    .tile.text .tile-text.sensor-value-size-40 { font-size:var(--fs40, 20px); }
    .tile.text .tile-text.sensor-value-size-32 { font-size:var(--fs32, 16px); }
    .tile.text .tile-text.sensor-value-size-24 { font-size:var(--fs24, 12px); }
    .tile.text .tile-text.sensor-value-size-20 { font-size:var(--fs20, 10px); }
    .tile.text .tile-icon { position:absolute; top:10px; right:8px; }
    .tile-settings textarea {
      font-size:14px;
    }
  </style>
)html";
}
