#pragma once

#include <cstdint>
#include <cmath>
#include <string>

namespace esphome {

struct Color {
  uint8_t red;
  uint8_t green;
  uint8_t blue;
  uint8_t alpha;

  constexpr Color() : red(0), green(0), blue(0), alpha(255) {}
  constexpr Color(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255)
      : red(r), green(g), blue(b), alpha(a) {}

  bool operator==(const Color &rhs) const {
    return red == rhs.red && green == rhs.green && blue == rhs.blue &&
           alpha == rhs.alpha;
  }
  bool operator!=(const Color &rhs) const { return !(*this == rhs); }
};

struct StringRef {
  const char *str_value_{""};
  explicit StringRef(const char *s = "") : str_value_(s) {}

  const char *c_str() const { return this->str_value_; }
};

namespace display {

// Minimal font stub used by ThermostatFonts.
class BaseFont {};

enum class TextAlign {
  TOP_LEFT = 0x00,
  TOP = 0x00,
  CENTER_VERTICAL = 0x01,
  BASELINE = 0x02,
  BOTTOM = 0x04,
  LEFT = 0x00,
  CENTER_HORIZONTAL = 0x08,
  RIGHT = 0x10,
  TOP_CENTER = TOP | CENTER_HORIZONTAL,
  CENTER_LEFT = CENTER_VERTICAL | LEFT,
  CENTER = CENTER_VERTICAL | CENTER_HORIZONTAL,
  BOTTOM_CENTER = BOTTOM | CENTER_HORIZONTAL,
};

class Display {
 public:
  virtual ~Display() = default;

  void fill(const Color &color) {
    this->fill_color_ = color;
    this->fill_calls_++;
  }
  void print(int, int, const void *, const Color &, int,
             const char *, const Color &) {
    this->print_calls_++;
  }
  void print(int, int, const void *, const Color &, TextAlign,
             const char *, const Color &) {
    this->print_calls_++;
  }
  void filled_circle(int, int, int, const Color &) { this->filled_circle_calls_++; }
  void draw_pixel_at(int, int, const Color &) { this->draw_pixel_calls_++; }

  void reset_stats() {
    this->fill_calls_ = 0;
    this->print_calls_ = 0;
    this->filled_circle_calls_ = 0;
    this->draw_pixel_calls_ = 0;
  }

  int fill_calls() const { return this->fill_calls_; }
  int print_calls() const { return this->print_calls_; }
  int filled_circle_calls() const { return this->filled_circle_calls_; }
  int draw_pixel_calls() const { return this->draw_pixel_calls_; }

 private:
  Color fill_color_{};
  int fill_calls_{0};
  int print_calls_{0};
  int filled_circle_calls_{0};
  int draw_pixel_calls_{0};
};

using DisplayWriter = void (*)(Display &);

}  // namespace display

namespace font {
class Font {};
}

namespace component {
class Component {};
}

namespace output {
class FloatOutput {
 public:
  void set_level(float) {}
};
}

namespace rtttl {
class Rtttl {
 public:
  void play(const std::string &) {}
};
}

namespace select {
class Select {
 public:
  virtual ~Select() = default;
  void publish_state(const std::string &) {}
};
}

}  // namespace esphome
