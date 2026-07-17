#include "src/types/climate/web_html.h"

#include "src/core/config_manager.h"
#include "src/core/i18n.h"
#include "src/web/web_admin_utils.h"

void append_climate_fields_html(String& html,
                                const String& tab_id,
                                const std::vector<String>& climate_options) {
  const auto& tr = i18n::strings(configManager.getConfig().language);
  html += "<div id=\"";
  html += tab_id;
  html += "_climate_fields\" class=\"type-fields\"><label>";
  html += strcmp(tr.html_lang, "de") == 0 ? "Klima-Entity" : "Climate Entity";
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
  html += "</option></select></div>\n";
}
