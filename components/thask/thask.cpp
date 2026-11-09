#include "thask.h"
#include <algorithm>
#include <climits>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

namespace esphome::thask {

using tessera::Font;
using tessera::Tone;
namespace ui = tessera::ui;

static constexpr uint32_t ICON_TASKS = 0xF05E1;   // check-circle-outline
static constexpr int TILE_ROWS = 12;              // the most rows a tile shows (the manifest's memory counts them)
static constexpr int CARD_ROWS = 16;              // the most a card shows: what about 3 KB of answer holds
static constexpr uint32_t REFRESH_S = 60;         // ask again this often while on the glass (a title changed, a new day)
static constexpr uint32_t AFTER_TAP_S = 4;        // and this soon after a tap, so a ticked task leaves the list

// ---- Words ----

// Copies `from` into `to` (of `size` bytes), keeping only the characters the screen's font has: a list called
// "🏡 Zuhause" shows as "Zuhause" instead of a gap. A character cut in half (the app cuts texts at 48 bytes) goes too.
static void covered(const char *from, char *to, size_t size, Font font) {
  size_t out = 0;
  const auto *s = reinterpret_cast<const unsigned char *>(from ? from : "");
  while (*s && out + 1 < size) {
    const int len = *s < 0x80 ? 1 : (*s >> 5) == 0x6 ? 2 : (*s >> 4) == 0xE ? 3 : (*s >> 3) == 0x1E ? 4 : 1;
    int have = 1;
    while (have < len && (s[have] & 0xC0) == 0x80) ++have;
    if (have < len) { s += have; continue; }
    const std::string one(reinterpret_cast<const char *>(s), len);
    const bool space = len == 1 && *s == ' ';
    if (space ? out > 0 && to[out - 1] != ' ' : ui::covers(one, font)) {
      if (out + len >= size) break;
      memcpy(to + out, s, len);
      out += len;
    }
    s += len;
  }
  while (out > 0 && to[out - 1] == ' ') --out;
  to[out] = '\0';
}

static void copy(const char *from, char *to, size_t size) {
  snprintf(to, size, "%s", from ? from : "");
}

// The name of a list as the tile's header shows it: Home Assistant calls tHAsk's lists "tHAsk Today"; the tile
// knows it is tHAsk.
static std::string list_name(const char *name) {
  std::string out = name ? name : "";
  if (out.rfind("tHAsk ", 0) == 0) out.erase(0, 6);
  return out;
}

// ---- The rows: what a tile and a card share ----

struct Row {
  char id[36];
  char title[52];
  char list[52];
  char due[40];
  int8_t priority;
  int16_t days;    // days from today to the last day it is due (negative: overdue); INT16_MIN for none
  bool done;       // ticked off here, until Home Assistant's answer takes it away
};

// The tasks of one list or view: its question to Home Assistant, its answer and the parts that show it, in a room of
// a tile or a card. Made in PSRAM like the tile and the card that own it.
class TaskView : public tessera::InPsram {
 public:
  explicit TaskView(Thask *plugin) : plugin_(plugin) { plugin_->listen(this); }
  ~TaskView() { plugin_->forget(this); }

  // The parts, once: up to `most` rows from the top of the room, each a ring, a title, the due date on the right and
  // with `detail` the list under the title. With `fill` the rows share the room's height (a tile), else they stand
  // from the top (a card).
  void create(lv_obj_t *parent, int x, int y, int width, int height, int most, bool detail, bool due, bool fill) {
    x_ = x;
    y_ = y;
    width_ = width;
    show_due_ = due;
    const int line = ui::line_height(Font::BODY_LARGE), second = detail ? ui::line_height(Font::BODY) : 0;
    row_h_ = std::max(line + second + ui::px(10), ui::mm(7));
    ring_ = std::max(ui::px(14), line - ui::px(4));
    text_x_ = ring_ + ui::px(ui::large() ? 12 : 8);
    fits_ = std::max(0, std::min(most, height / row_h_));
    if (fill && fits_ > 0) row_h_ = height / fits_;
    for (int i = 0; i < fits_; ++i) {
      Parts &p = parts_[i];
      const int top = y + i * row_h_ + (row_h_ - line - second) / 2;
      p.ring = ui::block(parent, theme::CARD);
      lv_obj_set_size(p.ring, ring_, ring_);
      lv_obj_set_style_radius(p.ring, LV_RADIUS_CIRCLE, 0);
      lv_obj_set_style_border_width(p.ring, std::max(2, ui::px(2)), 0);
      lv_obj_set_pos(p.ring, x, top + (line - ring_) / 2);
      p.title = ui::label(parent, Font::BODY_LARGE, theme::INK);
      lv_obj_set_pos(p.title, x + text_x_, top);
      p.due = ui::label(parent, Font::BODY, theme::MUTED);
      lv_obj_set_pos(p.due, x + width, top + (line - ui::line_height(Font::BODY)) / 2);
      if (detail) {
        p.list = ui::label(parent, Font::BODY, theme::MUTED);
        lv_obj_set_pos(p.list, x + text_x_, top + line);
        lv_obj_set_width(p.list, std::max(1, width - text_x_));
      }
      hide(i, true);
    }
    note_ = ui::label(parent, Font::BODY_LARGE, theme::MUTED);
    lv_obj_set_width(note_, width);
    lv_obj_set_style_text_align(note_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(note_, x, y + std::max(0, (height - line) / 2));
    say("waiting");
  }

  // The list or view to show: a todo entity of tHAsk, and the view of the option (auto: the entity's own).
  void follow(const char *entity, const char *view) {
    entity_ = entity ? entity : "";
    copy(view, view_, sizeof(view_));
  }

  bool has_entity() const { return !entity_.empty(); }
  int fits() const { return fits_; }
  int total() const { return total_; }
  bool answered() const { return answered_; }
  // How many of the shown rows are overdue, for the count's colour.
  int overdue() const {
    int n = 0;
    for (int i = 0; i < count_; ++i) n += rows_[i].days != INT16_MIN && rows_[i].days < 0 && !rows_[i].done;
    return n;
  }

  // Ask Home Assistant for the tasks, as many as fit (at least one, for the count).
  void ask() {
    if (entity_.empty()) {
      say("choose_list");
      return;
    }
    JsonDocument request;
    request["ask"] = "call_service:thask.get_tasks";
    request["data"]["entity_id"] = roomy::plain(entity_);
    if (strcmp(view_, "auto") != 0) request["data"]["view"] = view_;
    request["data"]["limit"] = std::max(1, fits_);
    request["data"]["compact"] = true;
    asked_ = tessera::send(plugin_, request.as<JsonObjectConst>());
    const uint32_t now = tessera::epoch();
    next_ = now ? now + REFRESH_S : 0;
    if (!asked_ && !answered_) say("failed");
  }

  // The app's answer: {"re", "ok", "result": {"tasks": [{id, title, priority, list, due_label, due_kind, due_days,
  // due_time}], "total"}}. False when it answers another question.
  bool answer(JsonObjectConst message) {
    if (!asked_ || (message["re"] | 0u) != asked_) return false;
    asked_ = 0;
    if (!(message["ok"] | false)) {
      if (!answered_) say("failed");
      return true;
    }
    JsonObjectConst result = message["result"];
    count_ = 0;
    for (JsonObjectConst task : result["tasks"].as<JsonArrayConst>()) {
      if (count_ >= CARD_ROWS) break;
      Row &r = rows_[count_++];
      copy(task["id"] | "", r.id, sizeof(r.id));
      covered(task["title"] | "", r.title, sizeof(r.title), Font::BODY_LARGE);
      covered(task["list"] | "", r.list, sizeof(r.list), Font::BODY);
      r.priority = static_cast<int8_t>(task["priority"] | 0);
      r.days = task["due_days"].is<int>() ? static_cast<int16_t>(std::clamp(task["due_days"].as<int>(), -32000, 32000))
                                         : INT16_MIN;
      r.done = task["completed"] | false;
      due_words(task, r);
    }
    total_ = result["total"] | count_;
    answered_ = true;
    paint();
    return true;
  }

  // Once a second: ask again when it is time.
  void tick(uint32_t epoch) {
    if (!epoch || asked_ || entity_.empty()) return;
    if (!next_) next_ = epoch + REFRESH_S;
    if (epoch >= next_) ask();
  }

  // A tap at x, y of the room: on a task's ring (or left of its title) it ticks the task off, or opens it again.
  // False when the tap was elsewhere, for the tile or card to do what a tap there does.
  bool tap(int x, int y) {
    if (x < 0 || y < y_ || x > x_ + std::max(text_x_ + ui::px(6), ui::mm(9))) return false;
    const int i = (y - y_) / row_h_;
    if (i < 0 || i >= std::min(count_, fits_)) return false;
    Row &r = rows_[i];
    JsonDocument fields;
    fields["task_id"] = r.id;
    fields["completed"] = !r.done;
    if (!tessera::action("thask.complete_task", "", fields.as<JsonVariantConst>())) return true;
    r.done = !r.done;
    paint_row(i);
    const uint32_t now = tessera::epoch();
    if (now) next_ = now + AFTER_TAP_S;
    return true;
  }

  void theme() {
    ui::set_color(note_, theme::MUTED);
    for (int i = 0; i < fits_; ++i) {
      ui::set_color(parts_[i].due, theme::MUTED);
      if (parts_[i].list) ui::set_color(parts_[i].list, theme::MUTED);
      if (i < count_) paint_row(i);
    }
  }

 private:
  struct Parts {
    lv_obj_t *ring{}, *title{}, *due{}, *list{};
  };

  // When it is due, in the screen's words for the days next to today, else in tHAsk's ("KW 46", "Mi 12. Nov").
  void due_words(JsonObjectConst task, Row &r) {
    r.due[0] = '\0';
    if (!show_due_ || r.days == INT16_MIN) return;
    const bool day = strcmp(task["due_kind"] | "", "day") == 0;
    std::string words;
    if (day && r.days == 0) words = plugin_->text("today");
    else if (day && r.days == 1) words = plugin_->text("tomorrow");
    else if (day && r.days == -1) words = plugin_->text("yesterday");
    else words = task["due_label"] | "";
    const char *time = task["due_time"] | "";
    if (*time) words += std::string(" ") + time;
    covered(words.c_str(), r.due, sizeof(r.due), Font::BODY);
  }

  void say(const char *key) {
    ui::set_text(note_, key ? plugin_->text(key) : "");
    if (key) lv_obj_remove_flag(note_, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(note_, LV_OBJ_FLAG_HIDDEN);
  }

  void hide(int i, bool hidden) {
    Parts &p = parts_[i];
    for (lv_obj_t *o : {p.ring, p.title, p.due, p.list}) {
      if (!o) continue;
      if (hidden) lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
      else lv_obj_remove_flag(o, LV_OBJ_FLAG_HIDDEN);
    }
  }

  void paint() {
    const int shown = std::min(count_, fits_);
    for (int i = 0; i < fits_; ++i) {
      hide(i, i >= shown);
      if (i < shown) paint_row(i);
    }
    say(count_ == 0 ? "all_done" : nullptr);
  }

  // A row: the ring in the colour of the priority (filled once ticked off), the title, and the due date on the
  // right, red when overdue.
  void paint_row(int i) {
    const Row &r = rows_[i];
    Parts &p = parts_[i];
    const Tone tone = r.priority >= 3 ? Tone::ALERT : r.priority == 2 ? Tone::BUSY : r.priority == 1 ? Tone::GOOD
                                                                                                   : Tone::NORMAL;
    const lv_color_t edge = ui::tone_color(tone, theme::MUTED);
    lv_obj_set_style_border_color(p.ring, edge, 0);
    lv_obj_set_style_bg_color(p.ring, r.done ? ui::tone_color(Tone::ACCENT, theme::ACCENT) : ui::color(theme::CARD), 0);
    lv_obj_set_style_bg_opa(p.ring, r.done ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    // On a narrow tile the title goes first: the due date only when the title keeps half the row.
    const int room = width_ - text_x_;
    int due_w = r.due[0] ? ui::text_width(r.due, Font::BODY) + ui::px(2) : 0;
    if (due_w && room - due_w - ui::px(8) < room / 2) due_w = 0;
    lv_obj_set_width(p.due, std::max(1, due_w));
    lv_obj_set_x(p.due, x_ + width_ - due_w);
    ui::set_text(p.due, due_w ? r.due : "");
    ui::set_tone(p.due, r.days != INT16_MIN && r.days < 0 && !r.done ? Tone::ALERT : Tone::NORMAL, theme::MUTED);
    lv_obj_set_width(p.title, std::max(1, width_ - text_x_ - due_w - (due_w ? ui::px(8) : 0)));
    ui::set_text(p.title, r.title);
    ui::set_color(p.title, r.done ? theme::MUTED : theme::INK);
    lv_obj_set_style_text_decor(p.title, r.done ? LV_TEXT_DECOR_STRIKETHROUGH : LV_TEXT_DECOR_NONE, 0);
    if (p.list) ui::set_text(p.list, r.list);
  }

  Thask *plugin_;
  tessera::Text entity_;
  char view_[16] = "auto";
  Row rows_[CARD_ROWS]{};
  Parts parts_[CARD_ROWS]{};
  lv_obj_t *note_{};
  int count_ = 0, total_ = 0, fits_ = 0;
  int x_ = 0, y_ = 0, width_ = 0, row_h_ = 1, ring_ = 0, text_x_ = 0;
  bool show_due_ = true, answered_ = false;
  uint32_t asked_ = 0, next_ = 0;
};

void Thask::listen(TaskView *view) { views_.push_back(view); }

void Thask::forget(TaskView *view) {
  views_.erase(std::remove(views_.begin(), views_.end(), view), views_.end());
}

void Thask::on_message(JsonObjectConst message) {
  for (TaskView *view : views_)
    if (view->answer(message)) return;
}

// ---- The tile: the name and the count, the tasks under it ----

class TasksTile : public tessera::Tile {
 public:
  explicit TasksTile(Thask *plugin) : plugin_(plugin), view_(new TaskView(plugin)) {}
  ~TasksTile() override { delete view_; }

  void create(const tessera::TileContext &c) override {
    entity_ = c.entity ? c.entity : "";
    tile_ = c.tile;
    name_ = roomy::text(list_name(c.name));
    copy(c.options["view"] | "auto", view_name_, sizeof(view_name_));
    const bool show_name = c.options["show_name"] | true;
    width_ = c.width;
    height_ = c.height;
    view_->follow(c.entity, view_name_);

    const int head = show_name ? ui::line_height(Font::TITLE) + ui::px(6) : 0;
    count_ = ui::label(c.parent, Font::TITLE, theme::MUTED);
    if (show_name) {
      name_label_ = ui::label(c.parent, Font::TITLE, theme::INK);
      lv_obj_set_pos(name_label_, 0, 0);
    }
    view_->create(c.parent, 0, head, width_, height_ - head, TILE_ROWS, c.options["show_list"] | false,
                  c.options["show_due"] | true, true);
    // A tile too small for a row shows the count large, with the name under it.
    small_ = view_->fits() == 0;
    if (small_) {
      ui::set_font(count_, Font::VALUE);
      lv_obj_set_width(count_, width_);
      lv_obj_set_style_text_align(count_, LV_TEXT_ALIGN_CENTER, 0);
      if (name_label_) {
        ui::set_font(name_label_, Font::BODY);
        ui::set_color(name_label_, theme::MUTED);
        lv_obj_set_width(name_label_, width_);
        lv_obj_set_style_text_align(name_label_, LV_TEXT_ALIGN_CENTER, 0);
      }
      const int nh = ui::line_height(Font::VALUE), bh = name_label_ ? ui::line_height(Font::BODY) : 0;
      const int top = std::max(0, (height_ - nh - bh) / 2);
      lv_obj_set_pos(count_, 0, top);
      if (name_label_) lv_obj_set_pos(name_label_, 0, top + nh);
    } else if (!show_name) {
      lv_obj_add_flag(count_, LV_OBJ_FLAG_HIDDEN);
    }
  }

  void on_state(JsonObjectConst data) override {
    if (name_.empty()) {
      name_ = roomy::text(list_name(data["name"] | ""));
      laid_ = false;
    }
    const char *state = data["state"] | "";
    open_ = atoi(state);
    known_ = *state && strcmp(state, "unavailable") != 0 && strcmp(state, "unknown") != 0;
    overdue_ = data["attributes"]["overdue"] | 0;
    // The list changed in Home Assistant (a task added, ticked off elsewhere): ask again now.
    if (!asked_once_ || !roomy::same(state_, std::string(state))) {
      state_ = state;
      asked_once_ = true;
      view_->ask();
    }
  }

  void on_tick(uint32_t epoch) override {
    view_->tick(epoch);
    paint_head();
  }

  void on_theme() override {
    if (name_label_) ui::set_color(name_label_, small_ ? theme::MUTED : theme::INK);
    view_->theme();
    paint_head();
  }

  void on_tap() override {
    const tessera::Point p = tessera::tap_point();
    if (!small_ && view_->tap(p.x, p.y)) return;
    copy(view_name_, plugin_->card_view, sizeof(plugin_->card_view));
    tessera::open_card(plugin_->plugin_id(), "list", roomy::plain(entity_), tile_, roomy::plain(name_).c_str());
  }

 private:
  // The name, and the count on its right: the tasks of the view, red while one of them is overdue.
  void paint_head() {
    const int count = view_->answered() ? view_->total() : open_;
    const bool late = view_->answered() ? view_->overdue() > 0 : overdue_ > 0;
    const std::string text = view_->answered() || known_ ? std::to_string(count) : "";
    ui::set_tone(count_, late ? Tone::ALERT : Tone::NORMAL, small_ ? theme::INK : theme::MUTED);
    // Lay the head out again only when the count changed: set_text and set_tone change nothing that is the same,
    // a size or a place would draw it again every second.
    if (laid_ && roomy::same(shown_, text)) return;
    laid_ = true;
    shown_ = roomy::text(text);
    if (small_) {
      ui::set_text(count_, view_->has_entity() ? text : "");
      if (name_label_) ui::set_text(name_label_, roomy::plain(name_));
    } else {
      const int cw = text.empty() ? 0 : ui::text_width(text, Font::TITLE) + ui::px(2);
      lv_obj_set_size(count_, std::max(1, cw), ui::line_height(Font::TITLE));
      lv_obj_set_pos(count_, width_ - cw, 0);
      ui::set_text(count_, text);
      if (name_label_) {
        lv_obj_set_width(name_label_, std::max(1, width_ - cw - (cw ? ui::px(8) : 0)));
        ui::set_text(name_label_, roomy::plain(name_));
      }
    }
  }

  Thask *plugin_;
  TaskView *view_;
  tessera::Text entity_, name_, state_, shown_;
  char view_name_[16] = "auto";
  lv_obj_t *count_{}, *name_label_{};
  int width_ = 0, height_ = 0, tile_ = -1, open_ = 0, overdue_ = 0;
  bool small_ = false, known_ = false, asked_once_ = false, laid_ = false;
};

// ---- The card: the whole list over the page, each task with its list ----

class ListCard : public tessera::Card {
 public:
  explicit ListCard(Thask *plugin) : plugin_(plugin), view_(new TaskView(plugin)) {}
  ~ListCard() override { delete view_; }

  void open(const tessera::CardContext &c) override {
    view_->follow(c.entity, plugin_->card_view);
    view_->create(c.parent, 0, 0, c.width, c.height, CARD_ROWS, true, true, false);
    view_->ask();
  }

  // The tile it came from changed: its list changed in Home Assistant.
  void on_state(JsonObjectConst) override { view_->ask(); }
  void on_tick(uint32_t epoch) override { view_->tick(epoch); }
  void on_theme() override { view_->theme(); }
  void on_tap() override {
    const tessera::Point p = tessera::tap_point();
    view_->tap(p.x, p.y);
  }

 private:
  Thask *plugin_;
  TaskView *view_;
};

// ---- The plugin ----

void Thask::setup() {
  add_tile("tasks", [this]() { return new TasksTile(this); });
  add_card("list", [this]() { return new ListCard(this); });
  add_bar_item("count", [this]() { return bar(); });
}

// The open tasks of the top bar's list, the icon red while one of them is overdue.
tessera::BarItem Thask::bar() const {
  tessera::BarItem item;
  if (!open_ || std::isnan(open_->state) || open_->state < 1) return item;
  item.shown = true;
  item.icon = ICON_TASKS;
  item.text = std::to_string(static_cast<int>(open_->state));
  const bool late = overdue_ && !std::isnan(overdue_->state) && overdue_->state >= 1;
  item.tone = late ? Tone::ALERT : Tone::NORMAL;
  return item;
}

}  // namespace esphome::thask
