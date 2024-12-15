#pragma once

#include <bitset>
#include <cassert>

namespace alloy
{
/**
 * BitFlags implements a bitset usable with `enum` and `enum class`.
 *
 * It provide a typesafe interface for manipulating the bitset. This helps
 * prevents mistake as the various operator and function will refuse a
 * parameter that doesn't match the expected enum type.
 *
 * A BitFlags supports one user-defined enumeration. The number of flags
 * (ie the member of the user enumeration) is not limited, as the underlying
 * bitset (std::bitset) can have an arbitrary large size.
 * 
 * REQUIREMENTS:
 *      * This code source required C++14 to compile.
 *      * The user enumeration shall not explicitely set any value.
 *      * The last enumeration member shall be: "MAX_VALUE"
 *
 */
template<typename T>
struct BitFlags
{
    BitFlags() = default;
    BitFlags(const BitFlags& o) : bitset(o.bitset) {}
    BitFlags(const T& val) {
        set_true(val);
    }

    BitFlags& operator|=(const T& val) {
        set_true(val);
        return *this;
    }

    BitFlags& operator&=(const T& val) {
        bool tmp = bitset.test(static_cast<utype>(val));
        bitset.reset();
        bitset[static_cast<utype>(val)] = tmp;
        return *this;
    }

    BitFlags& operator|=(const BitFlags& o) {
        bitset |= o.bitset;
        return *this;
    }

    BitFlags& operator&=(const BitFlags& o) {
        bitset &= o.bitset;
        return *this;
    }

    BitFlags& operator=(const BitFlags& o) = default;
    BitFlags& operator=(const T& val) {
        bitset.reset();
        bitset[static_cast<utype>(val)] = 1;
        return *this;
    }

  /**
   * Return a bitset containing the result of the
   * bitwise AND between *this and val.
   *
   * The resulting bitset can contain at most 1 bit.
   */
  BitFlags operator&(const T&val)
  {
    BitFlags ret(*this);
    ret &= val;

    assert(ret.bitset.count() <= 1);
    return ret;
  }

  /**
   * Perform a AND binary operation between *this and
   * `val` and return the result as a copy.
   */
  BitFlags operator&(const BitFlags &val)
  {
    BitFlags ret(*this);
    ret.bitset &= val.bitset;

    return ret;
  }

  /**
   * Return a bitset containing the result of the
   * bitwise OR between *this and val.
   *
   * The resulting bitset contains at least 1 bit.
   */
  BitFlags operator|(const T& val)
  {
    BitFlags ret(*this);
    ret |= val;

    assert(ret.bitset.count() >= 1);
    return ret;
  }

  /**
   * Perform a OR binary operation between *this and
   * `val` and return the result as a copy.
   */
  BitFlags operator|(const BitFlags &val)
  {
    BitFlags ret(*this);
    ret.bitset |= val.bitset;

    return ret;
  }

  BitFlags operator~()
  {
    BitFlags cp(*this);
    cp.bitset.flip();

    return cp;
  }

  /**
   * The bitset evaluates to true if any bit is set.
   */
  explicit operator bool() const { return bitset.any(); }

  /**
   * Below are the method from std::bitset that we expose.
   */

  bool operator==(const BitFlags &o) const
  {
    return bitset == o.bitset;
  }

    std::size_t size() const { return bitset.size(); }

    std::size_t count() const { return bitset.count(); }

    BitFlags &set()
    {
      bitset.set();
      return *this;
    }

  BitFlags &reset()
  {
    bitset.reset();
        return *this;
  }

  BitFlags &flip()
  {
    bitset.flip();
    return *this;
  }

  BitFlags &set(const T &val, bool value = true)
  {
    bitset.set(static_cast<utype>(val), value);
        return *this;
  }

  BitFlags &reset(const T&val)
  {
    bitset.reset(static_cast<utype>(val));
    return *this;
  }

  BitFlags &flip(const T &val)
  {
    bitset.flip(static_cast<utype>(val));
    return *this;
  }

  bool operator[] (const T&val) const
  {
    return bitset[static_cast<utype>(val)];
  }

  /**
   * Overload for std::ostream
   */
  friend std::ostream& operator<<(std::ostream& stream, const BitFlags& me)
  {
    return stream << me.bitset;
  }

private:
  using utype = std::underlying_type_t<T>;
  std::bitset<static_cast<utype>(T::MAX_VALUE)> bitset;

  void set_true(const T&val)
  {
    bitset[static_cast<utype>(val)] = 1;
  }

public:
};
/**
 * Provide a free operator allowing to combine two enumeration
 * member into a BitFlags.
 */
template<typename T>
std::enable_if_t<std::is_enum<T>::value, alloy::BitFlags<T>>
operator|(const T &lhs, const T &rhs)
{
  BitFlags<T> bs;
  bs |= lhs;
  bs |= rhs;

  return bs;
}

} // namespace alloy
