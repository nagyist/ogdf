/** \file
 * \brief Declaration of doubly linked lists and iterators
 *
 * \author Gereon Bartel
 *
 * \par License:
 * This file is part of the Open Graph Drawing Framework (OGDF).
 *
 * \par
 * Copyright (C)<br>
 * See README.md in the OGDF root directory for details.
 *
 * \par
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * Version 2 or 3 as published by the Free Software Foundation;
 * see the file LICENSE.txt included in the packaging of this file
 * for details.
 *
 * \par
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * \par
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, see
 * http://www.gnu.org/copyleft/gpl.html
 */

#include <ogdf/basic/EpsilonTest.h>
#include <ogdf/basic/Graph.h>
#include <ogdf/basic/GraphAttributes.h>
#include <ogdf/basic/GraphList.h>
#include <ogdf/basic/List.h>
#include <ogdf/basic/basic.h>
#include <ogdf/basic/geometry.h>
#include <ogdf/energybased/multilevel_mixer/MultilevelGraph.h>
#include <ogdf/graphalg/ConvexHull.h>

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

namespace ogdf {

ConvexHull::ConvexHull() { }

ConvexHull::~ConvexHull() { }

bool ConvexHull::sameDirection(const DPoint& start, const DPoint& end, const DPoint& s,
		const DPoint& e) const {
	DPoint a = (start - end);
	DPoint b = (s - e);
	DPoint c = a + b;
	double len2_a = a.m_x * a.m_x + a.m_y * a.m_y;
	double len2_b = b.m_x * b.m_x + b.m_y * b.m_y;
	double len2_c = c.m_x * c.m_x + c.m_y * c.m_y;

	return len2_c > max(len2_a, len2_b);
}

DPoint ConvexHull::calcNormal(const DPoint& start, const DPoint& end) const {
	double len = sqrt((end.m_x - start.m_x) * (end.m_x - start.m_x)
			+ (end.m_y - start.m_y) * (end.m_y - start.m_y));
	return DPoint((start.m_y - end.m_y) / len, (end.m_x - start.m_x) / len);
}

double ConvexHull::leftOfLine(const DPoint& normal, const DPoint& point,
		const DPoint& pointOnLine) const {
	return (point.m_x - pointOnLine.m_x) * normal.m_x + (point.m_y - pointOnLine.m_y) * normal.m_y;
}

// calculates a convex hull very quickly but only works with cross-free Polygons!
// polygon order (cw/ccw) must be set correctly
DPolygon ConvexHull::conv(const DPolygon& poly) const {
	DPolygon res(poly);

	DPolygon::iterator lastChange = res.cyclicPred(res.begin());
	for (DPolygon::iterator i = res.begin(); i != lastChange;) {
		DPolygon::iterator h = res.cyclicPred(i);
		DPolygon::iterator g = res.cyclicPred(h);

		if ((*i) == (*h)) {
			res.del(h);
			lastChange = g;
			continue;
		}

		if (g == i) {
			i = res.cyclicSucc(i);
			continue;
		}

		DPoint norm = calcNormal(*h, *i);
		if ((res.counterclock() && leftOfLine(norm, *g, *h) <= 0.0)
				|| (!res.counterclock() && leftOfLine(norm, *g, *h) >= 0.0)) {
			res.del(h);
			lastChange = g;
		} else {
			i = res.cyclicSucc(i);
		}
	}

	return res;
}

void ConvexHull::leftHull(std::vector<DPoint> points, DPoint& start, DPoint& end,
		DPolygon& hullPoly) const {
	// delete points faster by switching with last in vector!
	DPoint q1;
	DPoint q2;
	unsigned int indexQ1;
	unsigned int indexQ2;
	for (;;) {
		if (points.size() == 1) {
			hullPoly.pushBack(points.front());
		}
		if (points.size() <= 1) {
			return;
		}

		indexQ1 = randomNumber(0, (int)points.size() - 1);
		q1 = points[indexQ1];
		indexQ2 = randomNumber(0, (int)points.size() - 1);
		q2 = points[indexQ2];

		if (q1 == q2) {
			if (indexQ1 != indexQ2) {
				points[indexQ2] = points.back();
				points.pop_back();
			}
			continue;
		}

		DPolygon triangle(false);
		triangle.pushBack(start);
		triangle.pushBack(q2);
		triangle.pushBack(end);
		if (triangle.containsPoint(q1)) {
			points[indexQ1] = points.back();
			points.pop_back();
			continue;
		}
		triangle.clear();
		triangle.pushBack(start);
		triangle.pushBack(q1);
		triangle.pushBack(end);
		if (triangle.containsPoint(q2)) {
			points[indexQ2] = points.back();
			points.pop_back();
			continue;
		}

		break;
	}

	if ((leftOfLine(calcNormal(q1, q2), start, q1) >= 0.0)
			&& (leftOfLine(calcNormal(q1, q2), end, q1) >= 0.0)) {
		std::swap(q1, q2);
		std::swap(indexQ1, indexQ2);
	}

	double dist = 0.0;
	DPoint normal = calcNormal(q1, q2);
	std::vector<int> qCandidates;
	std::vector<double> qDistances;
	for (unsigned int i = 0; i < points.size(); i++) {
		double d = leftOfLine(normal, points[i], q1);
		if (!OGDF_GEOM_ET.greater(dist, d)) {
			if (OGDF_GEOM_ET.greater(d, dist)) {
				qCandidates.clear();
				qDistances.clear();
				dist = d;
			}
			if (d > dist) {
				dist = d;
			}
			qCandidates.push_back(i);
			qDistances.push_back(d);
		}
	}
	for (unsigned int i = 0; i < qCandidates.size();) {
		if (OGDF_GEOM_ET.less(qDistances[i], dist)) {
			qCandidates[i] = qCandidates.back();
			qCandidates.pop_back();
			qDistances[i] = qDistances.back();
			qDistances.pop_back();
		} else {
			i++;
		}
	}

	unsigned int indexQ = 0;
	DPoint q(points[qCandidates[0]]);
	for (unsigned int i = 0; i < qCandidates.size(); i++) {
		if (indexQ != i && sameDirection(points[qCandidates[i]], q, q1, q2)) {
			q = points[qCandidates[i]];
			indexQ = i;
		}
	}
	indexQ = qCandidates[indexQ];

	OGDF_ASSERT(q2 != q || q1 == q);

	int del[3];
	del[0] = indexQ;
	del[1] = indexQ1;
	del[2] = indexQ2;
	std::sort(del, del + 3);

	int last = (int)points.size();
	for (int i = 2; i >= 0; i--) {
		if (del[i] < last) {
			points[del[i]] = points.back();
			points.pop_back();
			last = del[i];
		}
	}

	std::vector<DPoint> lPoints;
	std::vector<DPoint> rPoints;
	DPoint sqNormal = calcNormal(start, q);
	DPoint qeNormal = calcNormal(q, end);

	if (indexQ1 != indexQ) {
		if (OGDF_GEOM_ET.greater(leftOfLine(sqNormal, q1, q), 0.0)) {
			lPoints.push_back(q1);
			OGDF_ASSERT(!OGDF_GEOM_ET.greater(leftOfLine(qeNormal, q1, q), 0.0));
		} else if (OGDF_GEOM_ET.greater(leftOfLine(qeNormal, q1, q), 0.0)) {
			rPoints.push_back(q1);
		}
	}

	if (indexQ2 != indexQ) {
		if (OGDF_GEOM_ET.greater(leftOfLine(sqNormal, q2, q), 0.0)) {
			lPoints.push_back(q2);
			OGDF_ASSERT(!OGDF_GEOM_ET.greater(leftOfLine(qeNormal, q2, q), 0.0));
		} else if (OGDF_GEOM_ET.greater(leftOfLine(qeNormal, q2, q), 0.0)) {
			rPoints.push_back(q2);
		}
	}

	if (!points.empty()) {
		DPolygon inner(false);
		inner.pushBack(start);
		inner.pushBack(q1);
		inner.pushBack(q);
		inner.pushBack(q2);
		inner.pushBack(end);
		inner = conv(inner);

		while (!points.empty()) {
			int indexP = randomNumber(0, (int)points.size() - 1);
			DPoint p = points[indexP];
			points[indexP] = points.back();
			points.pop_back();

			if (inner.containsPoint(p)) {
				continue;
			}

			if (points.size() > 0) {
				// add p to inner2
				DPolygon inner2(inner);
				DPolygon::iterator nearest = inner2.begin();
				dist = p.distance(*nearest);
				for (DPolygon::iterator ip = inner2.begin(); ip != inner2.end(); ++ip) {
					double d = p.distance(*ip);
					if (d > dist) {
						dist = d;
						nearest = ip;
					}
				}
				// after x insert p x
				inner2.insert(DPoint(*nearest), nearest);
				inner2.insert(p, nearest);
				inner2 = conv(inner2);

				int deletes = 0;
				int times = 2; // experimentally determine this value
				DPoint r;
				int indexR;
				do {
					indexR = randomNumber(0, (int)points.size() - 1);
					r = points[indexR];
					if (inner2.containsPoint(r)) {
						points[indexR] = points.back();
						points.pop_back();
						deletes++;
						times++; // bonus try if a point was deleted
					} else {
						times--;
					}
				} while (times > 0 && points.size() > 0);

				if ((deletes > 0 && inner2.size() <= 10) || inner2.size() <= inner.size()) {
					inner = inner2;
				} else if (deletes == 0) {
					points[indexR] = p;
					p = r;
				}
			}

			if (OGDF_GEOM_ET.greater(leftOfLine(sqNormal, p, q), 0.0)) {
				lPoints.push_back(p);
				OGDF_ASSERT(!OGDF_GEOM_ET.greater(leftOfLine(qeNormal, p, q), 0.0));
			} else if (OGDF_GEOM_ET.greater(leftOfLine(qeNormal, p, q), 0.0)) {
				rPoints.push_back(p);
			}
		}
	}

	leftHull(lPoints, start, q, hullPoly);
	hullPoly.pushBack(q);
	leftHull(rPoints, q, end, hullPoly);
}

DPolygon ConvexHull::call(std::vector<DPoint> points) const {
	DPolygon hullPoly(false);
	if (points.empty()) {
		return hullPoly;
	}

	if (points.size() <= 2) {
		hullPoly.pushBack(points.front());

		if (points.back() != points.front()) {
			hullPoly.pushBack(points.back());
		}
		return hullPoly;
	}

	// Find extremes in +-x, +-y, +-(x+-y)
	DPoint xpoints[8];
	double extremes[8];
	unsigned int xIndex[8];
	for (int i = 0; i < 8; i++) {
		xpoints[i] = *(points.begin());
		extremes[i] = 0.0;
		xIndex[i] = 0;
	}

	int num = 0;
	for (int ix = -1; ix <= 1; ix++) {
		for (int iy = -1; iy <= 1; iy++) {
			if (ix == 0 && iy == 0) {
				continue;
			}
			DPoint zero(0.0, 0.0);
			DPoint normal = calcNormal(zero, DPoint(ix, iy));
			for (unsigned int it = 0; it < points.size(); it++) {
				double dist = leftOfLine(normal, points[it], zero);
				if (dist >= extremes[num]) {
					xpoints[num] = points[it];
					extremes[num] = dist;
					xIndex[num] = it;
				}
			}
			num++;
		}
	}

	std::sort(xIndex, xIndex + 8);

	// delete known extremes
	unsigned int last = (unsigned int)points.size();
	for (int i = 7; i >= 0; i--) {
		if (xIndex[i] >= points.size() || xIndex[i] >= last) {
			continue;
		}
		last = xIndex[i];
		points[xIndex[i]] = points.back();
		points.pop_back();
	}

	// Make Polygon
	DPolygon poly(false);
	poly.pushBack(xpoints[0]);
	poly.pushBack(xpoints[1]);
	poly.pushBack(xpoints[2]);
	poly.pushBack(xpoints[4]);
	poly.pushBack(xpoints[7]);
	poly.pushBack(xpoints[6]);
	poly.pushBack(xpoints[5]);
	poly.pushBack(xpoints[3]);
	poly = conv(poly);

	// Subdivide into Pointlists
	std::vector<DPoint> normals;
	for (DPolygon::iterator i = poly.begin(); i != poly.end(); ++i) {
		DPolygon::iterator j = poly.cyclicSucc(i);
		normals.push_back(calcNormal(*i, *j));
	}
	std::vector<std::vector<DPoint>> pointArray;
	pointArray.resize(poly.size());
	for (const DPoint& p : points) {
		int component = 0;
		DPolygon::iterator sp = poly.begin();
		DPolygon::iterator spn = poly.cyclicSucc(sp);
		for (std::vector<DPoint>::iterator n = normals.begin(); n != normals.end(); ++n, ++sp) {
			if ((*sp != p) && (*spn != p) && OGDF_GEOM_ET.greater(leftOfLine(*n, p, *sp), 0.0)) {
				pointArray[component].push_back(p);
				break;
			}
			component++;
		}
	}

	int component = 0;
	for (DPolygon::iterator i = poly.begin(); i != poly.end(); ++i, ++component) {
		hullPoly.pushBack(*i);
		DPolygon::iterator j = poly.cyclicSucc(i);
		leftHull(pointArray[component], *i, *j, hullPoly);
	}

	return conv(hullPoly);
}

DPolygon ConvexHull::call(MultilevelGraph& MLG) const {
	std::vector<DPoint> points;

	for (node v : MLG.getGraph().nodes) {
		points.push_back(DPoint(MLG.x(v), MLG.y(v)));
	}

	return call(points);
}

DPolygon ConvexHull::call(GraphAttributes& GA) const {
	std::vector<DPoint> points;

	for (node v : GA.constGraph().nodes) {
		points.push_back(GA.point(v));
	}

	return call(points);
}

}
