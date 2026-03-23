#include "src/types/weather/web_styles.h"

void append_weather_styles(String& html) {
  html += R"html(
  <style>
    .tile.weather { position:relative; }
    .tile.weather .tile-icon {
      position:absolute;
      top:4px;
      left:6px;
    }
    .tile.weather .tile-title {
      position:absolute;
      top:8px;
      right:8px;
      text-align:right;
      max-width:60%;
      overflow:hidden;
      text-overflow:ellipsis;
      white-space:nowrap;
    }
  </style>
)html";
}
