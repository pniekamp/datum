//
// r-tree
//
// Copyright (C) Peter Niekamp
//
// This code remains the property of the copyright holder.
// The code contained herein may be used for any purpose provided
// this copyright notice is retained
//

#ifndef RTREE_HH
#define RTREE_HH

#include <leap/util.h>
#include <leap/lml/bound.h>
#include <functional>
#include <vector>
#include <stack>
#include <stdexcept>
#include <limits>
#include <algorithm>
#include <iterator>
#include <utility>
#include <iostream>
/**
 * \namespace leap::lml
 * \brief Leap Math Library containing mathmatical routines
 *
**/

/**
 * \defgroup leapdata RTree Container
 * \brief RTree Containor
 *
**/

namespace leap { namespace lml
{
  namespace RTree
  {
    // Traits

    //|///////////////////// bound //////////////////////////////////////////
    template<typename Item>
    decltype(auto) bound(Item const &item)
    {
      return item.box();
    }

    //|///////////////////// box ////////////////////////////////////////////
    template<typename Item>
    struct box
    {
      decltype(auto) operator()(Item const &item) const
      {
        return bound(dereference(item));
      }
    };


    //|-------------------- RTree ---------------------------------------------
    //|------------------------------------------------------------------------
    /**
     * \ingroup leapdata
     *
     * \brief RTree container
     *
    **/

    template<typename Item, size_t dimension, class box = box<Item>, typename Alloc = std::allocator<Item>>
    class basic_rtree
    {
        template<typename Iterator>
        class normal_iterator
        {
          public:

            typedef Item item_type;
            typedef std::decay_t<decltype(box()(std::declval<Item&>()))> bound_type;

            typedef item_type value_type;
            typedef item_type *pointer;
            typedef item_type &reference;
            typedef std::ptrdiff_t difference_type;
            typedef std::forward_iterator_tag iterator_category;

          public:
            normal_iterator();
            explicit normal_iterator(Iterator const &start);

            template<typename Iter>
            normal_iterator(normal_iterator<Iter> const &that);

            bool operator ==(normal_iterator const &that) const { return m_node == that.m_node; }
            bool operator !=(normal_iterator const &that) const { return m_node != that.m_node; }

            bound_type const &bound() const { return m_node->bound; }

            auto &items() const { return m_node->items; }

            size_t children() const { return m_node->nodes.size(); }
            normal_iterator child(size_t i) const { return normal_iterator(&m_node->nodes[i]); }

            normal_iterator &operator++();

            void descend();

          private:

            Iterator m_node;

            bool m_descend;

            template<typename T> friend class normal_iterator;
        };

        class Node;

      public:

        typedef Item item_type;
        typedef Item value_type;
        typedef Alloc allocator_type;
        typedef typename std::allocator_traits<Alloc>::template rebind_alloc<Item> item_allocator_type;
        typedef typename std::allocator_traits<Alloc>::template rebind_alloc<Node> node_allocator_type;
        typedef std::decay_t<decltype(box()(std::declval<Item&>()))> bound_type;

        typedef normal_iterator<Node *> iterator;
        typedef normal_iterator<Node const *> const_iterator;

      public:
        basic_rtree(Alloc const &allocator = Alloc()) noexcept;

        template<typename InputIterator>
        explicit basic_rtree(InputIterator first, InputIterator last, Alloc const &allocator = Alloc());

        void clear();

        template<typename Q>
        void insert(Q &&item);

        void remove(Item const &item);

      public:

        iterator begin() { return iterator(&m_root); }
        const_iterator begin() const  { return const_iterator(&m_root); }

        iterator end() { return iterator(); }
        const_iterator end() const  { return const_iterator(); }

      private:

        class Node
        {
          public:
            Node(Node *parent, allocator_type const &allocator)
              : m_parent(parent), bound(bound_limits<bound_type>::min()), items(allocator), nodes(allocator)
            {
            }

            Node(Node const &that)
              : m_parent(that.m_parent), bound(that.bound), items(that.items), nodes(that.nodes)
            {
              for(auto &node : nodes)
                node.m_parent = this;
            }

            template<typename Q>
            void insert(Q &&item, bound_type const &itembox);

            void remove(Item const &item, bound_type const &searchbox);

          public:

            Node *m_parent;

            bound_type bound;

            std::vector<Item, item_allocator_type> items;
            std::vector<Node, node_allocator_type> nodes;
        };

        class less_area_overlap;
        class less_area_expansion;

      private:

        Node m_root;
    };


    //|///////////////////// RTree::normal_iterator /////////////////////////
    template<typename Item, size_t dimension, class box, typename Alloc>
    template<typename Iterator>
    basic_rtree<Item, dimension, box, Alloc>::normal_iterator<Iterator>::normal_iterator()
    {
      m_node = nullptr;
      m_descend = false;
    }


    //|///////////////////// RTree::normal_iterator /////////////////////////
    template<typename Item, size_t dimension, class box, typename Alloc>
    template<typename Iterator>
    basic_rtree<Item, dimension, box, Alloc>::normal_iterator<Iterator>::normal_iterator(Iterator const &start)
    {
      m_node = start;
      m_descend = false;
    }


    //|///////////////////// RTree::normal_iterator /////////////////////////
    template<typename Item, size_t dimension, class box, typename Alloc>
    template<typename Iterator>
    template<typename Iter>
    basic_rtree<Item, dimension, box, Alloc>::normal_iterator<Iterator>::normal_iterator(normal_iterator<Iter> const &that)
    {
      m_node = that.m_node;
      m_descend = that.m_descend;
    }


    //|///////////////////// RTree::normal_iterator /////////////////////////
    template<typename Item, size_t dimension, class box, typename Alloc>
    template<typename Iterator>
    typename basic_rtree<Item, dimension, box, Alloc>::normal_iterator<Iterator> &basic_rtree<Item, dimension, box, Alloc>::normal_iterator<Iterator>::operator++()
    {
      if (m_descend)
      {
        m_node = &m_node->nodes[0];

        m_descend = false;

        return *this;
      }

      while (m_node != nullptr)
      {
        if (m_node->m_parent != nullptr && m_node != &m_node->m_parent->nodes.back())
        {
          ++m_node;

          m_descend = false;

          return *this;
        }

        m_node = m_node->m_parent;
      }

      return *this;
    }


    //|///////////////////// RTree::normal_iterator /////////////////////////
    template<typename Item, size_t dimension, class box, typename Alloc>
    template<typename NodeType>
    void basic_rtree<Item, dimension, box, Alloc>::normal_iterator<NodeType>::descend()
    {
      if (m_node->nodes.size() != 0)
        m_descend = true;
    }


    //|///////////////////// RTree::Constructor /////////////////////////////
    template<typename Item, size_t dimension, class box, typename Alloc>
    basic_rtree<Item, dimension, box, Alloc>::basic_rtree(Alloc const &allocator) noexcept
      : m_root(nullptr, allocator)
    {
    }


    //|///////////////////// RTree::Constructor /////////////////////////////
    template<typename Item, size_t dimension, class box, typename Alloc>
    template<typename InputIterator>
    basic_rtree<Item, dimension, box, Alloc>::basic_rtree(InputIterator first, InputIterator last, Alloc const &allocator)
      : m_root(nullptr, allocator)
    {
      for(InputIterator i = first; i != last; ++i)
        insert(*i);
    }


    //|///////////////////// RTree::clear ///////////////////////////////////
    template<typename Item, size_t dimension, class box, typename Alloc>
    void basic_rtree<Item, dimension, box, Alloc>::clear()
    {
      m_root.items.clear();
      m_root.nodes.clear();;
      m_root.bound = bound_limits<bound_type>::min();
    }


    //|///////////////////// RTree::insert //////////////////////////////////
    template<typename Item, size_t dimension, class box, typename Alloc>
    template<typename Q>
    void basic_rtree<Item, dimension, box, Alloc>::insert(Q &&item)
    {
      m_root.insert(std::forward<Q>(item), box()(std::forward<Q>(item)));
    }


    //|///////////////////// RTree::remove //////////////////////////////////
    template<typename Item, size_t dimension, class box, typename Alloc>
    void basic_rtree<Item, dimension, box, Alloc>::remove(Item const &item)
    {
      m_root.remove(item, box()(item));
    }


    //|///////////////////// less_area_overlap //////////////////////////////
    template<typename Item, size_t dimension, class box, typename Alloc>
    class basic_rtree<Item, dimension, box, Alloc>::less_area_overlap
    {
      public:

        less_area_overlap(bound_type const &bound, std::vector<Node, node_allocator_type> const &nodes)
          : m_bound(bound), m_nodes(nodes)
        {
        }

        bool operator()(Node const &a, Node const &b) const
        {
          auto ab = expand(a.bound, m_bound);

          double aa = 0;
          for(auto &node : m_nodes)
          {
            if (auto ai = intersection(ab, node.bound))
              aa += volume(*ai);
          }

          auto bb = expand(b.bound, m_bound);

          double ba = 0;
          for(auto &node : m_nodes)
          {
            if (auto bi = intersection(bb, node.bound))
              ba += volume(*bi);
          }

          return (aa == ba) ? (a.items.size() < b.items.size()) : (aa < ba);
        }

        bound_type m_bound;
        std::vector<Node, node_allocator_type> const &m_nodes;
    };


    //|///////////////////// less_area_expansion ////////////////////////////
    template<typename Item, size_t dimension, class box, typename Alloc>
    class basic_rtree<Item, dimension, box, Alloc>::less_area_expansion
    {
      public:

        less_area_expansion(bound_type const &bound)
          : m_bound(bound)
        {
        }

        bool operator()(Node const &a, Node const &b) const
        {
          double aa = volume(expand(a.bound, m_bound)) - volume(a.bound);
          double ba = volume(expand(b.bound, m_bound)) - volume(b.bound);

          return (aa == ba) ? (a.items.size() < b.items.size()) : (aa < ba);
        }

        bound_type m_bound;
    };


    //|///////////////////// RTree::Node::insert ////////////////////////////
    template<typename Item, size_t dimension, class box, typename Alloc>
    template<typename Q>
    void basic_rtree<Item, dimension, box, Alloc>::Node::insert(Q &&item, bound_type const &itembox)
    {
      bound = expand(bound, itembox);

      if (nodes.empty())
      {
        items.push_back(std::forward<Q>(item));

        if (items.size() >= 16)
        {
          nodes.resize(4, Node(this, nodes.get_allocator()));

          auto olditems = std::move(items);

          for(auto &olditem : olditems)
            insert(std::move(olditem), box()(olditem));
       }
      }
      else
      {
        if (nodes.front().nodes.empty())
          min_element(nodes.begin(), nodes.end(), less_area_overlap(itembox, nodes))->insert(std::forward<Q>(item), itembox);
        else
          min_element(nodes.begin(), nodes.end(), less_area_expansion(itembox))->insert(std::forward<Q>(item), itembox);
      }
    }


    //|///////////////////// RTree::Node::remove ////////////////////////////
    template<typename Item, size_t dimension, class box, typename Alloc>
    void basic_rtree<Item, dimension, box, Alloc>::Node::remove(Item const &item, bound_type const &searchbox)
    {
      if (!intersects(bound, searchbox))
        return;

      if (items.size() != 0)
      {
        auto j = std::find(items.begin(), items.end(), item);

        if (j != items.end())
        {
          items.erase(j);

          bound = bound_limits<bound_type>::min();         

          for(auto &item : items)
            bound = expand(bound, box()(item));
        }
      }

      if (nodes.size() != 0)
      {
        for(auto &node : nodes)
          node.remove(item, searchbox);

        bound = bound_limits<bound_type>::min();

        for(auto &node : nodes)
          bound = expand(bound, node.bound);

        if (bound == bound_limits<bound_type>::min())
          nodes.clear();
      }
    }


    /**
     * \name RTree Operations
     * \ingroup leapdata
     * General operations on RTrees
     * @{
    **/

    //|///////////////////// make_index /////////////////////////////////////
    template<size_t dimension, typename InputIterator>
    basic_rtree<typename std::iterator_traits<InputIterator>::pointer, dimension> make_index(InputIterator first, InputIterator last)
    {
      basic_rtree<typename std::iterator_traits<InputIterator>::pointer, dimension> result;

      for(auto i = first; i != last; ++i)
        result.insert(&*i);

      return result;
    }

    /**
     *  @}
    **/

  } // namespace RTree

  /**
   * \name Misc RTree
   * \ingroup lmlrtree
   * RTree helpers
   * @{
  **/

  template<typename T>
  using RTree2d = RTree::basic_rtree<T, 2>;

  template<typename T>
  using RTree3d = RTree::basic_rtree<T, 3>;

} // namespace lml
} // namespace leap

#endif
