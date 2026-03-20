#include "src/types/weather/web_html.h"
#include "src/web/web_admin_utils.h"

void append_weather_fields_html(String& html, const String& tab_id, const std::vector<String>& weatherOptions) {
  html += R"html(
            <!-- Weather Fields -->
            <div id=")html";
  html += tab_id;
  html += R"html(_weather_fields" class="type-fields">
              <label>Weather Entity</label>
              <select id=")html";
  html += tab_id;
  html += R"html(_weather_entity">
                <option value="">Keine Auswahl</option>
)html";

  for (const auto& opt : weatherOptions) {
    html += "<option value=\"";
    appendHtmlEscaped(html, opt);
    html += "\">";
    String label = humanizeIdentifier(opt, true) + " - " + opt;
    appendHtmlEscaped(html, label);
    html += "</option>";
  }

  html += R"html(
              </select>
              <label>Popup oeffnen</label>
              <select id=")html";
  html += tab_id;
  html += R"html(_weather_popup_open_mode">
                <option value="0">Long Press</option>
                <option value="1">Short Press</option>
              </select>
            </div>
)html";
}
