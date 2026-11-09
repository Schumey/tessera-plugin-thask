#pragma once
#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/smart_display/plugin_api.h"

namespace esphome::thask {

class TaskView;

// The plugin: a tile ("tasks") and a card ("list") that show the tasks of a tHAsk list or smart view, asked of Home
// Assistant through the app (thask.get_tasks), and an item for the top bar ("count") that counts the open tasks of the
// list chosen when the plugin was added (plugin.yaml's two sensors).
class Thask : public Component, public tessera::Plugin {
 public:
  void setup() override;
  float get_setup_priority() const override { return setup_priority::DATA; }
  void on_message(JsonObjectConst message) override;

  void set_open(sensor::Sensor *s) { open_ = s; }
  void set_overdue(sensor::Sensor *s) { overdue_ = s; }

  // Every view on the glass hears the answers; each takes only the one to its own question.
  void listen(TaskView *view);
  void forget(TaskView *view);

  // What the card shows: the view of the tile it was opened from, handed over as the tile opens it.
  char card_view[16] = "auto";

 protected:
  tessera::BarItem bar() const;
  sensor::Sensor *open_{}, *overdue_{};
  tessera::List<TaskView *> views_;
};

}  // namespace esphome::thask
