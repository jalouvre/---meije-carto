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

/**************************************************************************************************/

// #include "polygon.h"

// #include <QVector>

// #include "segment.h"
// #include "line.h"

/**************************************************************************************************/

// #include <sys/types.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring> // memset

/**************************************************************************************************/

#define DEBUG 1
// #define CLOCK
// #define STANDALONE
// #define SIMPLE

/**************************************************************************************************/

constexpr int T_X = 1;
constexpr int T_Y = 2;
constexpr int T_SINK = 3;

constexpr int QSIZE = 800; // maximum table sizes
constexpr int TRSIZE = 400; // max# trapezoids
constexpr int SEGSIZE = 100; // max# of segments

constexpr int FIRSTPT = 1; // checking whether pt. is inserted
constexpr int LASTPT = 2;

constexpr double EPS = 0.000005;

// constexpr int INFINITY 1<<30

constexpr double C_EPS = 1.0e-7;

constexpr int S_LEFT = 1; // for merge-direction
constexpr int S_RIGHT = 2;

constexpr int ST_VALID = 1; // for trapezium table
constexpr int ST_INVALID = 2;

constexpr int SP_SIMPLE_LRUP = 1; // for splitting trapezoids
constexpr int SP_SIMPLE_LRDN = 2;
constexpr int SP_2UP_2DN = 3;
constexpr int SP_2UP_LEFT = 4;
constexpr int SP_2UP_RIGHT = 5;
constexpr int SP_2DN_LEFT = 6;
constexpr int SP_2DN_RIGHT = 7;
constexpr int SP_NOSPLIT = -1;

constexpr int TR_FROM_UP = 1; // for traverse-direction
constexpr int TR_FROM_DN = 2;

constexpr int TRI_LHS = 1;
constexpr int TRI_RHS = 2;

/**************************************************************************************************/

typedef struct {
  double x, y;
} point_t, vector_t;

typedef struct {
  point_t v0, v1;
  int is_inserted;
  int root0, root1;
} segment_t;

typedef struct {
  int lseg, rseg;
  point_t hi, lo;
  int u0, u1;
  int d0, d1;
  int sink;
  int usave, uside;
  int state;
} trap_t;

typedef struct {
  int nodetype;
  int segnum;
  point_t yval;
  int trnum;
  int parent;
  int left, right;
} node_t;

typedef struct {
  int vnum;
  int next;
  int prev;
} monchain_t;

typedef struct {
  point_t pt;
  int vnext[4]; // next vertices for the 4 chains
  int vpos[4]; // position of v in the 4 chains
  int nextfree;
} vertexchain_t;

struct global_s {
  int nseg;
};

/**************************************************************************************************/

#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#define MIN(a, b) (((a) < (b)) ? (a) : (b))

#define CROSS(v0, v1, v2) (((v1).x - (v0).x)*((v2).y - (v0).y) -        \
                           ((v1).y - (v0).y)*((v2).x - (v0).x))

#define DOT(v0, v1) ((v0).x * (v1).x + (v0).y * (v1).y)

#define MODULO_NEXT(v0, n) (((v0) - 1) % (n) + 1)

#define FP_EQUAL(s, t) (fabs(s - t) <= C_EPS)

#define CROSS_SINE(v0, v1) ((v0).x * (v1).y - (v1).x * (v0).y)
#define LENGTH(v0) (sqrt((v0).x * (v0).x + (v0).y * (v0).y))

/**************************************************************************************************/

/* Global variables */

static node_t qs[QSIZE]; // Query structure
static trap_t tr[TRSIZE]; // Trapezoid structure
static segment_t seg[SEGSIZE]; // Segment table

struct global_s global;

static int choose_idx;
static int permute[SEGSIZE];

static int q_idx;
static int tr_idx;

/* Table to hold all the monotone polygons . Each monotone polygon is
 * a circularly linked list */
static monchain_t mchain[TRSIZE];

/* chain init. information. This is used to decide which monotone
 * polygon to split if there are several other polygons touching at
 * the same vertex */
static vertexchain_t vert[SEGSIZE];

static int mon[SEGSIZE]; /* contains position of any vertex in the monotone chain for the polygon */
static int visited[TRSIZE];
static int chain_idx, op_idx, mon_idx;

/**************************************************************************************************/

/* Generate a random permutation of the segments 1..n
 */
int
generate_random_ordering(int n)
{
  // Global: choose_idx, permute

  register int i;
  int m, st[SEGSIZE], *p;

  choose_idx = 1;

  for (i = 0; i <= n; i++)
    st[i] = i;

  p = st;
  for (i = 1; i <= n; i++, p++) {
    m = lrand48() % (n + 1 - i) + 1;
    permute[i] = p[m];
    if (m != 1)
      p[m] = p[1];
  }
  return 0;
}

/* Return the next segment in the generated random ordering of all the segments in S */
int
choose_segment()
{
#ifdef DEBUG
  fprintf(stderr, "choose_segment: %d\n", permute[choose_idx]);
#endif

  return permute[choose_idx++];
}

int
inserted(int segnum, int whichpt)
{
  // Global: whichpt

  int n1, n2;

  n1 = segnum % global.nseg + 1; /* next seg. */
  n2 = (segnum - 1 + global.nseg - 1) %global.nseg + 1; /* prev. */

  if (whichpt == FIRSTPT)
    return seg[n2].is_inserted;
  else
    return seg[n1].is_inserted;
}

// Get log*n for given n
int
math_logstar_n(int n)
{
  register int i;
  double v;

  for (i = 0, v = (double) n; v >= 1; i++)
    v = log2(v);

  return (i - 1);
}

int
math_N(int n, int h)
{
  register int i;
  double v;

  for (i = 0, v = (int) n; i < h; i++)
    v = log2(v);

  return (int) ceil((double) 1.0 * n / v);
}

/**************************************************************************************************/

/* Return a new node to be added into the query tree */
int
newnode()
{
  // Global:

  if (q_idx < QSIZE)
    return q_idx++;
  else {
    fprintf (stderr, "newnode: Query-table overflow\n");
    return -1;
  }
}

/* Return a free trapezoid */
int
newtrap()
{
  // Global: tr_idx, tr

  if (tr_idx < TRSIZE) {
    tr[tr_idx].lseg = -1;
    tr[tr_idx].rseg = -1;
    tr[tr_idx].state = ST_VALID;
    return tr_idx++;
  } else {
    fprintf(stderr, "newtrap: Trapezoid-table overflow\n");
    return -1;
  }
}

/* Return the maximum of the two points into the yval structure */
int
_max(point_t * yval, point_t * v0, point_t * v1)
{
  if (v0->y > v1->y + C_EPS)
    *yval = *v0;
  else if (FP_EQUAL(v0->y, v1->y)) {
    if (v0->x > v1->x + C_EPS)
      *yval = *v0;
    else
      *yval = *v1;
  } else
    *yval = *v1;

  return 0;
}

/* Return the minimum of the two points into the yval structure */
int
_min(point_t * yval, point_t * v0, point_t * v1)
{
  if (v0->y < v1->y - C_EPS)
    *yval = *v0;
  else if (FP_EQUAL(v0->y, v1->y)) {
    if (v0->x < v1->x)
      *yval = *v0;
    else
      *yval = *v1;
  } else
    *yval = *v1;

  return 0;
}

int
_greater_than(point_t * v0, point_t * v1)
{
  if (v0->y > v1->y + C_EPS)
    return true;
  else if (v0->y < v1->y - C_EPS)
    return false;
  else
    return (v0->x > v1->x);
}

int
_equal_to(point_t * v0, point_t * v1)
{
  return (FP_EQUAL(v0->y, v1->y) && FP_EQUAL(v0->x, v1->x));
}

int
_greater_than_equal_to(point_t * v0, point_t * v1)
{
  if (v0->y > v1->y + C_EPS)
    return true;
  else if (v0->y < v1->y - C_EPS)
    return false;
  else
    return (v0->x >= v1->x);
}

int
_less_than(point_t * v0, point_t * v1)
{
  if (v0->y < v1->y - C_EPS)
    return true;
  else if (v0->y > v1->y + C_EPS)
    return false;
  else
    return (v0->x < v1->x);
}

/* Initilialise the query structure (Q) and the trapezoid table (T)
 * when the first segment is added to start the trapezoidation
 */
int
init_query_structure(int segnum)
{
  // Global: seg, tr, qs

  int i1, i2, i3, i4, i5, i6, i7, root;
  int t1, t2, t3, t4;
  segment_t *s = &seg[segnum];

  memset((void *) tr, 0, sizeof(tr));
  memset((void *) qs, 0, sizeof(qs));

  i1 = newnode();
  qs[i1].nodetype = T_Y;
  _max(&qs[i1].yval, &s->v0, &s->v1); /* root */
  root = i1;

  qs[i1].right = i2 = newnode();
  qs[i2].nodetype = T_SINK;
  qs[i2].parent = i1;

  qs[i1].left = i3 = newnode();
  qs[i3].nodetype = T_Y;
  _min(&qs[i3].yval, &s->v0, &s->v1); /* root */
  qs[i3].parent = i1;

  qs[i3].left = i4 = newnode();
  qs[i4].nodetype = T_SINK;
  qs[i4].parent = i3;

  qs[i3].right = i5 = newnode();
  qs[i5].nodetype = T_X;
  qs[i5].segnum = segnum;
  qs[i5].parent = i3;

  qs[i5].left = i6 = newnode();
  qs[i6].nodetype = T_SINK;
  qs[i6].parent = i5;

  qs[i5].right = i7 = newnode();
  qs[i7].nodetype = T_SINK;
  qs[i7].parent = i5;

  t1 = newtrap(); /* middle left */
  t2 = newtrap(); /* middle right */
  t3 = newtrap(); /* bottom-most */
  t4 = newtrap(); /* topmost */

  tr[t1].hi = tr[t2].hi = tr[t4].lo = qs[i1].yval;
  tr[t1].lo = tr[t2].lo = tr[t3].hi = qs[i3].yval;
  tr[t4].hi.y = (double) (INFINITY);
  tr[t4].hi.x = (double) (INFINITY);
  tr[t3].lo.y = (double) -1 * (INFINITY);
  tr[t3].lo.x = (double) -1 * (INFINITY);
  tr[t1].rseg = tr[t2].lseg = segnum;
  tr[t1].u0 = tr[t2].u0 = t4;
  tr[t1].d0 = tr[t2].d0 = t3;
  tr[t4].d0 = tr[t3].u0 = t1;
  tr[t4].d1 = tr[t3].u1 = t2;

  tr[t1].sink = i6;
  tr[t2].sink = i7;
  tr[t3].sink = i4;
  tr[t4].sink = i2;

  tr[t1].state = tr[t2].state = ST_VALID;
  tr[t3].state = tr[t4].state = ST_VALID;

  qs[i2].trnum = t4;
  qs[i4].trnum = t3;
  qs[i6].trnum = t1;
  qs[i7].trnum = t2;

  s->is_inserted = true;
  return root;
}

/* Retun true if the vertex v is to the left of line segment no.
 * segnum
 */
int
is_left_of(int segnum, point_t * v)
{
  // Global: seg

  segment_t *s = &seg[segnum];
  double area;

  if (_greater_than(&s->v1, &s->v0)) { /* seg. going upwards */
    if (FP_EQUAL(s->v1.y, v->y)) {
      if (v->x < s->v1.x)
        area = 1.0;
      else
        area = -1.0;
    } else if (FP_EQUAL(s->v0.y, v->y)) {
      if (v->x < s->v0.x)
        area = 1.0;
      else
        area = -1.0;
    } else
      area = CROSS(s->v0, s->v1, (*v));
  } else { /* v0 > v1 */
    if (FP_EQUAL(s->v1.y, v->y)) {
      if (v->x < s->v1.x)
        area = 1.0;
      else
        area = -1.0;
    } else if (FP_EQUAL(s->v0.y, v->y)) {
      if (v->x < s->v0.x)
        area = 1.0;
      else
        area = -1.0;
    } else
      area = CROSS(s->v1, s->v0, (*v));
  }

  if (area > 0.0)
    return true;
  else
    return false;
}

int
is_collinear(int segnum, point_t * v, int is_swapped)
{
  // Global: global, seg

  int n;

  /* First check if the endpoint is already inserted */
  if (!is_swapped)
    n = MODULO_NEXT(segnum + 1, global.nseg);
  else if ((n = segnum - 1) == 0)
    n = 1;

  return seg[n].is_inserted;
}

/* This is query routine which determines which trapezoid does the
 * point v lie in. The return value is the trapezoid number
 */
int
locate_endpoint(point_t * v, point_t * vo, int r)
{
  // Global: qs, rptr, T_X, T_Y

  node_t *rptr = &qs[r];

  switch (rptr->nodetype) {
  case T_SINK:
    return rptr->trnum;

  case T_Y:
    if (_greater_than(v, &rptr->yval)) /* above */
      return locate_endpoint(v, vo, rptr->right);
    else if (_equal_to(v, &rptr->yval)) { /* the point is already */
      /* inserted. */
      if (_greater_than(vo, &rptr->yval)) /* above */
        return locate_endpoint(v, vo, rptr->right);
      else
        return locate_endpoint(v, vo, rptr->left); /* below */
    } else
      return locate_endpoint(v, vo, rptr->left); /* below */

  case T_X:
    if (_equal_to(v, &seg[rptr->segnum].v0) || _equal_to(v, &seg[rptr->segnum].v1)) {
      if (FP_EQUAL(v->y, vo->y)) {  /* horizontal segment */

        if (vo->x < v->x)
          return locate_endpoint(v, vo, rptr->left); /* left */
        else
          return locate_endpoint(v, vo, rptr->right); /* right */
      }

      else if (is_left_of(rptr->segnum, vo))
        return locate_endpoint(v, vo, rptr->left); /* left */
      else
        return locate_endpoint(v, vo, rptr->right); /* right */
    }
    else if (is_left_of(rptr->segnum, v))
      return locate_endpoint(v, vo, rptr->left); /* left */
    else
      return locate_endpoint(v, vo, rptr->right); /* right */

  default:
    fprintf(stderr, "Haggu !!!!!\n");
    return 0;
  }
}

/* Thread in the segment into the existing trapezoidation. The
 * limiting trapezoids are given by tfirst and tlast (which are the
 * trapezoids containing the two endpoints of the segment
 */
int
merge_trapezoids(int segnum, int tfirst, int tlast, int side)
{
  // Global: tfirst

  int t, tnext, cond;
  int ptnext;

  /* First merge polys on the LHS */
  t = tfirst;
  while ((t > 0) && _greater_than_equal_to(&tr[t].lo, &tr[tlast].lo)) {
    if (side == S_LEFT)
      cond = ((((tnext = tr[t].d0) > 0) && (tr[tnext].rseg == segnum)) ||
              (((tnext = tr[t].d1) > 0) && (tr[tnext].rseg == segnum)));
    else
      cond = ((((tnext = tr[t].d0) > 0) && (tr[tnext].lseg == segnum)) ||
              (((tnext = tr[t].d1) > 0) && (tr[tnext].lseg == segnum)));

    if (cond) {
      if ((tr[t].lseg == tr[tnext].lseg) && (tr[t].rseg == tr[tnext].rseg)) { /* good neighbours */
        /* merge them */
        /* Use the upper node as the new node i.e. t */

        ptnext = qs[tr[tnext].sink].parent;

        if (qs[ptnext].left == tr[tnext].sink)
          qs[ptnext].left = tr[t].sink;
        else
          qs[ptnext].right = tr[t].sink; /* redirect parent */

        /* Change the upper neighbours of the lower trapezoids */

        if ((tr[t].d0 = tr[tnext].d0) > 0) {
          if (tr[tr[t].d0].u0 == tnext)
            tr[tr[t].d0].u0 = t;
          else if (tr[tr[t].d0].u1 == tnext)
            tr[tr[t].d0].u1 = t;
        }

        if ((tr[t].d1 = tr[tnext].d1) > 0) {
          if (tr[tr[t].d1].u0 == tnext)
            tr[tr[t].d1].u0 = t;
          else if (tr[tr[t].d1].u1 == tnext)
            tr[tr[t].d1].u1 = t;
        }

        tr[t].lo = tr[tnext].lo;
        tr[tnext].state = ST_INVALID; /* invalidate the lower */
        /* trapezium */
      } else /* not good neighbours */
        t = tnext;
    } else /* do not satisfy the outer if */
      t = tnext;

  } /* end-while */

  return 0;
}

/* Add in the new segment into the trapezoidation and update Q and T structures
 */
int
add_segment(int segnum)
{
  // Global: seg, tr

  segment_t s;
  int tu, tl, sk, tfirst, tlast, tnext;
  int tfirstr, tlastr, tfirstl, tlastl;
  int i1, i2, t, tn;
  point_t vper;
  point_t tpt;
  int tritop = 0;
  int tribot = 0, is_swapped = 0;
  int tmptriseg;

  s = seg[segnum];
  if (_greater_than(&s.v1, &s.v0)) { /* Get higher vertex in v0 */
    int tmp;
    tpt = s.v0;
    s.v0 = s.v1;
    s.v1 = tpt;
    tmp = s.root0;
    s.root0 = s.root1;
    s.root1 = tmp;
    is_swapped = true;
  }

  if ((is_swapped) ? !inserted(segnum, LASTPT) : !inserted(segnum, FIRSTPT)) { /* insert v0 in the tree */
    int tmp_d;

    tu = locate_endpoint(&s.v0, &s.v1, s.root0);
    tl = newtrap(); /* tl is the new lower trapezoid */
    tr[tl].state = ST_VALID;
    tr[tl] = tr[tu];
    tr[tu].lo.y = tr[tl].hi.y = s.v0.y;
    tr[tu].lo.x = tr[tl].hi.x = s.v0.x;
    tr[tu].d0 = tl;
    tr[tu].d1 = 0;
    tr[tl].u0 = tu;
    tr[tl].u1 = 0;

    if (((tmp_d = tr[tl].d0) > 0) && (tr[tmp_d].u0 == tu))
      tr[tmp_d].u0 = tl;
    if (((tmp_d = tr[tl].d0) > 0) && (tr[tmp_d].u1 == tu))
      tr[tmp_d].u1 = tl;

    if (((tmp_d = tr[tl].d1) > 0) && (tr[tmp_d].u0 == tu))
      tr[tmp_d].u0 = tl;
    if (((tmp_d = tr[tl].d1) > 0) && (tr[tmp_d].u1 == tu))
      tr[tmp_d].u1 = tl;

    /* Now update the query structure and obtain the sinks for the */
    /* two trapezoids */

    i1 = newnode(); /* Upper trapezoid sink */
    i2 = newnode(); /* Lower trapezoid sink */
    sk = tr[tu].sink;

    qs[sk].nodetype = T_Y;
    qs[sk].yval = s.v0;
    qs[sk].segnum = segnum; /* not really reqd ... maybe later */
    qs[sk].left = i2;
    qs[sk].right = i1;

    qs[i1].nodetype = T_SINK;
    qs[i1].trnum = tu;
    qs[i1].parent = sk;

    qs[i2].nodetype = T_SINK;
    qs[i2].trnum = tl;
    qs[i2].parent = sk;

    tr[tu].sink = i1;
    tr[tl].sink = i2;
    tfirst = tl;
  }
  else { /* v0 already present */
    /* Get the topmost intersecting trapezoid */
    vper.x = s.v0.x + EPS * (s.v1.x - s.v0.x);
    vper.y = s.v0.y + EPS * (s.v1.y - s.v0.y);
    tfirst = locate_endpoint(&s.v0, &s.v1, s.root0);
    tritop = 1;
  }

  if ((is_swapped) ? !inserted(segnum, FIRSTPT) : !inserted(segnum, LASTPT)) { /* insert v1 in the tree */
    int tmp_d;

    tu = locate_endpoint(&s.v1, &s.v0, s.root1);

    tl = newtrap(); /* tl is the new lower trapezoid */
    tr[tl].state = ST_VALID;
    tr[tl] = tr[tu];
    tr[tu].lo.y = tr[tl].hi.y = s.v1.y;
    tr[tu].lo.x = tr[tl].hi.x = s.v1.x;
    tr[tu].d0 = tl;
    tr[tu].d1 = 0;
    tr[tl].u0 = tu;
    tr[tl].u1 = 0;

    if (((tmp_d = tr[tl].d0) > 0) && (tr[tmp_d].u0 == tu))
      tr[tmp_d].u0 = tl;
    if (((tmp_d = tr[tl].d0) > 0) && (tr[tmp_d].u1 == tu))
      tr[tmp_d].u1 = tl;

    if (((tmp_d = tr[tl].d1) > 0) && (tr[tmp_d].u0 == tu))
      tr[tmp_d].u0 = tl;
    if (((tmp_d = tr[tl].d1) > 0) && (tr[tmp_d].u1 == tu))
      tr[tmp_d].u1 = tl;

    /* Now update the query structure and obtain the sinks for the */
    /* two trapezoids */

    i1 = newnode(); /* Upper trapezoid sink */
    i2 = newnode(); /* Lower trapezoid sink */
    sk = tr[tu].sink;

    qs[sk].nodetype = T_Y;
    qs[sk].yval = s.v1;
    qs[sk].segnum = segnum; /* not really reqd ... maybe later */
    qs[sk].left = i2;
    qs[sk].right = i1;

    qs[i1].nodetype = T_SINK;
    qs[i1].trnum = tu;
    qs[i1].parent = sk;

    qs[i2].nodetype = T_SINK;
    qs[i2].trnum = tl;
    qs[i2].parent = sk;

    tr[tu].sink = i1;
    tr[tl].sink = i2;
    tlast = tu;
  } else { /* v1 already present */
    /* Get the lowermost intersecting trapezoid */
    vper.x = s.v1.x + EPS * (s.v0.x - s.v1.x);
    vper.y = s.v1.y + EPS * (s.v0.y - s.v1.y);
    tlast = locate_endpoint(&s.v1, &s.v0, s.root1);
    tribot = 1;
  }

  /* Thread the segment into the query tree creating a new X-node */
  /* First, split all the trapezoids which are intersected by s into */
  /* two */

  t = tfirst; /* topmost trapezoid */

  while ((t > 0) && _greater_than_equal_to(&tr[t].lo, &tr[tlast].lo)) {
    /* traverse from top to bot */
    int t_sav, tn_sav;
    sk = tr[t].sink;
    i1 = newnode(); /* left trapezoid sink */
    i2 = newnode(); /* right trapezoid sink */

    qs[sk].nodetype = T_X;
    qs[sk].segnum = segnum;
    qs[sk].left = i1;
    qs[sk].right = i2;

    qs[i1].nodetype = T_SINK; /* left trapezoid (use existing one) */
    qs[i1].trnum = t;
    qs[i1].parent = sk;

    qs[i2].nodetype = T_SINK; /* right trapezoid (allocate new) */
    qs[i2].trnum = tn = newtrap();
    tr[tn].state = ST_VALID;
    qs[i2].parent = sk;

    if (t == tfirst)
      tfirstr = tn;
    if (_equal_to(&tr[t].lo, &tr[tlast].lo))
      tlastr = tn;

    tr[tn] = tr[t];
    tr[t].sink = i1;
    tr[tn].sink = i2;
    t_sav = t;
    tn_sav = tn;

    /* error */

    if ((tr[t].d0 <= 0) && (tr[t].d1 <= 0)) { /* case cannot arise */
      fprintf(stderr, "add_segment: error\n");
      break;
    }

    /* only one trapezoid below. partition t into two and make the */
    /* two resulting trapezoids t and tn as the upper neighbours of */
    /* the sole lower trapezoid */

    else if ((tr[t].d0 > 0) && (tr[t].d1 <= 0)) {
      /* Only one trapezoid below */
      if ((tr[t].u0 > 0) && (tr[t].u1 > 0)) {
        /* continuation of a chain from abv. */
        if (tr[t].usave > 0) { /* three upper neighbours */
          if (tr[t].uside == S_LEFT) {
            tr[tn].u0 = tr[t].u1;
            tr[t].u1 = -1;
            tr[tn].u1 = tr[t].usave;

            tr[tr[t].u0].d0 = t;
            tr[tr[tn].u0].d0 = tn;
            tr[tr[tn].u1].d0 = tn;
          } else { /* intersects in the right */
            tr[tn].u1 = -1;
            tr[tn].u0 = tr[t].u1;
            tr[t].u1 = tr[t].u0;
            tr[t].u0 = tr[t].usave;

            tr[tr[t].u0].d0 = t;
            tr[tr[t].u1].d0 = t;
            tr[tr[tn].u0].d0 = tn;
          }

          tr[t].usave = tr[tn].usave = 0;
        } else { /* No usave.... simple case */
          tr[tn].u0 = tr[t].u1;
          tr[t].u1 = tr[tn].u1 = -1;
          tr[tr[tn].u0].d0 = tn;
        }
      } else { /* fresh seg. or upward cusp */
        int tmp_u = tr[t].u0;
        int td0, td1;
        if (((td0 = tr[tmp_u].d0) > 0) && ((td1 = tr[tmp_u].d1) > 0)) { /* upward cusp */
          if ((tr[td0].rseg > 0) && !is_left_of(tr[td0].rseg, &s.v1)) {
            tr[t].u0 = tr[t].u1 = tr[tn].u1 = -1;
            tr[tr[tn].u0].d1 = tn;
          } else { /* cusp going leftwards */
            tr[tn].u0 = tr[tn].u1 = tr[t].u1 = -1;
            tr[tr[t].u0].d0 = t;
          }
        } else { /* fresh segment */
          tr[tr[t].u0].d0 = t;
          tr[tr[t].u0].d1 = tn;
        }
      }

      if (FP_EQUAL(tr[t].lo.y, tr[tlast].lo.y) &&
          FP_EQUAL(tr[t].lo.x, tr[tlast].lo.x) && tribot) { /* bottom forms a triangle */

        if (is_swapped) {
          tmptriseg = segnum - 1;
          if (tmptriseg == 0)
            tmptriseg = global.nseg;
        } else
          tmptriseg = MODULO_NEXT(segnum + 1, global.nseg);

        if ((tmptriseg > 0) && is_left_of(tmptriseg, &s.v0)) {
          /* L-R downward cusp */
          tr[tr[t].d0].u0 = t;
          tr[tn].d0 = tr[tn].d1 = -1;
        } else {
          /* R-L downward cusp */
          tr[tr[tn].d0].u1 = tn;
          tr[t].d0 = tr[t].d1 = -1;
        }
      } else {
        if ((tr[tr[t].d0].u0 > 0) && (tr[tr[t].d0].u1 > 0)) {
          if (tr[tr[t].d0].u0 == t) { /* passes thru LHS */
            tr[tr[t].d0].usave = tr[tr[t].d0].u1;
            tr[tr[t].d0].uside = S_LEFT;
          } else {
            tr[tr[t].d0].usave = tr[tr[t].d0].u0;
            tr[tr[t].d0].uside = S_RIGHT;
          }
        }
        tr[tr[t].d0].u0 = t;
        tr[tr[t].d0].u1 = tn;
      }

      t = tr[t].d0;
    } else if ((tr[t].d0 <= 0) && (tr[t].d1 > 0)) { /* Only one trapezoid below */
      if ((tr[t].u0 > 0) && (tr[t].u1 > 0)) { /* continuation of a chain from abv. */
        if (tr[t].usave > 0) { /* three upper neighbours */
          if (tr[t].uside == S_LEFT) {
            tr[tn].u0 = tr[t].u1;
            tr[t].u1 = -1;
            tr[tn].u1 = tr[t].usave;

            tr[tr[t].u0].d0 = t;
            tr[tr[tn].u0].d0 = tn;
            tr[tr[tn].u1].d0 = tn;
          } else { /* intersects in the right */
            tr[tn].u1 = -1;
            tr[tn].u0 = tr[t].u1;
            tr[t].u1 = tr[t].u0;
            tr[t].u0 = tr[t].usave;

            tr[tr[t].u0].d0 = t;
            tr[tr[t].u1].d0 = t;
            tr[tr[tn].u0].d0 = tn;
          }

          tr[t].usave = tr[tn].usave = 0;
        } else { /* No usave.... simple case */
          tr[tn].u0 = tr[t].u1;
          tr[t].u1 = tr[tn].u1 = -1;
          tr[tr[tn].u0].d0 = tn;
        }
      } else { /* fresh seg. or upward cusp */
        int tmp_u = tr[t].u0;
        int td0, td1;
        if (((td0 = tr[tmp_u].d0) > 0) && ((td1 = tr[tmp_u].d1) > 0)) { /* upward cusp */
          if ((tr[td0].rseg > 0) && !is_left_of(tr[td0].rseg, &s.v1)) {
            tr[t].u0 = tr[t].u1 = tr[tn].u1 = -1;
            tr[tr[tn].u0].d1 = tn;
          } else {
            tr[tn].u0 = tr[tn].u1 = tr[t].u1 = -1;
            tr[tr[t].u0].d0 = t;
          }
        } else { /* fresh segment */
          tr[tr[t].u0].d0 = t;
          tr[tr[t].u0].d1 = tn;
        }
      }

      if (FP_EQUAL(tr[t].lo.y, tr[tlast].lo.y) &&
          FP_EQUAL(tr[t].lo.x, tr[tlast].lo.x) && tribot) { /* bottom forms a triangle */
        int tmpseg;
        if (is_swapped) {
          tmpseg = segnum - 1;
          if (tmpseg == 0)
            tmpseg = global.nseg;
        } else
          tmpseg = MODULO_NEXT(segnum + 1, global.nseg);

        if ((tmpseg > 0) && is_left_of(tmpseg, &s.v0)) {
          /* L-R downward cusp */
          tr[tr[t].d1].u0 = t;
          tr[tn].d0 = tr[tn].d1 = -1;
        } else {
          /* R-L downward cusp */
          tr[tr[tn].d1].u1 = tn;
          tr[t].d0 = tr[t].d1 = -1;
        }
      } else {
        if ((tr[tr[t].d1].u0 > 0) && (tr[tr[t].d1].u1 > 0)) {
          if (tr[tr[t].d1].u0 == t) { /* passes thru LHS */
            tr[tr[t].d1].usave = tr[tr[t].d1].u1;
            tr[tr[t].d1].uside = S_LEFT;
          } else {
            tr[tr[t].d1].usave = tr[tr[t].d1].u0;
            tr[tr[t].d1].uside = S_RIGHT;
          }
        }
        tr[tr[t].d1].u0 = t;
        tr[tr[t].d1].u1 = tn;
      }

      t = tr[t].d1;
    } else {
      /* two trapezoids below. Find out which one is intersected by */
      /* this segment and proceed down that one */
      double y0, yt;
      point_t tmppt;
      int i_d0;

      i_d0 = false;
      if (FP_EQUAL(tr[t].lo.y, s.v0.y)) {
        if (tr[t].lo.x > s.v0.x)
          i_d0 = true;
      } else {
        tmppt.y = y0 = tr[t].lo.y;
        yt = (y0 - s.v0.y) / (s.v1.y - s.v0.y);
        tmppt.x = s.v0.x + yt * (s.v1.x - s.v0.x);

        if (_less_than(&tmppt, &tr[t].lo))
          i_d0 = true;
      }

      /* check continuity from the top so that the lower-neighbour */
      /* values are properly filled for the upper trapezoid */

      if ((tr[t].u0 > 0) && (tr[t].u1 > 0)) { /* continuation of a chain from abv. */
        if (tr[t].usave > 0) { /* three upper neighbours */
          if (tr[t].uside == S_LEFT) {
            tr[tn].u0 = tr[t].u1;
            tr[t].u1 = -1;
            tr[tn].u1 = tr[t].usave;

            tr[tr[t].u0].d0 = t;
            tr[tr[tn].u0].d0 = tn;
            tr[tr[tn].u1].d0 = tn;
          } else { /* intersects in the right */
            tr[tn].u1 = -1;
            tr[tn].u0 = tr[t].u1;
            tr[t].u1 = tr[t].u0;
            tr[t].u0 = tr[t].usave;

            tr[tr[t].u0].d0 = t;
            tr[tr[t].u1].d0 = t;
            tr[tr[tn].u0].d0 = tn;
          }

          tr[t].usave = tr[tn].usave = 0;
        } else { /* No usave.... simple case */
          tr[tn].u0 = tr[t].u1;
          tr[tn].u1 = -1;
          tr[t].u1 = -1;
          tr[tr[tn].u0].d0 = tn;
        }
      } else { /* fresh seg. or upward cusp */
        int tmp_u = tr[t].u0;
        int td0, td1;
        if (((td0 = tr[tmp_u].d0) > 0) && ((td1 = tr[tmp_u].d1) > 0)) { /* upward cusp */
          if ((tr[td0].rseg > 0) && !is_left_of(tr[td0].rseg, &s.v1)) {
            tr[t].u0 = tr[t].u1 = tr[tn].u1 = -1;
            tr[tr[tn].u0].d1 = tn;
          } else {
            tr[tn].u0 = tr[tn].u1 = tr[t].u1 = -1;
            tr[tr[t].u0].d0 = t;
          }
        } else { /* fresh segment */
          tr[tr[t].u0].d0 = t;
          tr[tr[t].u0].d1 = tn;
        }
      }

      if (FP_EQUAL(tr[t].lo.y, tr[tlast].lo.y) &&
          FP_EQUAL(tr[t].lo.x, tr[tlast].lo.x) && tribot) {
        /* this case arises only at the lowest trapezoid.. i.e.
           tlast, if the lower endpoint of the segment is
           already inserted in the structure */

        tr[tr[t].d0].u0 = t;
        tr[tr[t].d0].u1 = -1;
        tr[tr[t].d1].u0 = tn;
        tr[tr[t].d1].u1 = -1;

        tr[tn].d0 = tr[t].d1;
        tr[t].d1 = tr[tn].d1 = -1;

        tnext = tr[t].d1;
      } else if (i_d0) {
        /* intersecting d0 */
        tr[tr[t].d0].u0 = t;
        tr[tr[t].d0].u1 = tn;
        tr[tr[t].d1].u0 = tn;
        tr[tr[t].d1].u1 = -1;

        /* new code to determine the bottom neighbours of the */
        /* newly partitioned trapezoid */

        tr[t].d1 = -1;

        tnext = tr[t].d0;
      } else { /* intersecting d1 */
        tr[tr[t].d0].u0 = t;
        tr[tr[t].d0].u1 = -1;
        tr[tr[t].d1].u0 = t;
        tr[tr[t].d1].u1 = tn;

        /* new code to determine the bottom neighbours of the */
        /* newly partitioned trapezoid */

        tr[tn].d0 = tr[t].d1;
        tr[tn].d1 = -1;

        tnext = tr[t].d1;
      }

      t = tnext;
    }

    tr[t_sav].rseg = tr[tn_sav].lseg = segnum;
  } /* end-while */

  /* Now combine those trapezoids which share common segments. We can */
  /* use the pointers to the parent to connect these together. This */
  /* works only because all these new trapezoids have been formed */
  /* due to splitting by the segment, and hence have only one parent */

  tfirstl = tfirst;
  tlastl = tlast;
  merge_trapezoids(segnum, tfirstl, tlastl, S_LEFT);
  merge_trapezoids(segnum, tfirstr, tlastr, S_RIGHT);

  seg[segnum].is_inserted = true;
  return 0;
}

/* Update the roots stored for each of the endpoints of the segment.
 * This is done to speed up the location-query for the endpoint when
 * the segment is inserted into the trapezoidation subsequently
 */
int
find_new_roots(int segnum)
{
  // Global:

  segment_t *s = &seg[segnum];
  point_t vper;

  if (s->is_inserted)
    return 0;

  vper.x = s->v0.x + EPS * (s->v1.x - s->v0.x);
  vper.y = s->v0.y + EPS * (s->v1.y - s->v0.y);
  s->root0 = locate_endpoint(&s->v0, &s->v1, s->root0);
  s->root0 = tr[s->root0].sink;

  vper.x = s->v1.x + EPS * (s->v0.x - s->v1.x);
  vper.y = s->v1.y + EPS * (s->v0.y - s->v1.y);
  s->root1 = locate_endpoint(&s->v1, &s->v0, s->root1);
  s->root1 = tr[s->root1].sink;
  return 0;
}

/* Main routine to perform trapezoidation */
void
construct_trapezoids(int nseg, segment_t seg[])
{
  // Global:

  register int i;
  int root, h;

  q_idx = tr_idx = 1;
  /* Add the first segment and get the query structure and trapezoid */
  /* list initialised */
  root = init_query_structure(choose_segment());

#ifdef SIMPLE /* no randomization */

  for (i = 1; i <= nseg; i++)
    seg[i].root0 = seg[i].root1 = root;

  for (i = 2; i <= nseg; i++)
    add_segment(choose_segment());

#else

  for (i = 1; i <= nseg; i++)
    seg[i].root0 = seg[i].root1 = root;

  for (h = 1; h <= math_logstar_n(nseg); h++)
    {
      for (i = math_N(nseg, h - 1) + 1; i <= math_N(nseg, h); i++)
        add_segment(choose_segment());

      /* Find a new root for each of the segment endpoints */
      for (i = 1; i <= nseg; i++)
        find_new_roots(i);
    }

  for (i = math_N(nseg, math_logstar_n(nseg)) + 1; i <= nseg; i++)
    add_segment(choose_segment());

#endif
}

/**************************************************************************************************/

/* Function returns true if the trapezoid lies inside the polygon */
int
inside_polygon(trap_t * t)
{
  int rseg = t->rseg;

  if (t->state == ST_INVALID)
    return 0;

  if ((t->lseg <= 0) || (t->rseg <= 0))
    return 0;

  if (((t->u0 <= 0) && (t->u1 <= 0)) || ((t->d0 <= 0) && (t->d1 <= 0))) /* triangle */
    return (_greater_than(&seg[rseg].v1, &seg[rseg].v0));

  return 0;
}

/* return a new mon structure from the table */
int
newmon()
{
  // Global:
  return ++mon_idx;
}

/* return a new chain element from the table */
int
new_chain_element()
{
  // Global:
  return ++chain_idx;
}

double
get_angle(point_t * vp0, point_t * vpnext, point_t * vp1)
{
  point_t v0, v1;

  v0.x = vpnext->x - vp0->x;
  v0.y = vpnext->y - vp0->y;

  v1.x = vp1->x - vp0->x;
  v1.y = vp1->y - vp0->y;

  if (CROSS_SINE(v0, v1) >= 0) /* sine is positive */
    return DOT(v0, v1) / LENGTH(v0) / LENGTH(v1);
  else
    return (-1.0 * DOT(v0, v1) / LENGTH(v0) / LENGTH(v1) - 2);
}

/* (v0, v1) is the new diagonal to be added to the polygon. Find which
 * chain to use and return the positions of v0 and v1 in p and q */
int
get_vertex_positions(int v0, int v1, int * ip, int * iq)
{
  // Global:
  vertexchain_t *vp0, *vp1;
  register int i;
  double angle, temp;
  int tp, tq;

  vp0 = &vert[v0];
  vp1 = &vert[v1];

  /* p is identified as follows. Scan from (v0, v1) rightwards till
   * you hit the first segment starting from v0. That chain is the
   * chain of our interest */

  angle = -4.0;
  for (i = 0; i < 4; i++)
    {
      if (vp0->vnext[i] <= 0)
        continue;
      if ((temp = get_angle(&vp0->pt, &(vert[vp0->vnext[i]].pt), &vp1->pt)) > angle)
        {
          angle = temp;
          tp = i;
        }
    }

  *ip = tp;

  /* Do similar actions for q */

  angle = -4.0;
  for (i = 0; i < 4; i++)
    {
      if (vp1->vnext[i] <= 0)
        continue;
      if ((temp = get_angle(&vp1->pt, &(vert[vp1->vnext[i]].pt), &vp0->pt)) > angle)
        {
          angle = temp;
          tq = i;
        }
    }

  *iq = tq;

  return 0;
}

/* v0 and v1 are specified in anti-clockwise order with respect to the
 * current monotone polygon mcur. Split the current polygon into two
 * polygons using the diagonal (v0, v1)
 */
int
make_new_monotone_poly(int mcur, int v0, int v1)
{
  // Global:

  int p, q, ip, iq;
  int mnew = newmon();
  int i, j, nf0, nf1;
  vertexchain_t *vp0, *vp1;

  vp0 = &vert[v0];
  vp1 = &vert[v1];

  get_vertex_positions(v0, v1, &ip, &iq);

  p = vp0->vpos[ip];
  q = vp1->vpos[iq];

  /* At this stage, we have got the positions of v0 and v1 in the
   * desired chain. Now modify the linked lists */

  i = new_chain_element(); /* for the new list */
  j = new_chain_element();

  mchain[i].vnum = v0;
  mchain[j].vnum = v1;

  mchain[i].next = mchain[p].next;
  mchain[mchain[p].next].prev = i;
  mchain[i].prev = j;
  mchain[j].next = i;
  mchain[j].prev = mchain[q].prev;
  mchain[mchain[q].prev].next = j;

  mchain[p].next = q;
  mchain[q].prev = p;

  nf0 = vp0->nextfree;
  nf1 = vp1->nextfree;

  vp0->vnext[ip] = v1;

  vp0->vpos[nf0] = i;
  vp0->vnext[nf0] = mchain[mchain[i].next].vnum;
  vp1->vpos[nf1] = j;
  vp1->vnext[nf1] = v0;

  vp0->nextfree++;
  vp1->nextfree++;

#ifdef DEBUG
  fprintf(stderr, "make_poly: mcur = %d, (v0, v1) = (%d, %d)\n", mcur, v0, v1);
  fprintf(stderr, "next posns = (p, q) = (%d, %d)\n", p, q);
#endif

  mon[mcur] = p;
  mon[mnew] = i;
  return mnew;
}

/* recursively visit all the trapezoids */
int
traverse_polygon(int mcur, int trnum, int from, int dir)
{
  // Global: 

  trap_t *t = &tr[trnum];
  int mnew;
  int v0, v1;
  int retval = 0;
  int do_switch = false;

  if ((trnum <= 0) || visited[trnum])
    return 0;

  visited[trnum] = true;

  /* We have much more information available here. */
  /* rseg: goes upwards   */
  /* lseg: goes downwards */

  /* Initially assume that dir = TR_FROM_DN(from the left) */
  /* Switch v0 and v1 if necessary afterwards */

  /* special cases for triangles with cusps at the opposite ends. */
  /* take care of this first */
  if ((t->u0 <= 0) && (t->u1 <= 0)) {
    if ((t->d0 > 0) && (t->d1 > 0)) { /* downward opening triangle */
      v0 = tr[t->d1].lseg;
      v1 = t->lseg;
      if (from == t->d1) {
        do_switch = true;
        mnew = make_new_monotone_poly(mcur, v1, v0);
        traverse_polygon(mcur, t->d1, trnum, TR_FROM_UP);
        traverse_polygon(mnew, t->d0, trnum, TR_FROM_UP);
      } else {
        mnew = make_new_monotone_poly(mcur, v0, v1);
        traverse_polygon(mcur, t->d0, trnum, TR_FROM_UP);
        traverse_polygon(mnew, t->d1, trnum, TR_FROM_UP);
      }
    } else {
      retval = SP_NOSPLIT; /* Just traverse all neighbours */
      traverse_polygon(mcur, t->u0, trnum, TR_FROM_DN);
      traverse_polygon(mcur, t->u1, trnum, TR_FROM_DN);
      traverse_polygon(mcur, t->d0, trnum, TR_FROM_UP);
      traverse_polygon(mcur, t->d1, trnum, TR_FROM_UP);
    }
  } else if ((t->d0 <= 0) && (t->d1 <= 0)) {
    if ((t->u0 > 0) && (t->u1 > 0)) { /* upward opening triangle */
      v0 = t->rseg;
      v1 = tr[t->u0].rseg;
      if (from == t->u1) {
        do_switch = true;
        mnew = make_new_monotone_poly(mcur, v1, v0);
        traverse_polygon(mcur, t->u1, trnum, TR_FROM_DN);
        traverse_polygon(mnew, t->u0, trnum, TR_FROM_DN);
      } else {
        mnew = make_new_monotone_poly(mcur, v0, v1);
        traverse_polygon(mcur, t->u0, trnum, TR_FROM_DN);
        traverse_polygon(mnew, t->u1, trnum, TR_FROM_DN);
      }
    } else {
      retval = SP_NOSPLIT; /* Just traverse all neighbours */
      traverse_polygon(mcur, t->u0, trnum, TR_FROM_DN);
      traverse_polygon(mcur, t->u1, trnum, TR_FROM_DN);
      traverse_polygon(mcur, t->d0, trnum, TR_FROM_UP);
      traverse_polygon(mcur, t->d1, trnum, TR_FROM_UP);
    }
  } else if ((t->u0 > 0) && (t->u1 > 0)) {
    if ((t->d0 > 0) && (t->d1 > 0)) {  /* downward + upward cusps */
      v0 = tr[t->d1].lseg;
      v1 = tr[t->u0].rseg;
      retval = SP_2UP_2DN;
      if (((dir == TR_FROM_DN) && (t->d1 == from)) || ((dir == TR_FROM_UP) && (t->u1 == from))) {
        do_switch = true;
        mnew = make_new_monotone_poly(mcur, v1, v0);
        traverse_polygon(mcur, t->u1, trnum, TR_FROM_DN);
        traverse_polygon(mcur, t->d1, trnum, TR_FROM_UP);
        traverse_polygon(mnew, t->u0, trnum, TR_FROM_DN);
        traverse_polygon(mnew, t->d0, trnum, TR_FROM_UP);
      } else {
        mnew = make_new_monotone_poly(mcur, v0, v1);
        traverse_polygon(mcur, t->u0, trnum, TR_FROM_DN);
        traverse_polygon(mcur, t->d0, trnum, TR_FROM_UP);
        traverse_polygon(mnew, t->u1, trnum, TR_FROM_DN);
        traverse_polygon(mnew, t->d1, trnum, TR_FROM_UP);
      }
    } else { /* only downward cusp */
      if (_equal_to(&t->lo, &seg[t->lseg].v1)) {
        v0 = tr[t->u0].rseg;
        v1 = MODULO_NEXT(t->lseg + 1, global.nseg);
        retval = SP_2UP_LEFT;
        if ((dir == TR_FROM_UP) && (t->u0 == from)) {
          do_switch = true;
          mnew = make_new_monotone_poly(mcur, v1, v0);
          traverse_polygon(mcur, t->u0, trnum, TR_FROM_DN);
          traverse_polygon(mnew, t->d0, trnum, TR_FROM_UP);
          traverse_polygon(mnew, t->u1, trnum, TR_FROM_DN);
          traverse_polygon(mnew, t->d1, trnum, TR_FROM_UP);
        } else {
          mnew = make_new_monotone_poly(mcur, v0, v1);
          traverse_polygon(mcur, t->u1, trnum, TR_FROM_DN);
          traverse_polygon(mcur, t->d0, trnum, TR_FROM_UP);
          traverse_polygon(mcur, t->d1, trnum, TR_FROM_UP);
          traverse_polygon(mnew, t->u0, trnum, TR_FROM_DN);
        }
      } else {
        v0 = t->rseg;
        v1 = tr[t->u0].rseg;
        retval = SP_2UP_RIGHT;
        if ((dir == TR_FROM_UP) && (t->u1 == from)) {
          do_switch = true;
          mnew = make_new_monotone_poly(mcur, v1, v0);
          traverse_polygon(mcur, t->u1, trnum, TR_FROM_DN);
          traverse_polygon(mnew, t->d1, trnum, TR_FROM_UP);
          traverse_polygon(mnew, t->d0, trnum, TR_FROM_UP);
          traverse_polygon(mnew, t->u0, trnum, TR_FROM_DN);
        } else {
          mnew = make_new_monotone_poly(mcur, v0, v1);
          traverse_polygon(mcur, t->u0, trnum, TR_FROM_DN);
          traverse_polygon(mcur, t->d0, trnum, TR_FROM_UP);
          traverse_polygon(mcur, t->d1, trnum, TR_FROM_UP);
          traverse_polygon(mnew, t->u1, trnum, TR_FROM_DN);
        }
      }
    }
  } else {
    if ((t->d0 > 0) && (t->d1 > 0)) { /* only upward cusp */
      if (_equal_to(&t->hi, &seg[t->lseg].v0)) {
        v0 = tr[t->d1].lseg;
        v1 = t->lseg;
        retval = SP_2DN_LEFT;
        if (!((dir == TR_FROM_DN) && (t->d0 == from))) {
          do_switch = true;
          mnew = make_new_monotone_poly(mcur, v1, v0);
          traverse_polygon(mcur, t->u1, trnum, TR_FROM_DN);
          traverse_polygon(mcur, t->d1, trnum, TR_FROM_UP);
          traverse_polygon(mcur, t->u0, trnum, TR_FROM_DN);
          traverse_polygon(mnew, t->d0, trnum, TR_FROM_UP);
        } else {
          mnew = make_new_monotone_poly(mcur, v0, v1);
          traverse_polygon(mcur, t->d0, trnum, TR_FROM_UP);
          traverse_polygon(mnew, t->u0, trnum, TR_FROM_DN);
          traverse_polygon(mnew, t->u1, trnum, TR_FROM_DN);
          traverse_polygon(mnew, t->d1, trnum, TR_FROM_UP);
        }
      } else {
        v0 = tr[t->d1].lseg;
        v1 = MODULO_NEXT(t->rseg + 1, global.nseg);
        retval = SP_2DN_RIGHT;
        if ((dir == TR_FROM_DN) && (t->d1 == from)) {
          do_switch = true;
          mnew = make_new_monotone_poly(mcur, v1, v0);
          traverse_polygon(mcur, t->d1, trnum, TR_FROM_UP);
          traverse_polygon(mnew, t->u1, trnum, TR_FROM_DN);
          traverse_polygon(mnew, t->u0, trnum, TR_FROM_DN);
          traverse_polygon(mnew, t->d0, trnum, TR_FROM_UP);
        } else {
          mnew = make_new_monotone_poly(mcur, v0, v1);
          traverse_polygon(mcur, t->u0, trnum, TR_FROM_DN);
          traverse_polygon(mcur, t->d0, trnum, TR_FROM_UP);
          traverse_polygon(mcur, t->u1, trnum, TR_FROM_DN);
          traverse_polygon(mnew, t->d1, trnum, TR_FROM_UP);
        }
      }
    } else { /* no cusp */
      if (_equal_to(&t->hi, &seg[t->lseg].v0) && _equal_to(&t->lo, &seg[t->rseg].v0)) {
        v0 = t->rseg;
        v1 = t->lseg;
        retval = SP_SIMPLE_LRDN;
        if (dir == TR_FROM_UP) {
          do_switch = true;
          mnew = make_new_monotone_poly(mcur, v1, v0);
          traverse_polygon(mcur, t->u0, trnum, TR_FROM_DN);
          traverse_polygon(mcur, t->u1, trnum, TR_FROM_DN);
          traverse_polygon(mnew, t->d1, trnum, TR_FROM_UP);
          traverse_polygon(mnew, t->d0, trnum, TR_FROM_UP);
        } else {
          mnew = make_new_monotone_poly(mcur, v0, v1);
          traverse_polygon(mcur, t->d1, trnum, TR_FROM_UP);
          traverse_polygon(mcur, t->d0, trnum, TR_FROM_UP);
          traverse_polygon(mnew, t->u0, trnum, TR_FROM_DN);
          traverse_polygon(mnew, t->u1, trnum, TR_FROM_DN);
        }
      } else if (_equal_to(&t->hi, &seg[t->rseg].v1) && _equal_to(&t->lo, &seg[t->lseg].v1)) {
        v0 = MODULO_NEXT(t->rseg + 1, global.nseg);
        v1 = MODULO_NEXT(t->lseg + 1, global.nseg);
        retval = SP_SIMPLE_LRUP;
        if (dir == TR_FROM_UP) {
          do_switch = true;
          mnew = make_new_monotone_poly(mcur, v1, v0);
          traverse_polygon(mcur, t->u0, trnum, TR_FROM_DN);
          traverse_polygon(mcur, t->u1, trnum, TR_FROM_DN);
          traverse_polygon(mnew, t->d1, trnum, TR_FROM_UP);
          traverse_polygon(mnew, t->d0, trnum, TR_FROM_UP);
        } else {
          mnew = make_new_monotone_poly(mcur, v0, v1);
          traverse_polygon(mcur, t->d1, trnum, TR_FROM_UP);
          traverse_polygon(mcur, t->d0, trnum, TR_FROM_UP);
          traverse_polygon(mnew, t->u0, trnum, TR_FROM_DN);
          traverse_polygon(mnew, t->u1, trnum, TR_FROM_DN);
        }
      } else { /* no split possible */
        retval = SP_NOSPLIT;
        traverse_polygon(mcur, t->u0, trnum, TR_FROM_DN);
        traverse_polygon(mcur, t->d0, trnum, TR_FROM_UP);
        traverse_polygon(mcur, t->u1, trnum, TR_FROM_DN);
        traverse_polygon(mcur, t->d1, trnum, TR_FROM_UP);
      }
    }
  }

  return retval;
}

/* Main routine to get monotone polygons from the trapezoidation of the polygon.
 */
int
monotonate_trapezoids(int n)
{
  // Global:

  register int i;
  int tr_start;

  memset((void *) vert, 0, sizeof(vert));
  memset((void *) visited, 0, sizeof(visited));
  memset((void *) mchain, 0, sizeof(mchain));
  memset((void *) mon, 0, sizeof(mon));

  /* First locate a trapezoid which lies inside the polygon */
  /* and which is triangular */
  for (i = 0; i < TRSIZE; i++)
    if (inside_polygon(&tr[i]))
      break;
  tr_start = i;

  /* Initialise the mon data-structure and start spanning all the */
  /* trapezoids within the polygon */

  for (i = 1; i <= n; i++) {
    mchain[i].prev = i - 1;
    mchain[i].next = i + 1;
    mchain[i].vnum = i;
    vert[i].pt = seg[i].v0;
    vert[i].vnext[0] = i + 1; /* next vertex */
    vert[i].vpos[0] = i; /* locn. of next vertex */
    vert[i].nextfree = 1;
  }
  mchain[1].prev = n;
  mchain[n].next = 1;
  vert[n].vnext[0] = 1;
  vert[n].vpos[0] = n;
  chain_idx = n;
  mon_idx = 0;
  mon[0] = 1; /* position of any vertex in the first */
  /* chain  */

  /* traverse the polygon */
  if (tr[tr_start].u0 > 0)
    traverse_polygon(0, tr_start, tr[tr_start].u0, TR_FROM_UP);
  else if (tr[tr_start].d0 > 0)
    traverse_polygon(0, tr_start, tr[tr_start].d0, TR_FROM_DN);

  /* return the number of polygons created */
  return newmon();
}

/* A greedy corner-cutting algorithm to triangulate a y-monotone polygon in O(n) time.
 * Joseph O-Rourke, Computational Geometry in C.
 */
int
triangulate_single_polygon(int posmax, int side, int op[][3])
{
  // Global:

  register int v;
  int rc[SEGSIZE], ri = 0; /* reflex chain */
  int endv, tmp, vpos;

  if (side == TRI_RHS) { /* RHS segment is a single segment */
    rc[0] = mchain[posmax].vnum;
    tmp = mchain[posmax].next;
    rc[1] = mchain[tmp].vnum;
    ri = 1;

    vpos = mchain[tmp].next;
    v = mchain[vpos].vnum;

    if ((endv = mchain[mchain[posmax].prev].vnum) == 0)
      endv = global.nseg;
  } else { /* LHS is a single segment */
    tmp = mchain[posmax].next;
    rc[0] = mchain[tmp].vnum;
    tmp = mchain[tmp].next;
    rc[1] = mchain[tmp].vnum;
    ri = 1;

    vpos = mchain[tmp].next;
    v = mchain[vpos].vnum;

    endv = mchain[posmax].vnum;
  }

  while ((v != endv) || (ri > 1)) {
    if (ri > 0) { /* reflex chain is non-empty */
      if (CROSS(vert[v].pt, vert[rc[ri - 1]].pt, vert[rc[ri]].pt) > 0) { /* convex corner: cut if off */
        op[op_idx][0] = rc[ri - 1];
        op[op_idx][1] = rc[ri];
        op[op_idx][2] = v;
        op_idx++;
        ri--;
      } else { /* non-convex */
        /* add v to the chain */
        ri++;
        rc[ri] = v;
        vpos = mchain[vpos].next;
        v = mchain[vpos].vnum;
      }
    } else { /* reflex-chain empty: add v to the */
      /* reflex chain and advance it  */
      rc[++ri] = v;
      vpos = mchain[vpos].next;
      v = mchain[vpos].vnum;
    }
  } /* end-while */

  /* reached the bottom vertex. Add in the triangle formed */
  op[op_idx][0] = rc[ri - 1];
  op[op_idx][1] = rc[ri];
  op[op_idx][2] = v;
  op_idx++;
  ri--;

  return 0;
}

void
triangulate_monotone_polygons(int nmonpoly, int op[][3])
{
  // Global:

  register int i;
  point_t ymax, ymin;
  int p, vfirst, posmax, posmin, v;
  int vcount;

#ifdef DEBUG
  for (i = 0; i < nmonpoly; i++) {
    fprintf(stderr, "\n\nPolygon %d: ", i);
    vfirst = mchain[mon[i]].vnum;
    p = mchain[mon[i]].next;
    fprintf(stderr, "%d ", mchain[mon[i]].vnum);
    while (mchain[p].vnum != vfirst)
      {
        fprintf(stderr, "%d ", mchain[p].vnum);
        p = mchain[p].next;
      }
  }
  fprintf(stderr, "\n");
#endif

  op_idx = 0;
  for (i = 0; i < nmonpoly; i++) {
    vcount = 1;
    vfirst = mchain[mon[i]].vnum;
    ymax = ymin = vert[vfirst].pt;
    posmax = posmin = mon[i];
    p = mchain[mon[i]].next;
    while ((v = mchain[p].vnum) != vfirst) {
      if (_greater_than(&vert[v].pt, &ymax)) {
        ymax = vert[v].pt;
        posmax = p;
      }
      if (_less_than(&vert[v].pt, &ymin)) {
        ymin = vert[v].pt;
        posmin = p;
      }
      p = mchain[p].next;
      vcount++;
    }

    if (vcount == 3) { /* already a triangle */
      op[op_idx][0] = mchain[p].vnum;
      op[op_idx][1] = mchain[mchain[p].next].vnum;
      op[op_idx][2] = mchain[mchain[p].prev].vnum;
      op_idx++;
    } else { /* triangulate the polygon */
      v = mchain[mchain[posmax].next].vnum;
      if (_equal_to(&vert[v].pt, &ymin)) { /* LHS is a single line */
        triangulate_single_polygon(posmax, TRI_LHS, op);
      } else
        triangulate_single_polygon(posmax, TRI_RHS, op);
    }
  }

#ifdef DEBUG
  for (i = 0; i < (global.nseg - 2); i++)
    fprintf(stderr, "tri #%d: (%d, %d, %d)\n", i, op[i][0], op[i][1], op[i][2]);
#endif
}

/**************************************************************************************************/

int
initialise(int nseg)
{
  // Global:

  register int i;

  for (i = 1; i <= nseg; i++)
    seg[i].is_inserted = false;
  generate_random_ordering(nseg);

  return 0;
}

#ifdef STANDALONE

/*
int
main(int argc, char *argv[])
{
  int n, nmonpoly;
  FILE *infile;
  int op[SEGSIZE][3], i;

  if (argc < 2) {
    fprintf(stderr, "usage: triangulate <filename>\n");
    exit(1);
  } else if ((infile = fopen(argv[1], "r")) == NULL) {
    perror(argv[1]);
    exit(1);
  }

#ifdef CLOCK
  clock();
#endif

  n = read_segments(infile);
  for (i = 0; i < 1; i++) {
    initialise(n);
    construct_trapezoids(n, seg);
    nmonpoly = monotonate_trapezoids(n);
    triangulate_monotone_polygons(nmonpoly, op);
  }

#ifdef CLOCK
  printf("CPU time used: %ld microseconds\n", clock());
#endif

  return 0;
}
*/

#else /* Not standalone. Use this as an interface routine */

/* The points constituting the polygon are specified in anticlockwise
 * order. If there are n points in the polygon, it would be the
 * points p0, p1....p(n) (where p0 and pn are the same point). The
 * output is contained in the array "triangles".
 *
 * Every triangle is output in anticlockwise order and the 3
 * integers are the indices of the points. Thus, the triangle (i, j, k)
 * refers to the triangle formed by the points (pi, pj, pk). Before
 * using this routine, please check-out that you do not conflict with
 * the global variables defined in basic.h.
 *
 * n:         number of points in polygon (p0 = pn)
 * vertices:  the vertices p0, p1..., p(n) of the polygon
 * triangles: output array containing the triangles
 */

int
triangulate_polygon(int n, double vertices[][2], int triangles[][3])
{
  // Global:

  register int i;
  int nmonpoly;

  memset((void *) seg, 0, sizeof(seg));
  for (i = 1; i <= n; i++) {
    seg[i].is_inserted = false;

    seg[i].v0.x = vertices[i][0]; /* x-cood */
    seg[i].v0.y = vertices[i][1]; /* y-cood */
    if (i == 1) {
      seg[n].v1.x = seg[i].v0.x;
      seg[n].v1.y = seg[i].v0.y;
    } else {
      seg[i - 1].v1.x = seg[i].v0.x;
      seg[i - 1].v1.y = seg[i].v0.y;
    }
  }

  global.nseg = n;
  initialise(n);
  construct_trapezoids(n, seg);
  nmonpoly = monotonate_trapezoids(n);
  triangulate_monotone_polygons(nmonpoly, triangles);

  return 0;
}

/* This function returns true or false depending upon whether the
 * vertex is inside the polygon or not. The polygon must already have
 * been triangulated before this routine is called.
 * This routine will always detect all the points belonging to the
 * set (polygon-area - polygon-boundary). The return value for points
 * on the boundary is not consistent!!!
 */
/*
int
is_point_inside_polygon(double vertex[2])
{
  point_t v;
  int trnum, rseg;
  trap_t *t;

  v.x = vertex[0];
  v.y = vertex[1];

  trnum = locate_endpoint(&v, &v, 1);
  t = &tr[trnum];

  if (t->state == ST_INVALID)
    return false;

  if ((t->lseg <= 0) || (t->rseg <= 0))
    return false;
  rseg = t->rseg;
  return _greater_than_equal_to(&seg[rseg].v1, &seg[rseg].v0);
}
*/

#endif

/***************************************************************************************************
 *
 * End
 *
 **************************************************************************************************/