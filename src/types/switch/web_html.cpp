#include "src/types/switch/web_html.h"

void append_switch_fields_html(String& html, const String& tab_id, const std::vector<String>& switchOptions) {
  html += R"html(
            <!-- Switch Fields -->
            <div id=")html";
  html += tab_id;
  html += R"html(_switch_fields" class="type-fields">
              <label>Schalter/Licht</label>
              <select id=")html";
  html += tab_id;
  html += R"html(_switch_entity">
                <option value="">Keine Auswahl</option>
)html";

  for (const auto& opt : switchOptions) {
    html += "<option value=\"";
    appendHtmlEscaped(html, opt);
    html += "\">";
    String label = humanizeIdentifier(opt, true) + " - " + opt;
    appendHtmlEscaped(html, label);
    html += "</option>";
  }

  html += R"html(
              </select>
              <label>Anzeige</label>
              <select id=")html";
  html += tab_id;
  html += R"html(_switch_style">
                <option value="0">Icon Button</option>
                <option value="1">LVGL Switch</option>
              </select>
            </div>
)html";
}
