#include "src/types/sensor/web_html.h"

void append_sensor_fields_html(String& html, const String& tab_id, const std::vector<String>& sensorOptions) {
  html += R"html(
            <!-- Sensor Fields -->
            <div id=")html";
  html += tab_id;
  html += R"html(_sensor_fields" class="type-fields">
              <label>Sensor Entity</label>
              <select id=")html";
  html += tab_id;
  html += R"html(_sensor_entity">
                <option value="">Keine Auswahl</option>
)html";

  for (const auto& opt : sensorOptions) {
    html += "<option value=\"";
    appendHtmlEscaped(html, opt);
    html += "\">";
    String label = humanizeIdentifier(opt, true) + " - " + opt;
    appendHtmlEscaped(html, label);
    html += "</option>";
  }

  html += R"html(
              </select>
              <label>Einheit</label>
              <input type="text" id=")html";
  html += tab_id;
  html += R"html(_sensor_unit" placeholder="z.B. °C">
                <label>Nachkommastellen (leer = Originalwert)</label>
                <input type="number" id=")html";
  html += tab_id;
  html += R"html(_sensor_decimals" min="0" max="6" step="1" placeholder="z.B. 1">
                <label>Wert-Groesse</label>
                <select id=")html";
  html += tab_id;
  html += R"html(_sensor_value_font">
                  <option value="0">Standard</option>
                  <option value="1">20</option>
                  <option value="2">24</option>
                </select>
                <label class="inline-checkbox">
                  <input type="checkbox" id=")html";
  html += tab_id;
  html += R"html(_sensor_gauge">
                  Zeiger-Gauge anzeigen
                </label>
                <div id=")html";
  html += tab_id;
  html += R"html(_sensor_gauge_fields" class="gauge-fields hidden">
                  <label>Gauge Min</label>
                  <input type="number" id=")html";
  html += tab_id;
  html += R"html(_sensor_gauge_min" step="1" placeholder="z.B. 0">
                  <label>Gauge Max</label>
                  <input type="number" id=")html";
  html += tab_id;
  html += R"html(_sensor_gauge_max" step="1" placeholder="z.B. 100">
                </div>
            </div>
)html";
}
