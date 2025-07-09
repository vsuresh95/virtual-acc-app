#ifndef __NN_TOKEN_H__
#define __NN_TOKEN_H__

struct nn_token_t {
    int32_t value;
    static constexpr int fractional_bits = 16;
    static constexpr int32_t scale = 1 << fractional_bits;

    // Constructors
    constexpr nn_token_t() : value(0) {}
    constexpr nn_token_t(float f) : value(static_cast<int32_t>(f * scale)) {}
    constexpr nn_token_t(int v) : value(v << fractional_bits) {}
    constexpr nn_token_t(int32_t raw, bool) : value(raw) {} // Internal use

    // Conversion to float
    constexpr operator float() const {
        return static_cast<float>(value) / scale;
    }

    // Arithmetic operators
    constexpr nn_token_t operator+(const nn_token_t& other) const {
        return nn_token_t(value + other.value, true);
    }
    constexpr nn_token_t operator-(const nn_token_t& other) const {
        return nn_token_t(value - other.value, true);
    }
    inline nn_token_t operator*(const nn_token_t& other) const {
        int64_t result = static_cast<int64_t>(value) * other.value;
        return nn_token_t(static_cast<int32_t>(result >> fractional_bits), true);
    }
    inline nn_token_t operator/(const nn_token_t& other) const {
        int64_t result = (static_cast<int64_t>(value) << fractional_bits) / other.value;
        return nn_token_t(static_cast<int32_t>(result), true);
    }

    // Compound assignment operators
    inline nn_token_t& operator+=(const nn_token_t& other) {
        value += other.value;
        return *this;
    }
    inline nn_token_t& operator-=(const nn_token_t& other) {
        value -= other.value;
        return *this;
    }
    inline nn_token_t& operator*=(const nn_token_t& other) {
        *this = *this * other;
        return *this;
    }
    inline nn_token_t& operator/=(const nn_token_t& other) {
        *this = *this / other;
        return *this;
    }

    // Comparison operators
    constexpr bool operator==(const nn_token_t& other) const { return value == other.value; }
    constexpr bool operator!=(const nn_token_t& other) const { return value != other.value; }
    constexpr bool operator<(const nn_token_t& other) const { return value < other.value; }
    constexpr bool operator<=(const nn_token_t& other) const { return value <= other.value; }
    constexpr bool operator>(const nn_token_t& other) const { return value > other.value; }
    constexpr bool operator>=(const nn_token_t& other) const { return value >= other.value; }
};

// Stream output for easy printing
inline std::ostream& operator<<(std::ostream& os, const nn_token_t& f) {
    os << static_cast<float>(f);
    return os;
}

#endif // __NN_TOKEN_H__
