#include "src/types/template/web_styles.h"

void append_template_styles(String& html) {
  html += R"html(
  <style>
    .tile.template { }
  </style>
)html";
}
