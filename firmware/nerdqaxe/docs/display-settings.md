# Screen size and panel selection

QAxe Beta8 carries forward the saved screen selector introduced in the unpublished
Beta7 work, under **Miner controls → Display → Screen**.
The **local OctAxe Beta6 test candidate** adds the same explicit choices in its
own matched application/web pair; public OctAxe Beta5 remains standard-only.
See the [OctAxe working assumption and limits](octaxe-480-display.md). One
application for each of those selected families supports these choices:

| Screen | Resolution | Panel settings |
| --- | --- | --- |
| Standard | 320×170 | Existing RGB, offset and orientation |
| 3.5-inch RGB | 480×320 | Full viewport, zero offset, RGB order and large-panel orientation |
| 3.5-inch BGR | 480×320 | Full viewport, zero offset, alternate colour order and large-panel orientation |

Click **Save this section**, then **Restart device**. Refresh the page after the
miner returns to read its active screen again. The page distinguishes the saved
choice from the active screen until that restart. Opening the page does not
change the screen. Existing settings default to **Standard** when no choice has
been stored. The selection stays in the miner's existing settings through
compatible application/WWW updates; clocks, voltage, cooling, pools, slots and
schedules are not part of a display-only edit.

A small interface surrounded by unused white space is consistent with a standard
320×170 profile on a larger screen. Yellow instead of cyan, or brown instead of
dark blue, is consistent with swapped colour order. Use the appropriate 3.5-inch
option; **Rotate miner screen** handles a 180-degree installation orientation.
The two 3.5-inch options also remove the standard panel's single-axis mirroring.
**Invert screen colours** now controls panel inversion while retaining the old
default appearance when it is off. A mirrored photograph alone cannot identify
a display controller.

The 3.5-inch options remain **experimental** until checked on the actual panel.
They retain the existing controller bus pins, flash/partition layout, board
identity guard and mining hardware behavior. Selecting a resolution does not
establish compatibility with another controller or a vendor's different flash
layout. No OctAxe larger panel is physically qualified, and Beta5's owned-board
screen observations do not validate the local Beta6 candidate.

The application rejects known older standard-only firmware when a larger screen is selected
or still active, before pausing mining or starting an update. The inherited
upstream factory updater is also blocked in that state. Use a matching paired
5tratumFW update that retains screen selection. Returning to Standard is an
explicit save and restart; neither an update check nor a web refresh resets it.

Runtime profile information is available read-only from `/api/v2/system`.
Existing MUX capability and status schemas are unchanged. `/api/v2/settings` contains only the durable
`displayProfile` value. Releases declare their supported profiles separately
from hardware qualification in `manifest.json`; `/build-info.json` retains its
existing source/version schema.

The [panel references and fixed-profile experiments](experimental-480-display.md)
record where the colour, timing, offset and orientation settings originate.
Use the matching OctAxe Beta6 pair only for the local OctAxe candidate; the
common choices do not make QAxe and OctAxe images interchangeable. No new
public OctAxe release or completed Beta6 build/bench is claimed by this guide.
