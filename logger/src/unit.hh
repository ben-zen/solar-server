#include <format>
#include <type_traits>

template <typename T>
concept Number = std::is_arithmetic_v<T>;

template <Number V, typename U>
struct dimensioned_value {
    V value;
    U unit;

    // dimensioned_value() : value(), unit() {};
    // dimensioned_value(dimensioned_value &other) : value(other.value), unit(other.unit) {};
    // dimensioned_value(dimensioned_value &&other) : value(std::move(other.value)), unit(std::move(other.unit)) {};
    // daytime_forecast &operator=(const dimensioned_value &other) {
    //     value = other.value;
    //     unit = other.unit;
    // }
    // daytime_forecast &&operator=(const dimensioned_value &&other) {
    //     value = std::move(other.value);
    //     unit = std::move(other.unit);
    // }
};

template<Number V, typename U>
struct std::formatter<dimensioned_value<V, U>> : std::formatter<std::string_view> {
  template <typename Context>
  auto format(const dimensioned_value<V, U> &value, Context &ctx) const {
    return format_to(ctx.out(), "{} {}", value.value, value.unit);
  }
};

struct volts {};
struct watts {};

template<>
struct std::formatter<volts> : std::formatter<std::string_view> {
    template <typename Context>
    auto format(const volts &v, Context &ctx) const {
        return format_to(ctx.out(), "V");
    }
};

template<>
struct std::formatter<watts> : std::formatter<std::string_view> {
    template <typename Context>
    auto format(const watts &w, Context &ctx) const {
        return format_to(ctx.out(), "W");
    }
};

template <Number V>
using voltage = dimensioned_value<V, volts>;

template <Number V>
using wattage = dimensioned_value<V, watts>;
