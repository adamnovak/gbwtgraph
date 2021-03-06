#ifndef GBWTGRAPH_UTILS_H
#define GBWTGRAPH_UTILS_H

#include <handlegraph/handle_graph.hpp>
#include <handlegraph/util.hpp>

#include <iostream>
#include <tuple>
#include <unordered_map>

/*
  utils.h: Common utilities and definitions.
*/

namespace gbwtgraph
{

//------------------------------------------------------------------------------

// Import type definitions from libhandlegraph.

using nid_t = handlegraph::nid_t;
using off_t = handlegraph::off_t;
using pos_t = handlegraph::pos_t;
using handle_t = handlegraph::handle_t;
using edge_t = handlegraph::edge_t;

using HandleGraph = handlegraph::HandleGraph;
using SerializableHandleGraph = handlegraph::SerializableHandleGraph;

//------------------------------------------------------------------------------

struct Version
{
  static std::string str(bool verbose = false);
  static void print(std::ostream& out, const std::string& tool_name, bool verbose = false, size_t new_lines = 2);

  constexpr static size_t MAJOR_VERSION     = 0;
  constexpr static size_t MINOR_VERSION     = 1;
  constexpr static size_t PATCH_VERSION     = 0;

  constexpr static size_t GRAPH_VERSION     = 1;
  constexpr static size_t MINIMIZER_VERSION = 5;
};

//------------------------------------------------------------------------------

// Functions for pos_t manipulation.

inline pos_t
make_pos_t(nid_t id, bool is_rev, off_t off)
{
  return std::make_tuple(id, is_rev, off);
}

inline nid_t
id(const pos_t& pos)
{
  return std::get<0>(pos);
}

inline bool
is_rev(const pos_t& pos)
{
  return std::get<1>(pos);
}

inline off_t
offset(const pos_t& pos)
{
  return std::get<2>(pos);
}

inline nid_t&
get_id(pos_t& pos)
{
  return std::get<0>(pos);
}

inline bool&
get_is_rev(pos_t& pos)
{
  return std::get<1>(pos);
}

inline off_t&
get_offset(pos_t& pos)
{
  return std::get<2>(pos);
}

inline bool
is_empty(const pos_t& pos)
{
  return (id(pos) == 0);
}

// Get a pos_t for the same base in the other orientation.
inline pos_t
reverse_base_pos(const pos_t& pos, size_t node_length)
{
  return make_pos_t(id(pos), !is_rev(pos), (node_length - 1) - offset(pos));
}

inline std::ostream&
operator<<(std::ostream& out, const pos_t& pos)
{
  return out << id(pos) << (is_rev(pos) ? "-" : "+") << offset(pos);
}

//------------------------------------------------------------------------------

// Utility functions.

std::string reverse_complement(const std::string& seq);
void reverse_complement_in_place(std::string& seq);

//------------------------------------------------------------------------------

/*
  A class that maps handles to strings. This can be used as a sequence source in
  GBWTGraph construction.
*/
class SequenceSource
{
public:
  SequenceSource() {}

  void add_node(nid_t node_id, const std::string& sequence)
  {
    this->sequences[this->get_handle(node_id, false)] = sequence;
  }

  handle_t get_handle(const nid_t& node_id, bool is_reverse = false) const
  {
    return handlegraph::number_bool_packing::pack(node_id, is_reverse);
  }

  size_t get_length(const handle_t& handle) const
  {
    auto iter = this->sequences.find(handle);
    if(iter == this->sequences.end()) { return 0; }
    return iter->second.length();
  }

  std::string get_sequence(const handle_t& handle) const
  {
    auto iter = this->sequences.find(handle);
    if(iter == this->sequences.end()) { return ""; }
    return iter->second;
  }

  std::unordered_map<handle_t, std::string> sequences;

private:
  SequenceSource(const SequenceSource&) = delete;
  SequenceSource& operator=(const SequenceSource&) = delete;
};

//------------------------------------------------------------------------------

} // namespace gbwtgraph

#endif // GBWTGRAPH_UTILS_H
