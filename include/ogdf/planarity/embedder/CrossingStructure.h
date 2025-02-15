/** \file
 * \brief Declaration of class CrossingStructure.
 *
 * \author Carsten Gutwenger
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

#pragma once

#include <ogdf/basic/Graph.h>
#include <ogdf/basic/SList.h>

namespace ogdf {
class GraphCopy;
class PlanRep;

namespace embedder {

class CrossingStructure {
public:
	CrossingStructure() : m_numCrossings(0), m_weightedCrossingNumber(0) { }

	void init(GraphCopy& PG, int weightedCrossingNumber);

	/**
	 * @warning The order of adjEntries around each node will not be restored.
	 * In particular, the order of edges around a dummy node may not reflect a
	 * crossing anymore. In this case, \p PG can be embedded via e.g.
	 * planarEmbed() and pseudo crossings can be removed afterwards via
	 * GraphCopy::removePseudoCrossings().
	 */
	void restore(PlanRep& PG, int cc);

	int numberOfCrossings() const { return m_numCrossings; }

	int weightedCrossingNumber() const { return m_weightedCrossingNumber; }

	const SListPure<int>& crossings(edge e) const { return m_crossings[e]; }

private:
	int m_numCrossings;
	int m_weightedCrossingNumber;
	EdgeArray<SListPure<int>> m_crossings;
};

}
}
