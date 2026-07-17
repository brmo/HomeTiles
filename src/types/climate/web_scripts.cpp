#include "src/types/climate/web_scripts.h"

void append_climate_scripts(String& html) {
  html += R"html(
  <script>
  function loadClimateFields(tab, data) {
    const entity = document.getElementById(tab + '_climate_entity');
    if (entity) entity.value = data.sensor_entity || data.climate_entity || '';
    const popup = document.getElementById(tab + '_climate_popup_open_mode');
    if (popup) popup.value = (data.popup_open_mode !== undefined)
      ? String(data.popup_open_mode) : '1';
    maybeFillTitleFromEntity(tab, '_climate_entity');
  }

  function parseClimatePreviewPayload(value) {
    const out = { current: '--', unit: '°C', mode: '', action: '' };
    if (value === undefined || value === null) return out;
    const text = String(value).trim();
    if (!text.length || !text.startsWith('{')) return out;
    try {
      const obj = JSON.parse(text);
      if (!obj || typeof obj !== 'object') return out;
      const attrs = obj.attributes && typeof obj.attributes === 'object'
        ? obj.attributes : obj;
      const current = attrs.current_temperature;
      if (current !== undefined && current !== null && Number.isFinite(Number(current))) {
        out.current = Number(current).toFixed(1).replace(/\.0$/, '');
      }
      out.unit = attrs.temperature_unit || attrs.unit_of_measurement || '°C';
      out.mode = String(obj.hvac_mode || obj.state || attrs.hvac_mode || '').toLowerCase();
      out.action = String(obj.hvac_action || attrs.hvac_action || '').toLowerCase();
    } catch (e) {}
    return out;
  }

  function climatePreviewIcon(state) {
    const action = String(state?.action || '').toLowerCase();
    const mode = String(state?.mode || '').toLowerCase();
    if (action === 'heating') return 'radiator';
    if (action === 'cooling') return 'snowflake';
    if (action === 'drying') return 'water-percent';
    if (action === 'fan') return 'fan';
    if (mode === 'off') return 'thermometer-off';
    if (action === 'idle' || action === 'off') return 'thermostat';
    if (mode === 'heat') return 'radiator';
    if (mode === 'cool') return 'snowflake';
    if (mode === 'dry') return 'water-percent';
    if (mode === 'fan_only') return 'fan';
    if (mode === 'auto' || mode === 'heat_cool') return 'thermostat-auto';
    return 'thermostat';
  }

  function climatePreviewColor(state) {
    const action = String(state?.action || '').toLowerCase();
    const mode = String(state?.mode || '').toLowerCase();
    if (action === 'heating') return '#ff8a3d';
    if (action === 'cooling') return '#4fc3f7';
    if (action === 'drying') return '#ffd54f';
    if (action === 'fan') return '#4db6ac';
    if (mode === 'off' || action === 'idle' || action === 'off') return '#9e9e9e';
    if (!action && mode === 'heat') return '#ff8a3d';
    if (!action && mode === 'cool') return '#4fc3f7';
    if (!action && mode === 'dry') return '#ffd54f';
    if (!action && mode === 'fan_only') return '#4db6ac';
    return '#ffffff';
  }

  function saveClimateFields(tab, formData) {
    formData.append('climate_entity',
      document.getElementById(tab + '_climate_entity')?.value || '');
    formData.append('popup_open_mode',
      document.getElementById(tab + '_climate_popup_open_mode')?.value || '1');
  }

  function resetClimateFields(tab) {
    const entity = document.getElementById(tab + '_climate_entity');
    if (entity) entity.value = '';
    const popup = document.getElementById(tab + '_climate_popup_open_mode');
    if (popup) popup.value = '1';
  }
  </script>
)html";
}
