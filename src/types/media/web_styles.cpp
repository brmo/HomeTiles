#include "src/types/media/web_styles.h"

void append_media_styles(String& html) {
  html += R"html(
  <style>
    .tile.media { position:relative; overflow:hidden; }
    .tile.media .tile-icon {
      position:absolute;
      top:4px;
      left:6px;
    }
    .tile.media .tile-title {
      position:absolute;
      top:8px;
      right:8px;
      text-align:right;
      max-width:60%;
      overflow:hidden;
      text-overflow:ellipsis;
      white-space:nowrap;
    }
    .tile.media .tile-media-title,
    .tile.media .tile-media-subtitle,
    .tile.media .tile-media-state {
      display:none;
    }
  </style>
)html";
}
