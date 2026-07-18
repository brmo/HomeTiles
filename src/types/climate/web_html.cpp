#include "src/types/climate/web_html.h"

#include "src/core/config_manager.h"
#include "src/core/i18n.h"
#include "src/tiles/tile_config.h"
#include "src/web/web_admin_utils.h"

void append_climate_fields_html(String& html,
                                const String& tab_id,
                                const std::vector<String>& climate_options) {
  const auto& tr = i18n::strings(configManager.getConfig().language);
  const bool german =
      String(configManager.getConfig().language).startsWith("de");
  html += "<div id=\"";
  html += tab_id;
  html += "_climate_fields\" class=\"type-fields\"><label>";
  html += i18n::climate_entity_label(configManager.getConfig().language);
  html += "</label><select id=\"";
  html += tab_id;
  html += "_climate_entity\"><option value=\"\">";
  html += tr.no_selection;
  html += "</option>";
  for (const auto& entity : climate_options) {
    html += "<option value=\"";
    appendHtmlEscaped(html, entity);
    html += "\">";
    appendHtmlEscaped(html, humanizeIdentifier(entity, true) + " - " + entity);
    html += "</option>";
  }
  html += "</select><label>";
  html += tr.popup_open;
  html += "</label><select id=\"";
  html += tab_id;
  html += "_climate_popup_open_mode\"><option value=\"1\">";
  html += tr.short_press;
  html += "</option><option value=\"0\">";
  html += tr.long_press;
  html += "</option></select>";

  html += "<div class=\"climate-content-config\"><label class=\"climate-content-heading\">";
  html += german ? "Tile-Inhalt" : "Tile content";
  html += "</label><div class=\"climate-content-grid\">";
  for (uint8_t slot = 0; slot < CLIMATE_TILE_MAX_CONTENT_SLOTS; ++slot) {
    html += "<div id=\"";
    html += tab_id;
    html += "_climate_slot_row_";
    html += String(slot);
    html += "\" class=\"climate-content-slot\"><label>";
    html += german ? "Element " : "Item ";
    html += String(slot + 1);
    html += "</label><select id=\"";
    html += tab_id;
    html += "_climate_slot_";
    html += String(slot);
    html += "\">";

    auto append_option = [&](uint8_t value,
                             const char* en,
                             const char* de) {
      html += "<option value=\"";
      html += String(value);
      html += "\">";
      html += german ? de : en;
      html += "</option>";
    };
    append_option(CLIMATE_TILE_CONTENT_AUTO,
                  "Automatic", "Automatisch");
    append_option(CLIMATE_TILE_CONTENT_EMPTY,
                  "Empty", "Leer");
    append_option(CLIMATE_TILE_CONTENT_CURRENT_TEMPERATURE,
                  "Current temperature", "Aktuelle Temperatur");
    append_option(CLIMATE_TILE_CONTENT_CURRENT_HUMIDITY,
                  "Current humidity", "Aktuelle Luftfeuchtigkeit");
    append_option(CLIMATE_TILE_CONTENT_TARGET_TEMPERATURE,
                  "Target temperature", "Solltemperatur");
    append_option(CLIMATE_TILE_CONTENT_TARGET_TEMPERATURE_LOW,
                  "Heating target", "Heiz-Sollwert");
    append_option(CLIMATE_TILE_CONTENT_TARGET_TEMPERATURE_HIGH,
                  "Cooling target", "K\u00FChl-Sollwert");
    append_option(CLIMATE_TILE_CONTENT_TARGET_HUMIDITY,
                  "Target humidity", "Soll-Luftfeuchtigkeit");
    append_option(CLIMATE_TILE_CONTENT_HVAC_MODE,
                  "Mode", "Modus");
    html += "</select><div id=\"";
    html += tab_id;
    html += "_climate_layout_row_";
    html += String(slot);
    html += "\" class=\"climate-target-layout hidden\"><label>";
    html += german ? "Regler-Ausrichtung" : "Control orientation";
    html += "</label><select id=\"";
    html += tab_id;
    html += "_climate_layout_";
    html += String(slot);
    html += "\"><option value=\"0\">";
    html += german ? "Automatisch" : "Automatic";
    html += "</option><option value=\"1\">";
    html += german ? "Waagerecht" : "Horizontal";
    html += "</option><option value=\"2\">";
    html += german ? "Senkrecht" : "Vertical";
    html += "</option></select></div></div>";
  }
  html += "</div><div id=\"";
  html += tab_id;
  html += "_climate_content_hint\" class=\"field-hint climate-content-hint\"></div></div>";
  html += "</div>\n";
}
