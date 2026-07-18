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
  const CLIMATE_GEOMETRY_PREFIX = 'CLG1:';

  function climateSlotCapacity(spanW, spanH) {
    const w = Math.max(1, Number(spanW) || 1);
    const h = Math.max(1, Number(spanH) || 1);
    if (w === 1 && h === 1) return 1;
    if (w >= 2 && h === 1) return 2;
    if (w === 1) return 3;
    return 6;
  }

  function climateGridDimensions(spanW, spanH) {
    return {
      columns: Math.max(1, Number(spanW) || 1) >= 2 ? 2 : 1,
      rows: Math.max(1, Number(spanH) || 1) >= 2 ? 3 : 1
    };
  }

  function clampClimateGeometryItem(item, columns, rows) {
    const col = Math.max(
      0, Math.min(columns - 1, Number(item?.col) || 0));
    const row = Math.max(
      0, Math.min(rows - 1, Number(item?.row) || 0));
    const spanW = Math.max(
      1, Math.min(columns - col, Number(item?.spanW) || 1));
    const spanH = Math.max(
      1, Math.min(rows - row, Number(item?.spanH) || 1));
    return { col, row, spanW, spanH };
  }

  function sanitizeClimateGeometryItem(item) {
    return {
      col: Math.max(0, Math.min(1, Number(item?.col) || 0)),
      row: Math.max(0, Math.min(2, Number(item?.row) || 0)),
      spanW: Math.max(
        1, Math.min(2, Number(item?.spanW) || 1)),
      spanH: Math.max(
        1, Math.min(3, Number(item?.spanH) || 1))
    };
  }

  function climateGeometryEquals(a, b) {
    return a.col === b.col &&
      a.row === b.row &&
      a.spanW === b.spanW &&
      a.spanH === b.spanH;
  }

  function defaultClimateGeometry(
      spanW, spanH, slotConfig = null,
      targetLayoutConfig = null) {
    const { columns, rows } =
      climateGridDimensions(spanW, spanH);
    const configured = Array.isArray(slotConfig)
      ? slotConfig : Array(6).fill(CLIMATE_TILE_CONTENT.AUTO);
    const layouts = Array.isArray(targetLayoutConfig)
      ? targetLayoutConfig : Array(6).fill(CLIMATE_TARGET_LAYOUT.AUTO);
    return Array.from({ length: 6 }, (_, index) => {
      const item = {
        col: index % columns,
        row: Math.min(rows - 1, Math.floor(index / columns)),
        spanW: 1,
        spanH: 1
      };
      const content = Number(configured[index]) || 0;
      if (content === CLIMATE_TILE_CONTENT.AUTO) {
        if (columns === 2 && rows === 1 && index === 0) {
          item.spanW = 2;
        } else if (columns === 1 && rows === 3 && index === 1) {
          item.spanH = 2;
        } else if (columns === 2 && rows === 3 &&
                   (index === 2 || index === 3)) {
          item.spanH = 2;
        }
        return item;
      }
      const adjustable =
        content >= CLIMATE_TILE_CONTENT.TARGET_TEMPERATURE &&
        content <= CLIMATE_TILE_CONTENT.TARGET_HUMIDITY;
      if (!adjustable) return item;
      const layout = Number(layouts[index]) || 0;
      const canHorizontal = item.col + 1 < columns;
      const canVertical = item.row + 1 < rows;
      if (layout === CLIMATE_TARGET_LAYOUT.HORIZONTAL &&
          canHorizontal) {
        item.spanW = 2;
      } else if (layout === CLIMATE_TARGET_LAYOUT.VERTICAL &&
                 canVertical) {
        item.spanH = 2;
      } else if (columns === 1 && canVertical) {
        item.spanH = 2;
      } else if (rows === 1 && canHorizontal) {
        item.spanW = 2;
      } else if (canVertical) {
        item.spanH = 2;
      } else if (canHorizontal) {
        item.spanW = 2;
      }
      return item;
    });
  }

  function decodeClimateGeometry(
      value, spanW, spanH, slotConfig = null,
      targetLayoutConfig = null) {
    const text = String(value || '').trim();
    const match = text.match(/^CLG1:([0-9a-fA-F]{9})$/);
    if (!match) {
      return defaultClimateGeometry(
        spanW, spanH, slotConfig, targetLayoutConfig);
    }
    let packed = BigInt('0x' + match[1]);
    return Array.from({ length: 6 }, (_, index) => {
      const raw = Number((packed >> BigInt(index * 6)) & 0x3fn);
      return sanitizeClimateGeometryItem({
        col: raw & 0x01,
        row: (raw >> 1) & 0x03,
        spanW: ((raw >> 3) & 0x01) + 1,
        spanH: ((raw >> 4) & 0x03) + 1
      });
    });
  }

  function encodeClimateGeometry(items) {
    let packed = 0n;
    Array.from({ length: 6 }, (_, index) => {
      const item = items[index] || {};
      const raw =
        (Math.max(0, Math.min(1, Number(item.col) || 0))) |
        (Math.max(0, Math.min(3, Number(item.row) || 0)) << 1) |
        ((Math.max(1, Math.min(2, Number(item.spanW) || 1)) - 1) << 3) |
        ((Math.max(1, Math.min(3, Number(item.spanH) || 1)) - 1) << 4);
      packed |= BigInt(raw & 0x3f) << BigInt(index * 6);
    });
    return CLIMATE_GEOMETRY_PREFIX +
      packed.toString(16).toUpperCase().padStart(9, '0');
  }

  function currentClimateGeometry(tab) {
    const spanW = document.getElementById(
      tab + '_tile_span_w')?.value || 1;
    const spanH = document.getElementById(
      tab + '_tile_span_h')?.value || 1;
    const input = document.getElementById(
      tab + '_climate_geometry');
    return decodeClimateGeometry(
      input?.value || '', spanW, spanH,
      currentClimateSlotConfig(tab),
      currentClimateTargetLayouts(tab));
  }

  function storeClimateGeometry(tab, items) {
    const input = document.getElementById(
      tab + '_climate_geometry');
    if (input) input.value = encodeClimateGeometry(items);
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

  function climateGeometryOverlaps(a, b) {
    return a.col < b.col + b.spanW &&
      a.col + a.spanW > b.col &&
      a.row < b.row + b.spanH &&
      a.row + a.spanH > b.row;
  }

  function canPlaceClimateItem(
      items, configured, index, candidate, capacity) {
    for (let other = 0; other < capacity; ++other) {
      if (other === index) continue;
      if (Number(configured[other]) === CLIMATE_TILE_CONTENT.EMPTY) {
        continue;
      }
      if (climateGeometryOverlaps(candidate, items[other])) {
        return false;
      }
    }
    return true;
  }

  function firstFreeClimateCell(
      items, configured, capacity, columns, rows,
      ignoreIndex = -1) {
    for (let row = 0; row < rows; ++row) {
      for (let col = 0; col < columns; ++col) {
        const candidate = { col, row, spanW: 1, spanH: 1 };
        if (canPlaceClimateItem(
              items, configured, ignoreIndex,
              candidate, capacity)) {
          return candidate;
        }
      }
    }
    return null;
  }

  function notifyClimateGridChanged(tab) {
    updateTilePreview(tab);
    updateDraft(tab);
    scheduleAutoSave(tab);
  }

  let climateGridDragState = null;

  function climateGridCellFromPointer(grid, event, columns, rows) {
    const rect = grid.getBoundingClientRect();
    const style = getComputedStyle(grid);
    const padLeft = parseFloat(style.paddingLeft) || 0;
    const padTop = parseFloat(style.paddingTop) || 0;
    const padRight = parseFloat(style.paddingRight) || 0;
    const padBottom = parseFloat(style.paddingBottom) || 0;
    const columnGap = parseFloat(style.columnGap) || 0;
    const rowGap = parseFloat(style.rowGap) || 0;
    const contentW = Math.max(
      1, rect.width - padLeft - padRight -
          columnGap * (columns - 1));
    const contentH = Math.max(
      1, rect.height - padTop - padBottom -
          rowGap * (rows - 1));
    const cellW = contentW / columns;
    const cellH = contentH / rows;
    const x = event.clientX - rect.left - padLeft;
    const y = event.clientY - rect.top - padTop;
    return {
      col: Math.max(
        0, Math.min(
          columns - 1,
          Math.floor(x / Math.max(1, cellW + columnGap)))),
      row: Math.max(
        0, Math.min(
          rows - 1,
          Math.floor(y / Math.max(1, cellH + rowGap))))
    };
  }

  const climateSelectedItemByTab = Object.create(null);

  function climateEditorState(tab) {
    const entity = document.getElementById(
      tab + '_climate_entity')?.value || '';
    return parseClimatePreviewPayload(metaValues[entity] ?? '');
  }

  function climateEditorContentInfo(tab, kind) {
    const german =
      String(document.documentElement.lang || '')
        .toLowerCase().startsWith('de');
    const state = climateEditorState(tab);
    const unit = state.unit || '\u00B0C';
    const temperature = value =>
      String(value ?? '--') + ' ' + unit;
    const selected = Number(kind) || 0;
    switch (selected) {
      case CLIMATE_TILE_CONTENT.CURRENT_TEMPERATURE:
        return {
          label: german ? 'Aktuell' : 'Current',
          value: temperature(state.current),
          adjustable: false
        };
      case CLIMATE_TILE_CONTENT.CURRENT_HUMIDITY:
        return {
          label: german ? 'Luftfeuchtigkeit' : 'Humidity',
          value: state.currentHumidity !== null
            ? state.currentHumidity + '%' : '--%',
          adjustable: false
        };
      case CLIMATE_TILE_CONTENT.TARGET_TEMPERATURE:
        return {
          label: german ? 'Solltemperatur' : 'Target',
          value: temperature(state.target),
          adjustable: true
        };
      case CLIMATE_TILE_CONTENT.TARGET_TEMPERATURE_LOW:
        return {
          label: german ? 'Heizen' : 'Heating',
          value: temperature(state.targetLow),
          adjustable: true
        };
      case CLIMATE_TILE_CONTENT.TARGET_TEMPERATURE_HIGH:
        return {
          label: german ? 'K\u00FChlen' : 'Cooling',
          value: temperature(state.targetHigh),
          adjustable: true
        };
      case CLIMATE_TILE_CONTENT.TARGET_HUMIDITY:
        return {
          label: german ? 'Soll-Luftfeuchtigkeit' : 'Target humidity',
          value: state.targetHumidity !== null
            ? state.targetHumidity + '%' : '--%',
          adjustable: true
        };
      case CLIMATE_TILE_CONTENT.HVAC_MODE:
        return {
          label: german ? 'Modus' : 'Mode',
          value: state.mode
            ? state.mode.replaceAll('_', '/')
            : '--',
          adjustable: false
        };
      case CLIMATE_TILE_CONTENT.AUTO:
      default:
        return {
          label: german ? 'Automatisch' : 'Automatic',
          value: '--',
          adjustable: false
        };
    }
  }

  function renderClimateEditorItem(tab, index, geometry, kind) {
    const preview = document.getElementById(
      tab + '_climate_preview_' + index);
    if (!preview) return;
    const info = climateEditorContentInfo(tab, kind);
    const expanded =
      geometry.spanW > 1 || geometry.spanH > 1;
    if (info.adjustable && expanded) {
      preview.innerHTML =
        '<small>' + info.label + '</small>' +
        '<div class="climate-mini-control">' +
        '<span>-</span><strong>' + info.value +
        '</strong><span>+</span></div>';
      return;
    }
    preview.innerHTML =
      (expanded ? '<small>' + info.label + '</small>' : '') +
      '<strong>' + info.value + '</strong>';
  }

  function selectClimateEditorItem(tab, index) {
    climateSelectedItemByTab[tab] = index;
    for (let candidate = 0; candidate < 6; ++candidate) {
      const item = document.getElementById(
        tab + '_climate_slot_row_' + candidate);
      const selected = candidate === index &&
        item && !item.classList.contains('hidden');
      item?.classList.toggle('active', !!selected);
      if (item) {
        item.dataset.selected = selected ? '1' : '0';
      }
    }
    const editor = document.getElementById(
      tab + '_climate_selected_content');
    const source = document.getElementById(
      tab + '_climate_slot_' + index);
    if (editor) {
      editor.disabled = !source;
      if (source) editor.value = source.value;
    }
  }

  function bindClimateMiniGrid(tab) {
    const grid = document.getElementById(
      tab + '_climate_content_grid');
    if (!grid || grid.dataset.climateBound === '1') return;
    grid.dataset.climateBound = '1';

    const selectedContent = document.getElementById(
      tab + '_climate_selected_content');
    selectedContent?.addEventListener('change', () => {
      const index = Number(climateSelectedItemByTab[tab]);
      if (!Number.isFinite(index) || index < 0 || index >= 6) {
        return;
      }
      const source = document.getElementById(
        tab + '_climate_slot_' + index);
      if (!source) return;
      source.value = selectedContent.value;
      syncClimateSlotFields(tab);
      notifyClimateGridChanged(tab);
    });

    for (let cellIndex = 0; cellIndex < 6; ++cellIndex) {
      const cell = document.getElementById(
        tab + '_climate_cell_' + cellIndex);
      cell?.addEventListener('click', event => {
        event.preventDefault();
        const spanW = document.getElementById(
          tab + '_tile_span_w')?.value || 1;
        const spanH = document.getElementById(
          tab + '_tile_span_h')?.value || 1;
        const { columns, rows } =
          climateGridDimensions(spanW, spanH);
        const capacity = climateSlotCapacity(spanW, spanH);
        const configured = currentClimateSlotConfig(tab);
        const index = configured.findIndex(
          (value, candidate) =>
            candidate < capacity &&
            Number(value) === CLIMATE_TILE_CONTENT.EMPTY);
        if (index < 0) return;
        const row = Math.floor(cellIndex / columns);
        const col = cellIndex % columns;
        if (row >= rows) return;
        const stored = currentClimateGeometry(tab);
        stored[index] = { col, row, spanW: 1, spanH: 1 };
        const source = document.getElementById(
          tab + '_climate_slot_' + index);
        if (source) {
          source.value = String(CLIMATE_TILE_CONTENT.AUTO);
        }
        storeClimateGeometry(tab, stored);
        climateSelectedItemByTab[tab] = index;
        syncClimateSlotFields(tab);
        notifyClimateGridChanged(tab);
      });
    }

    for (let index = 0; index < 6; ++index) {
      const item = document.getElementById(
        tab + '_climate_slot_row_' + index);
      if (!item) continue;

      item.addEventListener('pointerdown', event => {
        if (!event.isPrimary ||
            event.target.closest('[data-climate-resize]')) {
          return;
        }
        event.preventDefault();
        selectClimateEditorItem(tab, index);
        const spanW = document.getElementById(
          tab + '_tile_span_w')?.value || 1;
        const spanH = document.getElementById(
          tab + '_tile_span_h')?.value || 1;
        const { columns, rows } =
          climateGridDimensions(spanW, spanH);
        const capacity = climateSlotCapacity(spanW, spanH);
        const configured = currentClimateSlotConfig(tab);
        const stored = currentClimateGeometry(tab);
        const items = stored.map(entry =>
          clampClimateGeometryItem(entry, columns, rows));
        const origin = { ...items[index] };
        const startCell = climateGridCellFromPointer(
          grid, event, columns, rows);
        let changed = false;
        item.setPointerCapture(event.pointerId);
        const onMove = moveEvent => {
          const cell = climateGridCellFromPointer(
            grid, moveEvent, columns, rows);
          const candidate = clampClimateGeometryItem({
            ...origin,
            col: origin.col + cell.col - startCell.col,
            row: origin.row + cell.row - startCell.row
          }, columns, rows);
          if (climateGeometryEquals(candidate, items[index]) ||
              !canPlaceClimateItem(
                items, configured, index,
                candidate, capacity)) {
            return;
          }
          changed = true;
          items[index] = candidate;
          stored[index] = candidate;
          item.classList.add('dragging');
          item.style.gridColumn =
            (candidate.col + 1) + ' / span ' +
            candidate.spanW;
          item.style.gridRow =
            (candidate.row + 1) + ' / span ' +
            candidate.spanH;
        };
        const onEnd = endEvent => {
          item.removeEventListener('pointermove', onMove);
          item.removeEventListener('pointerup', onEnd);
          item.removeEventListener('pointercancel', onEnd);
          if (item.hasPointerCapture(endEvent.pointerId)) {
            item.releasePointerCapture(endEvent.pointerId);
          }
          item.classList.remove('dragging');
          if (!changed) return;
          storeClimateGeometry(tab, stored);
          syncClimateSlotFields(tab);
          notifyClimateGridChanged(tab);
        };
        item.addEventListener('pointermove', onMove);
        item.addEventListener('pointerup', onEnd);
        item.addEventListener('pointercancel', onEnd);
      });

      item.querySelectorAll('[data-climate-resize]')
        .forEach(handle => {
          handle.addEventListener('pointerdown', event => {
            event.preventDefault();
            event.stopPropagation();
            selectClimateEditorItem(tab, index);
            const spanW = document.getElementById(
              tab + '_tile_span_w')?.value || 1;
            const spanH = document.getElementById(
              tab + '_tile_span_h')?.value || 1;
            const { columns, rows } =
              climateGridDimensions(spanW, spanH);
            const capacity =
              climateSlotCapacity(spanW, spanH);
            const configured = currentClimateSlotConfig(tab);
            const stored = currentClimateGeometry(tab);
            const items = stored.map(entry =>
              clampClimateGeometryItem(entry, columns, rows));
            const origin = { ...items[index] };
            const direction =
              String(handle.dataset.climateResize || 'se');
            item.classList.add('resizing');
            handle.setPointerCapture(event.pointerId);
            const onMove = moveEvent => {
              const cell = climateGridCellFromPointer(
                grid, moveEvent, columns, rows);
              const candidate = { ...origin };
              if (direction.includes('e')) {
                candidate.spanW =
                  cell.col - origin.col + 1;
              }
              if (direction.includes('s')) {
                candidate.spanH =
                  cell.row - origin.row + 1;
              }
              const clamped = clampClimateGeometryItem(
                candidate, columns, rows);
              if (!canPlaceClimateItem(
                    items, configured, index,
                    clamped, capacity)) {
                item.classList.add('resize-invalid');
                return;
              }
              item.classList.remove('resize-invalid');
              items[index] = clamped;
              stored[index] = clamped;
              item.style.gridColumn =
                (clamped.col + 1) + ' / span ' +
                clamped.spanW;
              item.style.gridRow =
                (clamped.row + 1) + ' / span ' +
                clamped.spanH;
              renderClimateEditorItem(
                tab, index, clamped, configured[index]);
            };
            const onEnd = endEvent => {
              handle.removeEventListener(
                'pointermove', onMove);
              handle.removeEventListener(
                'pointerup', onEnd);
              handle.removeEventListener(
                'pointercancel', onEnd);
              if (handle.hasPointerCapture(
                    endEvent.pointerId)) {
                handle.releasePointerCapture(
                  endEvent.pointerId);
              }
              item.classList.remove(
                'resizing', 'resize-invalid');
              storeClimateGeometry(tab, stored);
              syncClimateSlotFields(tab);
              notifyClimateGridChanged(tab);
            };
            handle.addEventListener('pointermove', onMove);
            handle.addEventListener('pointerup', onEnd);
            handle.addEventListener('pointercancel', onEnd);
          });
        });
    }
  }

  function syncClimateSlotFields(tab) {
    const spanW = document.getElementById(
      tab + '_tile_span_w')?.value || 1;
    const spanH = document.getElementById(
      tab + '_tile_span_h')?.value || 1;
    const capacity = climateSlotCapacity(spanW, spanH);
    const { columns, rows } =
      climateGridDimensions(spanW, spanH);
    const configured = currentClimateSlotConfig(tab);
    const stored = currentClimateGeometry(tab);
    const items = stored.map(entry =>
      clampClimateGeometryItem(entry, columns, rows));
    const grid = document.getElementById(
      tab + '_climate_content_grid');
    if (grid) {
      grid.style.setProperty(
        '--climate-editor-columns', String(columns));
      grid.style.setProperty(
        '--climate-editor-rows', String(rows));
    }

    const occupied = Array(columns * rows).fill(false);
    const accepted = [];
    for (let index = 0; index < 6; ++index) {
      const item = document.getElementById(
        tab + '_climate_slot_row_' + index);
      const kind = Number(configured[index]) || 0;
      const active =
        index < capacity &&
        kind !== CLIMATE_TILE_CONTENT.EMPTY;
      if (!item) continue;
      item.classList.toggle('hidden', !active);
      if (!active) continue;

      let geometry = items[index];
      if (accepted.some(other =>
            climateGeometryOverlaps(
              geometry, other.geometry))) {
        const free = firstFreeClimateCell(
          items, configured, capacity,
          columns, rows, index);
        if (!free) {
          item.classList.add('hidden');
          continue;
        }
        geometry = free;
        items[index] = free;
        stored[index] = free;
      }
      accepted.push({ index, geometry });
      item.style.gridColumn =
        (geometry.col + 1) + ' / span ' +
        geometry.spanW;
      item.style.gridRow =
        (geometry.row + 1) + ' / span ' +
        geometry.spanH;
      renderClimateEditorItem(
        tab, index, geometry, kind);
      for (let row = geometry.row;
           row < geometry.row + geometry.spanH; ++row) {
        for (let col = geometry.col;
             col < geometry.col + geometry.spanW; ++col) {
          occupied[row * columns + col] = true;
        }
      }

      const layout = document.getElementById(
        tab + '_climate_layout_' + index);
      if (layout) {
        layout.value = String(
          geometry.spanW > 1 && geometry.spanH === 1
            ? CLIMATE_TARGET_LAYOUT.HORIZONTAL
            : (geometry.spanH > 1
                ? CLIMATE_TARGET_LAYOUT.VERTICAL
                : CLIMATE_TARGET_LAYOUT.AUTO));
      }
    }

    for (let cellIndex = 0; cellIndex < 6; ++cellIndex) {
      const cell = document.getElementById(
        tab + '_climate_cell_' + cellIndex);
      if (!cell) continue;
      const row = Math.floor(cellIndex / columns);
      const col = cellIndex % columns;
      const visible = row < rows;
      cell.classList.toggle('hidden', !visible);
      cell.classList.toggle(
        'occupied',
        !visible || !!occupied[cellIndex]);
      if (visible) {
        cell.style.gridColumn = String(col + 1);
        cell.style.gridRow = String(row + 1);
      }
    }

    storeClimateGeometry(tab, stored);
    bindClimateMiniGrid(tab);
    let selected = Number(climateSelectedItemByTab[tab]);
    const selectedItem = Number.isFinite(selected)
      ? document.getElementById(
          tab + '_climate_slot_row_' + selected)
      : null;
    if (!selectedItem ||
        selectedItem.classList.contains('hidden')) {
      selected = accepted.length ? accepted[0].index : -1;
    }
    selectClimateEditorItem(tab, selected);
    const selectedFields = document.getElementById(
      tab + '_climate_selected_fields');
    selectedFields?.classList.toggle(
      'hidden', selected < 0);
    const hint = document.getElementById(
      tab + '_climate_content_hint');
    if (hint) {
      const german =
        String(document.documentElement.lang || '')
          .toLowerCase().startsWith('de');
      hint.textContent = german
        ? 'Freies Feld anklicken, Inhalt ausw\u00E4hlen und das Feld wie ein Tile verschieben oder aufziehen.'
        : 'Click a free cell, choose its content, then move or resize it like a tile.';
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
    const geometry = document.getElementById(
      tab + '_climate_geometry');
    if (geometry) {
      geometry.value =
        data.climate_geometry || data.scene_alias || '';
    }
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
      targetLayoutConfig = null, geometryConfig = null) {
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
    const geometry = Array.isArray(geometryConfig)
      ? geometryConfig.slice(0, 6)
      : decodeClimateGeometry(
          geometryConfig || '', w, h,
          configured, targetLayouts);
    while (geometry.length < 6) {
      geometry.push({ col: 0, row: 0, spanW: 1, spanH: 1 });
    }

    const automatic = [];
    const temp = (value) => String(value ?? '--') + ' ' + state.unit;
    const addAutomatic = (kind) => {
      if (automatic.length < capacity) {
        automatic.push(kind);
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
      slots.push({
        ...slot,
        targetLayout,
        itemIndex: index,
        ...clampClimateGeometryItem(
          geometry[index], columns,
          Math.max(1, Math.ceil(capacity / columns)))
      });
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
    const placedSlots = [];
    slots.forEach(slot => {
      const candidate = {
        col: slot.col,
        row: slot.row,
        spanW: slot.spanW,
        spanH: slot.spanH
      };
      if (placedSlots.some(other =>
            climateGeometryOverlaps(candidate, {
              col: other.col,
              row: other.row,
              spanW: other.spanW,
              spanH: other.spanH
            }))) {
        return;
      }
      placedSlots.push(slot);
    });

    const tallTile = h > 1;
    return '<div class="climate-slots" style="--climate-columns:' +
      columns +
      ';--climate-slot-h:var(' +
      (tallTile
        ? '--climate-slot-h-tall'
        : '--climate-slot-h-wide') +
      ');--climate-slots-top:var(' +
      (tallTile
        ? '--climate-slots-top-tall'
        : '--climate-slots-top-wide') +
      ');--climate-slots-bottom:var(' +
      (tallTile
        ? '--climate-slots-bottom-tall'
        : '--climate-slots-bottom-wide') +
      ')">' +
      placedSlots.map(slot => {
        const row = slot.row + 1;
        const column = slot.col + 1;
        const horizontal =
          slot.adjustable && slot.spanW > 1 &&
          slot.spanH === 1;
        const vertical =
          slot.adjustable && slot.spanW === 1 &&
          slot.spanH > 1;
        const large =
          slot.adjustable && slot.spanW > 1 &&
          slot.spanH > 1;
        const compact =
          slot.adjustable && slot.spanW === 1 &&
          slot.spanH === 1;
        const gridStyle =
          'grid-column:' + column + ' / span ' +
          slot.spanW +
          ';grid-row:' + row + ' / span ' +
          slot.spanH;
        if (!slot.adjustable || compact) {
          return '<div class="climate-slot climate-slot-value' +
            (slot.kind === CLIMATE_TILE_CONTENT.HVAC_MODE
              ? ' climate-slot-mode' : '') +
            (compact ? ' climate-slot-target-compact' : '') +
            '" style="' +
            gridStyle + '"><strong>' + slot.value + '</strong></div>';
        }
        const controlClass = horizontal
          ? 'climate-slot-control-horizontal'
          : (large
              ? 'climate-slot-control-large'
              : 'climate-slot-control-vertical ' +
                (columns > 1
                  ? (slot.col === 0
                      ? 'climate-slot-column-left'
                      : 'climate-slot-column-right')
                  : ''));
        return '<div class="climate-slot climate-slot-control ' +
          controlClass +
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
    const geometry = document.getElementById(
      tab + '_climate_geometry')?.value || '';
    formData.append('climate_entity',
      document.getElementById(tab + '_climate_entity')?.value || '');
    formData.append('popup_open_mode',
      document.getElementById(tab + '_climate_popup_open_mode')?.value || '1');
    formData.append('climate_slots_packed', String(packed));
    formData.append('climate_layouts_packed', String(packedLayouts));
    formData.append('climate_geometry', geometry);
    formData.append('scene_alias', geometry);
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
    const geometry = document.getElementById(
      tab + '_climate_geometry');
    if (geometry) geometry.value = '';
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
