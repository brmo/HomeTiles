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
  const CLIMATE_GEOMETRY_PREFIX = 'CLG2:';
  const CLIMATE_GEOMETRY_LEGACY_PREFIX = 'CLG1:';

  function climateMaxGridColumns() {
    return Math.max(
      1, Number(
        typeof GRID_COLS === 'number' ? GRID_COLS : 7) || 7);
  }

  function climateMaxOuterRows() {
    return Math.max(
      1, Number(
        typeof GRID_ROWS === 'number' ? GRID_ROWS : 5) || 5);
  }

  function climateSlotCapacity(spanW, spanH) {
    const { columns, rows } =
      climateGridDimensions(spanW, spanH);
    return Math.min(6, columns * rows);
  }

  function climateGridDimensions(spanW, spanH) {
    const columns = Math.max(
      1, Math.min(
        climateMaxGridColumns(), Number(spanW) || 1));
    const outerRows = Math.max(
      1, Math.min(
        climateMaxOuterRows(), Number(spanH) || 1));
    return {
      columns,
      rows: outerRows * 2 - 1
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
    const maxColumns = climateMaxGridColumns();
    const maxRows = climateMaxOuterRows() * 2 - 1;
    return {
      col: Math.max(
        0, Math.min(maxColumns - 1, Number(item?.col) || 0)),
      row: Math.max(
        0, Math.min(maxRows - 1, Number(item?.row) || 0)),
      spanW: Math.max(
        1, Math.min(maxColumns, Number(item?.spanW) || 1)),
      spanH: Math.max(
        1, Math.min(maxRows, Number(item?.spanH) || 1))
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
    const currentMatch =
      text.match(/^CLG2:([0-9a-fA-F]{24})$/);
    if (currentMatch) {
      return Array.from({ length: 6 }, (_, index) => {
        const offset = index * 4;
        const raw = Number.parseInt(
          currentMatch[1].slice(offset, offset + 4), 16);
        return sanitizeClimateGeometryItem({
          col: raw & 0x07,
          row: (raw >> 3) & 0x0f,
          spanW: ((raw >> 7) & 0x07) + 1,
          spanH: ((raw >> 10) & 0x0f) + 1
        });
      });
    }
    const legacyMatch =
      text.match(/^CLG1:([0-9a-fA-F]{9})$/);
    if (!legacyMatch) {
      return defaultClimateGeometry(
        spanW, spanH, slotConfig, targetLayoutConfig);
    }
    let packed = BigInt('0x' + legacyMatch[1]);
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
    const encoded = Array.from({ length: 6 }, (_, index) => {
      const item = sanitizeClimateGeometryItem(
        items[index] || {});
      const raw =
        item.col |
        (item.row << 3) |
        ((item.spanW - 1) << 7) |
        ((item.spanH - 1) << 10);
      return (raw & 0x3fff)
        .toString(16).toUpperCase().padStart(4, '0');
    }).join('');
    return CLIMATE_GEOMETRY_PREFIX + encoded;
  }

  function currentClimateGeometry(tab) {
    const spanW = document.getElementById(
      tab + '_tile_span_w')?.value || 1;
    const spanH = document.getElementById(
      tab + '_tile_span_h')?.value || 1;
    const input = document.getElementById(
      tab + '_climate_geometry');
    const configured = currentClimateSlotConfig(tab);
    const resolved = climateResolvedEditorKinds(tab);
    const effectiveConfig = configured.map((value, index) =>
      Number(value) === CLIMATE_TILE_CONTENT.AUTO &&
      resolved[index] !== null
        ? resolved[index]
        : value);
    return decodeClimateGeometry(
      input?.value || '', spanW, spanH,
      effectiveConfig,
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

  function firstFreeClimatePlacement(
      items, configured, capacity, columns, rows,
      ignoreIndex = -1, spanW = 1, spanH = 1) {
    const safeSpanW = Math.max(
      1, Math.min(columns, Number(spanW) || 1));
    const safeSpanH = Math.max(
      1, Math.min(rows, Number(spanH) || 1));
    for (let row = 0; row + safeSpanH <= rows; ++row) {
      for (let col = 0; col + safeSpanW <= columns; ++col) {
        const candidate = {
          col, row,
          spanW: safeSpanW,
          spanH: safeSpanH
        };
        if (canPlaceClimateItem(
              items, configured, ignoreIndex,
              candidate, capacity)) {
          return candidate;
        }
      }
    }
    return null;
  }

  function firstFreeClimateCell(
      items, configured, capacity, columns, rows,
      ignoreIndex = -1) {
    return firstFreeClimatePlacement(
      items, configured, capacity, columns, rows,
      ignoreIndex, 1, 1);
  }

  function notifyClimateGridChanged(tab) {
    updateTilePreview(tab);
    updateDraft(tab);
    scheduleAutoSave(tab);
  }

  let climateGridDragState = null;
  let climateGridDragPreview = null;

  function createClimateMiniDragGhost(item, rect) {
    // Slot-Styles (Grid-Layout der Regler usw.) sind unter .tile.climate
    // gescopet; ein nackter Klon in document.body verliert sie und zerfaellt
    // zu Fliesstext. Der Wrapper stellt den Selektor-Kontext wieder her.
    const ghost = document.createElement('div');
    ghost.className =
      'tile climate climate-content-editing climate-mini-drag-ghost';
    ghost.style.position = 'absolute';
    ghost.style.top = '-9999px';
    ghost.style.left = '-9999px';
    ghost.style.width = rect.width + 'px';
    ghost.style.height = rect.height + 'px';
    ghost.style.padding = '0';
    ghost.style.border = '0';
    ghost.style.pointerEvents = 'none';
    const clone = item.cloneNode(true);
    clone.classList.remove('active', 'dragging');
    clone.querySelectorAll('.tile-resize-handle')
      .forEach(handle => handle.remove());
    clone.style.position = 'absolute';
    clone.style.inset = '0';
    clone.style.gridArea = 'auto';
    ghost.appendChild(clone);
    document.body.appendChild(ghost);
    return ghost;
  }

  const climateSelectedItemByTab = Object.create(null);
  const climateSelectedCellByTab = Object.create(null);
  const climatePendingEmptyByTab = Object.create(null);
  const climateEditorSnapshotByTab = Object.create(null);
  const climatePendingPreviewSelectionByTab =
    Object.create(null);

  function cloneClimateEditorSnapshot(snapshot) {
    if (!snapshot) return null;
    return {
      tileIndex: snapshot.tileIndex,
      spanW: snapshot.spanW,
      spanH: snapshot.spanH,
      resolvedKinds: Array.isArray(snapshot.resolvedKinds)
        ? snapshot.resolvedKinds.slice()
        : []
    };
  }

  function captureClimateOuterResizeState(tab) {
    return {
      geometry: document.getElementById(
        tab + '_climate_geometry')?.value || '',
      slots: currentClimateSlotConfig(tab),
      layouts: currentClimateTargetLayouts(tab),
      editorSnapshot: cloneClimateEditorSnapshot(
        climateEditorSnapshotByTab[tab]),
      selectedItem: Number(climateSelectedItemByTab[tab]),
      selectedCell: Number(climateSelectedCellByTab[tab]),
      pendingEmpty: climatePendingEmptyByTab[tab]
        ? {
            index: climatePendingEmptyByTab[tab].index,
            geometry: {
              ...climatePendingEmptyByTab[tab].geometry
            }
          }
        : null
    };
  }

  function restoreClimateOuterResizeState(tab, state) {
    if (!state) return;
    const geometry = document.getElementById(
      tab + '_climate_geometry');
    if (geometry) geometry.value = state.geometry || '';
    for (let index = 0; index < 6; ++index) {
      const slot = document.getElementById(
        tab + '_climate_slot_' + index);
      if (slot) {
        slot.value = String(
          state.slots?.[index] ??
          CLIMATE_TILE_CONTENT.AUTO);
      }
      const layout = document.getElementById(
        tab + '_climate_layout_' + index);
      if (layout) {
        layout.value = String(
          state.layouts?.[index] ??
          CLIMATE_TARGET_LAYOUT.AUTO);
      }
    }
    if (state.editorSnapshot) {
      climateEditorSnapshotByTab[tab] =
        cloneClimateEditorSnapshot(state.editorSnapshot);
    } else {
      delete climateEditorSnapshotByTab[tab];
    }
    climateSelectedItemByTab[tab] =
      Number.isFinite(state.selectedItem)
        ? state.selectedItem : -1;
    climateSelectedCellByTab[tab] =
      Number.isFinite(state.selectedCell)
        ? state.selectedCell : -1;
    if (state.pendingEmpty) {
      climatePendingEmptyByTab[tab] = {
        index: state.pendingEmpty.index,
        geometry: { ...state.pendingEmpty.geometry }
      };
    } else {
      delete climatePendingEmptyByTab[tab];
    }
  }

  function previewClimateOuterResize(tab, state) {
    restoreClimateOuterResizeState(tab, state);
    syncClimateSlotFields(tab);
  }

  function requestClimatePreviewSelection(
      tab, tileIndex, itemIndex = -1, cellIndex = -1) {
    const sameTile =
      currentTileTab === tab &&
      currentTileIndex === tileIndex;
    const pendingSelection = {
      tileIndex,
      itemIndex,
      cellIndex
    };
    climatePendingPreviewSelectionByTab[tab] =
      pendingSelection;
    if (!sameTile && typeof selectTile === 'function') {
      selectTile(tileIndex, tab);
      // selectTile parks the previous editor first. Re-apply the requested
      // mini selection after that cleanup so the async tile load can consume it.
      climatePendingPreviewSelectionByTab[tab] =
        pendingSelection;
    }
    if (sameTile) {
      mountClimateMiniEditor(tab);
      syncClimateSlotFields(tab, true);
    }
  }

  function bindClimatePreviewSelection() {
    if (document.documentElement.dataset
          .climatePreviewSelectionBound === '1') {
      return;
    }
    document.documentElement.dataset
      .climatePreviewSelectionBound = '1';
    const previewTarget = event =>
      event.target?.closest?.(
        '[data-climate-preview-item],' +
        '[data-climate-preview-cell]');
    document.addEventListener('pointerdown', event => {
      if (previewTarget(event)) event.stopPropagation();
    }, true);
    document.addEventListener('dragstart', event => {
      if (!previewTarget(event)) return;
      event.preventDefault();
      event.stopPropagation();
    }, true);
    document.addEventListener('click', event => {
      const target = previewTarget(event);
      if (!target) return;
      const tile = target.closest('.tile.climate');
      const section = tile?.closest('[id^="tab-tiles-"]');
      const tab = section?.id?.substring(
        'tab-tiles-'.length);
      const tileIndex = Number(tile?.dataset.index);
      if (!tab || !Number.isFinite(tileIndex)) return;
      event.preventDefault();
      event.stopPropagation();
      requestClimatePreviewSelection(
        tab,
        tileIndex,
        Number(target.dataset.climatePreviewItem ?? -1),
        Number(target.dataset.climatePreviewCell ?? -1));
    }, true);
  }

  function parkClimateMiniEditor(tab, preserveSelection = false) {
    const shell = document.getElementById(
      tab + '_climate_editor_shell');
    const stash = document.getElementById(
      tab + '_climate_editor_stash');
    if (shell && stash && shell.parentElement !== stash) {
      stash.appendChild(shell);
    }
    document.querySelectorAll(
      '#tab-tiles-' + tab +
      ' .tile-grid > .tile.climate.climate-content-editing')
      .forEach(tile => {
        tile.classList.remove(
          'climate-content-editing',
          'climate-mini-selection-active');
      });
    if (!preserveSelection) {
      climateSelectedItemByTab[tab] = -1;
      climateSelectedCellByTab[tab] = -1;
      delete climatePendingEmptyByTab[tab];
      delete climatePendingPreviewSelectionByTab[tab];
      delete climateEditorSnapshotByTab[tab];
      selectClimateEditorItem(tab, -1);
    }
  }

  function mountClimateMiniEditor(tab) {
    const shell = document.getElementById(
      tab + '_climate_editor_shell');
    const tile = document.getElementById(
      tab + '-tile-' + currentTileIndex);
    if (!shell ||
        currentTileTab !== tab ||
        !tile ||
        String(tile.dataset.type || '') !== '17') {
      parkClimateMiniEditor(tab);
      return false;
    }

    document.querySelectorAll(
      '#tab-tiles-' + tab +
      ' .tile-grid > .tile.climate.climate-content-editing')
      .forEach(candidate => {
        if (candidate !== tile) {
          candidate.classList.remove(
            'climate-content-editing');
        }
      });

    tile.classList.add('climate-content-editing');
    if (shell.parentElement !== tile) {
      shell.parentElement?.classList?.remove(
        'climate-mini-selection-active');
      tile.appendChild(shell);
    }
    return true;
  }

  function climateEditorState(tab) {
    const entity = document.getElementById(
      tab + '_climate_entity')?.value || '';
    return parseClimatePreviewPayload(
      sensorMetaCache?.values?.[entity] ?? '');
  }

  function climateAutomaticEditorKinds(tab) {
    const state = climateEditorState(tab);
    const spanW = Math.max(1, Number(
      document.getElementById(
        tab + '_tile_span_w')?.value) || 1);
    const spanH = Math.max(1, Number(
      document.getElementById(
        tab + '_tile_span_h')?.value) || 1);
    const capacity = climateSlotCapacity(spanW, spanH);
    const kinds = [];
    const add = kind => {
      if (kinds.length < capacity) kinds.push(kind);
    };

    if (spanW === 1 && spanH === 1) {
      add(CLIMATE_TILE_CONTENT.CURRENT_TEMPERATURE);
    } else if (spanW >= 2 && spanH === 1) {
      if (state.targetLow !== null &&
          state.targetHigh !== null) {
        add(CLIMATE_TILE_CONTENT.TARGET_TEMPERATURE_LOW);
      } else if (state.target !== null) {
        add(CLIMATE_TILE_CONTENT.TARGET_TEMPERATURE);
      } else if (state.targetHumidity !== null) {
        add(CLIMATE_TILE_CONTENT.TARGET_HUMIDITY);
      } else {
        add(CLIMATE_TILE_CONTENT.CURRENT_TEMPERATURE);
      }
    } else if (spanW === 1) {
      add(CLIMATE_TILE_CONTENT.CURRENT_TEMPERATURE);
      if (state.targetLow !== null &&
          state.targetHigh !== null) {
        add(CLIMATE_TILE_CONTENT.TARGET_TEMPERATURE_LOW);
      } else if (state.target !== null) {
        add(CLIMATE_TILE_CONTENT.TARGET_TEMPERATURE);
      }
      if (state.targetHumidity !== null) {
        add(CLIMATE_TILE_CONTENT.TARGET_HUMIDITY);
      }
    } else {
      add(CLIMATE_TILE_CONTENT.CURRENT_TEMPERATURE);
      if (state.currentHumidity !== null) {
        add(CLIMATE_TILE_CONTENT.CURRENT_HUMIDITY);
      }
      if (state.targetLow !== null &&
          state.targetHigh !== null) {
        add(CLIMATE_TILE_CONTENT.TARGET_TEMPERATURE_LOW);
        add(CLIMATE_TILE_CONTENT.TARGET_TEMPERATURE_HIGH);
      } else if (state.target !== null) {
        add(CLIMATE_TILE_CONTENT.TARGET_TEMPERATURE);
      }
      if (state.targetHumidity !== null) {
        add(CLIMATE_TILE_CONTENT.TARGET_HUMIDITY);
      }
      if (state.mode) add(CLIMATE_TILE_CONTENT.HVAC_MODE);
    }
    return kinds;
  }

  function climateResolvedEditorKinds(tab) {
    const configured = currentClimateSlotConfig(tab);
    const automatic = climateAutomaticEditorKinds(tab);
    const capacity = climateSlotCapacity(
      document.getElementById(
        tab + '_tile_span_w')?.value || 1,
      document.getElementById(
        tab + '_tile_span_h')?.value || 1);
    const explicit = new Set();
    configured.slice(0, capacity).forEach(selection => {
      const kind = Number(selection) || 0;
      if (kind !== CLIMATE_TILE_CONTENT.AUTO &&
          kind !== CLIMATE_TILE_CONTENT.EMPTY) {
        explicit.add(kind);
      }
    });
    let cursor = 0;
    return configured.map((selection, index) => {
      const kind = Number(selection) || 0;
      if (index >= capacity ||
          kind === CLIMATE_TILE_CONTENT.EMPTY) {
        return null;
      }
      if (kind !== CLIMATE_TILE_CONTENT.AUTO) return kind;
      while (cursor < automatic.length) {
        const candidate = automatic[cursor++];
        if (!explicit.has(candidate)) return candidate;
      }
      return null;
    });
  }

  function materializeClimateAutomaticItems(
      tab, resolvedOverride = null) {
    const configured = currentClimateSlotConfig(tab);
    const resolved = Array.isArray(resolvedOverride)
      ? resolvedOverride : climateResolvedEditorKinds(tab);
    for (let index = 0; index < 6; ++index) {
      if (Number(configured[index]) !== CLIMATE_TILE_CONTENT.AUTO) {
        continue;
      }
      const source = document.getElementById(
        tab + '_climate_slot_' + index);
      if (!source) continue;
      const kind = Number(resolved[index]);
      source.value = Number.isFinite(kind) && kind > 0
        ? String(kind)
        : String(CLIMATE_TILE_CONTENT.EMPTY);
    }
  }

  function climatePlacementConfig(
      configured, resolvedKinds) {
    return configured.map((selection, index) =>
      resolvedKinds[index] === null
        ? CLIMATE_TILE_CONTENT.EMPTY
        : selection);
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
    const resolvedKind =
      climateResolvedEditorKinds(tab)[index];
    const info = climateEditorContentInfo(
      tab, resolvedKind ?? kind);
    const expanded =
      geometry.spanW > 1 || geometry.spanH > 1;
    const item = document.getElementById(
      tab + '_climate_slot_row_' + index);
    item?.classList.toggle(
      'climate-mini-control-item',
      info.adjustable && expanded);
    if (info.adjustable && expanded) {
      const orientation =
        geometry.spanW > 1 && geometry.spanH === 1
          ? 'climate-slot-control-horizontal'
          : (geometry.spanW > 1 && geometry.spanH > 1
              ? 'climate-slot-control-large'
              : 'climate-slot-control-vertical');
      preview.innerHTML =
        '<div class="climate-slot climate-slot-control ' +
        orientation + '">' +
        '<small>' + info.label + '</small>' +
        '<span class="climate-minus">-</span>' +
        '<strong>' + info.value + '</strong>' +
        '<span class="climate-plus">+</span></div>';
      return;
    }
    preview.innerHTML =
      '<div class="climate-slot climate-slot-value' +
      (resolvedKind === CLIMATE_TILE_CONTENT.HVAC_MODE
        ? ' climate-slot-mode' : '') + '">' +
      '<strong>' + info.value + '</strong></div>';
  }

  function selectClimateEditorItem(
      tab, index, cellIndex = -1, valueOverride = null) {
    const source = index >= 0
      ? document.getElementById(
          tab + '_climate_slot_' + index)
      : null;
    const hasSelection = index >= 0 && !!source;
    climateSelectedItemByTab[tab] =
      hasSelection ? index : -1;
    climateSelectedCellByTab[tab] =
      hasSelection ? cellIndex : -1;
    for (let candidate = 0; candidate < 6; ++candidate) {
      const item = document.getElementById(
        tab + '_climate_slot_row_' + candidate);
      const selected = hasSelection &&
        candidate === index &&
        item && !item.classList.contains('hidden');
      item?.classList.toggle('active', !!selected);
      if (item) {
        item.dataset.selected = selected ? '1' : '0';
      }
    }
    document.getElementById(
      tab + '_climate_content_grid')
      ?.querySelectorAll('.climate-mini-cell')
      .forEach(cell => {
        cell.classList.toggle(
          'active',
          Number(cell.dataset.climateCell) === cellIndex);
      });
    const shell = document.getElementById(
      tab + '_climate_editor_shell');
    const outerTile = shell?.parentElement?.matches(
      '.tile.climate') ? shell.parentElement : null;
    outerTile?.classList.toggle(
      'climate-mini-selection-active',
      hasSelection);
    const editor = document.getElementById(
      tab + '_climate_selected_content');
    if (editor) {
      editor.disabled = !hasSelection;
      if (hasSelection) {
        editor.value = valueOverride !== null
          ? String(valueOverride) : source.value;
      }
    }
    const selectedFields = document.getElementById(
      tab + '_climate_selected_fields');
    selectedFields?.classList.toggle(
      'hidden', !hasSelection);
    if (selectedFields) {
      selectedFields.hidden = !hasSelection;
      selectedFields.closest('.climate-content-config')
        ?.classList.toggle('hidden', !hasSelection);
    }
  }

  function climateGridLayouts(tab, columns, rows) {
    return currentClimateGeometry(tab).map(entry => {
      const geometry =
        clampClimateGeometryItem(entry, columns, rows);
      return {
        col: geometry.col,
        row: geometry.row,
        span_w: geometry.spanW,
        span_h: geometry.spanH
      };
    });
  }

  function climateActiveGridIndices(tab, capacity) {
    const configured = currentClimateSlotConfig(tab);
    const resolved = climateResolvedEditorKinds(tab);
    const active = new Set();
    for (let index = 0; index < capacity; ++index) {
      // Nur real platzierte Items zaehlen: syncClimateSlotFields blendet
      // Slots ohne freien Platz aus; deren gespeicherte Geometrie darf
      // Drag/Resize nicht als Phantom-Belegung blockieren.
      const item = document.getElementById(
        tab + '_climate_slot_row_' + index);
      if (Number(configured[index]) !== CLIMATE_TILE_CONTENT.EMPTY &&
          resolved[index] !== null &&
          item && !item.classList.contains('hidden')) {
        active.add(index);
      }
    }
    return active;
  }

  function applyClimateGridLayouts(
      tab, layouts, activeIndices, baseLayouts = null) {
    activeIndices.forEach(index => {
      const item = document.getElementById(
        tab + '_climate_slot_row_' + index);
      const layout = layouts[index];
      if (!item || !layout) return;
      setGridItemPosition(
        item, layout.col, layout.row,
        layout.span_w, layout.span_h);
      const base = baseLayouts?.[index];
      item.classList.toggle(
        'reflow-preview',
        !!base &&
        (base.col !== layout.col || base.row !== layout.row));
    });
  }

  function storeClimateGridLayouts(tab, layouts) {
    const stored = currentClimateGeometry(tab);
    layouts.forEach((layout, index) => {
      if (!layout) return;
      stored[index] = {
        col: layout.col,
        row: layout.row,
        spanW: layout.span_w,
        spanH: layout.span_h
      };
    });
    storeClimateGeometry(tab, stored);
  }

  function bindClimateMiniGrid(tab) {
    const grid = document.getElementById(
      tab + '_climate_content_grid');
    if (!grid || grid.dataset.climateBound === '1') return;
    grid.dataset.climateBound = '1';
    const setOuterTileDragEnabled = enabled => {
      const outerTile = grid.closest('.tile.climate');
      if (!outerTile) return;
      outerTile.draggable = !!enabled;
    };
    const releaseOuterTileDrag = () => {
      if (!climateGridDragState) {
        setOuterTileDragEnabled(true);
      }
    };

    let dropPlaceholder = null;
    const ensureDropPlaceholder = () => {
      if (!dropPlaceholder) {
        dropPlaceholder = document.createElement('div');
        dropPlaceholder.className = 'climate-drop-placeholder';
      }
      if (dropPlaceholder.parentElement !== grid) {
        grid.appendChild(dropPlaceholder);
      }
      return dropPlaceholder;
    };
    const clearDropPlaceholder = () => {
      if (!dropPlaceholder) return;
      dropPlaceholder.classList.remove('show', 'invalid');
      dropPlaceholder.remove();
    };
    const showDropPlaceholder = (state, col, row, valid) => {
      const placeholder = ensureDropPlaceholder();
      const source = document.getElementById(
        state.tab + '_climate_slot_row_' + state.index);
      const sourcePreview =
        source?.querySelector('.climate-mini-preview');
      if (sourcePreview) {
        const preview = sourcePreview.cloneNode(true);
        preview.querySelectorAll('[id]').forEach(element => {
          element.removeAttribute('id');
        });
        placeholder.replaceChildren(preview);
      } else {
        placeholder.replaceChildren();
      }
      setGridItemPosition(
        placeholder, col, row,
        state.origin.span_w, state.origin.span_h);
      placeholder.classList.add('show');
      placeholder.classList.toggle('invalid', !valid);
    };
    grid.addEventListener('pointerdown', event => {
      setOuterTileDragEnabled(false);
      event.stopPropagation();
    });
    grid.addEventListener('click', event => {
      event.stopPropagation();
      window.setTimeout(releaseOuterTileDrag, 0);
    });
    window.addEventListener('pointerup', releaseOuterTileDrag, true);
    window.addEventListener('pointercancel', releaseOuterTileDrag, true);

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
      materializeClimateAutomaticItems(tab);
      const pending = climatePendingEmptyByTab[tab];
      if (pending && pending.index === index) {
        const stored = currentClimateGeometry(tab);
        stored[index] = pending.geometry;
        storeClimateGeometry(tab, stored);
        delete climatePendingEmptyByTab[tab];
      }
      source.value = selectedContent.value;
      climateSelectedCellByTab[tab] = -1;
      syncClimateSlotFields(tab);
      notifyClimateGridChanged(tab);
    });

    grid.querySelectorAll('.climate-mini-cell')
      .forEach(cell => {
        const cellIndex = Number(cell.dataset.climateCell);
        cell.addEventListener('click', event => {
        event.preventDefault();
        event.stopPropagation();
        const spanW = document.getElementById(
          tab + '_tile_span_w')?.value || 1;
        const spanH = document.getElementById(
          tab + '_tile_span_h')?.value || 1;
        const { columns, rows } =
          climateGridDimensions(spanW, spanH);
        const capacity = climateSlotCapacity(spanW, spanH);
        const configured = currentClimateSlotConfig(tab);
        const resolvedKinds =
          climateResolvedEditorKinds(tab);
        const index = configured.findIndex(
          (value, candidate) =>
            candidate < capacity &&
            (Number(value) === CLIMATE_TILE_CONTENT.EMPTY ||
             (Number(value) === CLIMATE_TILE_CONTENT.AUTO &&
              resolvedKinds[candidate] === null)));
        if (index < 0) return;
        const row = Math.floor(cellIndex / columns);
        const col = cellIndex % columns;
        if (row >= rows) return;
        climatePendingEmptyByTab[tab] = {
          index,
          geometry: { col, row, spanW: 1, spanH: 1 }
        };
        selectClimateEditorItem(
          tab, index, cellIndex,
          CLIMATE_TILE_CONTENT.EMPTY);
        document.getElementById(
          tab + '_climate_selected_fields')
          ?.classList.remove('hidden');
      });
      });

    const clearDragClasses = state => {
      if (!state) return;
      state.activeIndices.forEach(activeIndex => {
        document.getElementById(
          state.tab + '_climate_slot_row_' + activeIndex)
          ?.classList.remove(
            'dragging', 'drag-preview-positioned',
            'reflow-preview', 'invalid-drop');
      });
    };

    const restoreDragLayouts = state => {
      if (!state) return;
      applyClimateGridLayouts(
        state.tab, state.baseLayouts,
        state.activeIndices, state.baseLayouts);
      state.activeIndices.forEach(activeIndex => {
        document.getElementById(
          state.tab + '_climate_slot_row_' + activeIndex)
          ?.classList.remove(
            'drag-preview-positioned',
            'reflow-preview', 'invalid-drop');
      });
      document.getElementById(
        state.tab + '_climate_slot_row_' + state.index)
        ?.classList.add('dragging');
    };

    grid.addEventListener('dragenter', event => {
      if (!climateGridDragState ||
          climateGridDragState.tab !== tab) return;
      event.preventDefault();
      event.stopPropagation();
    });

    grid.addEventListener('dragover', event => {
      const state = climateGridDragState;
      if (!state || state.tab !== tab) return;
      event.preventDefault();
      event.stopPropagation();
      if (event.dataTransfer) {
        event.dataTransfer.dropEffect = 'move';
      }
      const raw = getGridElementCellFromPointer(
        grid, state.columns, state.rows,
        event.clientX, event.clientY);
      if (!raw) return;
      const targetCol = Math.max(
        0, Math.min(
          state.columns - state.origin.span_w,
          raw.col - state.anchorCol));
      const targetRow = Math.max(
        0, Math.min(
          state.rows - state.origin.span_h,
          raw.row - state.anchorRow));
      const previewKey = targetCol + ':' + targetRow;
      if (state.previewKey === previewKey) return;
      state.previewKey = previewKey;
      const preview = simulateGridReorderLayouts(
        state.baseLayouts, state.activeIndices,
        state.index, targetCol, targetRow,
        state.columns, state.rows, 0);
      state.preview = preview;
      showDropPlaceholder(
        state, targetCol, targetRow, !!preview);
      if (!preview) {
        restoreDragLayouts(state);
        return;
      }
      applyClimateGridLayouts(
        tab, preview.layouts,
        state.activeIndices, state.baseLayouts);
    });

    grid.addEventListener('drop', event => {
      const state = climateGridDragState;
      if (!state || state.tab !== tab) return;
      event.preventDefault();
      event.stopPropagation();
      if (!state.preview) return;
      state.committed = true;
      clearDropPlaceholder();
      storeClimateGridLayouts(tab, state.preview.layouts);
      syncClimateSlotFields(tab);
      notifyClimateGridChanged(tab);
    });

    for (let index = 0; index < 6; ++index) {
      const item = document.getElementById(
        tab + '_climate_slot_row_' + index);
      if (!item) continue;

      item.draggable = true;
      item.addEventListener('pointerdown', event => {
        if (event.target.closest('[data-climate-resize]')) {
          return;
        }
        event.stopPropagation();
        selectClimateEditorItem(tab, index);
        delete climatePendingEmptyByTab[tab];
      });
      item.addEventListener('click', event => {
        event.stopPropagation();
        selectClimateEditorItem(tab, index);
        delete climatePendingEmptyByTab[tab];
      });

      item.addEventListener('dragstart', event => {
        if (event.target.closest('[data-climate-resize]')) {
          event.preventDefault();
          return;
        }
        event.stopPropagation();
        setOuterTileDragEnabled(false);
        selectClimateEditorItem(tab, index);
        delete climatePendingEmptyByTab[tab];
        materializeClimateAutomaticItems(tab);
        const spanW = document.getElementById(
          tab + '_tile_span_w')?.value || 1;
        const spanH = document.getElementById(
          tab + '_tile_span_h')?.value || 1;
        const { columns, rows } =
          climateGridDimensions(spanW, spanH);
        const capacity = climateSlotCapacity(spanW, spanH);
        const activeIndices =
          climateActiveGridIndices(tab, capacity);
        const baseLayouts =
          climateGridLayouts(tab, columns, rows);
        const origin = cloneLayout(baseLayouts[index]);
        if (!origin || !activeIndices.has(index)) {
          event.preventDefault();
          return;
        }
        const metrics = getGridElementMetrics(
          grid, columns, rows);
        const itemRect = item.getBoundingClientRect();
        const stepX = metrics
          ? metrics.cellW + metrics.gapX : itemRect.width;
        const stepY = metrics
          ? metrics.cellH + metrics.gapY : itemRect.height;
        const localX = Math.max(
          0, event.clientX - itemRect.left);
        const localY = Math.max(
          0, event.clientY - itemRect.top);
        const anchorCol = Math.max(
          0, Math.min(
            origin.span_w - 1,
            Math.floor(localX / Math.max(1, stepX))));
        const anchorRow = Math.max(
          0, Math.min(
            origin.span_h - 1,
            Math.floor(localY / Math.max(1, stepY))));
        climateGridDragState = {
          tab,
          index,
          columns,
          rows,
          activeIndices,
          baseLayouts,
          origin,
          anchorCol,
          anchorRow,
          preview: null,
          previewKey: '',
          committed: false
        };
        if (event.dataTransfer) {
          event.dataTransfer.effectAllowed = 'move';
          event.dataTransfer.setData(
            'text/plain',
            'climate:' + tab + ':' + index);
          const rect = item.getBoundingClientRect();
          climateGridDragPreview =
            createClimateMiniDragGhost(item, rect);
          event.dataTransfer.setDragImage(
            climateGridDragPreview,
            Math.max(
              0, Math.min(
                rect.width,
                event.clientX - rect.left)),
            Math.max(
              0, Math.min(
                rect.height,
                event.clientY - rect.top)));
        }
        showDropPlaceholder(
          climateGridDragState, origin.col, origin.row, true);
        item.classList.add('dragging');
      });

      item.addEventListener('dragend', event => {
        event.stopPropagation();
        clearDropPlaceholder();
        const state = climateGridDragState;
        if (!state ||
            state.tab !== tab ||
            state.index !== index) {
          climateGridDragPreview?.remove();
          climateGridDragPreview = null;
          setOuterTileDragEnabled(true);
          return;
        }
        if (!state.committed) restoreDragLayouts(state);
        clearDragClasses(state);
        climateGridDragState = null;
        climateGridDragPreview?.remove();
        climateGridDragPreview = null;
        setOuterTileDragEnabled(true);
      });

      item.querySelectorAll('[data-climate-resize]')
        .forEach(handle => {
          handle.addEventListener('pointerdown', event => {
            event.preventDefault();
            event.stopPropagation();
            selectClimateEditorItem(tab, index);
            materializeClimateAutomaticItems(tab);
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
            const layouts =
              climateGridLayouts(tab, columns, rows);
            const activeIndices =
              climateActiveGridIndices(tab, capacity);
            const direction =
              String(handle.dataset.climateResize || 'se');
            item.classList.add('resizing');
            const onMove = moveEvent => {
              const cell = getGridElementCellFromPointer(
                grid, columns, rows,
                moveEvent.clientX, moveEvent.clientY);
              if (!cell) return;
              const candidate = {
                col: origin.col,
                row: origin.row,
                span_w: origin.spanW,
                span_h: origin.spanH
              };
              if (direction.includes('e')) {
                candidate.span_w =
                  cell.col - origin.col + 1;
              }
              if (direction.includes('s')) {
                candidate.span_h =
                  cell.row - origin.row + 1;
              }
              candidate.span_w = Math.max(
                1, Math.min(
                  columns - candidate.col,
                  candidate.span_w));
              candidate.span_h = Math.max(
                1, Math.min(
                  rows - candidate.row,
                  candidate.span_h));
              if (!canPlaceGridLayout(
                    layouts, activeIndices, index,
                    candidate, columns, rows, 0)) {
                item.classList.add('resize-invalid');
                return;
              }
              item.classList.remove('resize-invalid');
              layouts[index] = candidate;
              stored[index] = {
                col: candidate.col,
                row: candidate.row,
                spanW: candidate.span_w,
                spanH: candidate.span_h
              };
              setGridItemPosition(
                item, candidate.col, candidate.row,
                candidate.span_w, candidate.span_h);
              renderClimateEditorItem(
                tab, index, stored[index],
                configured[index]);
            };
            const onEnd = endEvent => {
              window.removeEventListener(
                'pointermove', onMove, true);
              window.removeEventListener(
                'pointerup', onEnd, true);
              window.removeEventListener(
                'pointercancel', onEnd, true);
              item.classList.remove(
                'resizing', 'resize-invalid');
              storeClimateGeometry(tab, stored);
              syncClimateSlotFields(tab);
              notifyClimateGridChanged(tab);
              setOuterTileDragEnabled(true);
            };
            window.addEventListener(
              'pointermove', onMove, true);
            window.addEventListener(
              'pointerup', onEnd, true);
            window.addEventListener(
              'pointercancel', onEnd, true);
          });
        });
    }
  }

  function syncClimateSlotFields(
      tab, finalizePreviewSelection = false) {
    mountClimateMiniEditor(tab);
    const spanW = Math.max(1, Number(document.getElementById(
      tab + '_tile_span_w')?.value) || 1);
    const spanH = Math.max(1, Number(document.getElementById(
      tab + '_tile_span_h')?.value) || 1);
    const capacity = climateSlotCapacity(spanW, spanH);
    const { columns, rows } =
      climateGridDimensions(spanW, spanH);
    let configured = currentClimateSlotConfig(tab);
    let resolvedKinds = climateResolvedEditorKinds(tab);
    const previousSnapshot = climateEditorSnapshotByTab[tab];
    if (previousSnapshot &&
        previousSnapshot.tileIndex === currentTileIndex &&
        (previousSnapshot.spanW !== spanW ||
         previousSnapshot.spanH !== spanH)) {
      // Sobald der Nutzer die aeussere Climate-Kachel vergroessert oder
      // verkleinert, bleibt der bisher sichtbare Inhalt erhalten. Ungenutzte
      // Automatic-Slots werden zu Leer, statt durch den zusaetzlichen Platz
      // ploetzlich neue Mini-Tiles zu erzeugen.
      materializeClimateAutomaticItems(
        tab, previousSnapshot.resolvedKinds);
      configured = currentClimateSlotConfig(tab);
      resolvedKinds = climateResolvedEditorKinds(tab);
    }
    const placementConfig = climatePlacementConfig(
      configured, resolvedKinds);
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
        kind !== CLIMATE_TILE_CONTENT.EMPTY &&
        resolvedKinds[index] !== null;
      if (!item) continue;
      item.classList.toggle('hidden', !active);
      if (!active) continue;

      let geometry = items[index];
      if (accepted.some(other =>
            climateGeometryOverlaps(
              geometry, other.geometry))) {
        const free = firstFreeClimatePlacement(
          items, placementConfig, capacity,
          columns, rows, index,
          geometry.spanW, geometry.spanH);
        if (!free) {
          item.classList.add('hidden');
          continue;
        }
        geometry = free;
        items[index] = free;
        stored[index] = free;
      }
      accepted.push({ index, geometry });
      setGridItemPosition(
        item, geometry.col, geometry.row,
        geometry.spanW, geometry.spanH);
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

    grid?.querySelectorAll('.climate-mini-cell')
      .forEach(cell => {
      const cellIndex = Number(cell.dataset.climateCell);
      const row = Math.floor(cellIndex / columns);
      const col = cellIndex % columns;
      const visible =
        Number.isFinite(cellIndex) &&
        cellIndex >= 0 &&
        cellIndex < columns * rows;
      cell.classList.toggle('hidden', !visible);
      cell.classList.toggle(
        'occupied',
        !visible || !!occupied[cellIndex]);
      if (visible) {
        cell.style.gridColumn = String(col + 1);
        cell.style.gridRow = String(row + 1);
      }
      });

    storeClimateGeometry(tab, stored);
    bindClimateMiniGrid(tab);
    const directSelection =
      climatePendingPreviewSelectionByTab[tab];
    if (directSelection &&
        directSelection.tileIndex === currentTileIndex) {
      const directItem = Number(directSelection.itemIndex);
      const directCell = Number(directSelection.cellIndex);
      if (Number.isFinite(directItem) &&
          directItem >= 0 && directItem < 6) {
        const item = document.getElementById(
          tab + '_climate_slot_row_' + directItem);
        if (item && !item.classList.contains('hidden')) {
          climateSelectedItemByTab[tab] = directItem;
          climateSelectedCellByTab[tab] = -1;
          delete climatePendingEmptyByTab[tab];
          if (finalizePreviewSelection) {
            delete climatePendingPreviewSelectionByTab[tab];
          }
        }
      } else if (Number.isFinite(directCell) &&
                 directCell >= 0 &&
                 directCell < columns * rows) {
        const cell = document.getElementById(
          tab + '_climate_cell_' + directCell);
        const index = configured.findIndex(
          (value, candidate) =>
            candidate < capacity &&
            (Number(value) === CLIMATE_TILE_CONTENT.EMPTY ||
             (Number(value) === CLIMATE_TILE_CONTENT.AUTO &&
              resolvedKinds[candidate] === null)));
        if (cell &&
            !cell.classList.contains('hidden') &&
            !cell.classList.contains('occupied') &&
            index >= 0) {
          const row = Math.floor(directCell / columns);
          const col = directCell % columns;
          climatePendingEmptyByTab[tab] = {
            index,
            geometry: { col, row, spanW: 1, spanH: 1 }
          };
          climateSelectedItemByTab[tab] = index;
          climateSelectedCellByTab[tab] = directCell;
          if (finalizePreviewSelection) {
            delete climatePendingPreviewSelectionByTab[tab];
          }
        }
      }
    }
    let selected = Number(climateSelectedItemByTab[tab]);
    let selectedCell = Number(climateSelectedCellByTab[tab]);
    const selectedItem = Number.isFinite(selected)
      ? document.getElementById(
          tab + '_climate_slot_row_' + selected)
      : null;
    const selectedItemVisible =
      !!selectedItem &&
      !selectedItem.classList.contains('hidden');
    const selectedCellElement =
      Number.isFinite(selectedCell) && selectedCell >= 0
        ? document.getElementById(
            tab + '_climate_cell_' + selectedCell)
        : null;
    const selectedEmptyVisible =
      !!selectedCellElement &&
      !selectedCellElement.classList.contains('hidden') &&
      !selectedCellElement.classList.contains('occupied') &&
      climatePendingEmptyByTab[tab]?.index === selected;
    if (selectedItemVisible) {
      selectedCell = -1;
      selectClimateEditorItem(tab, selected);
    } else if (selectedEmptyVisible) {
      selectClimateEditorItem(
        tab, selected, selectedCell,
        CLIMATE_TILE_CONTENT.EMPTY);
    } else {
      selected = -1;
      selectedCell = -1;
      delete climatePendingEmptyByTab[tab];
      selectClimateEditorItem(tab, -1);
    }
    const selectedFields = document.getElementById(
      tab + '_climate_selected_fields');
    selectedFields?.classList.toggle(
      'hidden', selected < 0);
    climateEditorSnapshotByTab[tab] = {
      tileIndex: currentTileIndex,
      spanW,
      spanH,
      resolvedKinds: resolvedKinds.slice()
    };
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
    syncClimateSlotFields(tab, true);
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
    const { columns, rows } =
      climateGridDimensions(w, h);
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
          geometry[index], columns, rows)
      });
    }

    const logicalRows = rows;
    const placedSlots = [];
    slots.forEach(slot => {
      let candidate = {
        col: slot.col,
        row: slot.row,
        spanW: slot.spanW,
        spanH: slot.spanH
      };
      const overlaps = value =>
        placedSlots.some(other =>
            climateGeometryOverlaps(value, {
              col: other.col,
              row: other.row,
              spanW: other.spanW,
              spanH: other.spanH
            }));
      if (overlaps(candidate)) {
        let free = null;
        for (let row = 0;
             row + candidate.spanH <= logicalRows && !free;
             ++row) {
          for (let col = 0;
               col + candidate.spanW <= columns;
               ++col) {
            const next = {
              col, row,
              spanW: candidate.spanW,
              spanH: candidate.spanH
            };
            if (!placedSlots.some(other =>
                  climateGeometryOverlaps(next, {
                    col: other.col,
                    row: other.row,
                    spanW: other.spanW,
                    spanH: other.spanH
                  }))) {
              free = next;
              break;
            }
          }
        }
        if (!free) return;
        candidate = free;
      }
      placedSlots.push({ ...slot, ...candidate });
    });

    const occupiedCells =
      Array(columns * logicalRows).fill(false);
    placedSlots.forEach(slot => {
      for (let row = slot.row;
           row < slot.row + slot.spanH; ++row) {
        for (let col = slot.col;
             col < slot.col + slot.spanW; ++col) {
          occupiedCells[row * columns + col] = true;
        }
      }
    });
    const emptyCellLabel =
      german ? 'Leeres Feld' : 'Empty field';
    const previewCells = occupiedCells.map(
      (occupied, cellIndex) => {
        if (occupied) return '';
        const row = Math.floor(cellIndex / columns) + 1;
        const column = cellIndex % columns + 1;
        return '<button type="button" ' +
          'class="climate-preview-cell" ' +
          'data-climate-preview-cell="' + cellIndex + '" ' +
          'aria-label="' + emptyCellLabel + '" ' +
          'style="grid-column:' + column +
          ';grid-row:' + row + '"></button>';
      }).join('');

    return '<div class="climate-slots" style="--climate-columns:' +
      columns +
      ';--climate-rows:' + logicalRows + '">' +
      previewCells +
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
            '" data-climate-preview-item="' +
            slot.itemIndex + '" style="' +
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
          '" data-climate-preview-item="' +
          slot.itemIndex + '" style="' + gridStyle + '">' +
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
  bindClimatePreviewSelection();
  </script>
)html";
}
