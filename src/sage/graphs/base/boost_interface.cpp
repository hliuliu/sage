#ifndef BOOSTGRAPH
#define BOOSTGRAPH
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/edge_connectivity.hpp>
#include <boost/graph/exterior_property.hpp>
#include <boost/graph/clustering_coefficient.hpp>
#include <boost/graph/dominator_tree.hpp>
#include <boost/graph/cuthill_mckee_ordering.hpp>
#include <boost/graph/king_ordering.hpp>
#include <boost/graph/kruskal_min_spanning_tree.hpp>
#include <boost/graph/prim_minimum_spanning_tree.hpp>
#include <boost/graph/bellman_ford_shortest_paths.hpp>
#include <boost/graph/dijkstra_shortest_paths.hpp>
#include <boost/graph/johnson_all_pairs_shortest.hpp>

#include <iostream>

using namespace std;
using namespace boost;

typedef int v_index;
typedef long e_index;

// This struct is the output of the edge connectivity Boost algorithm.
typedef struct {
    v_index ec; // The edge connectivity
    vector<v_index> edges; // The edges in a minimum cut, stored as a list of
                       // nodes. For instance, if the minimum cut is
                       // {(1,2),(3,4)}, the output vector will be (1,2,3,4).
} result_ec;

// This struct is the output of the clustering coefficient Boost algorithm.
typedef struct {
    double average_clustering_coefficient; // The average clustering coefficient
    vector<double> clust_of_v;             // The clustering coefficient of each node.
} result_cc;

// This struct is the output of the edge connectivity Boost algorithm.
typedef struct {
    vector<double> distances; // An array with all distances from the starting vertex
    vector<v_index> predecessors; // For each vertex v, the first vertex in a shortest
                                  // path from the starting vertex to v.
} result_distances;

template <class OutEdgeListS, // How neighbors are stored
          class VertexListS,  // How vertices are stored
          class DirectedS,    // The kind of graph (undirectedS, directedS, or bidirectionalS)
          class EdgeListS,    // How the list of edges is stored
          class EdgeProperty> // Properties of edges (weight)
class BoostGraph
/*
 * This generic class wraps a Boost graph, in order to make it Cython-friendly.
 *
 * In particular, it allows to "keep together" the Boost graph and the vector
 * *vertices: these two variables are generic, and Cython is not able to deal
 * with them properly, since it does not support generic classes.
 *
 * Vertices are numbers from 0 to n-1, where n is the total number of vertices:
 * this class takes care of the relation between number i and the corresponding
 * Boost vertex descriptor (which might be any object, depending on the value of
 * VertexListS). In particular, (*vertices)[i] contains the Boost vertex
 * corresponding to number i, while to transform a Boost vertex v into a number
 * we use vertex properties, and the syntax is (*graph)[v].
*/
{
    typedef typename boost::adjacency_list<OutEdgeListS, VertexListS, DirectedS,
    property<vertex_index_t, v_index>, EdgeProperty, no_property, EdgeListS> adjacency_list;
    typedef typename boost::graph_traits<adjacency_list>::vertex_descriptor vertex_descriptor;
    typedef typename boost::graph_traits<adjacency_list>::edge_descriptor edge_descriptor;
    typedef typename std::vector<edge_descriptor> edge_container;
    typedef typename boost::property_map<adjacency_list, boost::vertex_index_t>::type vertex_to_int_map;

public:
    adjacency_list *graph;
    vector<vertex_descriptor> *vertices;
    vertex_to_int_map index;

    BoostGraph() {
        graph = new adjacency_list();
        vertices = new vector<vertex_descriptor>();
    }

    ~BoostGraph() {
        delete graph;
        delete vertices;
    }

    v_index num_verts() {
        return num_vertices(*graph);
    }

    e_index num_edges() {
        return boost::num_edges(*graph);
    }

    void add_vertex() {
        (*vertices).push_back(boost::add_vertex((*vertices).size(), *graph));
    }

    void add_edge(v_index u, v_index v) {
        boost::add_edge((*vertices)[u], (*vertices)[v], *graph);
    }

    void add_edge(v_index u, v_index v, double weight) {
        boost::add_edge((*vertices)[u], (*vertices)[v], weight, *graph);
    }

    result_ec edge_connectivity() {
        result_ec to_return;
        edge_container disconnecting_set;
        back_insert_iterator<edge_container> inserter(disconnecting_set);
        to_return.ec = boost::edge_connectivity(*graph, inserter);

        for (v_index i = 0; i < disconnecting_set.size(); i++) {
            edge_descriptor edge = disconnecting_set[i];
            to_return.edges.push_back(index[boost::source(edge, *graph)]);
            to_return.edges.push_back(index[boost::target(edge, *graph)]);
        }
        return to_return;
    }

    double clustering_coeff(v_index v) {
        return clustering_coefficient(*graph, (*vertices)[v]);
    }

    result_cc clustering_coeff_all() {
        result_cc to_return;
        to_return.clust_of_v.resize(num_verts());
        to_return.average_clustering_coefficient = all_clustering_coefficients(*graph,
                        make_iterator_property_map(to_return.clust_of_v.begin(), index));
        return to_return;
    }

    vector<v_index> dominator_tree(v_index v) {
        vector<v_index> fathers(num_verts());
        vector<vertex_descriptor> fathers_descr(num_verts(),
                    boost::graph_traits<adjacency_list>::null_vertex());

        lengauer_tarjan_dominator_tree(*graph, (*vertices)[v],
                                       make_iterator_property_map(
                                           fathers_descr.begin(), index));

        for (v_index i = 0; i < num_verts(); i++) {
            vertex_descriptor v = fathers_descr[i];
            if (v == boost::graph_traits<adjacency_list>::null_vertex()) {
                fathers[i] = -1;
            } else {
                fathers[i] = index[v];
            }
        }
        return fathers;
    }

    // Works only in undirected graphs!
    vector<v_index> bandwidth_ordering(bool cuthill) {
        vector<v_index> to_return;
        vector<vertex_descriptor> inv_perm(num_vertices(*graph));

        if (cuthill) {
            boost::cuthill_mckee_ordering(*graph, inv_perm.rbegin());
        } else {
            boost::king_ordering(*graph, inv_perm.rbegin());
        }

        for (int i = 0; i < inv_perm.size(); i++) {
            to_return.push_back(index[inv_perm[i]]);
        }
        return to_return;
    }

    // This function works only on undirected graphs.
    vector<v_index> kruskal_min_spanning_tree() {
        vector<v_index> to_return;
        std::vector <edge_descriptor> spanning_tree;
        kruskal_minimum_spanning_tree(*graph, std::back_inserter(spanning_tree));

        for (unsigned int i = 0; i < spanning_tree.size(); i++) {
            to_return.push_back(index[source(spanning_tree[i], *graph)]);
            to_return.push_back(index[target(spanning_tree[i], *graph)]);
        }
        return to_return;
    }

    // This function works only on undirected graphs with no parallel edge.
    vector<v_index> prim_min_spanning_tree() {
        vector<v_index> to_return;
        vector<vertex_descriptor> predecessors(num_verts());
        prim_minimum_spanning_tree(*graph, make_iterator_property_map(predecessors.begin(), index));

        for (unsigned int i = 0; i < predecessors.size(); i++) {
            if (index[predecessors[i]] != i) {
                to_return.push_back(i);
                to_return.push_back(index[predecessors[i]]);
            }
        }
        return to_return;
    }

    result_distances dijkstra_shortest_paths(v_index s) {
         v_index N = num_verts();
         result_distances to_return;
         vector<double> distances(N, (std::numeric_limits < double >::max)());
         vector<vertex_descriptor> predecessors(N);
         try {
             boost::dijkstra_shortest_paths(*graph, (*vertices)[s], distance_map(make_iterator_property_map(distances.begin(), index))
                                            .predecessor_map(make_iterator_property_map(predecessors.begin(), index)));
         } catch (boost::exception_detail::clone_impl<boost::exception_detail::error_info_injector<boost::negative_edge> > e) {
             return to_return;
         }

         to_return.distances = distances;

         for (int i = 0; i < N; i++) {
             to_return.predecessors.push_back(index[predecessors[i]]);
         }

         return to_return;
     }

     result_distances bellman_ford_shortest_paths(v_index s) {
         v_index N = num_verts();

         std::vector<double> distance(N, (std::numeric_limits < double >::max)());
         std::vector<vertex_descriptor> predecessors(N);
         result_distances to_return;
         typename property_map<adjacency_list, edge_weight_t>::type weight = get(edge_weight, (*graph));

         for (v_index i = 0; i < N; ++i)
             predecessors[i] = (*vertices)[i];

         distance[s] = 0;
         bool r = boost::bellman_ford_shortest_paths
                 (*graph, N, weight_map(weight).distance_map(make_iterator_property_map(distance.begin(), index)).predecessor_map(make_iterator_property_map(predecessors.begin(), index)));

         if (!r) {
             return to_return;
         }

         to_return.distances = distance;
         for (int i = 0; i < N; i++) {
             to_return.predecessors.push_back(index[predecessors[i]]);
         }
         return to_return;
     }

     vector< vector<double> > johnson_shortest_paths() {
         v_index N = num_verts();

         vector< vector<double> > D(N, vector<double>(N));
         if (johnson_all_pairs_shortest_paths(*graph, D)) {
             return D;
         } else {
             return vector< vector<double> >();
         }
     }
};



#endif // BOOSTGRAPH
