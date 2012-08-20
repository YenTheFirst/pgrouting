/*
 * Alpha-Shapes for PostgreSQL
 *
 * Copyright (c) 2006 Anton A. Patrushev, Orkney, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 *
 * As a special exception, you have permission to link this program
 * with the CGAL library and distribute executables, as long as you
 * follow the requirements of the GNU GPL in regard to all of the
 * software in the executable aside from CGAL.
 *
 */


/***********************************************************************
Takes a list of points and returns a list of segments 
corresponding to the Alpha shape.
************************************************************************/

#include <CGAL/Simple_cartesian.h>
#include <CGAL/Filtered_kernel.h>
#include <CGAL/algorithm.h>

#include <vector>
#include <list>

#include "alpha.h"

#include <CGAL/Delaunay_triangulation_2.h>
#include <CGAL/Triangulation_2.h>
#include <CGAL/Triangulation_hierarchy_vertex_base_2.h>
#include <CGAL/Triangulation_hierarchy_2.h>
#include <CGAL/Triangulation_face_base_2.h>
#include <CGAL/Triangulation_euclidean_traits_2.h>
#include <CGAL/Alpha_shape_2.h>
#include <CGAL/Alpha_shape_face_base_2.h>
#include <CGAL/Alpha_shape_vertex_base_2.h>

#include "stdio.h"
#define DBG(format, arg...)                     \
    printf(format , ## arg);

#include <sstream>
#include <string>

typedef double coord_type;

typedef CGAL::Simple_cartesian<coord_type>  SC;
typedef CGAL::Filtered_kernel<SC> K;

typedef K::Point_2  Point;
typedef K::Segment_2  Segment;

typedef CGAL::Alpha_shape_vertex_base_2<K> Avb;
typedef CGAL::Triangulation_hierarchy_vertex_base_2<Avb> Av;

typedef CGAL::Triangulation_face_base_2<K> Tf;
typedef CGAL::Alpha_shape_face_base_2<K,Tf> Af;

typedef CGAL::Triangulation_default_data_structure_2<K,Av,Af> Tds;
typedef CGAL::Delaunay_triangulation_2<K,Tds> Dt;
typedef CGAL::Triangulation_hierarchy_2<Dt> Ht;
typedef CGAL::Alpha_shape_2<Ht> Alpha_shape_2;

typedef Alpha_shape_2::Face_circulator  Face_circulator;
typedef Alpha_shape_2::Vertex_circulator  Vertex_circulator;

typedef Alpha_shape_2::Alpha_iterator Alpha_iterator;
typedef Alpha_shape_2::Alpha_shape_edges_iterator Alpha_shape_edges_iterator;

//these bits are for the traverser
#include <CGAL/Unique_hash_map.h>
#include <CGAL/Edge_hash_function.h>
typedef typename Dt::Face_handle Face_handle;
typedef Tds::Edge Edge;
typedef CGAL::Unique_hash_map< Edge, bool, CGAL::Edge_hash_function > Marked_edge_set;

//---------------------------------------------------------------------

void traverse(const Edge& starting_edge, Marked_edge_set& marked_edge_set, const Alpha_shape_2& A, std::ostream &out_stream){
  typedef typename Marked_edge_set::Data Data;
  out_stream << A.segment(starting_edge).source();
  Edge current_edge = starting_edge;

  //loop through edges until we get back to the starting edge
  do
  { 
    //we want to find our source edge - the one that points to us.
    //1. get the source vertex, and the circulator of edges that point there
    Face_handle fh = current_edge.first;
    Tds::Vertex_handle v = fh->vertex(A.ccw(current_edge.second));
    Tds::Edge_circulator e_circ = A.incident_edges(v, fh);
    Tds::Edge_circulator start_circ = e_circ;

    //2. find the first edge, counterclockwise, that points to our source
    bool found=false;
    do{
      if (A.classify(*e_circ) == Alpha_shape_2::REGULAR){
        found=true;
        break;
      } 
      e_circ++;
    } 
    while (start_circ != e_circ);

    //3. add it
    if (!found){
      std::cout << "no source edge found\n";
      break;
    } 
    else
    { 
      //found the previous edge.
      current_edge = *e_circ;
      marked_edge_set[current_edge] = true;
      out_stream << ", " << A.segment(current_edge).source();
    } 
  }while (current_edge != starting_edge);
}


void visit_components(const Alpha_shape_2& A, std::ostream &out_stream){
  typedef typename Marked_edge_set::Data Data;
  Marked_edge_set marked_edge_set(false);
  Alpha_shape_edges_iterator edge_it;

  bool on_first = true;

  for( edge_it = A.alpha_shape_edges_begin(); edge_it != A.alpha_shape_edges_end(); ++edge_it)
  {
    Edge e = *edge_it;
    Data & data = marked_edge_set[e];
    if (data == false){
      data = true;
      if (!on_first) {
        out_stream << ",";
      }
      else {
        on_first = false;
      }
      out_stream << "((";
      traverse(e, marked_edge_set, A, out_stream);
      out_stream << "))";
    }
  }
}


int alpha_shape(vertex_t *vertices, unsigned int count,  
                char ** res, char **err_msg)
{
  std::list<Point> points;

  //std::copy(begin(vertices), end(vertices), std::back_inserter(points)); 
  
  for (std::size_t j = 0; j < count; ++j)
    {
      Point p(vertices[j].x, vertices[j].y);
      points.push_back(p);
    }
  

  Alpha_shape_2 A(points.begin(), points.end(),
                  coord_type(10000),
                  Alpha_shape_2::REGULARIZED);
  
  
  //A.set_alpha(*A.find_optimal_alpha(1)*6); 
  //A.set_alpha(*A.find_optimal_alpha(1)); 
  A.set_alpha(0.000050); 

  std::stringstream out_stream;

  out_stream << "MULTIPOLYGON(";
  visit_components( A, out_stream);
  out_stream << ")";
  std::string str = out_stream.str();

  //convert the stringstream into a c_str we can return it
  const char * c_str = str.c_str();
  (*res) = (char *)malloc(sizeof(char) * strlen(c_str));
  strcpy(*res, c_str);

  return EXIT_SUCCESS;
}
