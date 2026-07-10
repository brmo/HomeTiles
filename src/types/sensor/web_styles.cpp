#include "src/types/sensor/web_styles.h"

void append_sensor_styles(String& html) {
  html += R"html(
  <style>
    .tile.sensor { display:grid; grid-template-rows:auto 1fr; grid-template-columns:1fr; }
    .tile.sensor .tile-title {
      text-align:right;
      align-self:start;
      width:auto;
      grid-column:1;
      grid-row:1;
      justify-self:end;
      max-width:70%;
      overflow:hidden;
      text-overflow:ellipsis;
      white-space:nowrap;
    }
    /* Wie auf dem Display: Wert+Einheit ein Label in derselben Schrift,
       horizontal zentriert, vertikal Mitte + value-dy (LV_ALIGN_CENTER 0,28) */
    .tile-value {
      position:absolute;
      left:0;
      right:0;
      top:50%;
      transform:translateY(calc(-50% + var(--value-dy, 14px)));
      margin:0;
      color:#fff;
      font-size:var(--fs28, 14px);
      font-weight:normal;
      text-align:center;
      line-height:1;
    }
    .tile-value.sensor-value-size-default { font-size:var(--fs28, 14px); }
    .tile-value.sensor-value-size-40 { font-size:var(--fs40, 20px); }
    .tile-value.sensor-value-size-32 { font-size:var(--fs32, 16px); }
    .tile-value.sensor-value-size-24 { font-size:var(--fs24, 12px); }
    .tile-value.sensor-value-size-20 { font-size:var(--fs20, 10px); }
    .tile-unit { color:inherit; font-size:inherit; margin-left:0.28em; }
    .tile.sensor .tile-icon {
      position:absolute;
      top:8px;
      left:6px;
    }
  </style>
)html";
}
