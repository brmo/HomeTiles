#include "src/types/climate/web_styles.h"

void append_climate_styles(String& html) {
  html += R"html(
  <style>
    .tile.climate {
      display:grid;
      grid-template-rows:auto 1fr;
      grid-template-columns:1fr;
    }
    .tile.climate .tile-title {
      grid-column:1;
      grid-row:1;
      justify-self:end;
      align-self:start;
      width:auto;
      max-width:70%;
      text-align:right;
      overflow:hidden;
      text-overflow:ellipsis;
      white-space:nowrap;
    }
    .tile.climate .tile-icon {
      position:absolute;
      top:8px;
      left:6px;
    }
    .tile.climate .tile-value {
      position:absolute;
      left:0;
      right:0;
      top:50%;
      transform:translateY(calc(-50% + var(--value-dy, 14px)));
      margin:0;
      color:#fff;
      font-size:var(--fs28, 14px);
      font-weight:normal;
      text-align:center;
      line-height:1;
    }
    .tile.climate .climate-slots {
      position:absolute;
      /* Every Web tile already owns a permanent transparent 3 px selection
         border. Compensate it here so the visible climate geometry uses the
         same scaled insets as LVGL instead of adding the border a second
         time. */
      left:calc(var(--climate-margin-x, 5px) - 3px);
      right:calc(var(--climate-margin-x, 5px) - 3px);
      top:calc(var(--climate-slots-top, 35px) - 3px);
      bottom:calc(var(--climate-slots-bottom, 3px) - 3px);
      display:grid;
      grid-template-columns:repeat(var(--climate-columns, 1), minmax(0, 1fr));
      grid-template-rows:
        repeat(var(--climate-rows, 1), minmax(0, 1fr));
      align-content:stretch;
      gap:var(--climate-grid-gap, 5px);
      pointer-events:auto;
    }
    .tile.climate .climate-slot {
      position:relative;
      z-index:2;
      box-sizing:border-box;
      min-width:0;
      min-height:0;
      display:flex;
      align-items:center;
      justify-content:center;
      color:#fff;
      line-height:1;
      overflow:hidden;
      cursor:pointer;
      pointer-events:auto;
    }
    .tile.climate:not(.climate-content-editing)
      .climate-slot[data-climate-preview-item]:hover {
      outline:2px dashed rgba(38,166,154,.72);
      outline-offset:-2px;
      border-radius:var(--climate-control-radius, 8px);
    }
    .climate-preview-cell {
      position:relative;
      z-index:1;
      min-width:0;
      min-height:0;
      padding:0;
      box-sizing:border-box;
      border:1px solid transparent;
      border-radius:var(--climate-control-radius, 8px);
      background:transparent;
      color:transparent;
      cursor:pointer;
    }
    .climate-preview-cell:hover {
      border:2px dashed rgba(38,166,154,.72);
      background:rgba(38,166,154,.06);
    }
    .tile.climate .climate-slot strong {
      min-width:0;
      overflow:hidden;
      text-overflow:ellipsis;
      white-space:nowrap;
      font-size:var(--fs28, 14px);
      font-weight:400;
      text-align:center;
    }
    .tile.climate .climate-slot-mode strong {
      font-size:var(--fs20, 10px);
    }
    .tile.climate .climate-slot-control {
      display:grid;
      align-items:center;
      justify-items:center;
      border:0;
      /* 11 px outer radius - 3 px effective inset = 8 px. */
      border-radius:var(--climate-control-radius, 8px);
      background:#3a3a3a;
    }
    .tile.climate .climate-slot-control span {
      font-size:var(--fs24, 12px);
      text-align:center;
    }
    .tile.climate .climate-slot-control small {
      min-width:0;
      overflow:hidden;
      color:#e0e0e0;
      font-size:var(--fs20, 10px);
      font-weight:400;
      line-height:1.2;
      text-align:center;
      text-overflow:ellipsis;
      white-space:nowrap;
    }
    .tile.climate .climate-slot-control-horizontal {
      grid-template-columns:
        var(--climate-control-caption-w, 48px)
        var(--climate-control-button-w, 20px)
        minmax(0, 1fr)
        var(--climate-control-button-w, 20px);
      grid-template-rows:1fr;
      padding:0 var(--climate-control-side-pad, 4px);
    }
    .tile.climate .climate-slot-control-horizontal small {
      grid-column:1;
    }
    .tile.climate .climate-slot-control-horizontal .climate-minus {
      grid-column:2;
    }
    .tile.climate .climate-slot-control-horizontal strong {
      grid-column:3;
      font-size:var(--fs28, 14px);
    }
    .tile.climate .climate-slot-control-horizontal .climate-plus {
      grid-column:4;
    }
    .tile.climate .climate-slot-control-vertical {
      width:100%;
      grid-template-columns:1fr 1fr;
      grid-template-rows:repeat(3, minmax(0, 1fr));
      padding:
        var(--climate-control-v-pad-top, 4px) 0
        var(--climate-control-v-pad-bottom, 1px);
    }
    .tile.climate .climate-slot-control-vertical small {
      grid-column:1 / span 2;
      grid-row:1;
      align-self:center;
    }
    .tile.climate .climate-slot-control-vertical strong {
      grid-column:1 / span 2;
      grid-row:2;
      align-self:center;
      font-size:var(--fs28, 14px);
    }
    .tile.climate .climate-slot-control-vertical .climate-minus {
      grid-column:1;
      grid-row:3;
    }
    .tile.climate .climate-slot-control-vertical .climate-plus {
      grid-column:2;
      grid-row:3;
    }
    .tile.climate .climate-slot-control-large {
      grid-template-columns:1fr 1fr;
      grid-template-rows:repeat(3, minmax(0, 1fr));
      padding:12px 18px;
    }
    .tile.climate .climate-slot-control-large small {
      grid-column:1 / span 2;
      grid-row:1;
      align-self:end;
    }
    .tile.climate .climate-slot-control-large strong {
      grid-column:1 / span 2;
      grid-row:2;
      align-self:center;
      font-size:var(--fs28, 14px);
    }
    .tile.climate .climate-slot-control-large .climate-minus {
      grid-column:1;
      grid-row:3;
    }
    .tile.climate .climate-slot-control-large .climate-plus {
      grid-column:2;
      grid-row:3;
    }
    .climate-content-config {
      margin-top:14px;
      padding-top:14px;
      border-top:1px solid #2d2d2d;
    }
    .climate-editor-stash {
      display:none;
    }
    .climate-mini-editor-shell {
      padding:10px;
      border:1px solid var(--line);
      border-radius:18px;
      background:#000;
    }
    .climate-content-grid {
      --climate-editor-cell-w:128px;
      --climate-editor-cell-h:72px;
      --climate-editor-gap:7px;
      position:relative;
      display:grid;
      grid-template-columns:
        repeat(var(--climate-editor-columns, 1),
          var(--climate-editor-cell-w));
      grid-template-rows:
        repeat(var(--climate-editor-rows, 1),
          var(--climate-editor-cell-h));
      gap:var(--climate-editor-gap);
      width:fit-content;
      min-height:var(--climate-editor-cell-h);
      margin:0 auto;
      isolation:isolate;
    }
    .climate-mini-cell {
      z-index:1;
      min-width:0;
      min-height:0;
      padding:0;
      box-sizing:border-box;
      border:1px solid transparent;
      border-radius:var(--climate-control-radius, 8px);
      background:transparent;
      color:transparent;
      cursor:pointer;
    }
    .climate-mini-cell:hover {
      border:2px dashed rgba(38,166,154,.72);
      background:rgba(38,166,154,.06);
    }
    .climate-mini-cell.active {
      z-index:3;
      border-color:#26a69a;
      box-shadow:0 0 0 2px rgba(38,166,154,.3) inset;
    }
    .climate-mini-cell.occupied {
      visibility:hidden;
      pointer-events:none;
    }
    .climate-mini-cell.hidden {
      display:none;
    }
    .climate-mini-tile {
      z-index:2;
      position:relative;
      min-width:0;
      min-height:0;
      padding:7px 8px;
      box-sizing:border-box;
      overflow:hidden;
      border:0;
      border-radius:var(--climate-control-radius, 8px);
      background:transparent;
      color:#fff;
      cursor:grab;
      user-select:none;
      touch-action:auto;
    }
    .climate-mini-tile::after {
      content:"";
      position:absolute;
      z-index:4;
      inset:0;
      box-sizing:border-box;
      border:1px solid transparent;
      border-radius:var(--climate-control-radius, 8px);
      pointer-events:none;
    }
    .climate-mini-tile.hidden {
      display:none;
    }
    .climate-mini-tile:hover:not(.active):not(.dragging) {
      background:rgba(38,166,154,.08);
    }
    .climate-mini-tile:hover:not(.active):not(.dragging)::after {
      border:2px dashed rgba(38,166,154,.72);
      box-shadow:0 0 0 1px rgba(38,166,154,.18) inset;
    }
    .climate-mini-tile.dragging {
      opacity:.02;
      box-shadow:none;
    }
    /* Gestricheltes Drop-Ziel analog .tile-drop-placeholder des grossen
       Grids, nur in Mini-Masstab. Wird per JS im Grid positioniert. */
    .climate-drop-placeholder {
      z-index:6;
      position:relative;
      display:none;
      min-width:0;
      min-height:0;
      overflow:hidden;
      border:2px dashed #26a69a;
      border-radius:var(--climate-control-radius, 8px);
      background:rgba(38,166,154,0.18);
      color:#fff;
      box-shadow:0 0 0 1px rgba(38,166,154,0.2) inset;
      pointer-events:none;
    }
    .climate-drop-placeholder > .climate-mini-preview {
      inset:0;
    }
    .climate-drop-placeholder.show { display:block; }
    .climate-drop-placeholder.invalid {
      border-color:#ef4444;
      background:rgba(239,68,68,0.16);
      box-shadow:0 0 0 1px rgba(239,68,68,0.18) inset;
    }
    /* Drag-Geist: Wrapper traegt .tile.climate, damit die darunter
       gescopten Slot-Styles auch ausserhalb der Kachel greifen. */
    .climate-mini-drag-ghost {
      border-radius:var(--climate-control-radius, 8px);
      clip-path:inset(
        0 round var(--climate-control-radius, 8px));
    }
    .climate-mini-tile.reflow-preview {
      filter:brightness(1.06);
    }
    .climate-mini-tile.reflow-preview::after {
      border:2px solid rgba(100,216,203,.72);
      box-shadow:
        0 0 0 1px rgba(100,216,203,.34) inset,
        0 0 8px rgba(38,166,154,.22);
    }
    .climate-mini-tile.invalid-drop::after,
    .climate-mini-tile.resize-invalid::after {
      border:2px dashed #ef4444;
      box-shadow:0 0 0 2px rgba(239,68,68,.18) inset;
    }
    .climate-mini-preview {
      position:absolute;
      inset:0;
      display:flex;
      align-items:center;
      justify-content:center;
      min-width:0;
      color:#fff;
      text-align:center;
      pointer-events:none;
    }
    .climate-mini-preview > .climate-slot {
      width:100%;
      height:100%;
      min-height:0;
    }
    .climate-mini-tile.active {
      z-index:4;
    }
    .climate-mini-tile.active::after {
      border:2px solid #26a69a;
      box-shadow:0 0 0 2px rgba(38,166,154,.42) inset;
    }
    .climate-mini-tile > .tile-resize-handle {
      z-index:5;
    }
    .tile.active .climate-mini-tile:not(.active) >
      .tile-resize-handle,
    .tile[data-selected="1"] .climate-mini-tile:not(.active) >
      .tile-resize-handle {
      opacity:0 !important;
      pointer-events:none !important;
    }
    .climate-mini-tile.active > .tile-resize-handle {
      opacity:.46 !important;
      pointer-events:auto !important;
    }
    .tile.climate.climate-content-editing >
      .climate-slots {
      visibility:hidden;
    }
    .tile.climate.climate-content-editing >
      .climate-mini-editor-shell {
      position:absolute;
      z-index:12;
      left:calc(var(--climate-margin-x, 5px) - 3px);
      right:calc(var(--climate-margin-x, 5px) - 3px);
      top:calc(var(--climate-slots-top, 35px) - 3px);
      bottom:calc(var(--climate-slots-bottom, 3px) - 3px);
      padding:0;
      border:0;
      border-radius:0;
      background:transparent;
      overflow:visible;
    }
    .tile.climate.climate-content-editing >
      .climate-mini-editor-shell >
      .climate-content-grid {
      --climate-editor-cell-w:auto;
      --climate-editor-cell-h:auto;
      --climate-editor-gap:var(--climate-grid-gap, 5px);
      grid-template-columns:
        repeat(var(--climate-editor-columns, 1), minmax(0, 1fr));
      grid-template-rows:
        repeat(var(--climate-editor-rows, 1), minmax(0, 1fr));
      align-content:stretch;
      width:100%;
      height:100%;
      min-height:0;
      margin:0;
    }
    .tile.climate.climate-content-editing
      .climate-mini-tile {
      padding:0;
      border-radius:var(--climate-control-radius, 8px);
    }
    .tile.climate.climate-content-editing
      .climate-mini-preview {
      inset:0;
    }
    .climate-slot-storage,
    .climate-layout-storage {
      display:none !important;
    }
    .climate-selected-fields {
      margin-top:8px;
    }
    .climate-selected-fields label {
      display:block;
      margin-bottom:6px;
    }
    @media (max-width:1100px) {
      .climate-content-grid {
        --climate-editor-cell-w:112px;
        --climate-editor-cell-h:64px;
      }
    }
  </style>
)html";
}
