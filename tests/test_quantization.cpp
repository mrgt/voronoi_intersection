#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
#include <CGAL/Delaunay_triangulation_2.h>

#include <MA/voronoi_triangulation_intersection.hpp>
#include <MA/voronoi_polygon_intersection.hpp>
#include <MA/misc.hpp>
#include <CImg.h>

#include <cstdlib>

typedef CGAL::Exact_predicates_inexact_constructions_kernel K;
typedef CGAL::Point_2<K> Point;
typedef CGAL::Vector_2<K> Vector;
typedef CGAL::Line_2<K> Line;
typedef CGAL::Polygon_2<K> Polygon;
typedef K::FT FT;

typedef CGAL::Delaunay_triangulation_2<K> DT;
typedef DT T;

double rr() 
{ 
  return 2*double(rand() / (RAND_MAX + 1.0))-1;
}

#include <boost/timer/timer.hpp>

template <class K, class F>
typename K::FT
integrate(const typename CGAL::Polygon_2<K> &p, const F &f)
{
  typedef typename K::FT FT;
  FT r = 0;
  if (p.size() <= 2)
    return FT(0);
  FT f0 = f(p[0]), fprev = f(p[1]);
  for (size_t i = 1; i < p.size() - 1; ++i)
    {
      FT fnext = f(p[i+1]);
      r += CGAL::area(p[0],p[i],p[i+1]) * (f0 + fprev + fnext)/3 ;
      fprev = fnext;
    }
  return r;
}

// Compute barycentric coordinates (u, v, w) for
// point p with respect to triangle (a, b, c)
template <class Point, class FT>
void barycentric(const Point &p,
		 const Point &a,
		 const Point &b,
		 const Point &c,
		 FT &u, FT &v, FT &w)
{
  auto v0 = b - a, v1 = c - a, v2 = p - a;
  FT d00 = v0*v0;
  FT d01 = v0*v1;
  FT d11 = v1*v1;
  FT d20 = v2*v0;
  FT d21 = v2*v1;
  FT denom = d00 * d11 - d01 * d01;
  v = (d11 * d20 - d01 * d21) / denom;
  w = (d00 * d21 - d01 * d20) / denom;
  u = FT(1) - v - w;
}

template <class Point, class FT>
FT extrapolate(const Point &p,
	       const Point &a, FT fa,
	       const Point &b, FT fb,
	       const Point &c, FT fc)
{
  FT u, v, w;
  barycentric(p, a, b, c, u, v, w);
  return u * fa + v * fb + w * fc;
}

template<class K>
class Linear_function
{
  typedef typename K::Point_2 Point;
  typedef typename K::FT FT;
  FT _a, _b, _c;

public:
  Linear_function(): _a(0), _b(0), _c(0) {}
  Linear_function(const Point &p, FT fp, 
		  const Point &q, FT fq,
		  const Point &r, FT fr) 
  {
    _c = extrapolate(Point(0,0), p, fp, q, fq, r, fr); // const term
    _a = extrapolate(Point(1,0), p, fp, q, fq, r, fr) - _c;
    _b = extrapolate(Point(0,1), p, fp, q, fq, r, fr) - _c;
  }

  FT
  operator () (const CGAL::Point_2<K> &p) const
  {
    return _a * p.x() + _b * p.y() + _c;
  }

  FT
  operator () (const CGAL::Polygon_2<K> &p) const
  {
    return ::integrate(p, *this);
  }

  typedef FT result_type;
};

template <class DT, class T, class F>
struct Voronoi_integrator
{
  typedef typename DT::Point Point;
  typedef typename CGAL::Kernel_traits<Point>::Kernel::FT FT;
  typedef typename DT::Vertex_handle Vertex_handle_DT;
  typedef typename T::Face_handle Face_handle_T;
  typedef typename F::result_type result_type;

  std::map<Vertex_handle_DT, result_type> _integrals;
  std::map<Face_handle_T, F> _functions;
  FT _total;
  
  Voronoi_integrator(const std::map<Face_handle_T, F> &f): 
    _functions(f), _total(0)
  {}

  template <class FH, class VH>
  void operator() (const CGAL::Polygon_2<K> &p,
		   FH tri, VH v)
  {
    FT k = _functions[tri](p);
    _integrals[v] += k;
    _total += k;
  }
};


int main(int argc, const char **argv)
{
  std::vector<Point> pts;
  for (size_t i = 0; i < 20000; ++i)
    pts.push_back(Point(rr(), rr()));
  DT dt (pts.begin(), pts.end());

  if (argc < 2)
    return -1;
  cimg_library::CImg<double> image(argv[1]);
  std::vector<Point> grid;
  std::map<Point, double> fgrid;
  size_t n = image.width();
  size_t m = image.height();
  double dx = 2/double(n-1), x0=-1.0;
  double dy = 2/double(m-1), y0=-1.0;
  for (size_t i = 0; i < n; ++i)
    {
      double y = -1;
      for (size_t j = 0; j < m; ++j)
	{
	  Point p(x0 + i * dx,
		  y0 + j * dy);
	  grid.push_back(p);
	  fgrid[p] = image(i,m-j)/double(255);
	}
    }
  T t (grid.begin(), grid.end());

  typedef Linear_function<K> Function;
  typedef Voronoi_integrator<DT,T,Function> Integrator;

  std::map<T::Face_handle, Function> functions;
  FT tot_orig(0);
  for (auto f = t.finite_faces_begin(); 
       f != t.finite_faces_end(); ++f)
    {
      Point p = f->vertex(0)->point(), 
	q = f->vertex(1)->point(), 
	r = f->vertex(2)->point();
      functions[f] = Function(p, fgrid[p], 
      			      q, fgrid[q],
      			      r, fgrid[r]);
      Polygon poly;
      poly.push_back(p);
      poly.push_back(q);
      poly.push_back(r);
      tot_orig += functions[f](poly);
    }
  Integrator integrator(functions);
  
  boost::timer::auto_cpu_timer tm(std::cerr);
  
  MA::voronoi_triangulation_intersection(t,dt,integrator);
  
  Polygon P;
  P.push_back(Point(-1,-1));
  P.push_back(Point(1,-1));
  P.push_back(Point(1,1));
  P.push_back(Point(-1,1));

  MA::ps_begin(std::cout);
  for (auto v = dt.finite_vertices_begin();
       v != dt.finite_vertices_end(); ++v)
    {
      Polygon R = MA::voronoi_polygon_intersection(P, dt, v);
      double a = CGAL::to_double(R.area());
      double ig = integrator._integrals[v]/a;
      MA::ps_polygon(std::cout, R,
		     0.001, 
		     ig, ig, ig, true);
    }

  std::cerr << integrator._total << " vs "
	    << tot_orig << "\n";
  MA::ps_end(std::cout);
  return 0;
}

