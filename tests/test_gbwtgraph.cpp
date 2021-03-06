#include <gtest/gtest.h>

#include <fstream>
#include <set>
#include <utility>
#include <vector>

#include <omp.h>

#include <gbwtgraph/gbwtgraph.h>

#include "shared.h"

using namespace gbwtgraph;

namespace
{

//------------------------------------------------------------------------------

class GraphOperations : public ::testing::Test
{
public:
  typedef std::pair<gbwt::node_type, gbwt::node_type> gbwt_edge;

  gbwt::GBWT index;
  SequenceSource source;
  GBWTGraph graph;
  std::set<nid_t> correct_nodes;
  std::set<gbwt_edge> correct_edges, reverse_edges;
  std::set<gbwt::vector_type> correct_paths;

  GraphOperations()
  {
  }

  void SetUp() override
  {
    this->index = build_gbwt_index();
    build_source(this->source);
    this->graph = GBWTGraph(this->index, this->source);

    this->correct_nodes =
    {
      static_cast<nid_t>(1),
      static_cast<nid_t>(2),
      static_cast<nid_t>(4),
      static_cast<nid_t>(5),
      static_cast<nid_t>(6),
      static_cast<nid_t>(7),
      static_cast<nid_t>(8),
      static_cast<nid_t>(9)
    };      

    this->correct_edges =
    {
      { gbwt::Node::encode(1, false), gbwt::Node::encode(2, false) },
      { gbwt::Node::encode(1, false), gbwt::Node::encode(4, false) },
      { gbwt::Node::encode(2, false), gbwt::Node::encode(4, false) },
      { gbwt::Node::encode(4, false), gbwt::Node::encode(5, false) },
      { gbwt::Node::encode(5, false), gbwt::Node::encode(6, false) },
      { gbwt::Node::encode(6, false), gbwt::Node::encode(7, false) },
      { gbwt::Node::encode(6, false), gbwt::Node::encode(8, false) },
      { gbwt::Node::encode(7, false), gbwt::Node::encode(9, false) },
      { gbwt::Node::encode(8, false), gbwt::Node::encode(9, false) }
    };
    for(gbwt_edge edge : this->correct_edges)
    {
      this->reverse_edges.insert(gbwt_edge(gbwt::Node::reverse(edge.second), gbwt::Node::reverse(edge.first)));
    }

    this->correct_paths = { alt_path, short_path };
  }
};

TEST_F(GraphOperations, EmptyGraph)
{
  gbwt::GBWT empty_index;
  SequenceSource empty_source;
  GBWTGraph empty_graph(empty_index, empty_source);
  EXPECT_EQ(empty_graph.get_node_count(), static_cast<size_t>(0)) << "Empty graph contains nodes";
}

TEST_F(GraphOperations, CorrectNodes)
{
  ASSERT_EQ(this->graph.get_node_count(), this->correct_nodes.size()) << "Wrong number of nodes";
  EXPECT_EQ(this->graph.min_node_id(), *(this->correct_nodes.begin())) << "Wrong minimum node id";
  EXPECT_EQ(this->graph.max_node_id(), *(this->correct_nodes.rbegin())) << "Wrong maximum node id";
  for(nid_t id = this->graph.min_node_id(); id <= this->graph.max_node_id(); id++)
  {
    bool should_exist = (this->correct_nodes.find(id) != this->correct_nodes.end());
    EXPECT_EQ(this->graph.has_node(id), should_exist) << "Node set incorrect at " << id;
  }
}

TEST_F(GraphOperations, Handles)
{
  for(nid_t id : this->correct_nodes)
  {
    handle_t forward_handle = this->graph.get_handle(id, false);
    handle_t reverse_handle = this->graph.get_handle(id, true);
    EXPECT_EQ(this->graph.get_id(forward_handle), id) << "Wrong node id for forward handle " << id;
    EXPECT_FALSE(this->graph.get_is_reverse(forward_handle)) << "Forward handle " << id << " is reverse";
    EXPECT_EQ(this->graph.get_id(reverse_handle), id) << "Wrong node id for reverse handle " << id;
    EXPECT_TRUE(this->graph.get_is_reverse(reverse_handle)) << "Reverse handle " << id << " is not reverse";
    handle_t flipped_fw = this->graph.flip(forward_handle);
    handle_t flipped_rev = this->graph.flip(reverse_handle);
    EXPECT_NE(as_integer(forward_handle), as_integer(reverse_handle)) << "Forward and reverse handles are identical";
    EXPECT_EQ(as_integer(flipped_fw), as_integer(reverse_handle)) << "Flipped forward handle is not identical to the reverse handle";
    EXPECT_EQ(as_integer(flipped_rev), as_integer(forward_handle)) << "Flipped reverse handle is not identical to the forward handle";
  }
}

TEST_F(GraphOperations, Sequences)
{
  for(nid_t id : this->correct_nodes)
  {
    handle_t gbwt_fw = this->graph.get_handle(id, false);
    handle_t gbwt_rev = this->graph.get_handle(id, true);
    handle_t source_fw = this->source.get_handle(id, false);
    EXPECT_EQ(this->graph.get_length(gbwt_fw), this->source.get_length(source_fw)) << "Wrong forward length at node " << id;
    EXPECT_EQ(this->graph.get_sequence(gbwt_fw), this->source.get_sequence(source_fw)) << "Wrong forward sequence at node " << id;
    std::string source_rev = reverse_complement(this->source.get_sequence(source_fw));
    EXPECT_EQ(this->graph.get_length(gbwt_rev), source_rev.length()) << "Wrong reverse length at node " << id;
    EXPECT_EQ(this->graph.get_sequence(gbwt_rev), source_rev) << "Wrong reverse sequence at node " << id;
  }
}

TEST_F(GraphOperations, Substrings)
{
  for(nid_t id : correct_nodes)
  {
    handle_t fw = this->graph.get_handle(id, false);
    handle_t rev = this->graph.get_handle(id, true);
    std::string fw_str = this->graph.get_sequence(fw);
    std::string rev_str = this->graph.get_sequence(rev);
    ASSERT_EQ(fw_str.length(), rev_str.length()) << "Forward and reverse sequences have different lengths at node " << id;
    for(size_t i = 0; i < fw_str.length(); i++)
    {
      EXPECT_EQ(this->graph.get_base(fw, i), fw_str[i]) << "Wrong forward base " << i << " at node " << id;
      EXPECT_EQ(this->graph.get_base(rev, i), rev_str[i]) << "Wrong reverse base " << i << " at node " << id;
      EXPECT_EQ(this->graph.get_subsequence(fw, i, 2), fw_str.substr(i, 2)) << "Wrong forward substring " << i << " at node " << id;
      EXPECT_EQ(this->graph.get_subsequence(rev, i, 2), rev_str.substr(i, 2)) << "Wrong reverse substring " << i << " at node " << id;
    }
  }
}

TEST_F(GraphOperations, SequenceView)
{
  for(nid_t id : correct_nodes)
  {
    for(bool orientation : { false, true })
    {
      handle_t handle = this->graph.get_handle(id, orientation);
      std::string sequence = this->graph.get_sequence(handle);
      GBWTGraph::view_type view = this->graph.get_sequence_view(handle);
      std::string view_sequence(view.first, view.second);
      EXPECT_EQ(view_sequence, sequence) << "Wrong sequence view at node " << id << ", orientation " << orientation;
      EXPECT_TRUE(this->graph.starts_with(handle, sequence.front())) << "Wrong first character at node " << id << ", orientation " << orientation;
      EXPECT_FALSE(this->graph.starts_with(handle, 'x')) << "Wrong first character at node " << id << ", orientation " << orientation;
      EXPECT_TRUE(this->graph.ends_with(handle, sequence.back())) << "Wrong last character at node " << id << ", orientation " << orientation;
      EXPECT_FALSE(this->graph.ends_with(handle, 'x')) << "Wrong last character at node " << id << ", orientation " << orientation;
    }
  }
}

TEST_F(GraphOperations, Edges)
{
  std::set<gbwt_edge> fw_succ, fw_pred, rev_succ, rev_pred;
  for(nid_t id : correct_nodes)
  {
    handle_t forward_handle = this->graph.get_handle(id, false);
    handle_t reverse_handle = this->graph.get_handle(id, true);
    this->graph.follow_edges(forward_handle, false, [&](const handle_t& handle) {
      fw_succ.insert(gbwt_edge(GBWTGraph::handle_to_node(forward_handle), GBWTGraph::handle_to_node(handle)));
    });
    this->graph.follow_edges(forward_handle, true, [&](const handle_t& handle) {
      fw_pred.insert(gbwt_edge(GBWTGraph::handle_to_node(handle), GBWTGraph::handle_to_node(forward_handle)));
    });
    this->graph.follow_edges(reverse_handle, false, [&](const handle_t& handle) {
      rev_succ.insert(gbwt_edge(GBWTGraph::handle_to_node(reverse_handle), GBWTGraph::handle_to_node(handle)));
    });
    this->graph.follow_edges(reverse_handle, true, [&](const handle_t& handle) {
      rev_pred.insert(gbwt_edge(GBWTGraph::handle_to_node(handle), GBWTGraph::handle_to_node(reverse_handle)));
    });
  }
  EXPECT_EQ(fw_succ, correct_edges) << "Wrong forward successors";
  EXPECT_EQ(fw_pred, correct_edges) << "Wrong forward predecessors";
  EXPECT_EQ(rev_succ, reverse_edges) << "Wrong reverse successors";
  EXPECT_EQ(rev_pred, reverse_edges) << "Wrong reverse predecessors";
}

TEST_F(GraphOperations, ForEachHandle)
{
  std::vector<handle_t> found_handles;
  this->graph.for_each_handle([&](const handle_t& handle)
  {
    found_handles.push_back(handle);
  }, false);
  ASSERT_EQ(found_handles.size(), correct_nodes.size()) << "Wrong number of handles in sequential iteration";
  for(handle_t& handle : found_handles)
  {
    nid_t id = this->graph.get_id(handle);
    EXPECT_TRUE(this->correct_nodes.find(id) != this->correct_nodes.end()) << "Sequential: Found invalid node " << id;
    EXPECT_FALSE(this->graph.get_is_reverse(handle)) << "Sequential: Found reverse node " << id;
  }

  found_handles.clear();
  int old_thread_count = omp_get_max_threads();
  omp_set_num_threads(2);
  this->graph.for_each_handle([&](const handle_t& handle)
  {
    #pragma omp critical
    {
      found_handles.push_back(handle);
    }
  }, false);
  omp_set_num_threads(old_thread_count);
  ASSERT_EQ(found_handles.size(), correct_nodes.size()) << "Wrong number of handles in parallel iteration";
  for(handle_t& handle : found_handles)
  {
    nid_t id = this->graph.get_id(handle);
    EXPECT_TRUE(this->correct_nodes.find(id) != this->correct_nodes.end()) << "Parallel: Found invalid node " << id;
    EXPECT_FALSE(this->graph.get_is_reverse(handle)) << "Parallel: Found reverse node " << id;
  }
}

TEST_F(GraphOperations, ForwardTraversal)
{
  typedef std::pair<gbwt::SearchState, gbwt::vector_type> state_type;
  std::vector<gbwt::vector_type> found_paths;
  std::stack<state_type> states;

  // Extract all paths starting from node 1 in forward orientation.
  handle_t first_node = this->graph.get_handle(1, false);
  states.push({ this->graph.get_state(first_node),
                { static_cast<gbwt::vector_type::value_type>(GBWTGraph::handle_to_node(first_node)) } });
  while(!states.empty())
  {
    state_type curr = states.top();
    states.pop();
    bool extend_success = false;
    this->graph.follow_paths(curr.first, [&](const gbwt::SearchState& next_search) -> bool
    {
      if(!next_search.empty())
      {
        extend_success = true;
        gbwt::vector_type next_path = curr.second;
        next_path.push_back(next_search.node);
        states.push({ next_search, next_path });
      }
      return true;
    });
    if(!extend_success) { found_paths.push_back(curr.second); }
  }

  ASSERT_EQ(found_paths.size(), correct_paths.size()) << "Found a wrong number of paths";
  for(size_t i = 0; i < found_paths.size(); i++)
  {
    const gbwt::vector_type& path = found_paths[i];
    EXPECT_TRUE(correct_paths.find(path) != correct_paths.end()) << "Path " << i << " is incorrect";
  }
}

TEST_F(GraphOperations, BidirectionalTraversal)
{
  typedef std::pair<gbwt::BidirectionalState, gbwt::vector_type> state_type;
  std::vector<gbwt::vector_type> found_paths;
  std::stack<state_type> fw_states, rev_states;

  // Extract all paths visiting node 4 in forward orientation.
  handle_t first_node = this->graph.get_handle(4, false);
  fw_states.push({ this->graph.get_bd_state(first_node),
                 { static_cast<gbwt::vector_type::value_type>(GBWTGraph::handle_to_node(first_node)) } });
  while(!fw_states.empty())
  {
    state_type curr = fw_states.top();
    fw_states.pop();
    bool extend_success = false;
    this->graph.follow_paths(curr.first, false, [&](const gbwt::BidirectionalState& next_search) -> bool
    {
      if(!next_search.empty())
      {
        extend_success = true;
        gbwt::vector_type next_path = curr.second;
        next_path.push_back(next_search.forward.node);
        fw_states.push({ next_search, next_path });
      }
      return true;
    });
    if (!extend_success) { rev_states.push(curr); }
  }
  while (!rev_states.empty())
  {
    state_type curr = rev_states.top();
    rev_states.pop();
    bool extend_success = false;
    this->graph.follow_paths(curr.first, true, [&](const gbwt::BidirectionalState& next_search) -> bool
    {
      if(!next_search.empty())
      {
        extend_success = true;
        gbwt::vector_type next_path
        {
          static_cast<gbwt::vector_type::value_type>(gbwt::Node::reverse(next_search.backward.node))
        };
        next_path.insert(next_path.end(), curr.second.begin(), curr.second.end());
        rev_states.push({ next_search, next_path });
      }
      return true;
    });
    if (!extend_success) { found_paths.push_back(curr.second); }
  }

  ASSERT_EQ(found_paths.size(), correct_paths.size()) << "Found a wrong number of paths";
  for(size_t i = 0; i < found_paths.size(); i++)
  {
    const gbwt::vector_type& path = found_paths[i];
    EXPECT_TRUE(correct_paths.find(path) != correct_paths.end()) << "Path " << i << " is incorrect";
  }
}

//------------------------------------------------------------------------------

class GraphSerialization : public ::testing::Test
{
public:
  gbwt::GBWT index;
  SequenceSource source;
  GBWTGraph graph;

  GraphSerialization()
  {
  }

  void SetUp() override
  {
    this->index = build_gbwt_index();
    build_source(this->source);
    this->graph = GBWTGraph(this->index, this->source);
  }
};

TEST_F(GraphSerialization, EmptyGraph)
{
  GBWTGraph empty_graph;
  std::string filename = gbwt::TempFile::getName("gbwtgraph");
  std::ofstream out(filename, std::ios_base::binary);
  empty_graph.serialize(out);
  out.close();

  GBWTGraph duplicate_graph;
  std::ifstream in(filename, std::ios_base::binary);
  duplicate_graph.deserialize(in);
  in.close();
  gbwt::TempFile::remove(filename);
  EXPECT_EQ(duplicate_graph.get_node_count(), static_cast<size_t>(0)) << "Loaded empty graph contains nodes";
}

TEST_F(GraphSerialization, NonemptyGraph)
{
  std::string filename = gbwt::TempFile::getName("gbwtgraph");
  std::ofstream out(filename, std::ios_base::binary);
  this->graph.serialize(out);
  out.close();

  GBWTGraph duplicate_graph;
  std::ifstream in(filename, std::ios_base::binary);
  duplicate_graph.deserialize(in);
  duplicate_graph.set_gbwt(this->index);
  in.close();
  gbwt::TempFile::remove(filename);

  EXPECT_EQ(duplicate_graph.header, this->graph.header) << "Serialization did not preserve the header";
  EXPECT_EQ(duplicate_graph.sequences, this->graph.sequences) << "Serialization did not preserve the sequences";
  EXPECT_EQ(duplicate_graph.offsets, this->graph.offsets) << "Serialization did not preserve the offsets";
  EXPECT_EQ(duplicate_graph.real_nodes, this->graph.real_nodes) << "Serialization did not preserve the real nodes";
}

//------------------------------------------------------------------------------

class ForEachWindow : public ::testing::Test
{
public:
  typedef std::pair<std::vector<handle_t>, std::string> kmer_type;

  gbwt::GBWT index;
  SequenceSource source;
  GBWTGraph graph;
  std::set<kmer_type> correct_kmers;

  ForEachWindow()
  {
  }

  void SetUp() override
  {
    this->index = build_gbwt_index();
    build_source(this->source);
    this->graph = GBWTGraph(this->index, this->source);

    this->correct_kmers =
    {
      // alt_path forward
      { { this->graph.get_handle(1, false), this->graph.get_handle(2, false), this->graph.get_handle(4, false) }, "GAG" },
      { { this->graph.get_handle(2, false), this->graph.get_handle(4, false) }, "AGG" },
      { { this->graph.get_handle(4, false), this->graph.get_handle(5, false), this->graph.get_handle(6, false) }, "GGGTA" },
      { { this->graph.get_handle(5, false), this->graph.get_handle(6, false), this->graph.get_handle(8, false) }, "TAA" },
      { { this->graph.get_handle(6, false), this->graph.get_handle(8, false), this->graph.get_handle(9, false) }, "AAA" },

      // alt_path reverse
      { { this->graph.get_handle(9, true), this->graph.get_handle(8, true), this->graph.get_handle(6, true) }, "TTT" },
      { { this->graph.get_handle(8, true), this->graph.get_handle(6, true), this->graph.get_handle(5, true) }, "TTA" },
      { { this->graph.get_handle(6, true), this->graph.get_handle(5, true), this->graph.get_handle(4, true) }, "TAC" },
      { { this->graph.get_handle(5, true), this->graph.get_handle(4, true) }, "ACC" },
      { { this->graph.get_handle(4, true), this->graph.get_handle(2, true), this->graph.get_handle(1, true) }, "CCCTC" },

      // short_path forward
      { { this->graph.get_handle(1, false), this->graph.get_handle(4, false) }, "GGG" },
      { { this->graph.get_handle(4, false), this->graph.get_handle(5, false), this->graph.get_handle(6, false) }, "GGGTA" },
      { { this->graph.get_handle(5, false), this->graph.get_handle(6, false), this->graph.get_handle(7, false) }, "TAC" },
      { { this->graph.get_handle(6, false), this->graph.get_handle(7, false), this->graph.get_handle(9, false) }, "ACA" },

      // short_path reverse
      { { this->graph.get_handle(9, true), this->graph.get_handle(7, true), this->graph.get_handle(6, true) }, "TGT" },
      { { this->graph.get_handle(7, true), this->graph.get_handle(6, true), this->graph.get_handle(5, true) }, "GTA" },
      { { this->graph.get_handle(6, true), this->graph.get_handle(5, true), this->graph.get_handle(4, true) }, "TAC" },
      { { this->graph.get_handle(5, true), this->graph.get_handle(4, true) }, "ACC" },
      { { this->graph.get_handle(4, true), this->graph.get_handle(1, true) }, "CCCC" }
    };
  }
};

TEST_F(ForEachWindow, KmerExtraction)
{
  // Extract all haplotype-consistent windows of length 3.
  std::set<kmer_type> found_kmers;
  for_each_haplotype_window(this->graph, 3, [&found_kmers](const std::vector<handle_t>& traversal, const std::string& seq)
  {
    found_kmers.insert(kmer_type(traversal, seq));
  }, false);

  ASSERT_EQ(found_kmers.size(), this->correct_kmers.size()) << "Found a wrong number of kmers";
  for(const kmer_type& kmer : found_kmers)
  {
    EXPECT_TRUE(correct_kmers.find(kmer) != correct_kmers.end()) << "Kmer " << kmer.second << " is incorrect";
  }
}

//------------------------------------------------------------------------------

} // namespace
