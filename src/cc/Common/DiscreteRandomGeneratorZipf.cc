/*
 * Copyright (C) 2007-2016 Hypertable, Inc.
 *
 * This file is part of Hypertable.
 *
 * Hypertable is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 3
 * of the License, or any later version.
 *
 * Hypertable is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

/** @file
 * Discrete Random Generator creating a zipfian distribution.
 * See http://en.wikipedia.org/wiki/Zipf%27s_law
 */

#include "Common/Compat.h"
#include "DiscreteRandomGeneratorZipf.h"

using namespace Hypertable;

DiscreteRandomGeneratorZipf::DiscreteRandomGeneratorZipf(double s)
  : DiscreteRandomGenerator(), m_initialized(false), m_s(s)
{
  assert(m_s > 0 && m_s < 1);
}

double DiscreteRandomGeneratorZipf::pmf(uint64_t val)
{
  if (!m_initialized) {
    m_norm = (1 - m_s) / (pow(m_value_count + 1, 1 - m_s));
    m_initialized = true;
  }
  assert(val >= 0 && val <= m_value_count + 1);
  val++;
  double prob = m_norm / pow(val, m_s);
  return (prob);
}
