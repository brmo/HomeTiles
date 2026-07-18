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
      left:8px;
      right:8px;
      /* 1x1 value center: 50 px. A 32 px slot therefore begins at 34 px.
         This keeps the first row of every tile size on one exact line. */
      top:34px;
      /* The last row keeps the same lower edge distance as a 1x1 tile. */
      bottom:7px;
      display:grid;
      grid-template-columns:repeat(var(--climate-columns, 1), minmax(0, 1fr));
      grid-auto-rows:32px;
      align-content:space-between;
      gap:8px;
      pointer-events:none;
    }
    .tile.climate .climate-slot {
      min-width:0;
      min-height:32px;
      display:flex;
      align-items:center;
      justify-content:center;
      color:#fff;
      line-height:1;
      overflow:hidden;
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
      border:1px solid #4a4a4a;
      border-radius:10px;
      background:transparent;
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
      line-height:1;
      text-align:center;
      text-overflow:ellipsis;
      white-space:nowrap;
    }
    .tile.climate .climate-slot-control-horizontal {
      grid-template-columns:108px 32px minmax(0, 1fr) 32px;
      grid-template-rows:1fr;
      padding:0 4px;
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
      grid-template-columns:1fr 1fr;
      grid-template-rows:repeat(3, minmax(0, 1fr));
      padding:8px 0 2px;
    }
    .tile.climate .climate-slot-control-vertical small {
      grid-column:1 / span 2;
      grid-row:1;
      align-self:start;
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
    .climate-content-config {
      margin-top:14px;
      padding-top:14px;
      border-top:1px solid #2d2d2d;
    }
    .climate-content-heading {
      display:block;
      margin:0 0 8px;
    }
    .climate-content-grid {
      display:grid;
      grid-template-columns:repeat(2, minmax(0, 1fr));
      gap:10px;
    }
    .climate-content-slot {
      min-width:0;
    }
    .climate-content-slot label {
      display:block;
      margin:0 0 5px;
      color:#9ca3af;
      font-size:12px;
    }
    .climate-content-slot select {
      width:100%;
      min-width:0;
    }
    .climate-target-layout {
      margin-top:8px;
      padding:8px;
      border-radius:8px;
      background:#191919;
    }
    .climate-target-layout label {
      margin-bottom:5px;
    }
    .climate-content-slot.hidden {
      display:none;
    }
    .climate-content-hint {
      margin-top:8px;
    }
    @media (max-width:1100px) {
      .climate-content-grid {
        grid-template-columns:1fr;
      }
    }
  </style>
)html";
}
