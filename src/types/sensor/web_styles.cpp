#include "src/types/sensor/web_styles.h"

void append_sensor_styles(String& html) {
  html += R"html(
  <style>
    .tile.sensor { display:grid; grid-template-rows:auto 1fr; grid-template-columns:1fr; }
    .tile.sensor .tile-title {
      text-align:left;
      align-self:start;
      width:100%;
    }
    .tile-value {
      color:#fff;
      font-size:24px;
      font-weight:normal;
      text-align:center;
      opacity:0.95;
      line-height:1;
      align-self:center;
      justify-self:center;
      margin-left:-15px;
      margin-top:9px;
    }
    .tile-value.sensor-value-size-default { font-size:28px; }
    .tile-value.sensor-value-size-24 { font-size:24px; }
    .tile-value.sensor-value-size-20 { font-size:20px; }
    .tile-unit { color:#e6e6e6; font-size:14px; opacity:0.95; margin-left:7px; }
    .tile.sensor .tile-icon {
      position:absolute;
      top:10px;
      right:8px;
    }
  </style>
)html";
}
