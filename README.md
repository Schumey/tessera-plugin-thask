# tHAsk

> Needs the tHAsk integration 0.18.2 or later in Home Assistant, and a screen that may perform Home Assistant actions.

Your [tHAsk](https://github.com/Schumey/tHAsk) tasks on a Tessera screen: a list or a smart view (Today, This week,
Important, ...) with the fuzzy due dates tHAsk knows ("KW 46", "November", "Q1"), ticked off with a tap.

- **Tile "Tasks"**, 1x1 to 4x4: the list's name and how many tasks are open, then a row per task: a ring in the colour of
  its priority (red high, orange medium, green low), the title, and on the right when it is due, red when overdue. A
  1x1 tile shows the count large.
- **Tap the ring** to tick a task off; tap it again while it still shows to open it again. A recurring task gets its
  next date, as in tHAsk.
- **Text size** (tile option): small, medium, large or very large; larger text fits fewer tasks.
- **Tap anywhere else** to open the whole list over the page, each task with its list.
- **Top bar**: how many tasks of one list are open (Today by default), the icon red while one is overdue.

## Set up

1. Install tHAsk 0.18.2 or later in Home Assistant (github.com/Schumey/tHAsk).
2. In Tessera, add this plugin to a screen with its link. Choose the **List for the top bar** (tHAsk Today, for
   example); the screen builds once.
3. In Layout, place the **Tasks** tile from the library's Plugins group and choose a tHAsk list as its entity: a list of
   your own, or a smart list such as tHAsk Today or tHAsk This week.
4. Let the screen perform Home Assistant actions: Settings → Devices & services → ESPHome → the screen → Configure →
   **Allow the device to perform Home Assistant actions**. Without it the tasks show, but a tap ticks nothing off.
5. Optional: in the tile's inspector choose another **View** (Today, Overdue, This week, This month, Important) to show
   only those tasks of the list, or of all lists with a smart list. Place **Open tasks** in a page's top bar.

## Good to know

- The screen asks Home Assistant for the tasks (`thask.get_tasks`) when the list changes, and once a minute while the
  tile is on the glass. Nothing leaves your home.
- Emoji in list names do not show on the screen; the screen's font has none.
- A tile shows up to twelve tasks, the card up to sixteen: the soonest due first.
