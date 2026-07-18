#include "src/types/climate/web_scripts.h"

void append_climate_scripts(String& html) {
  html += R"html(
  <script>
  const CLIMATE_TILE_CONTENT = Object.freeze({
    AUTO: 0,
    EMPTY: 1,
    CURRENT_TEMPERATURE: 2,
    CURRENT_HUMIDITY: 3,
    TARGET_TEMPERATURE: 4,
    TARGET_TEMPERATURE_LOW: 5,
    TARGET_TEMPERATURE_HIGH: 6,
    TARGET_HUMIDITY: 7,
    HVAC_MODE: 8
  });
  const CLIMATE_TARGET_LAYOUT = Object.freeze({
    AUTO: 0,
    HORIZONTAL: 1,
    VERTICAL: 2
  });
  const CLIMATE_LAYOUT_MAGIC = 0x434c0000;
  const CLIMATE_LAYOUT_MAGIC_MASK = 0xffff0000;
  const CLIMATE_LAYOUT_VALUE_MASK = 0x00000fff;

  function climateSlotCapacity(spanW, spanH) {
    const w = Math.max(1, Number(spanW) || 1);
    const h = Math.max(1, Number(spanH) || 1);
    if (w === 1 && h === 1) return 1;
    if (w >= 2 && h === 1) return 2;
    if (w === 1) return 3;
    return 6;
  }

  function decodeClimateSlotConfig(packedValue) {
    const packed = Math.max(0, Number(packedValue) || 0) >>> 0;
    return Array.from({ length: 6 }, (_, index) => {
      const value = (packed >>> (index * 4)) & 0x0f;
      return value <= CLIMATE_TILE_CONTENT.HVAC_MODE
        ? value : CLIMATE_TILE_CONTENT.AUTO;
    });
  }

  function packClimateSlotConfig(tab) {
    let packed = 0;
    for (let index = 0; index < 6; ++index) {
      const select = document.getElementById(
        tab + '_climate_slot_' + index);
      const value = Math.max(
        0, Math.min(
          CLIMATE_TILE_CONTENT.HVAC_MODE,
          Number(select?.value) || 0));
      packed |= (value & 0x0f) << (index * 4);
    }
    return packed >>> 0;
  }

  function currentClimateSlotConfig(tab) {
    return Array.from({ length: 6 }, (_, index) => {
      const select = document.getElementById(
        tab + '_climate_slot_' + index);
      return Number(select?.value) || 0;
    });
  }

  function getClimateLayoutPayload(storedValue) {
    const stored = Math.max(0, Number(storedValue) || 0) >>> 0;
    if (((stored & CLIMATE_LAYOUT_MAGIC_MASK) >>> 0) ===
        CLIMATE_LAYOUT_MAGIC) {
      return stored & CLIMATE_LAYOUT_VALUE_MASK;
    }
    return stored <= CLIMATE_LAYOUT_VALUE_MASK ? stored : 0;
  }

  function decodeClimateTargetLayouts(storedValue) {
    const packed = getClimateLayoutPayload(storedValue);
    return Array.from({ length: 6 }, (_, index) => {
      const value = (packed >>> (index * 2)) & 0x03;
      return value <= CLIMATE_TARGET_LAYOUT.VERTICAL
        ? value : CLIMATE_TARGET_LAYOUT.AUTO;
    });
  }

  function packClimateTargetLayouts(tab) {
    let packed = 0;
    for (let index = 0; index < 6; ++index) {
      const select = document.getElementById(
        tab + '_climate_layout_' + index);
      const value = Math.max(
        0, Math.min(
          CLIMATE_TARGET_LAYOUT.VERTICAL,
          Number(select?.value) || 0));
      packed |= (value & 0x03) << (index * 2);
    }
    return packed >>> 0;
  }

  function currentClimateTargetLayouts(tab) {
    return Array.from({ length: 6 }, (_, index) => {
      const select = document.getElementById(
        tab + '_climate_layout_' + index);
      return Number(select?.value) || CLIMATE_TARGET_LAYOUT.AUTO;
    });
  }

  function syncClimateSlotFields(tab) {
    const spanW = document.getElementById(
      tab + '_tile_span_w')?.value || 1;
    const spanH = document.getElementById(
      tab + '_tile_span_h')?.value || 1;
    const capacity = climateSlotCapacity(spanW, spanH);
    for (let index = 0; index < 6; ++index) {
      const row = document.getElementById(
        tab + '_climate_slot_row_' + index);
      row?.classList.toggle('hidden', index >= capacity);
      const selected = Number(document.getElementById(
        tab + '_climate_slot_' + index)?.value) || 0;
      const adjustable =
        selected >= CLIMATE_TILE_CONTENT.TARGET_TEMPERATURE &&
        selected <= CLIMATE_TILE_CONTENT.TARGET_HUMIDITY;
      document.getElementById(
        tab + '_climate_layout_row_' + index)
        ?.classList.toggle(
          'hidden', index >= capacity || !adjustable);
    }
    const hint = document.getElementById(
      tab + '_climate_content_hint');
    if (hint) {
      const german =
        String(document.documentElement.lang || '')
          .toLowerCase().startsWith('de');
      hint.textContent = german
        ? (capacity === 1
            ? 'Diese Tile-Gr\u00F6\u00DFe zeigt ein Element.'
            : 'Normale Werte belegen einen Rasterplatz. Ein Regler ' +
              'verbindet automatisch zwei benachbarte Rasterpl\u00E4tze.')
        : (capacity === 1
            ? 'This tile size shows one item.'
            : 'Normal values use one grid cell. A control automatically ' +
              'joins two adjacent grid cells.');
    }
  }

  function loadClimateFields(tab, data) {
    const entity = document.getElementById(tab + '_climate_entity');
    if (entity) entity.value = data.sensor_entity || data.climate_entity || '';
    const popup = document.getElementById(tab + '_climate_popup_open_mode');
    if (popup) popup.value = (data.popup_open_mode !== undefined)
      ? String(data.popup_open_mode) : '1';
    const slots = decodeClimateSlotConfig(
      data.climate_slots_packed ?? data.sensor_gauge_min ?? 0);
    slots.forEach((value, index) => {
      const select = document.getElementById(
        tab + '_climate_slot_' + index);
      if (select) select.value = String(value);
    });
    const layouts = decodeClimateTargetLayouts(
      data.climate_layouts_packed ?? data.sensor_gauge_max ?? 0);
    layouts.forEach((value, index) => {
      const select = document.getElementById(
        tab + '_climate_layout_' + index);
      if (select) select.value = String(value);
    });
    syncClimateSlotFields(tab);
    maybeFillTitleFromEntity(tab, '_climate_entity');
  }

  function parseClimatePreviewPayload(value) {
    const out = {
      current: '--',
      currentHumidity: null,
      target: null,
      targetHumidity: null,
      targetLow: null,
      targetHigh: null,
      unit: '\u00B0C',
      mode: '',
      action: '',
      preset: ''
    };
    if (value === undefined || value === null) return out;
    const text = String(value).trim();
    if (!text.length) return out;
    if (!text.startsWith('{')) {
      const numeric = Number(text.replace(',', '.'));
      if (Number.isFinite(numeric)) {
        out.current = formatLocalizedNumber(numeric, 1, true);
      }
      return out;
    }
    try {
      const obj = JSON.parse(text);
      if (!obj || typeof obj !== 'object') return out;
      const attrs = obj.attributes && typeof obj.attributes === 'object'
        ? obj.attributes : obj;
      const current = attrs.current_temperature;
      if (current !== undefined && current !== null && Number.isFinite(Number(current))) {
        out.current = formatLocalizedNumber(Number(current), 1, true);
      }
      const currentHumidity = attrs.current_humidity;
      if (currentHumidity !== undefined && currentHumidity !== null &&
          Number.isFinite(Number(currentHumidity))) {
        out.currentHumidity = formatLocalizedNumber(
          Number(currentHumidity), 0, true);
      }
      const target = attrs.temperature;
      if (target !== undefined && target !== null &&
          Number.isFinite(Number(target))) {
        out.target = formatLocalizedNumber(Number(target), 1, true);
      }
      const targetHumidity = attrs.humidity;
      if (targetHumidity !== undefined && targetHumidity !== null &&
          Number.isFinite(Number(targetHumidity))) {
        out.targetHumidity = formatLocalizedNumber(
          Number(targetHumidity), 0, true);
      }
      const targetLow = attrs.target_temp_low;
      if (targetLow !== undefined && targetLow !== null &&
          Number.isFinite(Number(targetLow))) {
        out.targetLow = formatLocalizedNumber(Number(targetLow), 1, true);
      }
      const targetHigh = attrs.target_temp_high;
      if (targetHigh !== undefined && targetHigh !== null &&
          Number.isFinite(Number(targetHigh))) {
        out.targetHigh = formatLocalizedNumber(Number(targetHigh), 1, true);
      }
      out.unit = attrs.temperature_unit || attrs.unit_of_measurement || '\u00B0C';
      out.mode = String(obj.hvac_mode || obj.state || attrs.hvac_mode || '').toLowerCase();
      out.action = String(obj.hvac_action || attrs.hvac_action || '').toLowerCase();
      out.preset = String(obj.preset_mode || attrs.preset_mode || '').toLowerCase();
    } catch (e) {}
    return out;
  }

  function climatePreviewIcon(state, baseIcon) {
    const action = String(state?.action || '').toLowerCase();
    const mode = String(state?.mode || '').toLowerCase();
    const fallback = normalizeMdiIconName(baseIcon) || 'thermostat';
    if (action === 'heating' || action === 'preheating') return 'fire';
    if (action === 'cooling') return 'snowflake';
    if (action === 'drying') return 'water-percent';
    if (action === 'fan') return 'fan';
    if (action === 'defrosting') return 'snowflake-melt';
    if (mode === 'off' || action === 'idle' || action === 'off' || action) {
      return fallback;
    }
    if (mode === 'heat') return 'fire';
    if (mode === 'cool') return 'snowflake';
    if (mode === 'dry') return 'water-percent';
    if (mode === 'fan_only') return 'fan';
    if (mode === 'heat_cool') return 'sun-snowflake-variant';
    if (mode === 'auto') return 'thermostat-auto';
    return fallback;
  }

  function climatePreviewColor(state) {
    const action = String(state?.action || '').toLowerCase();
    const mode = String(state?.mode || '').toLowerCase();
    if (action === 'heating' || action === 'preheating') return '#ff8a3d';
    if (action === 'cooling') return '#4fc3f7';
    if (action === 'drying') return '#ffd54f';
    if (action === 'fan') return '#4db6ac';
    if (action === 'defrosting') return '#81d4fa';
    if (mode === 'off' || action === 'idle' || action === 'off') return '#9e9e9e';
    if (!action && mode === 'heat') return '#ff8a3d';
    if (!action && mode === 'cool') return '#4fc3f7';
    if (!action && mode === 'dry') return '#ffd54f';
    if (!action && mode === 'fan_only') return '#4db6ac';
    return '#ffffff';
  }

  function climatePreviewSlots(
      state, spanW, spanH, slotConfig = null,
      targetLayoutConfig = null) {
    const w = Math.max(1, Number(spanW) || 1);
    const h = Math.max(1, Number(spanH) || 1);
    const capacity = climateSlotCapacity(w, h);
    const columns = w >= 2 ? 2 : 1;
    const configured = Array.isArray(slotConfig)
      ? slotConfig.slice(0, 6)
      : decodeClimateSlotConfig(slotConfig || 0);
    while (configured.length < 6) {
      configured.push(CLIMATE_TILE_CONTENT.AUTO);
    }
    const targetLayouts = Array.isArray(targetLayoutConfig)
      ? targetLayoutConfig.slice(0, 6)
      : decodeClimateTargetLayouts(targetLayoutConfig || 0);
    while (targetLayouts.length < 6) {
      targetLayouts.push(CLIMATE_TARGET_LAYOUT.AUTO);
    }

    const automatic = [];
    let automaticUnits = 0;
    const temp = (value) => String(value ?? '--') + ' ' + state.unit;
    const slotCost = (kind) =>
      kind >= CLIMATE_TILE_CONTENT.TARGET_TEMPERATURE &&
      kind <= CLIMATE_TILE_CONTENT.TARGET_HUMIDITY ? 2 : 1;
    const addAutomatic = (kind) => {
      const cost = slotCost(kind);
      if (automatic.length < capacity &&
          automaticUnits + cost <= capacity) {
        automatic.push(kind);
        automaticUnits += cost;
      }
    };

    if (w === 1 && h === 1) {
      addAutomatic(CLIMATE_TILE_CONTENT.CURRENT_TEMPERATURE);
    } else if (w >= 2 && h === 1) {
      if (state.targetLow !== null && state.targetHigh !== null) {
        addAutomatic(CLIMATE_TILE_CONTENT.TARGET_TEMPERATURE_LOW);
      } else if (state.target !== null) {
        addAutomatic(CLIMATE_TILE_CONTENT.TARGET_TEMPERATURE);
      } else if (state.targetHumidity !== null) {
        addAutomatic(CLIMATE_TILE_CONTENT.TARGET_HUMIDITY);
      } else {
        addAutomatic(CLIMATE_TILE_CONTENT.CURRENT_TEMPERATURE);
      }
    } else if (w === 1) {
      addAutomatic(CLIMATE_TILE_CONTENT.CURRENT_TEMPERATURE);
      if (state.targetLow !== null && state.targetHigh !== null) {
        addAutomatic(CLIMATE_TILE_CONTENT.TARGET_TEMPERATURE_LOW);
      } else if (state.target !== null) {
        addAutomatic(CLIMATE_TILE_CONTENT.TARGET_TEMPERATURE);
      }
      if (state.targetHumidity !== null) {
        addAutomatic(CLIMATE_TILE_CONTENT.TARGET_HUMIDITY);
      }
    } else {
      addAutomatic(CLIMATE_TILE_CONTENT.CURRENT_TEMPERATURE);
      if (state.currentHumidity !== null) {
        addAutomatic(CLIMATE_TILE_CONTENT.CURRENT_HUMIDITY);
      }
      if (state.targetLow !== null && state.targetHigh !== null) {
        addAutomatic(CLIMATE_TILE_CONTENT.TARGET_TEMPERATURE_LOW);
        addAutomatic(CLIMATE_TILE_CONTENT.TARGET_TEMPERATURE_HIGH);
      } else if (state.target !== null) {
        addAutomatic(CLIMATE_TILE_CONTENT.TARGET_TEMPERATURE);
      }
      if (state.targetHumidity !== null) {
        addAutomatic(CLIMATE_TILE_CONTENT.TARGET_HUMIDITY);
      }
      if (state.mode) {
        addAutomatic(CLIMATE_TILE_CONTENT.HVAC_MODE);
      }
    }

    const modeText = () => {
      const raw = String(state.mode || '').toLowerCase();
      if (!raw) return '--';
      const german =
        String(document.documentElement.lang || '')
          .toLowerCase().startsWith('de');
      const labels = german
        ? {
            off: 'Aus', heat: 'Heizen', cool: 'K\u00FChlen',
            auto: 'Auto', dry: 'Entfeuchten',
            fan_only: 'L\u00FCfter', heat_cool: 'Heizen/K\u00FChlen'
          }
        : {
            off: 'Off', heat: 'Heat', cool: 'Cool',
            auto: 'Auto', dry: 'Dry',
            fan_only: 'Fan only', heat_cool: 'Heat/Cool'
          };
      return labels[raw] || raw.replaceAll('_', ' ');
    };

    const german =
      String(document.documentElement.lang || '')
        .toLowerCase().startsWith('de');
    const targetCaption = (kind) => {
      if (kind === CLIMATE_TILE_CONTENT.TARGET_HUMIDITY) {
        return german ? 'Soll-Luftfeuchtigkeit' : 'Target humidity';
      }
      if (kind === CLIMATE_TILE_CONTENT.TARGET_TEMPERATURE_LOW) {
        return german ? 'Heizbetrieb' : 'Heating';
      }
      if (kind === CLIMATE_TILE_CONTENT.TARGET_TEMPERATURE_HIGH) {
        return german ? 'K\u00FChlbetrieb' : 'Cooling';
      }
      const action = String(state.action || '').toLowerCase();
      const mode = String(state.mode || '').toLowerCase();
      if (action === 'heating' || action === 'preheating' ||
          mode === 'heat') {
        return german ? 'Heizbetrieb' : 'Heating';
      }
      if (action === 'cooling' || mode === 'cool') {
        return german ? 'K\u00FChlbetrieb' : 'Cooling';
      }
      if (mode === 'heat_cool') {
        return german ? 'Heizen/K\u00FChlen' : 'Heat/Cool';
      }
      return german ? 'Solltemperatur' : 'Target temperature';
    };

    const slotForKind = (kind) => {
      switch (kind) {
        case CLIMATE_TILE_CONTENT.CURRENT_TEMPERATURE:
          return { kind, value: temp(state.current), adjustable: false };
        case CLIMATE_TILE_CONTENT.CURRENT_HUMIDITY:
          return {
            kind,
            value: state.currentHumidity !== null
              ? state.currentHumidity + '%' : '--%',
            adjustable: false
          };
        case CLIMATE_TILE_CONTENT.TARGET_TEMPERATURE:
          return {
            kind,
            value: temp(state.target),
            adjustable: true,
            caption: targetCaption(kind)
          };
        case CLIMATE_TILE_CONTENT.TARGET_TEMPERATURE_LOW:
          return {
            kind,
            value: temp(state.targetLow),
            adjustable: true,
            caption: targetCaption(kind)
          };
        case CLIMATE_TILE_CONTENT.TARGET_TEMPERATURE_HIGH:
          return {
            kind,
            value: temp(state.targetHigh),
            adjustable: true,
            caption: targetCaption(kind)
          };
        case CLIMATE_TILE_CONTENT.TARGET_HUMIDITY:
          return {
            kind,
            value: state.targetHumidity !== null
              ? state.targetHumidity + '%' : '--%',
            adjustable: true,
            caption: targetCaption(kind)
          };
        case CLIMATE_TILE_CONTENT.HVAC_MODE:
          return {
            kind, value: modeText(), adjustable: false
          };
        default:
          return null;
      }
    };

    const explicitlyConfigured = new Set();
    configured.slice(0, capacity).forEach(selection => {
      const kind = Number(selection) || 0;
      if (kind !== CLIMATE_TILE_CONTENT.AUTO &&
          kind !== CLIMATE_TILE_CONTENT.EMPTY) {
        explicitlyConfigured.add(kind);
      }
    });

    const slots = [];
    let usedUnits = 0;
    let automaticCursor = 0;
    for (let index = 0; index < capacity; ++index) {
      const selection = Number(configured[index]) || 0;
      if (selection === CLIMATE_TILE_CONTENT.EMPTY) continue;
      let kind = selection;
      let targetLayout = Number(targetLayouts[index]) || 0;
      if (selection === CLIMATE_TILE_CONTENT.AUTO) {
        kind = undefined;
        targetLayout = CLIMATE_TARGET_LAYOUT.AUTO;
        while (automaticCursor < automatic.length) {
          const candidate = automatic[automaticCursor++];
          if (explicitlyConfigured.has(candidate)) continue;
          kind = candidate;
          break;
        }
      }
      const slot = slotForKind(kind);
      if (!slot) continue;
      const cost = slotCost(kind);
      if (usedUnits + cost > capacity) continue;
      slots.push({
        ...slot,
        unitSpan: cost,
        targetLayout
      });
      usedUnits += cost;
    }

    if (w === 1 && h === 1) {
      if (!slots.length) return '';
      const value = slots[0].value;
      const unitSuffix = value.endsWith(state.unit)
        ? '<span class="tile-unit">' + state.unit + '</span>'
        : '';
      const plainValue = unitSuffix
        ? value.slice(0, -String(state.unit).length).trim()
        : value;
      return '<div class="tile-value climate-legacy-value">' +
        plainValue + unitSuffix + '</div>';
    }

    if (!slots.length) return '';
    const logicalRows = Math.max(1, Math.ceil(capacity / columns));
    const occupied = Array.from(
      { length: logicalRows },
      () => Array(columns).fill(false));
    const placedSlots = [];

    slots.forEach(slot => {
      let placement = null;
      const tryVertical = () => {
        for (let row = 0; row + 1 < logicalRows; ++row) {
          for (let column = 0; column < columns; ++column) {
            if (!occupied[row][column] &&
                !occupied[row + 1][column]) {
              return { row, column, rowSpan: 2, columnSpan: 1 };
            }
          }
        }
        return null;
      };
      const tryHorizontal = () => {
        if (columns < 2) return null;
        for (let row = 0; row < logicalRows; ++row) {
          if (!occupied[row][0] && !occupied[row][1]) {
            return { row, column: 0, rowSpan: 1, columnSpan: 2 };
          }
        }
        return null;
      };
      const tryValue = () => {
        for (let row = 0; row < logicalRows; ++row) {
          for (let column = 0; column < columns; ++column) {
            if (!occupied[row][column]) {
              return { row, column, rowSpan: 1, columnSpan: 1 };
            }
          }
        }
        return null;
      };

      if (slot.adjustable) {
        if (slot.targetLayout === CLIMATE_TARGET_LAYOUT.HORIZONTAL) {
          placement = tryHorizontal() || tryVertical();
        } else if (
            slot.targetLayout === CLIMATE_TARGET_LAYOUT.VERTICAL) {
          placement = tryVertical() || tryHorizontal();
        } else {
          placement = columns === 1
            ? tryVertical()
            : (logicalRows === 1
                ? tryHorizontal()
                : (tryVertical() || tryHorizontal()));
        }
      } else {
        placement = tryValue();
      }
      if (!placement) return;

      for (let row = placement.row;
           row < placement.row + placement.rowSpan; ++row) {
        for (let column = placement.column;
             column < placement.column + placement.columnSpan;
             ++column) {
          occupied[row][column] = true;
        }
      }
      placedSlots.push({ ...slot, ...placement });
    });

    return '<div class="climate-slots" style="--climate-columns:' +
      columns + '">' +
      placedSlots.map(slot => {
        const row = slot.row + 1;
        const column = slot.column + 1;
        const horizontal =
          slot.adjustable && slot.columnSpan === 2;
        const vertical =
          slot.adjustable && slot.rowSpan === 2;
        const fullWidthValue =
          !slot.adjustable && columns === 2 && placedSlots.length === 1;
        const gridStyle =
          'grid-column:' + column + ' / span ' +
          ((horizontal || fullWidthValue) ? 2 : 1) +
          ';grid-row:' + row + ' / span ' +
          (vertical ? 2 : 1);
        if (!slot.adjustable) {
          return '<div class="climate-slot climate-slot-value' +
            (slot.kind === CLIMATE_TILE_CONTENT.HVAC_MODE
              ? ' climate-slot-mode' : '') +
            '" style="' +
            gridStyle + '"><strong>' + slot.value + '</strong></div>';
        }
        return '<div class="climate-slot climate-slot-control ' +
          (horizontal
            ? 'climate-slot-control-horizontal'
            : 'climate-slot-control-vertical') +
          '" style="' + gridStyle + '">' +
          '<small>' + slot.caption + '</small>' +
          '<span class="climate-minus">-</span><strong>' +
          slot.value + '</strong><span class="climate-plus">+</span></div>';
      }).join('') +
      '</div>';
  }

  function saveClimateFields(tab, formData) {
    const packed = packClimateSlotConfig(tab);
    const packedLayouts = packClimateTargetLayouts(tab);
    formData.append('climate_entity',
      document.getElementById(tab + '_climate_entity')?.value || '');
    formData.append('popup_open_mode',
      document.getElementById(tab + '_climate_popup_open_mode')?.value || '1');
    formData.append('climate_slots_packed', String(packed));
    formData.append('climate_layouts_packed', String(packedLayouts));
    // Keep the local tile/draft representation in sync with the V7 storage
    // field used by the firmware.
    formData.append('sensor_gauge_min', String(packed));
    formData.append(
      'sensor_gauge_max',
      String((CLIMATE_LAYOUT_MAGIC | packedLayouts) >>> 0));
  }

  function resetClimateFields(tab) {
    const entity = document.getElementById(tab + '_climate_entity');
    if (entity) entity.value = '';
    const popup = document.getElementById(tab + '_climate_popup_open_mode');
    if (popup) popup.value = '1';
    for (let index = 0; index < 6; ++index) {
      const select = document.getElementById(
        tab + '_climate_slot_' + index);
      if (select) select.value = String(CLIMATE_TILE_CONTENT.AUTO);
      const layout = document.getElementById(
        tab + '_climate_layout_' + index);
      if (layout) {
        layout.value = String(CLIMATE_TARGET_LAYOUT.AUTO);
      }
    }
    syncClimateSlotFields(tab);
  }
  </script>
)html";
}
