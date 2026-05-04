# Touch debug capture

Captured: 2026-05-04T15:07:23
Port: /dev/cu.usbmodem1101
Duration: 180s
Filter: TOUCHDBG

## Repro matrix (run in this order)

- **A** Tap each of the 4 screen corners
- **B** Tap each of the 4 button-hint slots in the bottom bar
- **C** Tap the center of every grid cell on Dashboard
- **D** Tap exactly on a 6 px gap between two grid cells
- **E** Repeat A-D after rotating the device (settings -> orientation)

## Captured lines (`[TOUCHDBG]` only)

```text
+ 122213ms  [706938] [INF] [TOUCHDBG] hint_miss x=24 y=46 hint_y=[752,800)
+ 122214ms  [706938] [INF] [TOUCHDBG] fallback_zone event=tap x=24 y=46 button=back edge=80 thirds=(160,320)
+ 122214ms  [706938] [INF] [TOUCHDBG] hint_miss x=24 y=46 hint_y=[752,800)
+ 122214ms  [706938] [INF] [TOUCHDBG] raw_type=tap oriented_type=tap raw=(24,46) oriented=(24,46) screen=480x800 orient=0 hintMapped=0 hintButton=255
+ 122214ms  [706938] [INF] [TOUCHDBG] hint_miss x=24 y=46 hint_y=[752,800)
+ 122214ms  [706939] [INF] [TOUCHDBG] hint_miss x=24 y=46 hint_y=[752,800)
+ 122215ms  [706939] [INF] [TOUCHDBG] dashboard_miss layout=ios-grid raw=(24,46) oriented=(24,46) items=9
+ 123070ms  [707794] [INF] [TOUCHDBG] hint_miss x=479 y=14 hint_y=[752,800)
+ 123070ms  [707794] [INF] [TOUCHDBG] fallback_zone event=tap x=479 y=14 button=right edge=80 thirds=(160,320)
+ 123071ms  [707794] [INF] [TOUCHDBG] hint_miss x=479 y=14 hint_y=[752,800)
+ 123071ms  [707794] [INF] [TOUCHDBG] raw_type=tap oriented_type=tap raw=(479,14) oriented=(479,14) screen=480x800 orient=0 hintMapped=0 hintButton=255
+ 123071ms  [707794] [INF] [TOUCHDBG] hint_miss x=479 y=14 hint_y=[752,800)
+ 123072ms  [707795] [INF] [TOUCHDBG] hint_miss x=479 y=14 hint_y=[752,800)
+ 123073ms  [707795] [INF] [TOUCHDBG] dashboard_miss layout=ios-grid raw=(479,14) oriented=(479,14) items=9
+ 123944ms  [708669] [INF] [TOUCHDBG] hint_gap x=0 y=799 hint_y=[752,800)
+ 123945ms  [708669] [INF] [TOUCHDBG] fallback_bottom_guard event=tap x=0 y=799 bottom_zone_y=[690,800)
+ 123945ms  [708669] [INF] [TOUCHDBG] hint_gap x=0 y=799 hint_y=[752,800)
+ 123946ms  [708669] [INF] [TOUCHDBG] raw_type=tap oriented_type=tap raw=(0,799) oriented=(0,799) screen=480x800 orient=0 hintMapped=0 hintButton=255
+ 123946ms  [708669] [INF] [TOUCHDBG] hint_gap x=0 y=799 hint_y=[752,800)
+ 123946ms  [708669] [INF] [TOUCHDBG] hint_gap x=0 y=799 hint_y=[752,800)
+ 123947ms  [708670] [INF] [TOUCHDBG] dashboard_miss layout=ios-grid raw=(0,799) oriented=(0,799) items=9
+ 140882ms  [725607] [INF] [TOUCHDBG] hint_miss x=68 y=236 hint_y=[752,800)
+ 140882ms  [725607] [INF] [TOUCHDBG] fallback_zone event=tap x=68 y=236 button=back edge=80 thirds=(160,320)
+ 140882ms  [725607] [INF] [TOUCHDBG] hint_miss x=68 y=236 hint_y=[752,800)
+ 140882ms  [725607] [INF] [TOUCHDBG] raw_type=tap oriented_type=tap raw=(68,236) oriented=(68,236) screen=480x800 orient=0 hintMapped=0 hintButton=255
+ 140883ms  [725607] [INF] [TOUCHDBG] dashboard_hit layout=ios-grid target=shortcut index=0 raw=(68,236) oriented=(68,236) rect=(20,105,146,218)
+ 141464ms  [726188] [INF] [TOUCHDBG] hint_miss x=260 y=225 hint_y=[752,800)
+ 141465ms  [726188] [INF] [TOUCHDBG] fallback_zone event=tap x=260 y=225 button=confirm edge=80 thirds=(160,320)
+ 141466ms  [726188] [INF] [TOUCHDBG] hint_miss x=260 y=225 hint_y=[752,800)
+ 141467ms  [726188] [INF] [TOUCHDBG] raw_type=tap oriented_type=tap raw=(260,225) oriented=(260,225) screen=480x800 orient=0 hintMapped=0 hintButton=255
```

## Summary

Kept 30 `[TOUCHDBG]` lines.
