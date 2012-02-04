/** -*- C++ -*-
 * Copyright (C) 2010-2012 Thalmann Software & Consulting, http://www.softdev.ch
 *
 * This file is part of ht4w.
 *
 * ht4w is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or any later version.
 *
 * Hypertable is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#ifndef HYPERTABLE_SPIRITPARSER_H
#define HYPERTABLE_SPIRITPARSER_H

#if defined(BOOST_SPIRIT_STATIC_HPP)
#error BOOST_SPIRIT_STATIC_HPP
#endif
#define BOOST_SPIRIT_STATIC_HPP

#include <boost/noncopyable.hpp>
#include <boost/call_traits.hpp>
#include <boost/aligned_storage.hpp>

#include <boost/type_traits/add_pointer.hpp>
#include <boost/type_traits/alignment_of.hpp>

#include <boost/thread/once.hpp>

#include <memory>   // for placement new

#include <boost/spirit/home/classic/namespace.hpp>

namespace boost { namespace spirit {

BOOST_SPIRIT_CLASSIC_NAMESPACE_BEGIN

    //
    //  Provides thread-safe initialization of a single static instance of T.
    //
    //  This instance is guaranteed to be constructed on static storage in a
    //  thread-safe manner, on the first call to the constructor of static_.
    //
    //  Requirements:
    //      T is default constructible
    //          (There's an alternate implementation that relaxes this
    //              requirement -- Joao Abecasis)
    //      T::T() MUST not throw!
    //          this is a requirement of boost::call_once.
    //
    // Remark:
    //      Manual destruction has been added in order destruct befor TLS slot ahs been destroyed
    //
    template <class T, class Tag>
    struct static_
        : boost::noncopyable
    {
    private:

        struct destructor
        {
            ~destructor()
            {
                if (!static_::destructed_) {
                  static_::get_address()->~value_type();
                  static_::destructed_ = true;
                }
            }
        };
        friend struct destructor;

        struct default_ctor
        {
            static void construct()
            {
                ::new (static_::get_address()) value_type();
                static destructor d;
            }
        };

    public:

        typedef T value_type;
        typedef typename boost::call_traits<T>::reference reference;
        typedef typename boost::call_traits<T>::const_reference const_reference;

        static_(Tag = Tag())
        {
            boost::call_once(&default_ctor::construct, constructed_);
        }

        operator reference()
        {
            return this->get();
        }

        operator const_reference() const
        {
            return this->get();
        }

        reference get()
        {
            return *this->get_address();
        }

        const_reference get() const
        {
            return *this->get_address();
        }

        static void free()
        {
            static_::get_address()->~value_type();
            static_::destructed_ = true;
        }

    private:
        typedef typename boost::add_pointer<value_type>::type pointer;

        static pointer get_address()
        {
            return static_cast<pointer>(data_.address());
        }

        typedef boost::aligned_storage<sizeof(value_type),
            boost::alignment_of<value_type>::value> storage_type;

        static storage_type data_;
        static once_flag constructed_;
        static bool destructed_;
    };

    template <class T, class Tag>
    typename static_<T, Tag>::storage_type static_<T, Tag>::data_;

    template <class T, class Tag>
    once_flag static_<T, Tag>::constructed_ = BOOST_ONCE_INIT;

    template <class T, class Tag>
    bool static_<T, Tag>::destructed_ = false;

BOOST_SPIRIT_CLASSIC_NAMESPACE_END

}} // namespace BOOST_SPIRIT_CLASSIC_NS


#include <boost/algorithm/string.hpp>
#include <boost/spirit/include/classic_core.hpp>
#include <boost/spirit/include/classic_symbols.hpp>
#include <boost/spirit/include/classic_confix.hpp>
#include <boost/spirit/include/classic_escape_char.hpp>

#endif // HYPERTABLE_SPIRITPARSER_H
