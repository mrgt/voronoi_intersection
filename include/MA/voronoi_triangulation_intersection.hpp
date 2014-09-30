#ifndef MA_VORONOI_TRIANGULATION_INTERSECTION
#define MA_VORONOI_TRIANGULATION_INTERSECTION

#include <CGAL/Regular_triangulation_2.h>
#include <CGAL/Delaunay_triangulation_2.h>
#include <MA/voronoi_polygon_intersection.hpp>
#include <queue>

namespace MA
{
  template <class K>
  typename CGAL::Delaunay_triangulation_2<K>::Vertex_handle
  nearest_vertex(const typename CGAL::Delaunay_triangulation_2<K> &dt,
		 const typename CGAL::Point_2<K>  &p)
  {
    return dt.nearest_vertex(p);
  }

  template <class K, class Gt>
  typename CGAL::Regular_triangulation_2<Gt>::Vertex_handle
  nearest_vertex(const typename CGAL::Regular_triangulation_2<Gt> &dt,
		 const typename CGAL::Point_2<K> &p)
  {
    return dt.nearest_power_vertex(p);
  }


  template <class T, class DT, class F>
  void
  voronoi_triangulation_intersection_raw(const T &t,
					 const DT &dt,
					 F out)
  {
    typedef typename CGAL::Kernel_traits<typename DT::Point>::Kernel K;
    typedef Voronoi_intersection_traits<K> Traits;
    typedef typename DT::Vertex_handle Vertex_handle_DT;
    typedef typename T::Face_handle Face_handle_T;
    typedef typename K::Point_2 Point;

    typedef CGAL::Polygon_2<K> Polygon;
    typedef Pgon_intersector<Polygon, DT, Traits> Pgon_isector;
    typedef typename Pgon_isector::Pgon Pgon;

    if (t.number_of_vertices() == 0)
      return;

    // insert seed
    Face_handle_T f = t.finite_faces_begin();
    Vertex_handle_DT v = nearest_vertex(dt, f->vertex(0)->point());

    typedef std::pair<Vertex_handle_DT, Face_handle_T> VF_pair;
    std::priority_queue<VF_pair> Q;
    std::set<VF_pair> visited;

    Q.push(VF_pair(v,f));
    visited.insert(VF_pair(v,f));
    while (!Q.empty())
      {
	VF_pair vfp = Q.top(); Q.pop();
	Vertex_handle_DT v = vfp.first;
	Face_handle_T f = vfp.second;

	Polygon Ptri;
	Ptri.push_back(f->vertex(0)->point());
	Ptri.push_back(f->vertex(1)->point());
	Ptri.push_back(f->vertex(2)->point());

	Pgon R;
	R.push_back(std::make_pair(0,1));
	R.push_back(std::make_pair(1,2));
	R.push_back(std::make_pair(2,0));

	Pgon_isector isector (Ptri,dt);
	auto c = dt.incident_edges (v), done(c);
	do
	  {
	    Pgon Rl;
	    if (dt.is_infinite(c))
	      continue;
	    auto w = c->first->vertex(dt.ccw(c->second));
	    isector(R, v, w, Rl);
	    R = Rl;
	  }
	while (++c != done);
	
	// propagate to neighbors
	for (auto E:R)
	  {
	    for (auto seg:{E.first, E.second})
	      {
		VF_pair p;
		if(isector.edge_type(seg) == DELAUNAY)
		  {
		    auto u = *boost::get<Vertex_handle_DT> (&seg);
		    p = VF_pair(u, f);
		  }
		else // type = POLYGON
		  {
		    size_t i = *boost::get<size_t> (&seg);
		    Face_handle_T fn = f->neighbor(i);
		    if (t.is_infinite(fn))
		      continue;
		    p = VF_pair(v, fn);
		  }
		if (visited.find(p) != visited.end())
		  continue;
		visited.insert(p);
		Q.push(p);
	      }
    	  }
	out(Ptri, R, f, v);
      }
  }

  template <class T, class DT, class F>
  void
  voronoi_triangulation_intersection(const T &t,
				     const DT &dt,
				     F out)
  {
    typedef typename CGAL::Kernel_traits<typename DT::Point>::Kernel K;
    typedef Voronoi_intersection_traits<K> Traits;
    typedef CGAL::Polygon_2<K> Polygon;
    typedef Pgon_intersector<Polygon, DT, Traits> Pgon_isector;
    typedef typename Pgon_isector::Pgon Pgon;

    voronoi_triangulation_intersection_raw
      (t, dt,
       [&] (const Polygon &Ptri,
	    const Pgon &R,
	    typename T::Face_handle f,
	    typename DT::Vertex_handle v)
       {
	 Pgon_isector isector(Ptri,dt);
	 Polygon res;
	 for (auto E:R)
	   res.push_back(isector.vertex_to_point(v, E));
	 out(res,f,v);
       });
  }  
}

#endif
