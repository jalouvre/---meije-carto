// -*- mode: c++ -*-

/***************************************************************************************************
**
** $QTCARTO_BEGIN_LICENSE:GPL3$
**
** Copyright (C) 2016 Fabrice Salvaire
** Contact: http://www.fabrice-salvaire.fr
**
** This file is part of the QtCarto library.
**
** This program is free software: you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation, either version 3 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program.  If not, see <http://www.gnu.org/licenses/>.
**
** $QTCARTO_END_LICENSE$
**
***************************************************************************************************/

#ifndef __VECTOR_H__
#define __VECTOR_H__

/**************************************************************************************************/

#include <cmath>
#include "math/qc_math.h"

// #include <QtCore/QMetaType>

#include "qtcarto_global.h"

/**************************************************************************************************/

// QC_BEGIN_NAMESPACE

/**************************************************************************************************/

template <typename T>
class QC_EXPORT QcVector
{
 public:
  QcVector()
    : QcVector(0, 0)
    {
    }

  QcVector(T x, T y)
    : m_x(x), m_y(y)
    {}

  QcVector(const QcVector<T> & other)
    : m_x(other.m_x), m_y(other.m_y)
    {}

  QcVector<T> &operator=(const QcVector<T> & other)
    {
      if (this != &other) {
	m_x = other.m_x;
	m_y = other.m_y;
      }

      return *this;
    }

  inline T x() const {
    return m_x;
  }

  inline void set_x(T value) {
    m_x = value;
  }

  inline T y() const {
    return m_y;
  }

  inline void set_y(T value) {
    m_y = value;
  }

  T & operator[](size_t i) {
    if (i == 0)
      return m_x;
    else if (i == 1)
      return m_y;
    else
      throw std::invalid_argument("invalid index");
  }

  T operator[](size_t i) const {
    if (i == 0)
      return m_x;
    else if (i == 1)
      return m_y;
    else
      throw std::invalid_argument("invalid index");
  }

  inline T magnitude_square() const {
    return m_x * m_x + m_y * m_y;
  }

  inline T magnitude() const {
    return sqrt(magnitude_square());
  }

  void normalise() {
    *this /= magnitude();
  }

  QcVector<T> normalised() const {
    return *this / magnitude();
  }

  inline bool is_null() const {
    return m_x == 0 && m_y == 0;
  }

  bool operator==(const QcVector<T> & other) const {
    return (m_x == other.m_x) && (m_y == other.m_y);
  }
  inline bool operator!=(const QcVector<T> & other) const
  {
    return !operator==(other);
  }

  QcVector<T> & operator+=(const QcVector<T> & other) {
    m_x += other.m_x;
    m_y += other.m_y;
    return *this;
  }

  friend QcVector<T> operator+(const QcVector<T> & vector1, const QcVector<T> & vector2)
  {
    return QcVector(vector1.m_x + vector2.m_x, vector1.m_y + vector2.m_y);
  }

  QcVector<T> & operator-=(const QcVector<T> & other) {
    m_x -= other.m_x;
    m_y -= other.m_y;
    return *this;
  }

  friend QcVector<T> operator-(const QcVector<T> & vector1, const QcVector<T> & vector2)
  {
    return QcVector(vector1.m_x - vector2.m_x, vector1.m_y - vector2.m_y);
  }

  QcVector<T> & operator*=(T factor) {
    m_x *= factor;
    m_y *= factor;
    return *this;
  }

  friend QcVector<T> operator*(const QcVector<T> & vector, T factor)
  {
    return QcVector(vector.m_x * factor, vector.m_y * factor);
  }

  QcVector<T> & operator/=(T factor) {
    m_x /= factor;
    m_y /= factor;
    return *this;
  }

  friend QcVector<T> operator/(const QcVector<T> & vector, T factor)
  {
    return QcVector(vector.m_x / factor, vector.m_y / factor);
  }

  // Return the orientation in degree
  T orientation() const {
    /*
     * 2 | 1
     * - + -
     * 4 | 3
     *
     *       | 1    | 2         | 3    | 4         |
     * x     | +    | -         | +    | -         |
     * y     | +    | +         | -    | -         |
     * tan   | +    | -         | -    | +         |
     * atan  | +    | -         | -    | +         |
     * theta | atan | atan + pi | atan | atan - pi |
     */

    if (is_null())
      throw std::invalid_argument("Null Vector");

    if (m_x == 0)
      return copysign(90, m_y);
    else if (m_y == 0)
      return m_x >= 0 ? 0 : 180;
    else {
      orientation = qRadiansToDegrees(atan(tan()));
      if (m_x < 0) {
	if (m_y > 0)
	  orientation += 180;
	else
	  orientation -= 180;
      }
      return orientation;
    }
  }

  // Return a new vector equal to self rotated of angle degree in the counter clockwise direction
  QcVector<T> rotate_counter_clockwise(T angle) const {
    T radians = qDegreesToRadians(angle);
    T c = cos(radians);
    T s = sin(radians);

    T xp = c * m_x - s * m_y;
    T yp = s * m_x + c * m_y;

    return QcVector<T>(xp, yp);
  }

  //  Return a new vector equal to self rotated of 90 degree in the counter clockwise direction
  QcVector<T> rotate_counter_clockwise_90() const {
    return QcVector<T>(-m_y, m_x);
  }

  //  Return a new vector equal to  rotated of 90 degree in the clockwise direction
  QcVector<T> rotate_clockwise_90() const {
    return QcVector<T>(m_y, -m_x);
  }

  // Return a new vector equal to  rotated of 180 degree
  // parity
  QcVector<T> rotate_180() const {
    return QcVector<T> (-m_x, -m_y);
  }

  // Return the tangent
  T tan() const {
    if (m_x != 0)
      return m_y / m_x;
    else
      throw std::invalid_argument("x is null");
  }

  // Return the inverse tangent
  T inverse_tan() const {
    if (m_y != 0)
      return m_x / m_y;
    else
      throw std::invalid_argument("y is null");
  }

  // Return the dot product of  with other
  T dot(const QcVector<T> & other) const {
    return m_x * other.m_x + m_y * other.m_y;
  }

  // Return the cross product of  with other
  T cross(const QcVector<T> & other) const {
    return m_x * other.m_y - m_y * other.m_x;
  }

  // is parallel with other
  bool is_parallel(const QcVector<T> & other) const {
    // Fixme: round
    return round(cross(other), 7) == 0;
  }

  // is orthogonal with other
  bool is_orthogonal(const QcVector<T> & other) const {
    // Fixme: round
    return round(dot(other), 7) == 0;
  }

  // Return the cosinus of  with direction
  T cos_with(const QcVector<T> & direction) const {
    T cos = dot(direction) / (direction.magnitude() * magnitude());
    return trignometric_clamp(cos);
  }

  //  Return the projection of  on direction
  T projection_on(const QcVector<T> & direction) const {
    return dot(direction) / direction.magnitude();
  }

  //  Return the sinus of  with other
  T sin_with(const QcVector<T> & direction) const {
    //turn from direction to
    T sin = direction.cross(*this) / (direction.magnitude() * magnitude());
    return trignometric_clamp(sin);
  }

  //  Return the deviation of  with other
  T deviation_with(const QcVector<T> & direction) const {
    return direction.cross(*this) / direction.magnitude();
  }

  T orientation_with(const QcVector<T> & direction) const {
    // Fixme: check all cases
    // -> angle_with
    //  Return the angle of on direction

    T angle = acos(cos_with(direction));
    T angle_sign = sign(sin_with(direction));

    return angle_sign * qRadiansToDegrees(angle);
  }

 private:
  T m_x;
  T m_y;
};

typedef QcVector<double> QcVectorDouble;

/**************************************************************************************************/

// QT_END_NAMESPACE

/**************************************************************************************************/

#endif /* __VECTOR_H__ */

/***************************************************************************************************
 *
 * End
 *
 **************************************************************************************************/