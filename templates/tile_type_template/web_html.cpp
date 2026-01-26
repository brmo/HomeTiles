#include "src/types/template/web_html.h"

void append_template_fields_html(String& html, const String& tab_id) {
  html += R"html(
            <div id=")html";
  html += tab_id;
  html += R"html(_template_fields" class="type-fields">
              <label>Field label</label>
              <input id=")html";
  html += tab_id;
  html += R"html(_template_value" type="text">
            </div>
)html";
}
