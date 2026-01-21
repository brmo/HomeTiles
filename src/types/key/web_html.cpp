#include "src/types/key/web_html.h"

void append_key_fields_html(String& html, const String& tab_id) {
  html += R"html(
            <!-- Key Fields -->
            <div id=")html";
  html += tab_id;
  html += R"html(_key_fields" class="type-fields">
              <label>Makro</label>
              <input type="text" id=")html";
  html += tab_id;
  html += R"html(_key_macro" placeholder="z.B. ctrl+g">
              <div style="font-size:11px;color:#64748b;margin-top:4px;">Beispiele: g, ctrl+g, ctrl+shift+a</div>
            </div>
)html";
}
