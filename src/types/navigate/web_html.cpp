#include "src/types/navigate/web_html.h"

void append_navigate_fields_html(String& html, const String& tab_id, const String& navigateOptionsHtml) {
  html += R"html(
            <!-- Navigate Fields -->
            <div id=")html";
  html += tab_id;
  html += R"html(_navigate_fields" class="type-fields">
              <label>Ziel-Ordner</label>
              <select id=")html";
  html += tab_id;
  html += R"html(_navigate_target">
                <option value="0">Neuer Ordner</option>
)html";
  html += navigateOptionsHtml;
  html += R"html(
              </select>
              <div id=")html";
  html += tab_id;
  html += R"html(_navigate_note" style="font-size:11px;color:#64748b;margin-top:6px;"></div>
            </div>
)html";
}
