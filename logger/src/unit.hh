#include <format>
#include <type_traits>

template <typename T>
concept Number = std::is_arithmetic_v<T>;

template <Number V, typename U>
struct dimensioned_value {
    V value;
    U unit;
};

template<Number V, typename U>
struct std::formatter<dimensioned_value<V, U>> : std::formatter<std::string_view> {
  template <typename Context>
  auto format(const dimensioned_value<V, U> value, Context &ctx) const -> auto {
    return format_to(ctx.out(), "{} {}", value.value, value.unit);
  }
};